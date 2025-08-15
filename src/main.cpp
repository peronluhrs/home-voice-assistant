#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <iomanip>
#if __has_include(<filesystem>)
#include <filesystem>
#else
#include <experimental/filesystem>
namespace std { namespace filesystem = experimental::filesystem; }
#endif
#include <chrono>
#include <ctime>
#include <sstream>

#include "OpenAIClient.h"
#include "Audio.h"
#include "Utils.h"
#include "Memory.h"

#ifdef WITH_VOSK
#include "AsrVosk.h"
#endif
#ifdef WITH_PIPER
#include "TtsPiper.h"
#endif
#ifdef WITH_AUDIO
#include "Audio.h"
#endif

#ifdef WITH_HTTP
#include "HttpServer.h"
#include <thread>
#endif

// --- config minimale (fallback si pas d'Env.h)
struct AppCfg {
    std::string apiBase = "http://localhost:8000/v1";
    std::string apiKey  = "EMPTY";
    std::string model   = "gpt-4o-mini";
};
static AppCfg loadCfg(const std::string& path) {
    AppCfg c;
    std::ifstream f(path);
    if (!f) return c;
    std::string line;
    auto trim = [](std::string s){
        auto issp=[](unsigned char ch){return std::isspace(ch);};
        while(!s.empty()&&issp(s.front())) s.erase(s.begin());
        while(!s.empty()&&issp(s.back()))  s.pop_back();
        return s;
    };
    while (std::getline(f,line)) {
        if (line.empty()||line[0]=='#') continue;
        auto p=line.find('=');
        if (p==std::string::npos) continue;
        std::string k=trim(line.substr(0,p)), v=trim(line.substr(p+1));
        if (k=="API_BASE") c.apiBase=v; else if (k=="API_KEY") c.apiKey=v; else if (k=="MODEL") c.model=v;
    }
    return c;
}

// Generates a timestamp string.
static std::string generateTimestamp(const std::string& format = "%Y-%m-%dT%H:%M:%SZ") {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &in_time_t);
#else
    localtime_r(&in_time_t, &tm_buf);
#endif
    std::stringstream ss;
    ss << std::put_time(&tm_buf, format.c_str());
    return ss.str();
}

static std::string intentTypeToString(IntentType type) {
    switch (type) {
        case IntentType::NONE: return "NONE";
        case IntentType::NOTE_ADD: return "NOTE_ADD";
        case IntentType::REMINDER_ADD: return "REMINDER_ADD";
        case IntentType::FACT_SET: return "FACT_SET";
        default: return "UNKNOWN";
    }
}

#ifdef WITH_AUDIO
static void printDeviceList() {
    auto devices = Audio::listDevices();
    if (devices.empty()) {
        std::cout << "No audio devices found.\n";
        return;
    }

    // Find column widths
    size_t maxName = 10, maxApi = 8;
    for(const auto& d : devices) {
        if (d.name.length() > maxName) maxName = d.name.length();
        if (d.hostApi.length() > maxApi) maxApi = d.hostApi.length();
    }

    // Print header
    std::cout << std::left
              << std::setw(5) << "Index"
              << std::setw(maxName + 2) << "Name"
              << std::setw(maxApi + 2) << "Host API"
              << std::setw(8) << "In/Out"
              << "Supported Sample Rates (kHz)\n";
    std::cout << std::string(5 + maxName + 2 + maxApi + 2 + 8 + 30, '-') << "\n";

    // Print device info
    for (const auto& d : devices) {
        std::string channels = std::to_string(d.maxInputChannels) + "/" + std::to_string(d.maxOutputChannels);
        std::cout << std::left
                  << std::setw(5) << d.id
                  << std::setw(maxName + 2) << d.name
                  << std::setw(maxApi + 2) << d.hostApi
                  << std::setw(8) << channels;

        for(size_t i = 0; i < d.supportedSampleRates.size(); ++i) {
            std::cout << (d.supportedSampleRates[i] / 1000.0) << (i < d.supportedSampleRates.size() - 1 ? ", " : "");
        }
        std::cout << "\n";
    }
}
#endif

#ifdef WITH_PIPER
#include "TtsPiper.h"
#endif

#include "Memory.h"

struct Args {
    bool help = false;
    bool offline = false;
    bool withAudio = false;
    bool listDevices = false;
    int recordSeconds = 5;
    std::string inKey, outKey;
#ifdef WITH_VOSK
    // ASR options
    bool withVosk = false;
    std::string voskModel;
    std::string sttFromWav;
    std::string sttDumpJson;
#endif
    // Piper TTS options
    bool withPiper = false;
    std::string piperBin;
    std::string piperModel;
    std::string say;
    // PTT mode
    bool ptt = false;
    std::string saveWavPath;
    double sampleRateIn = 16000.0;

    // Memory store options
    std::vector<std::string> memSet;
    std::string memGet;
    std::string memDel;
    bool memList = false;
    std::string noteAdd;
    bool noteList = false;
    std::string noteDel;
    std::string remAdd;
    std::string remWhen;
    bool remList = false;
    std::string remDone;

    // Conversation loop options
    bool loop = false;
    int loopMaxTurns = 0;
    int loopPttSeconds = 10;
    std::string loopSaveWavs;
    std::string logJsonl;

    // HTTP Server options
    bool http = false;
    std::string httpHost = "127.0.0.1";
    int httpPort = 8787;
    std::string httpBearer;
    bool noWs = false;
};

static void printHelp() {
    std::cout << "Usage: home_assistant [options]\n\n"
              << "General Options:\n"
              << "  --help                Show this help message and exit.\n"
              << "  --offline             Run in offline mode (no API calls, echoes input).\n\n"
              << "Audio Options (require building with -DWITH_AUDIO=ON):\n"
              << "  --with-audio          Enable audio input/output via PortAudio.\n"
              << "  --list-devices        List available audio devices and exit.\n"
              << "  --input-device <key>  Keyword or index for input device.\n"
              << "  --output-device <key> Keyword or index for output device.\n"
              << "  --record-seconds <N>  Hard cap for recording duration (default: 5s).\n"
              << "  --ptt                 Enable Push-to-Talk mode (press Enter to start/stop).\n"
              << "  --save-wav <path>     If set, save capture to WAV. If path is a dir, use a timestamped filename.\n"
              << "  --sample-rate-in <Hz> Requested input sample rate (default: 16000). Falls back if unsupported.\n\n"
              << "ASR/TTS Options:\n"
#ifdef WITH_VOSK
              << "  --with-vosk           Enable Vosk ASR feature path (e.g. for PTT transcription).\n"
              << "  --vosk-model <dir>    Path to the Vosk model directory.\n"
              << "  --stt-from-wav <path> Transcribe a WAV file and print the text (no audio stack needed).\n"
              << "  --stt-dump-json <path> Optional: write full ASR result to a JSON file.\n"
#endif
              << "  --with-piper          Enable Piper TTS (requires build with -DWITH_PIPER=ON).\n"
              << "  --piper-bin <path>    Optional path to the 'piper' executable.\n"
              << "  --piper-model <path>  Path to the Piper TTS model file (.onnx), required if --with-piper.\n"
              << "  --say \"<text>\"        Synthesize and speak text using Piper, then exit.\n\n"
              << "Memory/State Options:\n"
              << "  --mem-set <key> <val> Set a key-value fact.\n"
              << "  --mem-get <key>       Get a key-value fact.\n"
              << "  --mem-del <key>       Delete a key-value fact.\n"
              << "  --mem-list            List all key-value facts.\n"
              << "  --note-add \"<text>\"   Add a note.\n"
              << "  --note-list           List all notes.\n"
              << "  --note-del <id>       Delete a note by its ID.\n"
              << "  --rem-add \"<text>\"    Add a reminder.\n"
              << "  --rem-when <ISO>      Set the time for the next reminder (e.g., 2024-05-20T10:00:00).\n"
              << "  --rem-list            List all reminders.\n"
              << "  --rem-done <id>       Mark a reminder as done.\n\n"
              << "Conversation Loop Options:\n"
              << "  --loop                Start the interactive conversation loop.\n"
              << "  --loop-max-turns <N>  Exit after N turns (default: 0 = unlimited).\n"
              << "  --loop-ptt-seconds <N>  Fallback cap if VAD doesn’t stop (default: 10s).\n"
              << "  --loop-save-wavs <dir> If set, save input WAVs + TTS WAVs in that dir.\n"
              << "  --log-jsonl <path>    Append JSONL logs of each turn.\n\n"
              << "HTTP Server Options (require building with -DWITH_HTTP=ON):\n"
              << "  --http                Enable HTTP server.\n"
              << "  --http-host <host>    HTTP server host (default: 127.0.0.1).\n"
              << "  --http-port <int>     HTTP server port (default: 8787).\n"
              << "  --http-bearer <token> Optional bearer token for authentication.\n"
              << "  --no-ws               Disable WebSocket events.\n";
}

static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        auto next = [&](std::string& dst){ if (i + 1 < argc) dst = argv[++i]; };

        if (s == "--help") a.help = true;
        else if (s == "--offline") a.offline = true;
        else if (s == "--with-audio") a.withAudio = true;
        else if (s == "--list-devices") a.listDevices = true;
        else if (s == "--record-seconds") { std::string v; next(v); a.recordSeconds = std::max(1, std::atoi(v.c_str())); }
        else if (s == "--input-device") next(a.inKey);
        else if (s == "--output-device") next(a.outKey);
#ifdef WITH_VOSK
        // ASR
        else if (s == "--with-vosk") a.withVosk = true;
        else if (s == "--vosk-model") next(a.voskModel);
        else if (s == "--stt-from-wav") next(a.sttFromWav);
        else if (s == "--stt-dump-json") next(a.sttDumpJson);
#endif
        // PTT
        else if (s == "--ptt") { a.ptt = true; a.withAudio = true; }
        else if (s == "--save-wav") next(a.saveWavPath);
        else if (s == "--sample-rate-in") { std::string v; next(v); a.sampleRateIn = std::atof(v.c_str()); }
        // Piper args
        else if (s == "--with-piper") a.withPiper = true;
        else if (s == "--piper-bin") next(a.piperBin);
        else if (s == "--piper-model") next(a.piperModel);
        else if (s == "--say") next(a.say);
        // Memory args
        else if (s == "--mem-set") { if (i + 2 < argc) { a.memSet.push_back(argv[++i]); a.memSet.push_back(argv[++i]); } }
        else if (s == "--mem-get") next(a.memGet);
        else if (s == "--mem-del") next(a.memDel);
        else if (s == "--mem-list") a.memList = true;
        else if (s == "--note-add") next(a.noteAdd);
        else if (s == "--note-list") a.noteList = true;
        else if (s == "--note-del") next(a.noteDel);
        else if (s == "--rem-add") next(a.remAdd);
        else if (s == "--rem-when") next(a.remWhen);
        else if (s == "--rem-list") a.remList = true;
        else if (s == "--rem-done") next(a.remDone);
        // Loop args
        else if (s == "--loop") a.loop = true;
        else if (s == "--loop-max-turns") { std::string v; next(v); a.loopMaxTurns = std::max(0, std::atoi(v.c_str())); }
        else if (s == "--loop-ptt-seconds") { std::string v; next(v); a.loopPttSeconds = std::max(1, std::atoi(v.c_str())); }
        else if (s == "--loop-save-wavs") next(a.loopSaveWavs);
        else if (s == "--log-jsonl") next(a.logJsonl);
        // HTTP server args
        else if (s == "--http") a.http = true;
        else if (s == "--http-host") next(a.httpHost);
        else if (s == "--http-port") { std::string v; next(v); a.httpPort = std::atoi(v.c_str()); }
        else if (s == "--http-bearer") next(a.httpBearer);
        else if (s == "--no-ws") a.noWs = true;
    }
    // --say implies --with-piper
    if (!a.say.empty()) {
        a.withPiper = true;
    }
    return a;
}

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);

    if (args.help) {
        printHelp();
        return 0;
    }

#ifdef WITH_HTTP
    std::unique_ptr<HttpServer> http_server;
#endif

#ifdef WITH_AUDIO
    Audio audio;
#endif
#ifdef WITH_VOSK
    AsrVosk asr(args.voskModel);
#endif
#ifdef WITH_PIPER
    TtsPiper piper(args.piperModel, args.piperBin);
#endif

    if (args.loop) {
        std::cout << "[loop] Starting conversation loop..." << std::endl;

#ifdef WITH_AUDIO
        int inIdx = -1, outIdx = -1;
        if (args.withAudio) {
            inIdx = Audio::findDevice(args.inKey, false);
            if (!args.inKey.empty() && inIdx == -1) {
                std::cerr << "Error: Could not find requested input device: " << args.inKey << std::endl;
                return 1;
            }
            outIdx = Audio::findDevice(args.outKey, true);
            if (!args.outKey.empty() && outIdx == -1) {
                std::cerr << "Error: Could not find requested output device: " << args.outKey << std::endl;
                return 1;
            }
        }
#endif

#ifdef WITH_VOSK
        if (args.withVosk && !asr.isAvailable()) {
            std::cerr << "[asr] Vosk is enabled but model not loaded: " << asr.lastError() << ". ASR will be skipped." << std::endl;
        }
#endif

#ifdef WITH_PIPER
        if (args.withPiper && !piper.isAvailable()) {
            std::cerr << "[tts] Piper is enabled but not available: " << piper.lastError() << ". TTS will be skipped." << std::endl;
        }
#endif

        MemoryStore mem;
        mem.load();

        AppCfg cfg = loadCfg("config/app.env");
        const char* e;
        if ((e=getenv("API_BASE"))) cfg.apiBase = e;
        if ((e=getenv("API_KEY" ))) cfg.apiKey  = e;
        if ((e=getenv("MODEL"   ))) cfg.model   = e;
        OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model, args.offline);
        std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

#ifdef WITH_HTTP
        if (args.http) {
            HttpOpts http_opts;
            http_opts.host = args.httpHost;
            http_opts.port = args.httpPort;
            http_opts.bearer = args.httpBearer;
            http_opts.enable_ws = !args.noWs;
            http_server = std::make_unique<HttpServer>(http_opts, &mem,
#ifdef WITH_PIPER
                &piper,
#else
                nullptr,
#endif
#ifdef WITH_VOSK
                &asr,
#else
                nullptr,
#endif
#ifdef WITH_AUDIO
                &audio
#else
                nullptr
#endif
            );
            http_server->start();
        }
#endif

        for (int turn = 1; args.loopMaxTurns == 0 || turn <= args.loopMaxTurns; ++turn) {
            std::cout << "\n--- Turn " << turn << " ---" << std::endl;

            std::vector<int16_t> pcm_data;
            double sample_rate = args.sampleRateIn;
            std::string input_wav_path;

#ifdef WITH_AUDIO
            if (args.withAudio) {
                std::cout << "[audio] Press Enter to start recording (" << args.loopPttSeconds << "s max)... " << std::flush;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Recording..." << std::endl;

                audio.recordPtt(inIdx, args.loopPttSeconds, sample_rate, pcm_data);

                if (pcm_data.empty()) {
                    std::cout << "[audio] No audio recorded, skipping turn." << std::endl;
                    continue;
                }
                std::cout << "[audio] Recorded " << pcm_data.size() << " samples." << std::endl;

                if (!args.loopSaveWavs.empty()) {
                    std::filesystem::path save_dir(args.loopSaveWavs);
                    try {
                        if (!std::filesystem::exists(save_dir)) {
                            std::filesystem::create_directories(save_dir);
                        }
                        std::string timestamp = generateTimestamp("%Y%m%d_%H%M%S");
                        std::string filename = "input_" + timestamp + ".wav";
                        std::filesystem::path full_path = save_dir / filename;
                        input_wav_path = full_path.string();
                        if (saveWav(input_wav_path, pcm_data, static_cast<int32_t>(sample_rate))) {
                            std::cout << "[audio] Input saved to " << input_wav_path << std::endl;
                        } else {
                            std::cerr << "[audio] Failed to save WAV to " << input_wav_path << std::endl;
                            input_wav_path = "";
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                         std::cerr << "[io] Error with path " << args.loopSaveWavs << ": " << e.what() << std::endl;
                         input_wav_path = "";
                    }
                }
            }
#endif

            std::string userText;

#ifdef WITH_VOSK
            if (args.withVosk && asr.isAvailable()) {
                if (!pcm_data.empty()) {
                    std::cout << "[asr] Transcribing..." << std::endl;
                    userText = asr.transcribe(pcm_data, sample_rate);
                    std::cout << "[asr] Transcript: \"" << userText << "\"" << std::endl;
                }
            }
#endif

            // If transcription is empty (e.g. silence, or ASR not used/available)
            if (userText.empty()) {
                if (args.offline) {
                    // This handles no-audio offline mode for the smoke test.
                    userText = "(no speech)";
                } else {
                    std::cout << "[loop] No user text, skipping to next turn." << std::endl;
                    continue;
                }
            }

            std::string assistantText;

            // 3. Intent/memory
            Intent intent = parseIntent(userText);
            if (intent.type != IntentType::NONE) {
                std::cout << "[intent] Recognized intent " << static_cast<int>(intent.type) << std::endl;
                bool mem_changed = false;
                switch (intent.type) {
                    case IntentType::NOTE_ADD:
                        mem.addNote(intent.text);
                        assistantText = "Note enregistrée.";
                        mem_changed = true;
                        break;
                    case IntentType::REMINDER_ADD:
                        mem.addReminder(intent.text, intent.when_iso);
                        assistantText = "Rappel ajouté pour " + intent.when_iso + ".";
                        mem_changed = true;
                        break;
                    case IntentType::FACT_SET:
                        mem.set(intent.key, intent.value);
                        assistantText = "Ok, j'ai noté " + intent.key + "=" + intent.value + ".";
                        mem_changed = true;
                        break;
                    case IntentType::NONE: break;
                }

                if (mem_changed) {
                    mem.save();
                }
            } else {
                // 4. LLM reply
                if (args.offline) {
                    assistantText = "(offline) Echo: " + userText;
                } else {
                    std::cout << "[llm] Sending to OpenAI..." << std::endl;
                    ChatResult r = client.chatOnce(userText);
                    if (r.ok) {
                        assistantText = r.text;
                    } else {
                        assistantText = "[error] " + (!r.error.empty() ? r.error : r.text);
                        std::cerr << "[llm] " << assistantText << std::endl;
                    }
                }
                std::cout << "[llm] Assistant reply: \"" << assistantText << "\"" << std::endl;
            }

            // 5. TTS
            bool tts_done = false;
#ifdef WITH_PIPER
            if (args.withPiper && piper.isAvailable()) {
                if (!assistantText.empty()) {
                    std::cout << "[tts] Synthesizing..." << std::endl;
                    double tts_sample_rate = 0;
                    std::vector<int16_t> tts_pcm = piper.synthesize(assistantText, tts_sample_rate);

                    if (!tts_pcm.empty()) {
                        tts_done = true;
#ifdef WITH_AUDIO
                        if (args.withAudio) {
                            std::cout << "[audio] Playing TTS..." << std::endl;
                            audio.playback(outIdx, tts_sample_rate, tts_pcm);
                        }
#endif
                        if (!args.loopSaveWavs.empty()) {
                            std::filesystem::path save_dir(args.loopSaveWavs);
                             try {
                                if (!std::filesystem::exists(save_dir)) {
                                    std::filesystem::create_directories(save_dir);
                                }
                                std::string timestamp = generateTimestamp("%Y%m%d_%H%M%S");
                                std::string filename = "tts_" + timestamp + ".wav";
                                std::filesystem::path full_path = save_dir / filename;
                                if (saveWav(full_path.string(), tts_pcm, static_cast<int32_t>(tts_sample_rate))) {
                                    std::cout << "[tts] TTS audio saved to " << full_path.string() << std::endl;
                                } else {
                                    std::cerr << "[tts] Failed to save TTS WAV to " << full_path.string() << std::endl;
                                }
                            } catch (const std::filesystem::filesystem_error& e) {
                                std::cerr << "[io] Error with path " << args.loopSaveWavs << ": " << e.what() << std::endl;
                            }
                        }
                    } else {
                        std::cerr << "[tts] TTS synthesis failed: " << piper.lastError() << std::endl;
                    }
                }
            }
#endif
            if (!tts_done && !assistantText.empty()) {
                // Fallback for no TTS
                std::cout << "ASSISTANT: " << assistantText << std::endl;
            }

            // 6. Logging
            if (!args.logJsonl.empty()) {
                nlohmann::json log_entry;
                log_entry["ts"] = generateTimestamp("%Y-%m-%dT%H:%M:%SZ");
                log_entry["input_wav"] = !input_wav_path.empty() ? nlohmann::json(input_wav_path) : nlohmann::json(nullptr);
                log_entry["transcript"] = userText;

                nlohmann::json intent_json;
                intent_json["type"] = intentTypeToString(intent.type);
                intent_json["key"] = (intent.type == IntentType::FACT_SET) ? nlohmann::json(intent.key) : nlohmann::json(nullptr);
                intent_json["value"] = (intent.type == IntentType::FACT_SET) ? nlohmann::json(intent.value) : nlohmann::json(nullptr);
                intent_json["when_iso"] = (intent.type == IntentType::REMINDER_ADD) ? nlohmann::json(intent.when_iso) : nlohmann::json(nullptr);
                log_entry["intent"] = intent_json;

                log_entry["assistant_text"] = assistantText;
                log_entry["tts_done"] = tts_done;

                std::ofstream log_file(args.logJsonl, std::ios::app);
                if (log_file) {
                    log_file << log_entry.dump() << std::endl;
                } else {
                    std::cerr << "[log] Failed to open log file for appending: " << args.logJsonl << std::endl;
                }
#ifdef WITH_HTTP
                if (http_server && http_server->isRunning() && !args.noWs) {
                    http_server->pushEvent(log_entry.dump());
                }
#endif
            }
        }

        std::cout << "[loop] Loop finished." << std::endl;
        return 0;
    }

#ifdef WITH_HTTP
    // If only http is enabled, keep the process alive
    if (args.http && !args.loop) {
        HttpOpts http_opts;
        http_opts.host = args.httpHost;
        http_opts.port = args.httpPort;
        http_opts.bearer = args.httpBearer;
        http_opts.enable_ws = !args.noWs;

        MemoryStore mem;
        mem.load();

        http_server = std::make_unique<HttpServer>(http_opts, &mem,
#ifdef WITH_PIPER
            &piper,
#else
            nullptr,
#endif
#ifdef WITH_VOSK
            &asr,
#else
            nullptr,
#endif
#ifdef WITH_AUDIO
            &audio
#else
            nullptr
#endif
        );
        http_server->start();

        std::cout << "[http] Server is running. Press Ctrl+C to exit." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
#endif

    // --- Memory CLI flags ---
    bool isMemoryOp = !args.memSet.empty() || !args.memGet.empty() || !args.memDel.empty() || args.memList
                   || !args.noteAdd.empty() || args.noteList || !args.noteDel.empty()
                   || !args.remAdd.empty() || args.remList || !args.remDone.empty();

    if (isMemoryOp) {
        MemoryStore mem;
        mem.load();

        bool changed = false;

        if (!args.memSet.empty()) {
            mem.set(args.memSet[0], args.memSet[1]);
            std::cout << "Set fact: " << args.memSet[0] << " = " << args.memSet[1] << std::endl;
            changed = true;
        } else if (!args.memGet.empty()) {
            std::string value;
            if (mem.get(args.memGet, value)) {
                std::cout << value << std::endl;
            } else {
                std::cerr << "Fact not found: " << args.memGet << std::endl;
                return 1;
            }
        } else if (!args.memDel.empty()) {
            if (mem.del(args.memDel)) {
                std::cout << "Deleted fact: " << args.memDel << std::endl;
                changed = true;
            } else {
                std::cerr << "Fact not found: " << args.memDel << std::endl;
                return 1;
            }
        } else if (args.memList) {
            auto facts = mem.listFacts();
            nlohmann::json j = nlohmann::json::object();
            for(const auto& p : facts) j[p.first] = p.second;
            std::cout << j.dump(2) << std::endl;
        } else if (!args.noteAdd.empty()) {
            std::string id = mem.addNote(args.noteAdd);
            std::cout << "Added note with ID: " << id << std::endl;
            changed = true;
        } else if (args.noteList) {
            auto notes = mem.listNotes();
            nlohmann::json j = nlohmann::json::array();
            for(const auto& n : notes) {
                j.push_back({{"id", n.id}, {"text", n.text}, {"created_at", n.created_at}});
            }
            std::cout << j.dump(2) << std::endl;
        } else if (!args.noteDel.empty()) {
            if (mem.deleteNote(args.noteDel)) {
                std::cout << "Deleted note: " << args.noteDel << std::endl;
                changed = true;
            } else {
                std::cerr << "Note not found: " << args.noteDel << std::endl;
                return 1;
            }
        } else if (!args.remAdd.empty()) {
            std::string id = mem.addReminder(args.remAdd, args.remWhen);
            std::cout << "Added reminder with ID: " << id << std::endl;
            changed = true;
        } else if (args.remList) {
            auto reminders = mem.listReminders();
            nlohmann::json j = nlohmann::json::array();
            for(const auto& r : reminders) {
                j.push_back({{"id", r.id}, {"text", r.text}, {"when_iso", r.when_iso}, {"done", r.done}});
            }
            std::cout << j.dump(2) << std::endl;
        } else if (!args.remDone.empty()) {
            if (mem.completeReminder(args.remDone)) {
                std::cout << "Completed reminder: " << args.remDone << std::endl;
                changed = true;
            } else {
                std::cerr << "Reminder not found: " << args.remDone << std::endl;
                return 1;
            }
        }

        if (changed) {
            mem.save();
        }
        return 0;
    }

#ifdef WITH_VOSK
    // --- Offline STT from WAV file ---
    if (!args.sttFromWav.empty()) {
        if (args.voskModel.empty()) {
            std::cerr << "Error: --vosk-model <dir> is required for --stt-from-wav." << std::endl;
            return 1;
        }

        std::vector<int16_t> pcm;
        uint32_t sample_rate;
        if (!loadWav(args.sttFromWav, pcm, sample_rate)) {
            // loadWav already prints an error
            return 1;
        }
        std::cout << "[asr] Loaded " << pcm.size() << " samples from " << args.sttFromWav << " (rate: " << sample_rate << ")" << std::endl;

        AsrVosk asr(args.voskModel);
        if (!asr.isAvailable()) {
            std::cerr << "Error: " << asr.lastError() << std::endl;
            return 1;
        }

        std::string transcript = asr.transcribe(pcm, sample_rate);
        std::cout << "Transcript: " << transcript << std::endl;

        if (!args.sttDumpJson.empty()) {
            std::string full_json = asr.transcribe_and_get_full_json(pcm, sample_rate);
            std::ofstream out(args.sttDumpJson);
            if (out) {
                out << full_json;
                std::cout << "[asr] Dumped full JSON result to " << args.sttDumpJson << std::endl;
            } else {
                std::cerr << "Error: Could not write to JSON file: " << args.sttDumpJson << std::endl;
            }
        }
        return 0;
    }
#endif

#ifdef WITH_PIPER
    // --- Standalone TTS (--say) ---
    if (args.withPiper && !args.say.empty()) {
        if (args.piperModel.empty()) {
            std::cerr << "Error: --piper-model <path> is required for --say." << std::endl;
            return 1;
        }

        TtsPiper piper(args.piperModel, args.piperBin);
        if (!piper.isAvailable()) {
            std::cerr << "Error: Piper is not available. " << piper.lastError() << std::endl;
            return 1;
        }

        std::cout << "[tts] Synthesizing text: \"" << args.say << "\"" << std::endl;
        double sampleRate = 0;
        std::vector<int16_t> pcm = piper.synthesize(args.say, sampleRate);

        if (pcm.empty()) {
            std::cerr << "Error: TTS synthesis failed. " << piper.lastError() << std::endl;
            return 1;
        }

        std::cout << "[tts] Synthesized " << pcm.size() << " samples at " << sampleRate << " Hz." << std::endl;

        // Always save the output WAV file.
        std::string savePath = "captures/tts_say.wav";
        std::filesystem::path p(savePath);
        if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
            std::filesystem::create_directories(p.parent_path());
        }
        if (saveWav(savePath, pcm, static_cast<int32_t>(sampleRate))) {
             std::cout << "[tts] Audio saved to " << savePath << std::endl;
        } else {
            std::cerr << "Error: Failed to save audio to " << savePath << std::endl;
        }

#ifdef WITH_AUDIO
        // If audio is enabled, also play it back.
        if (args.withAudio) {
            Audio audio; // Needs its own scope for RAII
            int outIdx = Audio::findDevice(args.outKey, true);
             if (!args.outKey.empty() && outIdx == -1) {
                std::cerr << "Error: Could not find requested output device: " << args.outKey << std::endl;
                return 1;
            }
            std::cout << "[audio] Playing back synthesized audio..." << std::endl;
            audio.playback(outIdx, sampleRate, pcm);
            std::cout << "[audio] Playback finished." << std::endl;
        }
#endif
        return 0; // Exit after --say is handled
    }
#endif


#ifdef WITH_AUDIO
    // Must be constructed for Pa_Initialize/Terminate RAII
    Audio audio;
    if (args.listDevices) {
        printDeviceList();
        return 0;
    }

    // --- PTT mode ---
    if (args.ptt) {
        int inIdx = Audio::findDevice(args.inKey, false);
        if (!args.inKey.empty() && inIdx == -1) {
            std::cerr << "Error: Could not find requested input device: " << args.inKey << std::endl;
            return 1;
        }
        std::cout << "[audio] Using input device index: " << inIdx << std::endl;

        std::cout << "[audio] Press Enter to start recording (" << args.recordSeconds << "s max)...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        std::vector<int16_t> pcm;
        double sampleRate = args.sampleRateIn;

        std::cout << "[audio] Recording..." << std::endl;
        audio.recordPtt(inIdx, args.recordSeconds, sampleRate, pcm);

        if (!args.saveWavPath.empty()) {
            std::string finalPath = args.saveWavPath;
            try {
                if (std::filesystem::is_directory(finalPath)) {
                    // Create captures/ if it doesn't exist
                    std::filesystem::path dir = finalPath;
                    if (dir.filename() == "." || dir.filename() == "..") { // e.g. "captures/"
                       dir = std::filesystem::path(finalPath);
                    } else if (finalPath.back() == '/' || finalPath.back() == '\\') {
                       dir = std::filesystem::path(finalPath);
                    } else {
                       // This case is ambiguous, but let's assume it's a directory
                       dir = std::filesystem::path(finalPath);
                    }

                    if (!std::filesystem::exists(dir)) {
                        std::filesystem::create_directories(dir);
                    }

                    auto now = std::chrono::system_clock::now();
                    auto in_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm tm_buf;
#ifdef _WIN32
                    localtime_s(&tm_buf, &in_time_t);
#else
                    localtime_r(&in_time_t, &tm_buf);
#endif
                    std::stringstream ss;
                    ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
                    finalPath = (dir / (ss.str() + ".wav")).string();
                }
                // If it's a file path, we'll just use it.
                // Ensure parent directory exists for file paths too.
                else {
                    std::filesystem::path p(finalPath);
                    if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
                        std::filesystem::create_directories(p.parent_path());
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                // If path is not a valid directory or file name (e.g. "captures"), it throws.
                // In this case, we treat it as a directory to be created.
                std::filesystem::path dir(finalPath);
                std::filesystem::create_directories(dir);

                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);
                std::tm tm_buf;
#ifdef _WIN32
                localtime_s(&tm_buf, &in_time_t);
#else
                localtime_r(&in_time_t, &tm_buf);
#endif
                std::stringstream ss;
                ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
                finalPath = (dir / (ss.str() + ".wav")).string();
            }

            if (!pcm.empty()) {
                // The function needs to be called from the global namespace
                ::saveWav(finalPath, pcm, static_cast<int32_t>(sampleRate));
            } else {
                std::cout << "[audio] No audio recorded, WAV file not saved." << std::endl;
            }
        }

#ifdef WITH_VOSK
        // --- PTT with ASR ---
        if (args.withVosk) {
            if (args.voskModel.empty()) {
                std::cerr << "Error: --vosk-model <dir> is required when using --with-vosk in PTT mode." << std::endl;
            } else if (pcm.empty()) {
                std::cout << "[asr] No audio recorded, skipping transcription." << std::endl;
            } else {
                AsrVosk asr(args.voskModel);
                if (asr.isAvailable()) {
                    std::cout << "[asr] Transcribing recorded audio..." << std::endl;
                    std::string transcript = asr.transcribe(pcm, sampleRate);
                    std::cout << "Transcript: " << transcript << std::endl;
#ifdef WITH_PIPER
                    if (args.withPiper && !args.piperModel.empty()) {
                        std::cout << "[tts] Synthesizing confirmation..." << std::endl;
                        TtsPiper piper(args.piperModel, args.piperBin);
                        if (piper.isAvailable()) {
                            double ttsSampleRate = 0;
                            std::string confirmation = "OK, j'ai compris.";
                            std::vector<int16_t> ttsPcm = piper.synthesize(confirmation, ttsSampleRate);
                            if (!ttsPcm.empty()) {
                                int outIdx = Audio::findDevice(args.outKey, true);
                                audio.playback(outIdx, ttsSampleRate, ttsPcm);
                            } else {
                                std::cerr << "[tts] Error: " << piper.lastError() << std::endl;
                            }
                        } else {
                             std::cerr << "[tts] Error: " << piper.lastError() << std::endl;
                        }
                    }
#endif
                } else {
                    std::cerr << "Error: " << asr.lastError() << std::endl;
                }
            }
        }
#endif
        return 0; // PTT mode exits after recording
    }
#endif

    AppCfg cfg = loadCfg("config/app.env");
    const char* e;
    if ((e=getenv("API_BASE"))) cfg.apiBase = e;
    if ((e=getenv("API_KEY" ))) cfg.apiKey  = e;
    if ((e=getenv("MODEL"   ))) cfg.model   = e;

    std::cout << "[assistant] prêt. tape /exit pour quitter.\n";
    std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

    OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model);

    MemoryStore mem;
    mem.load();

#ifdef WITH_AUDIO
    if (args.withAudio || !args.outKey.empty()) {
        int inIdx = -1, outIdx = -1;
        if (!args.inKey.empty()) {
            inIdx = Audio::findDevice(args.inKey, false);
            std::cout << "[audio] Selected input device " << inIdx << "\n";
        }
        if (!args.outKey.empty()) {
            outIdx = Audio::findDevice(args.outKey, true);
            std::cout << "[audio] Selected output device " << outIdx << "\n";
        }

        // If only output device is specified, play a test tone
        if (args.inKey.empty() && !args.outKey.empty()) {
            std::cout << "[audio] Playing test tone on output device.\n";
            double sampleRate = 44100.0; // Standard rate for test tone
            auto pcm = Audio::generateSineWave(440.0, 1000, sampleRate);
            audio.playback(outIdx, sampleRate, pcm);
            return 0;
        }

        // Full record and playback functionality
        if (args.withAudio) {
            std::cout << "[audio] Appuie Entrée pour parler (" << args.recordSeconds << "s)…";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::vector<int16_t> pcm;
            double sampleRate = 16000.0; // Preferred rate for ASR
            audio.record(inIdx, args.recordSeconds, sampleRate, pcm);

            std::string userText = "[audio] (capture " + std::to_string(args.recordSeconds) + "s)";
            std::string reply;
            if (args.offline) {
                reply = "(offline) Echo: " + userText;
            } else {
                auto r = client.chatOnce(userText);
                reply = r.ok ? r.text : std::string("[error] ")+(!r.error.empty()?r.error:r.text);
            }
            std::cout << "assistant> " << reply << "\n";

            // For now, just play back the recorded audio.
            // In a future step, this would be TTS output.
            audio.playback(outIdx, sampleRate, pcm);
            return 0;
        }
    }
#endif

    std::string line;
    while (true) {
        std::cout << "you> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "/exit") { std::cout << "[assistant] au revoir.\n"; break; }

        // --- Intent parsing ---
        Intent intent = parseIntent(line);
        bool changed = false;
        switch(intent.type) {
            case IntentType::NOTE_ADD:
                mem.addNote(intent.text);
                std::cout << "assistant> OK, j'ai noté." << std::endl;
                changed = true;
                break;
            case IntentType::REMINDER_ADD:
                mem.addReminder(intent.text, intent.when_iso);
                std::cout << "assistant> OK, je m'en souviendrai." << std::endl;
                changed = true;
                break;
            case IntentType::FACT_SET:
                mem.set(intent.key, intent.value);
                std::cout << "assistant> OK, c'est noté: " << intent.key << " est " << intent.value << "." << std::endl;
                changed = true;
                break;
            case IntentType::NONE:
                // No intent, proceed to chat
                break;
        }

        if (changed) {
            mem.save();
            continue;
        }

        if (args.offline) { std::cout << "assistant> (offline) Echo: " << line << "\n"; continue; }
        ChatResult r = client.chatOnce(line);
        if (!r.ok) std::cout << "assistant> [error] " << (!r.error.empty()?r.error:r.text) << "\n";
        else       std::cout << "assistant> " << r.text << "\n";
    }
    return 0;
}
