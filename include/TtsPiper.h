#pragma once

#include <string>
#include <vector>

class TtsPiper {
public:
    TtsPiper(const std::string& model_path);
    ~TtsPiper();

    bool synthesize(const std::string& text, std::vector<float>& audio_buffer);

private:
    std::string model_path;
};
