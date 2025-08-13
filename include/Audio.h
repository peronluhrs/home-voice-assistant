#pragma once

#ifdef WITH_AUDIO
#include <portaudio.h>
#endif

#include <vector>
#include <string>

struct AudioDeviceInfo {
    int id;
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultSampleRate;
};

class Audio {
public:
    Audio();
    ~Audio();

    static std::vector<AudioDeviceInfo> listDevices();
    bool record(int deviceId, int seconds, std::vector<float>& buffer);
    bool playback(int deviceId, const std::vector<float>& buffer);

private:
    static bool initialize();
    static void terminate();
};
