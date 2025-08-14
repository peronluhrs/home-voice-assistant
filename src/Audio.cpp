#include "Audio.h"
#include "Vad.h"
#include "Utils.h"

// Forward decl from Utils.h to satisfy this TU
std::vector<int16_t> resample(const std::vector<int16_t>&, double, double);


#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <thread>
#include <chrono>
#include <limits>

#ifdef WITH_AUDIO

static std::string toLower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

Audio::Audio(){ initialize(); }
Audio::~Audio(){ terminate(); }

bool Audio::initialize() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error (Pa_Initialize): " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    return true;
}

void Audio::terminate() { Pa_Terminate(); }

std::vector<AudioDeviceInfo> Audio::listDevices() {
    std::vector<AudioDeviceInfo> devices;
    int n = Pa_GetDeviceCount();
    if (n < 0) {
        std::cerr << "PortAudio error (Pa_GetDeviceCount): " << Pa_GetErrorText(n) << std::endl;
        return devices;
    }
    const double stdRates[] = {48000.0, 44100.0, 32000.0, 16000.0, 8000.0};

    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        if (!di) continue;
        AudioDeviceInfo info{};
        info.id = i;
        info.name = di->name ? di->name : "Unknown";
        const PaHostApiInfo* api = Pa_GetHostApiInfo(di->hostApi);
        info.hostApi = api ? api->name : "Unknown";
        info.maxInputChannels = di->maxInputChannels;
        info.maxOutputChannels = di->maxOutputChannels;
        info.defaultSampleRate = di->defaultSampleRate;

        // Probe des rates supportés
        PaStreamParameters inP{}; inP.device = i; inP.channelCount = 1; inP.sampleFormat = paInt16; inP.suggestedLatency = di->defaultLowInputLatency;
        PaStreamParameters outP{}; outP.device = i; outP.channelCount = 1; outP.sampleFormat = paInt16; outP.suggestedLatency = di->defaultLowOutputLatency;

        for (double r : stdRates) {
            bool ok = false;
            if (di->maxInputChannels > 0 && Pa_IsFormatSupported(&inP, nullptr, r) == paFormatIsSupported) ok = true;
            if (di->maxOutputChannels > 0 && Pa_IsFormatSupported(nullptr, &outP, r) == paFormatIsSupported) ok = true;
            if (ok) info.supportedSampleRates.push_back(r);
        }
        std::sort(info.supportedSampleRates.begin(), info.supportedSampleRates.end());
        info.supportedSampleRates.erase(std::unique(info.supportedSampleRates.begin(), info.supportedSampleRates.end()), info.supportedSampleRates.end());

        devices.push_back(info);
    }
    return devices;
}

int Audio::findDevice(const std::string& key, bool isOutput) {
    // index ?
    try {
        size_t pos=0; int idx = std::stoi(key, &pos);
        if (pos == key.size() && idx >= 0 && idx < Pa_GetDeviceCount()) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(idx);
            if (di && ((isOutput && di->maxOutputChannels>0) || (!isOutput && di->maxInputChannels>0))) return idx;
        }
    } catch(...) {}

    // nom partiel
    std::string lk = toLower(key);
    int n = Pa_GetDeviceCount();
    for (int i=0;i<n;++i){
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        if (!di) continue;
        if (isOutput && di->maxOutputChannels<=0) continue;
        if (!isOutput && di->maxInputChannels<=0) continue;
        std::string name = di->name ? di->name : "";
        if (toLower(name).find(lk) != std::string::npos) return i;
    }

    PaDeviceIndex def = isOutput ? Pa_GetDefaultOutputDevice() : Pa_GetDefaultInputDevice();
    return def == paNoDevice ? -1 : def;
}

double Audio::pickSupportedRate(PaDeviceIndex dev, bool isOutput, double preferredRate) {
    const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    if (!di) return -1.0;
    double cand[] = {preferredRate, 48000.0, 44100.0, 32000.0, 16000.0};
    PaStreamParameters p{}; p.device = dev; p.sampleFormat = paInt16;
    p.suggestedLatency = isOutput ? di->defaultLowOutputLatency : di->defaultLowInputLatency;
    p.channelCount = 1;
    PaStreamParameters *inP = isOutput ? nullptr : &p;
    PaStreamParameters *outP = isOutput ? &p : nullptr;
    for (double r : cand) {
        if (r>0 && Pa_IsFormatSupported(inP, outP, r) == paFormatIsSupported) return r;
    }
    if (di->defaultSampleRate>0 && Pa_IsFormatSupported(inP, outP, di->defaultSampleRate) == paFormatIsSupported)
        return di->defaultSampleRate;
    return -1.0;
}

bool Audio::record(int deviceId, int seconds, double& sampleRate, std::vector<int16_t>& buffer) {
    PaDeviceIndex dev = (deviceId>=0)? deviceId : Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) { std::cerr << "[audio] No input device\n"; return false; }
    sampleRate = pickSupportedRate(dev, false, sampleRate);
    if (sampleRate <= 0) { std::cerr << "[audio] No supported rate\n"; return false; }

    PaStream* stream=nullptr;
    PaStreamParameters inP{}; const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    inP.device = dev; inP.channelCount = 1; inP.sampleFormat = paInt16; inP.suggestedLatency = di? di->defaultLowInputLatency : 0.05;

    PaError err = Pa_OpenStream(&stream, &inP, nullptr, sampleRate, paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) { std::cerr << "Pa_OpenStream: " << Pa_GetErrorText(err) << "\n"; return false; }
    if ((err = Pa_StartStream(stream)) != paNoError) { std::cerr << "Pa_StartStream: " << Pa_GetErrorText(err) << "\n"; Pa_CloseStream(stream); return false; }

    size_t frames = static_cast<size_t>(seconds * sampleRate);
    buffer.assign(frames, 0);
    err = Pa_ReadStream(stream, buffer.data(), (unsigned long)frames);
    if (err != paNoError && err != paInputOverflowed) std::cerr << "Pa_ReadStream: " << Pa_GetErrorText(err) << "\n";

    Pa_StopStream(stream); Pa_CloseStream(stream);
    return err == paNoError || err == paInputOverflowed;
}

bool Audio::recordPtt(int deviceId, int maxSeconds, double& sampleRate, const VadParams& vadParams, std::vector<int16_t>& buffer) {
    PaDeviceIndex dev = (deviceId>=0)? deviceId : Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) { std::cerr << "[audio] No input device\n"; return false; }
    sampleRate = pickSupportedRate(dev, false, sampleRate);
    if (sampleRate <= 0) { std::cerr << "[audio] No supported rate\n"; return false; }

    PaStream* stream=nullptr;
    PaStreamParameters inP{}; const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    inP.device = dev; inP.channelCount = 1; inP.sampleFormat = paInt16; inP.suggestedLatency = di? di->defaultLowInputLatency : 0.05;

    int frame_size = std::max(80, (int)std::round(sampleRate * vadParams.window_ms / 1000.0));
    PaError err = Pa_OpenStream(&stream, &inP, nullptr, sampleRate, frame_size, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) { std::cerr << "Pa_OpenStream: " << Pa_GetErrorText(err) << "\n"; return false; }
    if ((err = Pa_StartStream(stream)) != paNoError) { std::cerr << "Pa_StartStream: " << Pa_GetErrorText(err) << "\n"; Pa_CloseStream(stream); return false; }

    auto start = std::chrono::steady_clock::now();
    Vad vad(vadParams, sampleRate);
    std::vector<int16_t> full;
    full.reserve(maxSeconds * (int)sampleRate);
    std::vector<int16_t> frame(frame_size);

    // purge entrée console
    while (std::cin.rdbuf()->in_avail() > 0) std::cin.get();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= maxSeconds) { std::cout << "[audio] Max time\n"; break; }

        if (std::cin.rdbuf()->in_avail() > 0) { std::cout << "[audio] Stop signal\n"; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); break; }

        err = Pa_ReadStream(stream, frame.data(), frame_size);
        if (err != paNoError && err != paInputOverflowed) { std::cerr << "Pa_ReadStream: " << Pa_GetErrorText(err) << "\n"; break; }

        full.insert(full.end(), frame.begin(), frame.end());
        if (vadParams.enabled && vad.process(frame.data(), frame.size())) { std::cout << "[VAD] Auto-stop\n"; break; }
    }

    Pa_StopStream(stream); Pa_CloseStream(stream);

    if (vadParams.enabled) {
        vad.finalize();
        const auto& segs = vad.get_segments();
        if (!segs.empty()) {
            for (const auto& s : segs) {
                size_t a = (size_t)((double)s.first  / 1000.0 * sampleRate);
                size_t b = (size_t)((double)s.second / 1000.0 * sampleRate);
                a = std::min(a, full.size()); b = std::min(b, full.size());
                if (a < b) buffer.insert(buffer.end(), full.begin()+a, full.begin()+b);
            }
        } else {
            buffer = full;
        }
    } else {
        buffer = full;
    }
    return true;
}

bool Audio::playback(int deviceId, double sampleRate, const std::vector<int16_t>& buffer) {
    if (buffer.empty()) { std::cerr << "[audio] empty buffer\n"; return false; }
    PaDeviceIndex dev = (deviceId>=0)? deviceId : Pa_GetDefaultOutputDevice();
    if (dev == paNoDevice) { std::cerr << "[audio] No output device\n"; return false; }

    double rate = pickSupportedRate(dev, true, sampleRate);
    if (rate <= 0) { std::cerr << "[audio] No supported output rate\n"; return false; }

    std::vector<int16_t> buf = (rate == sampleRate) ? buffer : resample(buffer, sampleRate, rate);

    PaStream* stream=nullptr;
    PaStreamParameters outP{}; const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    outP.device = dev; outP.channelCount = 1; outP.sampleFormat = paInt16; outP.suggestedLatency = di? di->defaultLowOutputLatency : 0.05;

    PaError err = Pa_OpenStream(&stream, nullptr, &outP, rate, paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) { std::cerr << "Pa_OpenStream: " << Pa_GetErrorText(err) << "\n"; return false; }
    if ((err = Pa_StartStream(stream)) != paNoError) { std::cerr << "Pa_StartStream: " << Pa_GetErrorText(err) << "\n"; Pa_CloseStream(stream); return false; }

    err = Pa_WriteStream(stream, buf.data(), (unsigned long)buf.size());
    if (err != paNoError && err != paOutputUnderflowed) std::cerr << "Pa_WriteStream: " << Pa_GetErrorText(err) << "\n";
    Pa_Sleep((long)(buf.size() * 1000 / rate));
    Pa_StopStream(stream); Pa_CloseStream(stream);
    return err == paNoError || err == paOutputUnderflowed;
}

#else // !WITH_AUDIO

Audio::Audio(){}
Audio::~Audio(){}
bool Audio::initialize(){ return false; }
void Audio::terminate(){}

#endif // WITH_AUDIO
