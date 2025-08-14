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

// Saves a mono 16-bit PCM audio buffer to a WAV file.
bool saveWav(
    const std::string& filePath,
    const std::vector<int16_t>& pcm_data,
    int32_t sample_rate
);
