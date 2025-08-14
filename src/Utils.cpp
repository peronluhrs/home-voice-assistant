#include "Utils.h"
#include <cctype>
#include <algorithm>

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

// --- Added for VAD/WAV support ---
#include "dr_wav.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>

bool saveWav(const std::string& filePath, const std::vector<int16_t>& pcm_data, int32_t sample_rate) {
    drwav_data_format fmt;
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = 1;
    fmt.sampleRate = sample_rate;
    fmt.bitsPerSample = 16;

    drwav wav;
    if (!drwav_init_file_write(&wav, filePath.c_str(), &fmt, NULL)) {
        std::cerr << "[wav] open failed: " << filePath << std::endl;
        return false;
    }
    drwav_uint64 written = drwav_write_pcm_frames(&wav, pcm_data.size(), pcm_data.data());
    drwav_uninit(&wav);
    if (written != pcm_data.size()) {
        std::cerr << "[wav] partial write: " << written << " / " << pcm_data.size() << std::endl;
        return false;
    }
    std::cout << "[wav] saved " << pcm_data.size() << " samples to " << filePath << std::endl;
    return true;
}

std::vector<int16_t> resample(const std::vector<int16_t>& in, double in_rate, double out_rate) {
    if (in_rate == out_rate) return in;
    double ratio = in_rate / out_rate;
    size_t out_len = static_cast<size_t>(in.size() / ratio);
    std::vector<int16_t> out(out_len);
    for (size_t i = 0; i < out_len; ++i) {
        double idx = i * ratio;
        size_t i0 = static_cast<size_t>(idx);
        double frac = idx - i0;
        if (i0 + 1 < in.size()) {
            double v1 = in[i0], v2 = in[i0 + 1];
            out[i] = static_cast<int16_t>(v1 + (v2 - v1) * frac);
        } else {
            out[i] = in.back();
        }
    }
    return out;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
std::vector<int16_t> generateSineWave(double frequency, int duration_ms, double sampleRate) {
    int n = static_cast<int>((duration_ms / 1000.0) * sampleRate);
    std::vector<int16_t> buf(n);
    double amp = 0.5 * 32767.0;
    for (int i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        buf[i] = static_cast<int16_t>(amp * sin(2.0 * M_PI * frequency * t));
    }
    return buf;
}
// --- end added ---
