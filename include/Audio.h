#pragma once

#ifdef WITH_AUDIO
#include <portaudio.h>
#endif

#include <vector>
#include <string>
#include <cstdint>

#ifdef WITH_AUDIO
// Forward declaration
struct PaDeviceInfo;

struct AudioDeviceInfo {
    int id;
    std::string name;
    std::string hostApi;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultSampleRate;
    std::vector<double> supportedSampleRates;
};
#endif

class Audio {
public:
    Audio();
    ~Audio();

#ifdef WITH_AUDIO
    // Enhanced device listing
    static std::vector<AudioDeviceInfo> listDevices();

    // Device selection
    static int findDevice(const std::string& key, bool isOutput);

    // Audio operations using int16_t samples
    bool record(int deviceId, int seconds, double& sampleRate, std::vector<int16_t>& buffer);
    bool recordPtt(int deviceId, int maxSeconds, double& sampleRate, std::vector<int16_t>& buffer);
    bool playback(int deviceId, double sampleRate, const std::vector<int16_t>& buffer);
#endif

    // Test tone generation
    static std::vector<int16_t> generateSineWave(double frequency, int duration_ms, double sampleRate);

private:
    // PortAudio management
    bool initialize();
    void terminate();

#ifdef WITH_AUDIO
    // Helper for checking sample rates
    static std::vector<double> getSupportedSampleRates(const PaDeviceInfo* deviceInfo);
    static double pickSupportedRate(PaDeviceIndex dev, bool isOutput, double preferredRate);
#endif
};
