#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <iomanip>

#include "OpenAIClient.h"
#include "Audio.h"

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

struct Args {
    bool help = false;
    bool offline = false;
    bool withAudio = false;
    bool listDevices = false;
    int recordSeconds = 5;
    std::string inKey, outKey;
    std::string voskModel, piperModel;
};

static void printHelp() {
    std::cout << "Usage: home_assistant [options]\n\n"
              << "General Options:\n"
              << "  --help                Show this help message and exit.\n"
              << "  --offline             Run in offline mode (no API calls, echoes input).\n\n"
              << "Audio Options (require building with -DWITH_AUDIO=ON):\n"
              << "  --with-audio          Enable audio input/output via PortAudio.\n"
              << "  --list-devices        List available audio devices and exit.\n"
              << "  --input-device <key>  Keyword or index for input device (default: system default).\n"
              << "  --output-device <key> Keyword or index for output device (default: system default).\n"
              << "  --record-seconds <N>  Duration of audio recording in seconds (default: 5).\n\n"
              << "ASR/TTS Options:\n"
              << "  --with-vosk           (Build-time) Enable Vosk ASR. Requires -DWITH_VOSK=ON.\n"
              << "  --vosk-model <path>   Path to the Vosk model directory.\n"
              << "  --with-piper          (Build-time) Enable Piper TTS. Requires -DWITH_PIPER=ON.\n"
              << "  --piper-model <path>  Path to the Piper TTS model file (.onnx).\n";
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
        else if (s == "--piper-model") next(a.piperModel);
    }
    return a;
}

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);

    if (args.help) {
        printHelp();
        return 0;
    }

#ifdef WITH_AUDIO
    // Must be constructed for Pa_Initialize/Terminate RAII
    Audio audio;
    if (args.listDevices) {
        printDeviceList();
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
