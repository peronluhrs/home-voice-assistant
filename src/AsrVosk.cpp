#include "AsrVosk.h"
#include "Utils.h" // For resample and extractJsonStringField
#include <iostream>

// Include Vosk API only when the build flag is enabled
#ifdef WITH_VOSK
#include <vosk_api.h>

// Target sample rate for the Vosk models
constexpr double VOSK_TARGET_SAMPLE_RATE = 16000.0;

// --- Implementation with Vosk enabled ---

AsrVosk::AsrVosk(const std::string& modelDir) {
    model_ = vosk_model_new(modelDir.c_str());
    if (model_ == nullptr) {
        last_error_ = "Failed to load Vosk model from: " + modelDir;
        std::cerr << "[asr-vosk] " << last_error_ << std::endl;
        is_available_ = false;
    } else {
        is_available_ = true;
    }
}

AsrVosk::~AsrVosk() {
    if (model_ != nullptr) {
        vosk_model_free(model_);
    }
}

bool AsrVosk::isAvailable() const {
    return is_available_;
}

std::string AsrVosk::lastError() const {
    return last_error_;
}

std::string AsrVosk::transcribe(const std::vector<int16_t>& pcm, double sampleRate) {
    if (!is_available_) {
        last_error_ = "Vosk model not available.";
        return "";
    }

    const std::vector<int16_t>* pcm_ptr = &pcm;
    std::vector<int16_t> resampled_pcm;

    // Resample if the source sample rate doesn't match the target
    if (sampleRate != VOSK_TARGET_SAMPLE_RATE) {
        resampled_pcm = resample(pcm, sampleRate, VOSK_TARGET_SAMPLE_RATE);
        pcm_ptr = &resampled_pcm;
    }

    // Create a recognizer for this specific transcription call
    VoskRecognizer* recognizer = vosk_recognizer_new(model_, (float)VOSK_TARGET_SAMPLE_RATE);
    if (recognizer == nullptr) {
        last_error_ = "Failed to create Vosk recognizer.";
        return "";
    }

    // Feed the PCM data to the recognizer
    vosk_recognizer_accept_waveform_s(
        recognizer,
        pcm_ptr->data(),
        pcm_ptr->size()
    );

    // Get the final result as a JSON string
    const char* result_json = vosk_recognizer_final_result(recognizer);
    std::string transcript = extractJsonStringField(result_json, "text");

    // Clean up the recognizer for this call
    vosk_recognizer_free(recognizer);

    return transcript;
}

std::string AsrVosk::transcribe_and_get_full_json(const std::vector<int16_t>& pcm, double sampleRate) {
    if (!is_available_) {
        last_error_ = "Vosk model not available.";
        return "{}";
    }

    const std::vector<int16_t>* pcm_ptr = &pcm;
    std::vector<int16_t> resampled_pcm;

    if (sampleRate != VOSK_TARGET_SAMPLE_RATE) {
        resampled_pcm = resample(pcm, sampleRate, VOSK_TARGET_SAMPLE_RATE);
        pcm_ptr = &resampled_pcm;
    }

    VoskRecognizer* recognizer = vosk_recognizer_new(model_, (float)VOSK_TARGET_SAMPLE_RATE);
    if (recognizer == nullptr) {
        last_error_ = "Failed to create Vosk recognizer.";
        return "{}";
    }

    vosk_recognizer_accept_waveform_s(recognizer, pcm_ptr->data(), pcm_ptr->size());
    const char* result_json = vosk_recognizer_final_result(recognizer);
    std::string full_result = result_json;
    vosk_recognizer_free(recognizer);

    return full_result;
}

#else

// --- Stub implementation when Vosk is disabled ---

AsrVosk::AsrVosk(const std::string& modelDir) {
    (void)modelDir; // Unused
    last_error_ = "Vosk support is not enabled in this build (WITH_VOSK=OFF).";
    is_available_ = false;
}

AsrVosk::~AsrVosk() {}

bool AsrVosk::isAvailable() const {
    return false;
}

std::string AsrVosk::lastError() const {
    return last_error_;
}

std::string AsrVosk::transcribe(const std::vector<int16_t>& pcm, double sampleRate) {
    (void)pcm; // Unused
    (void)sampleRate; // Unused
    return "";
}

std::string AsrVosk::transcribe_and_get_full_json(const std::vector<int16_t>& pcm, double sampleRate) {
    (void)pcm;
    (void)sampleRate;
    return "{\"text\": \"Vosk not enabled\"}";
}

#endif // WITH_VOSK
