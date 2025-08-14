#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>

#include "OpenAIClient.h"
#include "Audio.h"
#include "Utils.h"

#ifdef WITH_VOSK
#include "AsrVosk.h"
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
              << "  --say \"<text>\"        Synthesize and speak text using Piper, then exit.\n";
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
        if (args.offline) { std::cout << "assistant> (offline) Echo: " << line << "\n"; continue; }
        ChatResult r = client.chatOnce(line);
        if (!r.ok) std::cout << "assistant> [error] " << (!r.error.empty()?r.error:r.text) << "\n";
        else       std::cout << "assistant> " << r.text << "\n";
    }
    return 0;
}
