#pragma once

#include <string>
#include <vector>
#include <cstdint>

class TtsPiper {
public:
    // If piper_bin_path is empty, searches PATH. model_path is required.
    TtsPiper(std::string model_path, std::string piper_bin_path = "");
    ~TtsPiper();

    // Checks if the piper executable is available and logs the path.
    bool isAvailable();

    // Synthesizes text to raw PCM audio.
    // Returns the audio buffer. sample_rate is an output parameter from the WAV.
    // Throws std::runtime_error on failure.
    std::vector<int16_t> synthesize(const std::string& text, double& sample_rate);

private:
    std::string _model_path;
    std::string _piper_bin_path; // Resolved path to the piper executable.

    // Internal helper to find the executable path.
    std::string findPiperExecutable(const std::string& custom_path);
};
