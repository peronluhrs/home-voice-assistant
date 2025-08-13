#include "Audio.h"
#include <iostream>
#include <vector>

#ifdef WITH_AUDIO
#include <portaudio.h>
#endif

#define SAMPLE_RATE         (16000)
#define FRAMES_PER_BUFFER   (512)
#define NUM_CHANNELS        (1)
#define SAMPLE_FORMAT       paFloat32

Audio::Audio() {
    initialize();
}

Audio::~Audio() {
    terminate();
}

bool Audio::initialize() {
#ifdef WITH_AUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    return true;
#else
    return false;
#endif
}

void Audio::terminate() {
#ifdef WITH_AUDIO
    Pa_Terminate();
#endif
}

std::vector<AudioDeviceInfo> Audio::listDevices() {
    std::vector<AudioDeviceInfo> devices;
#ifdef WITH_AUDIO
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(numDevices) << std::endl;
        return devices;
    }

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* paInfo = Pa_GetDeviceInfo(i);
        if (paInfo) {
            AudioDeviceInfo info;
            info.id = i;
            info.name = paInfo->name;
            info.maxInputChannels = paInfo->maxInputChannels;
            info.maxOutputChannels = paInfo->maxOutputChannels;
            info.defaultSampleRate = paInfo->defaultSampleRate;
            devices.push_back(info);
        }
    }
#endif
    return devices;
}

bool Audio::record(int deviceId, int seconds, std::vector<float>& buffer) {
#ifdef WITH_AUDIO
    PaStreamParameters inputParameters;
    inputParameters.device = deviceId;
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default input device.\n");
        return false;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = SAMPLE_FORMAT;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    PaStream* stream;
    PaError err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
    if (err != paNoError) return false;

    buffer.resize(seconds * SAMPLE_RATE);
    err = Pa_StartStream(stream);
    if (err != paNoError) return false;

    Pa_ReadStream(stream, buffer.data(), buffer.size());

    err = Pa_CloseStream(stream);
    if (err != paNoError) return false;

    return true;
#else
    (void)deviceId; (void)seconds; (void)buffer;
    return false;
#endif
}

bool Audio::playback(int deviceId, const std::vector<float>& buffer) {
#ifdef WITH_AUDIO
    PaStreamParameters outputParameters;
    outputParameters.device = deviceId;
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        return false;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = SAMPLE_FORMAT;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    PaStream* stream;
    PaError err = Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
    if (err != paNoError) return false;

    err = Pa_StartStream(stream);
    if (err != paNoError) return false;

    Pa_WriteStream(stream, buffer.data(), buffer.size());

    err = Pa_CloseStream(stream);
    if (err != paNoError) return false;

    return true;
#else
    (void)deviceId; (void)buffer;
    return false;
#endif
}
