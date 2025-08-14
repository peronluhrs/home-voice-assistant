#pragma once

#ifdef WITH_AUDIO
#include <portaudio.h>
#endif

#include <vector>
#include <string>
#include <cstdint>

// VAD params
struct VadParams;

#ifdef WITH_AUDIO
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
    // Listing + sélection
    static std::vector<AudioDeviceInfo> listDevices();
    static int findDevice(const std::string& key, bool isOutput);

    // I/O PCM int16
    bool record(int deviceId, int seconds, double& sampleRate, std::vector<int16_t>& buffer);
    bool recordPtt(int deviceId, int maxSeconds, double& sampleRate, const VadParams& vadParams, std::vector<int16_t>& buffer);
    bool playback(int deviceId, double sampleRate, const std::vector<int16_t>& buffer);
#endif

private:
    bool initialize();
    void terminate();

#ifdef WITH_AUDIO
    static double pickSupportedRate(PaDeviceIndex dev, bool isOutput, double preferredRate);
#endif
};
