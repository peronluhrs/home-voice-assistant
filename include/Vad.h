#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Parameters for VAD
struct VadParams {
    bool enabled = false;
    double threshold_db = -35.0;
    int min_voice_ms = 120;
    int silence_close_ms = 600;
    int preroll_ms = 150;
    int window_ms = 20;
};

class Vad {
public:
    Vad(const VadParams& params, double sample_rate);

    // Process a chunk of audio data. Returns true if recording should stop.
    bool process(const int16_t* frame_data, size_t frame_size);

    // Finalize VAD processing.
    void finalize();

    // Get all detected segments in milliseconds.
    const std::vector<std::pair<int, int>>& get_segments() const;

    // Get the preroll buffer in samples.
    int get_preroll_samples() const;

private:
    VadParams params;
    double sample_rate;
    
    enum class State { SILENCE, VOICE };
    State state = State::SILENCE;

    int samples_per_window;
    int min_voice_frames;
    int silence_close_frames;
    int preroll_frames;

    int consecutive_voice_frames = 0;
    int consecutive_silence_frames = 0;
    
    std::vector<std::pair<int, int>> detected_segments;
    long long current_segment_start_frame = 0;
    long long total_frames_processed = 0;
};
