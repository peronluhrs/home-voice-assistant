#include "Vad.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iostream>

Vad::Vad(const VadParams& params, double sample_rate)
    : params(params), sample_rate(sample_rate) {
    samples_per_window = static_cast<int>(std::round(sample_rate * params.window_ms / 1000.0));
    // Clamp window size to a minimum to avoid issues at low sample rates, as per prompt
    if (samples_per_window < 80 && sample_rate == 16000) {
        samples_per_window = 80;
    }

    min_voice_frames = static_cast<int>(params.min_voice_ms / params.window_ms);
    silence_close_frames = static_cast<int>(params.silence_close_ms / params.window_ms);
    preroll_frames = static_cast<int>(params.preroll_ms / params.window_ms);
}

bool Vad::process(const int16_t* frame_data, size_t frame_size) {
    if (frame_size == 0) return false;

    // This simple VAD expects fixed-size frames, but we can handle variable sizes by processing in chunks.
    // For now, we assume the input `frame_data` is one analysis window.
    if (frame_size != samples_per_window) {
        // In a real scenario, we might buffer and process only full windows.
        // For this implementation, we'll process what we're given.
    }

    // Calculate RMS and dBFS
    double sum_sq = 0.0;
    for (size_t i = 0; i < frame_size; ++i) {
        double s = static_cast<double>(frame_data[i]);
        sum_sq += s * s;
    }
    double rms = frame_size == 0 ? 0.0 : std::sqrt(sum_sq / frame_size);
    double db = 20.0 * std::log10(std::max(rms / 32768.0, 1e-12));

    bool is_speech = db > params.threshold_db;

    if (state == State::SILENCE) {
        if (is_speech) {
            consecutive_voice_frames++;
            if (consecutive_voice_frames >= min_voice_frames) {
                state = State::VOICE;
                current_segment_start_frame = total_frames_processed - consecutive_voice_frames - preroll_frames;
                if (current_segment_start_frame < 0) {
                    current_segment_start_frame = 0;
                }
                consecutive_silence_frames = 0;
            }
        } else {
            consecutive_voice_frames = 0;
        }
    } else { // state == State::VOICE
        if (!is_speech) {
            consecutive_silence_frames++;
            if (consecutive_silence_frames >= silence_close_frames) {
                state = State::SILENCE;
                int segment_end_frame = total_frames_processed - consecutive_silence_frames;
                detected_segments.push_back({
                    static_cast<int>(current_segment_start_frame * params.window_ms),
                    static_cast<int>(segment_end_frame * params.window_ms)
                });
                consecutive_voice_frames = 0;
                return true; // Signal to stop recording
            }
        } else {
            consecutive_silence_frames = 0;
        }
    }
    
    total_frames_processed++;
    return false; // Continue recording
}

void Vad::finalize() {
    if (state == State::VOICE) {
        detected_segments.push_back({
            static_cast<int>(current_segment_start_frame * params.window_ms),
            static_cast<int>(total_frames_processed * params.window_ms)
        });
    }
}

const std::vector<std::pair<int, int>>& Vad::get_segments() const {
    return detected_segments;
}

int Vad::get_preroll_samples() const {
    return preroll_frames * samples_per_window;
}
