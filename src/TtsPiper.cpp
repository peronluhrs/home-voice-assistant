#include "TtsPiper.h"
#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

TtsPiper::TtsPiper(const std::string& model_path) : model_path(model_path) {}

TtsPiper::~TtsPiper() {}

bool TtsPiper::synthesize(const std::string& text, std::vector<float>& audio_buffer) {
#ifdef WITH_PIPER
    std::string command = "echo \"" + text + "\" | piper --model " + model_path + " --output_raw";

    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // The output is raw 16-bit PCM audio. We need to convert it to float.
    audio_buffer.resize(result.size() / sizeof(int16_t));
    for (size_t i = 0; i < audio_buffer.size(); ++i) {
        audio_buffer[i] = static_cast<float>(reinterpret_cast<int16_t*>(result.data())[i]) / 32767.0f;
    }

    return true;
#else
    (void)text; (void)audio_buffer;
    return false;
#endif
}
