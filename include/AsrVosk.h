#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declare Vosk types to avoid including the header here
// for projects that build without WITH_VOSK.
struct VoskModel;

class AsrVosk {
public:
    AsrVosk(const std::string& modelDir);
    ~AsrVosk();

    // Returns true if the Vosk model was loaded successfully.
    bool isAvailable() const;

    // Transcribes a mono 16-bit PCM audio buffer.
    std::string transcribe(const std::vector<int16_t>& pcm, double sampleRate);

    // Returns the last error message, if any.
    std::string lastError() const;

    // Transcribes and returns the full JSON result from Vosk.
    std::string transcribe_and_get_full_json(const std::vector<int16_t>& pcm, double sampleRate);

private:
    // PImpl idiom would be cleaner, but this is simple enough.
#ifdef WITH_VOSK
    VoskModel* model_ = nullptr;
#endif
    bool is_available_ = false;
    std::string last_error_;
};
