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
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "No filesystem support"
#endif
#include <chrono>
#include <ctime>
#include <sstream>

#include "OpenAIClient.h"
#include "Audio.h"
#include "Utils.h"
#include "Vad.h"
#include "nlohmann/json.hpp"
#include "dr_wav.h"

// --- Forward decls from Utils.h (compat) ---
std::vector<int16_t> generateSineWave(double, int, double);
bool saveWav(const std::string&, const std::vector<int16_t>&, int32_t);
std::vector<int16_t> resample(const std::vector<int16_t>&, double, double);


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
    std::string voskModel;
    // Piper options
    bool withPiper = false;
    std::string piperBin;
    std::string piperModel;
    std::string say;
    // PTT mode
    bool ptt = false;
    std::string saveWavPath;
    double sampleRateIn = 16000.0;
    // VAD options
    VadParams vadParams;
    std::string vadFromWav;
    std::string vadDumpJson;
    std::string genVadFixtureWav;
};

static void printHelp() {
    std::cout << "Usage: home_assistant [options]\n\n"
              << "General Options:\n"
              << "  --help                Show this help message and exit.\n"
              << "  --offline             Run in offline mode (no API calls, echoes input).\n\n"
#ifdef WITH_AUDIO
              << "Audio Options:\n"
#else
              << "Audio Options (audio disabled at build time):\n"
#endif
              << "  --with-audio          Enable audio input/output via PortAudio.\n"
              << "  --list-devices        List available audio devices and exit.\n"
              << "  --input-device <key>  Keyword or index for input device.\n"
              << "  --output-device <key> Keyword or index for output device.\n"
              << "  --record-seconds <N>  Hard cap for recording duration (default: 5s).\n"
              << "  --ptt                 Enable Push-to-Talk mode (press Enter to start/stop).\n"
              << "  --save-wav <path>     If set, save capture to WAV. If path is a dir, use a timestamped filename.\n"
              << "  --sample-rate-in <Hz> Requested input sample rate (default: 16000). Falls back if unsupported.\n\n"
              << "ASR/TTS Options:\n"
              << "  --with-vosk           Enable Vosk ASR (requires build with -DWITH_VOSK=ON).\n"
              << "  --vosk-model <path>   Path to the Vosk model directory.\n"
              << "  --with-piper          Enable Piper TTS (requires build with -DWITH_PIPER=ON).\n"
              << "  --piper-bin <path>    Optional path to the 'piper' executable.\n"
              << "  --piper-model <path>  Path to the Piper TTS model file (.onnx), required if --with-piper.\n"
              << "  --say \"<text>\"        Synthesize and speak text using Piper, then exit.\n\n"
              << "VAD Options:\n"
              << "  --vad                 Enable VAD during PTT.\n"
              << "  --vad-threshold-db <dB> RMS threshold for VAD (default: -35.0).\n"
              << "  --vad-min-voice-ms <ms> Minimal contiguous voice to trigger 'voice' state (default: 120).\n"
              << "  --vad-silence-close-ms <ms> Contiguous silence to auto-stop recording (default: 600).\n"
              << "  --vad-preroll-ms <ms> Keep audio from before voice onset (default: 150).\n"
              << "  --vad-window-ms <ms>  Analysis window size for VAD (default: 20).\n\n"
              << "VAD Offline Validator:\n"
              << "  --vad-from-wav <path> Run VAD pipeline on a WAV file.\n"
              << "  --vad-dump-json <path> Optional JSON output for detected segments.\n"
              << "  --gen-vad-fixture-wav <path> Generate a test WAV file for VAD.\n";
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
        else if (s == "--vosk-model") next(a.voskModel);
        // PTT
        else if (s == "--ptt") { a.ptt = true; a.withAudio = true; }
        else if (s == "--save-wav") next(a.saveWavPath);
        else if (s == "--sample-rate-in") { std::string v; next(v); a.sampleRateIn = std::atof(v.c_str()); }
        // Piper args
        else if (s == "--with-piper") a.withPiper = true;
        else if (s == "--piper-bin") next(a.piperBin);
        else if (s == "--piper-model") next(a.piperModel);
        else if (s == "--say") next(a.say);
        // VAD
        else if (s == "--vad") a.vadParams.enabled = true;
        else if (s == "--vad-threshold-db") { std::string v; next(v); a.vadParams.threshold_db = std::atof(v.c_str()); }
        else if (s == "--vad-min-voice-ms") { std::string v; next(v); a.vadParams.min_voice_ms = std::atoi(v.c_str()); }
        else if (s == "--vad-silence-close-ms") { std::string v; next(v); a.vadParams.silence_close_ms = std::atoi(v.c_str()); }
        else if (s == "--vad-preroll-ms") { std::string v; next(v); a.vadParams.preroll_ms = std::atoi(v.c_str()); }
        else if (s == "--vad-window-ms") { std::string v; next(v); a.vadParams.window_ms = std::atoi(v.c_str()); }
        else if (s == "--vad-from-wav") next(a.vadFromWav);
        else if (s == "--vad-dump-json") next(a.vadDumpJson);
        else if (s == "--gen-vad-fixture-wav") next(a.genVadFixtureWav);
    }
    // --say implies --with-piper and --with-audio
    if (!a.say.empty()) {
        a.withPiper = true;
        a.withAudio = true;
    }
    return a;
}

// --- Fixture generation ---
static void generateVadFixture(const std::string& path) {
    const int sample_rate = 16000;
    const double duration_s = 0.4 + 1.2 + 0.7;
    std::vector<int16_t> pcm;
    pcm.reserve(static_cast<size_t>(sample_rate * duration_s));

    // 0.4s silence
    std::vector<int16_t> silence(static_cast<size_t>(sample_rate * 0.4), 0);
    pcm.insert(pcm.end(), silence.begin(), silence.end());

    // 1.2s 440Hz tone
    std::vector<int16_t> tone = generateSineWave(440.0, 1200, sample_rate);
    pcm.insert(pcm.end(), tone.begin(), tone.end());

    // 0.7s silence
    silence.assign(static_cast<size_t>(sample_rate * 0.7), 0);
    pcm.insert(pcm.end(), silence.begin(), silence.end());

    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    saveWav(path, pcm, sample_rate);
}

// --- Offline VAD validation ---
static void runVadOffline(const Args& args) {
    unsigned int channels;
    unsigned int sampleRate;
    drwav_uint64 totalPCMFrameCount;
    int16_t* pSampleData = drwav_open_file_and_read_pcm_frames_s16(args.vadFromWav.c_str(), &channels, &sampleRate, &totalPCMFrameCount, NULL);

    if (pSampleData == NULL) {
        std::cerr << "Error: Could not open or read WAV file: " << args.vadFromWav << std::endl;
        return;
    }

    if (channels != 1) {
        std::cerr << "Error: Only mono WAV files are supported for VAD validation." << std::endl;
        drwav_free(pSampleData, NULL);
        return;
    }

    std::vector<int16_t> pcm(pSampleData, pSampleData + totalPCMFrameCount);
    drwav_free(pSampleData, NULL);

    // Resample if necessary
    if (sampleRate != 16000) {
        std::cout << "[VAD] Resampling from " << sampleRate << " Hz to 16000 Hz." << std::endl;
        pcm = resample(pcm, sampleRate, 16000);
        sampleRate = 16000;
    }

    Vad vad(args.vadParams, sampleRate);
    const int window_size_samples = static_cast<int>(sampleRate * args.vadParams.window_ms / 1000.0);

    for (size_t i = 0; i + window_size_samples <= pcm.size(); i += window_size_samples) {
        vad.process(pcm.data() + i, window_size_samples);
    }
    vad.finalize();

    const auto& segments = vad.get_segments();
    std::cout << "Detected " << segments.size() << " voice segments:" << std::endl;
    for (const auto& seg : segments) {
        std::cout << "  - Start: " << seg.first << " ms, End: " << seg.second << " ms" << std::endl;
    }

    if (!args.vadDumpJson.empty()) {
        nlohmann::json json_segments = nlohmann::json::array();
        for (const auto& seg : segments) {
            nlohmann::json s;
            s["start_ms"] = seg.first;
            s["end_ms"] = seg.second;
            json_segments.push_back(s);
        }
        std::ofstream json_file(args.vadDumpJson);
        json_file << json_segments.dump(2);
    }

    if (!segments.empty()) {
        const auto& first_segment = segments[0];
        size_t start_sample = static_cast<size_t>(static_cast<double>(first_segment.first) / 1000.0 * sampleRate);
        size_t end_sample = static_cast<size_t>(static_cast<double>(first_segment.second) / 1000.0 * sampleRate);
        
        if (end_sample > pcm.size()) {
            end_sample = pcm.size();
        }
        if (start_sample > end_sample) {
            start_sample = end_sample;
        }

        std::vector<int16_t> segment_pcm(pcm.begin() + start_sample, pcm.begin() + end_sample);
        
        fs::path captures_dir("captures");
        if (!fs::exists(captures_dir)) {
            fs::create_directory(captures_dir);
        }
        saveWav("captures/vad_first.wav", segment_pcm, sampleRate);
    }
}

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);

    if (args.help) {
        printHelp();
        return 0;
    }

    if (!args.genVadFixtureWav.empty()) {
        generateVadFixture(args.genVadFixtureWav);
        return 0;
    }

    if (!args.vadFromWav.empty()) {
        runVadOffline(args);
        return 0;
    }

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
        audio.recordPtt(inIdx, args.recordSeconds, sampleRate, args.vadParams, pcm);

        auto t_stop = std::chrono::high_resolution_clock::now();

        if (!args.saveWavPath.empty()) {
            std::string finalPath = args.saveWavPath;
            try {
                if (fs::is_directory(finalPath)) {
                    // Create captures/ if it doesn't exist
                    fs::path dir = finalPath;
                    if (dir.filename() == "." || dir.filename() == "..") { // e.g. "captures/"
                       dir = fs::path(finalPath);
                    } else if (finalPath.back() == '/' || finalPath.back() == '\\') {
                       dir = fs::path(finalPath);
                    } else {
                       // This case is ambiguous, but let's assume it's a directory
                       dir = fs::path(finalPath);
                    }

                    if (!fs::exists(dir)) {
                        fs::create_directories(dir);
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
                    fs::path p(finalPath);
                    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
                        fs::create_directories(p.parent_path());
                    }
                }
            } catch (const fs::filesystem_error& e) {
                // If path is not a valid directory or file name (e.g. "captures"), it throws.
                // In this case, we treat it as a directory to be created.
                fs::path dir(finalPath);
                fs::create_directories(dir);

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
                auto t_end_save = std::chrono::high_resolution_clock::now();
                auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_save - t_stop).count();
                std::cout << "[main] Latency (stop -> file ready): " << latency_ms << " ms" << std::endl;
            } else {
                std::cout << "[audio] No audio recorded, WAV file not saved." << std::endl;
            }
        }
        return 0; // PTT mode exits after recording
    }

#if defined(WITH_PIPER) && defined(WITH_AUDIO)
    if (args.withPiper && !args.say.empty()) {
        if (args.piperModel.empty()) {
            std::cerr << "Error: --piper-model <path> is required when using --say." << std::endl;
            return 1;
        }

        try {
            TtsPiper piper(args.piperModel, args.piperBin);
            if (!piper.isAvailable()) {
                // isAvailable() already prints detailed errors
                return 1;
            }

            std::cout << "[main] Synthesizing text: \"" << args.say << "\"" << std::endl;
            double sampleRate = 0;
            std::vector<int16_t> pcm = piper.synthesize(args.say, sampleRate);

            if (pcm.empty()) {
                std::cerr << "Error: TTS synthesis failed or produced empty audio." << std::endl;
                return 1;
            }

            int outIdx = Audio::findDevice(args.outKey, true);
            if (!args.outKey.empty() && outIdx == -1) {
                std::cerr << "Error: Could not find requested output device: " << args.outKey << std::endl;
                return 1;
            }

            std::cout << "[audio] Using output device index: " << outIdx << std::endl;
            audio.playback(outIdx, sampleRate, pcm);

        } catch (const std::runtime_error& e) {
            std::cerr << "An error occurred during TTS processing: " << e.what() << std::endl;
            return 1;
        }
        return 0; // Exit after --say is handled
    }
#endif
#else
    if (args.ptt || args.withAudio || args.listDevices || !args.inKey.empty() || !args.outKey.empty()) {
        std::cout << "Audio features are disabled (built without PortAudio). Rebuild with -DWITH_AUDIO=ON." << std::endl;
        return 0;
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
            auto pcm = generateSineWave(440.0, 1000, sampleRate);
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
