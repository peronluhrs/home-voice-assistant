
#include <iostream>
#include <string>
#include <vector>
#include "Env.h"
#include "OpenAIClient.h"
#include "Audio.h"

int main(int argc, char* argv[]) {
    // Load config
    const std::string envPath = "config/app.env";
    EnvConfig cfg = loadEnvFile(envPath);
    bool offline = false;
    bool list_devices = false;
    bool with_audio = false;
    std::string input_device = "";
    std::string output_device = "";
    int record_seconds = 5;

    // Parse command-line arguments
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--api-base" && i + 1 < args.size()) {
            cfg.apiBase = args[++i];
        } else if (args[i] == "--model" && i + 1 < args.size()) {
            cfg.model = args[++i];
        } else if (args[i] == "--api-key" && i + 1 < args.size()) {
            cfg.apiKey = args[++i];
        } else if (args[i] == "--offline") {
            offline = true;
        } else if (args[i] == "--list-devices") {
            list_devices = true;
        } else if (args[i] == "--with-audio") {
            with_audio = true;
        } else if (args[i] == "--input-device" && i + 1 < args.size()) {
            input_device = args[++i];
        } else if (args[i] == "--output-device" && i + 1 < args.size()) {
            output_device = args[++i];
        } else if (args[i] == "--record-seconds" && i + 1 < args.size()) {
            record_seconds = std::stoi(args[++i]);
        }
    }

    if (list_devices) {
        Audio audio;
        auto devices = Audio::listDevices();
        std::cout << "Available audio devices:\n";
        for (const auto& dev : devices) {
            std::cout << "  - ID: " << dev.id << ", Name: " << dev.name
                      << ", Inputs: " << dev.maxInputChannels
                      << ", Outputs: " << dev.maxOutputChannels
                      << ", Sample Rate: " << dev.defaultSampleRate << " Hz\n";
        }
        return 0;
    }

    if (with_audio) {
        Audio audio;
        std::vector<float> audio_buffer;
        int inputDeviceId = -1; // Default device
        int outputDeviceId = -1; // Default device

        // Simple device selection by name substring match
        if (!input_device.empty() || !output_device.empty()) {
            auto devices = Audio::listDevices();
            for (const auto& dev : devices) {
                if (!input_device.empty() && dev.name.find(input_device) != std::string::npos) {
                    inputDeviceId = dev.id;
                }
                if (!output_device.empty() && dev.name.find(output_device) != std::string::npos) {
                    outputDeviceId = dev.id;
                }
            }
        }

        std::cout << "Press Enter to start recording for " << record_seconds << " seconds..." << std::endl;
        std::cin.get();

        if (audio.record(inputDeviceId, record_seconds, audio_buffer)) {
            std::cout << "Recording finished. Playing back..." << std::endl;
            if (!audio.playback(outputDeviceId, audio_buffer)) {
                std::cerr << "Playback failed." << std::endl;
            }
        } else {
            std::cerr << "Recording failed." << std::endl;
        }
        return 0;
    }

    std::cout << "[assistant] prêt. tape /exit pour quitter.\n";
    std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

    OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model, offline);

    std::string line;
    while (true) {
        std::cout << "you> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "/exit") break;
        if (line.empty()) continue;

        ChatResult r = client.chatOnce(line);
        if (!r.ok && !r.error.empty()) {
            std::cout << "assistant> [error] " << r.error << "\n";
            if (!r.text.empty()) {
                std::cout << "assistant> [raw] " << r.text.substr(0, 4000) << "\n";
            }
            continue;
        }
        std::cout << "assistant> " << r.text << "\n";
    }
    std::cout << "[assistant] au revoir.\n";
    return 0;
}
