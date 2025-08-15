#include "TtsPiper.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "dr_wav.h"

#ifdef WITH_PIPER

// Helper to check if a file exists
static bool fileExists(const std::string& path) {
    // This check is basic. A more robust check would handle permissions.
    std::ifstream f(path.c_str());
    return f.good();
}

TtsPiper::TtsPiper(const std::string& modelPath, const std::string& piperBin)
    : bin_(piperBin), model_(modelPath) {}

bool TtsPiper::isAvailable() const {
    if (model_.empty() || !fileExists(model_)) {
        lastErr_ = "Model file not found or is not accessible: " + model_;
        return false;
    }

    // Check if bin_ is a direct path or can be found in PATH
    std::string command = "which " + bin_ + " > /dev/null 2>&1";
    if (system(command.c_str()) != 0) {
        if (!fileExists(bin_)) {
            lastErr_ = "Piper binary not found or not executable: " + bin_;
            return false;
        }
    }
    return true;
}

std::vector<int16_t> TtsPiper::synthesize(const std::string& text, double& sampleRate) {
    if (!isAvailable()) {
        // lastErr_ is already set by isAvailable()
        return {};
    }

    std::filesystem::path tmpDir = "captures";
    std::filesystem::create_directories(tmpDir);
    std::string tmpWav = (tmpDir / "tts_tmp.wav").string();

    // Escape double quotes in the text to prevent breaking the command
    std::string sanitized_text = text;
    size_t pos = 0;
    while ((pos = sanitized_text.find('"', pos)) != std::string::npos) {
        sanitized_text.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::string command = bin_ + " --model " + model_ + " --output_file " + tmpWav + " --text \"" + sanitized_text + "\"";

    int ret = system(command.c_str());
    if (ret != 0) {
        lastErr_ = "Piper command failed with exit code " + std::to_string(ret);
        remove(tmpWav.c_str());
        return {};
    }

    unsigned int channels;
    unsigned int sr;
    drwav_uint64 totalPcmFrameCount;
    int16_t* pcmData = drwav_open_file_and_read_pcm_frames_s16(tmpWav.c_str(), &channels, &sr, &totalPcmFrameCount, nullptr);

    if (pcmData == nullptr) {
        lastErr_ = "Failed to read or decode WAV file: " + tmpWav;
        remove(tmpWav.c_str());
        return {};
    }

    std::vector<int16_t> audioBuffer;
    if (channels > 1) {
        audioBuffer.resize(totalPcmFrameCount);
        for (drwav_uint64 i = 0; i < totalPcmFrameCount; ++i) {
            int32_t sample_sum = 0;
            for (unsigned int j = 0; j < channels; ++j) {
                sample_sum += pcmData[i * channels + j];
            }
            audioBuffer[i] = static_cast<int16_t>(sample_sum / channels);
        }
    } else {
        audioBuffer.assign(pcmData, pcmData + totalPcmFrameCount);
    }

    drwav_free(pcmData, nullptr);
    remove(tmpWav.c_str());

    sampleRate = static_cast<double>(sr);
    lastErr_ = "";
    return audioBuffer;
}

const std::string& TtsPiper::lastError() const {
    return lastErr_;
}

#else // WITH_PIPER is OFF

// Stubs
TtsPiper::TtsPiper(const std::string&, const std::string&) : lastErr_("Piper support is disabled in this build.") {}
bool TtsPiper::isAvailable() const { return false; }
std::vector<int16_t> TtsPiper::synthesize(const std::string&, double&) { return {}; }
const std::string& TtsPiper::lastError() const { return lastErr_; }

#endif // WITH_PIPER
