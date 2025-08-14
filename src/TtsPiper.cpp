#include "TtsPiper.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>
#include <unistd.h> // For access()
#include <sys/stat.h> // For stat()
#include <cstdlib> // For getenv()
#include <vector>
#include <cstdint>

// --- Helper Functions ---

// WAV header parsing logic
namespace WavUtils {
    struct WavHeader {
        char chunkId[4];
        uint32_t chunkSize;
        char format[4];
        char subchunk1Id[4];
        uint32_t subchunk1Size;
        uint16_t audioFormat;
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        char subchunk2Id[4];
        uint32_t subchunk2Size;
    };

    bool read_header(std::ifstream& file, WavHeader& header) {
        if (!file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader))) {
            return false;
        }
        // Basic validation
        return std::string(header.chunkId, 4) == "RIFF" &&
               std::string(header.format, 4) == "WAVE" &&
               std::string(header.subchunk1Id, 4) == "fmt " &&
               std::string(header.subchunk2Id, 4) == "data";
    }
}


// --- TtsPiper Implementation ---

TtsPiper::TtsPiper(std::string model_path, std::string piper_bin_path)
    : _model_path(std::move(model_path)) {
    _piper_bin_path = findPiperExecutable(std::move(piper_bin_path));
}

TtsPiper::~TtsPiper() {}

std::string TtsPiper::findPiperExecutable(const std::string& custom_path) {
    auto is_executable = [](const std::string& p) {
        struct stat st;
        return !p.empty() && stat(p.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
    };

    // 1. Check custom path first
    if (is_executable(custom_path)) {
        return custom_path;
    }
    if (!custom_path.empty()) {
         std::cerr << "[tts-piper] Warning: Binary specified at '" << custom_path << "' is not executable or not found." << std::endl;
    }

    // 2. Search in PATH
    const char* path_env = getenv("PATH");
    if (path_env) {
        std::string path_str = path_env;
        size_t start = 0;
        size_t end = path_str.find(':');
        while (end != std::string::npos) {
            std::string p = path_str.substr(start, end - start) + "/piper";
            if (is_executable(p)) return p;
            start = end + 1;
            end = path_str.find(':', start);
        }
        std::string p = path_str.substr(start) + "/piper";
        if (is_executable(p)) return p;
    }

    // 3. Check common locations as a fallback
    if (is_executable("/usr/local/bin/piper")) return "/usr/local/bin/piper";
    if (is_executable("/usr/bin/piper")) return "/usr/bin/piper";


    return ""; // Not found
}


bool TtsPiper::isAvailable() {
    if (_piper_bin_path.empty()) {
        std::cerr << "[tts-piper] Error: 'piper' executable not found in PATH or specified location." << std::endl;
        return false;
    }
    struct stat st;
    if (stat(_model_path.c_str(), &st) != 0) {
        std::cerr << "[tts-piper] Error: Model file not found at '" << _model_path << "'" << std::endl;
        return false;
    }
    std::cout << "[tts-piper] Found executable at: " << _piper_bin_path << std::endl;
    std::cout << "[tts-piper] Using model: " << _model_path << std::endl;
    return true;
}

std::vector<int16_t> TtsPiper::synthesize(const std::string& text, double& sample_rate) {
#ifdef WITH_PIPER
    // 1. Create a unique temporary file path
    char tmp_filename[] = "/tmp/piper_output_XXXXXX.wav";
    int fd = mkstemps(tmp_filename, 4);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file for WAV output.");
    }
    close(fd); // We just needed the name, piper will open it.

    // 2. Build and execute the command
    std::string command = "echo \"" + text + "\" | " + _piper_bin_path +
                          " --model " + _model_path +
                          " --output-file " + tmp_filename;

    int ret = std::system(command.c_str());
    if (ret != 0) {
        remove(tmp_filename);
        throw std::runtime_error("Piper command failed with exit code " + std::to_string(ret));
    }

    // 3. Open and parse the WAV file
    std::ifstream wav_file(tmp_filename, std::ios::binary);
    if (!wav_file) {
        remove(tmp_filename);
        throw std::runtime_error("Could not open temporary WAV file: " + std::string(tmp_filename));
    }

    WavUtils::WavHeader header;
    if (!WavUtils::read_header(wav_file, header)) {
        wav_file.close();
        remove(tmp_filename);
        throw std::runtime_error("Failed to read or parse WAV header from " + std::string(tmp_filename));
    }

    // Check format (must be 16-bit PCM mono)
    if (header.audioFormat != 1 || header.numChannels != 1 || header.bitsPerSample != 16) {
        wav_file.close();
        remove(tmp_filename);
        throw std::runtime_error("Unsupported WAV format. Expecting 16-bit PCM mono.");
    }

    sample_rate = header.sampleRate;

    // 4. Read audio data
    std::vector<int16_t> pcm_buffer(header.subchunk2Size / sizeof(int16_t));
    if (!wav_file.read(reinterpret_cast<char*>(pcm_buffer.data()), header.subchunk2Size)) {
        wav_file.close();
        remove(tmp_filename);
        throw std::runtime_error("Failed to read PCM data from WAV file.");
    }

    // 5. Cleanup and return
    wav_file.close();
    remove(tmp_filename);

    std::cout << "[tts-piper] Synthesized " << pcm_buffer.size() << " samples at " << sample_rate << " Hz." << std::endl;

    return pcm_buffer;
#else
    (void)text; (void)sample_rate;
    throw std::runtime_error("Piper support not compiled. Re-run CMake with -DWITH_PIPER=ON.");
    return {};
#endif
}
