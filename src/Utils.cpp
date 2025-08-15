#include "Utils.h"
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iostream>

static inline bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && is_space(s[i])) ++i;
    return s.substr(i);
}

std::string rtrim(const std::string& s) {
    size_t i = s.size();
    while (i > 0 && is_space(s[i - 1])) --i;
    return s.substr(0, i);
}

std::string trim(const std::string& s) {
    return rtrim(ltrim(s));
}

// naïf: cherche "field":"..."; à remplacer par nlohmann/json plus tard
std::string extractJsonStringField(const std::string& json, const std::string& field) {
    std::string pat = "\"" + field + "\":\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return std::string();
    pos += pat.size();

    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\') {
            if (pos < json.size()) {
                char next = json[pos++];
                switch (next) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'r':  out.push_back('\r'); break;
                    default:   out.push_back(next); break;
                }
            }
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& items, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += sep;
        out += items[i];
    }
    return out;
}

#include <cmath>

#ifdef WITH_VOSK
bool loadWav(const std::string& filePath, std::vector<int16_t>& pcm_data, uint32_t& sample_rate) {
    unsigned int channels;
    unsigned int sr;
    drwav_uint64 totalFrameCount;
    int16_t* pcm_frames = drwav_open_file_and_read_pcm_frames_s16(filePath.c_str(), &channels, &sr, &totalFrameCount, nullptr);

    if (pcm_frames == nullptr) {
        std::cerr << "Error: Could not load WAV file: " << filePath << std::endl;
        return false;
    }

    if (channels != 1) {
        std::cerr << "Error: Only mono WAV files are supported. File has " << channels << " channels." << std::endl;
        drwav_free(pcm_frames, nullptr);
        return false;
    }

    sample_rate = sr;
    pcm_data.assign(pcm_frames, pcm_frames + totalFrameCount);
    drwav_free(pcm_frames, nullptr);

    return true;
}
#endif

std::vector<int16_t> resample(const std::vector<int16_t>& pcm_in, double sample_rate_in, double sample_rate_out) {
    if (sample_rate_in == sample_rate_out) {
        return pcm_in;
    }

    double ratio = sample_rate_in / sample_rate_out;
    size_t out_len = static_cast<size_t>(pcm_in.size() / ratio);
    std::vector<int16_t> pcm_out(out_len);

    for (size_t i = 0; i < out_len; ++i) {
        double in_idx_f = i * ratio;
        size_t in_idx_i = static_cast<size_t>(in_idx_f);
        double frac = in_idx_f - in_idx_i;

        if (in_idx_i + 1 < pcm_in.size()) {
            // Linear interpolation
            pcm_out[i] = static_cast<int16_t>(
                pcm_in[in_idx_i] * (1.0 - frac) + pcm_in[in_idx_i + 1] * frac
            );
        } else {
            // Last sample
            pcm_out[i] = pcm_in.back();
        }
    }

    return pcm_out;
}

// Helper to write little-endian values
template<typename T>
static void write_le(std::ofstream& stream, T value) {
    stream.write(reinterpret_cast<char*>(&value), sizeof(T));
}

bool saveWav(const std::string& filePath, const std::vector<int16_t>& pcm_data, int32_t sample_rate) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file for writing: " << filePath << std::endl;
        return false;
    }

    // --- WAV Header ---
    const int16_t num_channels = 1;
    const int16_t bits_per_sample = 16;
    const int32_t subchunk2_size = pcm_data.size() * num_channels * bits_per_sample / 8;
    const int32_t chunk_size = 36 + subchunk2_size;

    // "RIFF" chunk descriptor
    file.write("RIFF", 4);
    write_le(file, chunk_size);
    file.write("WAVE", 4);

    // "fmt " sub-chunk
    file.write("fmt ", 4);
    write_le(file, (int32_t)16); // Subchunk1Size for PCM
    write_le(file, (int16_t)1);  // AudioFormat (1 for PCM)
    write_le(file, num_channels);
    write_le(file, sample_rate);
    write_le(file, (int32_t)(sample_rate * num_channels * bits_per_sample / 8)); // ByteRate
    write_le(file, (int16_t)(num_channels * bits_per_sample / 8)); // BlockAlign
    write_le(file, bits_per_sample);

    // "data" sub-chunk
    file.write("data", 4);
    write_le(file, subchunk2_size);

    // --- PCM Data ---
    file.write(reinterpret_cast<const char*>(pcm_data.data()), pcm_data.size() * sizeof(int16_t));

    if (!file) {
        std::cerr << "Error: Failed to write all data to WAV file: " << filePath << std::endl;
        return false;
    }

    std::cout << "[wav] Saved " << pcm_data.size() << " samples to " << filePath << std::endl;
    return true;
}
