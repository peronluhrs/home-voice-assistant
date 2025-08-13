#pragma once

#ifdef WITH_VOSK
#include <vosk_api.h>
#endif

#include <string>
#include <vector>

class AsrVosk {
public:
    AsrVosk(const std::string& model_path);
    ~AsrVosk();

    std::string transcribe(const std::vector<float>& audio_buffer);

private:
#ifdef WITH_VOSK
    VoskModel* model;
    VoskRecognizer* recognizer;
#endif
};
