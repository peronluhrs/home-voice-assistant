#pragma once
#include <string>
#include <vector>
#include <cstdint>

std::string ltrim(const std::string& s);
std::string rtrim(const std::string& s);
std::string trim(const std::string& s);

// extraction naïve de "field":"value" (pas un vrai parseur JSON)
std::string extractJsonStringField(const std::string& json, const std::string& field);

std::string join(const std::vector<std::string>& items, const std::string& sep);

#ifdef WITH_VOSK
// Loads a WAV file into a mono 16-bit PCM audio buffer.
// Returns false if the file cannot be loaded or is not mono/16-bit.
bool loadWav(
    const std::string& filePath,
    std::vector<int16_t>& pcm_data,
    uint32_t& sample_rate
);
#endif

// Resamples a PCM audio buffer from a source to a target sample rate.
std::vector<int16_t> resample(
    const std::vector<int16_t>& pcm_in,
    double sample_rate_in,
    double sample_rate_out
);

// Saves a mono 16-bit PCM audio buffer to a WAV file.
bool saveWav(
    const std::string& filePath,
    const std::vector<int16_t>& pcm_data,
    int32_t sample_rate
);
