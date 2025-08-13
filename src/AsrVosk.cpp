#include "AsrVosk.h"
#include <iostream>

#ifdef WITH_VOSK
#include <vosk_api.h>
#endif

AsrVosk::AsrVosk(const std::string& model_path) {
#ifdef WITH_VOSK
    model = vosk_model_new(model_path.c_str());
    if (!model) {
        std::cerr << "Failed to load Vosk model from " << model_path << std::endl;
        return;
    }
    recognizer = vosk_recognizer_new(model, 16000.0f);
#else
    (void)model_path;
#endif
}

AsrVosk::~AsrVosk() {
#ifdef WITH_VOSK
    if (recognizer) vosk_recognizer_free(recognizer);
    if (model) vosk_model_free(model);
#endif
}

std::string AsrVosk::transcribe(const std::vector<float>& audio_buffer) {
#ifdef WITH_VOSK
    if (!recognizer) return "";

    std::vector<int16_t> pcm(audio_buffer.size());
    for (size_t i = 0; i < audio_buffer.size(); ++i) {
        pcm[i] = static_cast<int16_t>(audio_buffer[i] * 32767.0f);
    }

    int result = vosk_recognizer_accept_waveform_s(recognizer, pcm.data(), pcm.size());
    if (result == 0) {
        return vosk_recognizer_result(recognizer);
    } else {
        return vosk_recognizer_partial_result(recognizer);
    }
#else
    (void)audio_buffer;
    return "";
#endif
}
