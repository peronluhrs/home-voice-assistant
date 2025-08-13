#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <limits>

#include "OpenAIClient.h"

// --- config minimale (fallback si pas d'Env.h)
struct AppCfg {
    std::string apiBase = "http://localhost:8000/v1";
    std::string apiKey  = "EMPTY";
    std::string model   = "gpt-4o-mini";
};
static AppCfg loadCfg(const std::string& path) {
    AppCfg c;
    std::ifstream f(path);
    if (!f) return c;
    std::string line;
    auto trim = [](std::string s){
        auto issp=[](unsigned char ch){return std::isspace(ch);};
        while(!s.empty()&&issp(s.front())) s.erase(s.begin());
        while(!s.empty()&&issp(s.back()))  s.pop_back();
        return s;
    };
    while (std::getline(f,line)) {
        if (line.empty()||line[0]=='#') continue;
        auto p=line.find('=');
        if (p==std::string::npos) continue;
        std::string k=trim(line.substr(0,p)), v=trim(line.substr(p+1));
        if (k=="API_BASE") c.apiBase=v; else if (k=="API_KEY") c.apiKey=v; else if (k=="MODEL") c.model=v;
    }
    return c;
}

#ifdef WITH_AUDIO
#include <portaudio.h>
struct DeviceInfo { int index; std::string name; std::string host; };
static double g_lastSampleRate = 16000.0;

static std::vector<DeviceInfo> listPaDevices() {
    std::vector<DeviceInfo> out;
    Pa_Initialize();
    int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
        out.push_back({ i, di && di->name ? di->name : "", hai && hai->name ? hai->name : "" });
    }
    Pa_Terminate();
    return out;
}

static int findDeviceIndex(const std::string& key, bool output) {
    Pa_Initialize();
    int n = Pa_GetDeviceCount();
    int best = -1;
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        if (!di) continue;
        if (output && di->maxOutputChannels <= 0) continue;
        if (!output && di->maxInputChannels <= 0) continue;
        std::string nm = di->name ? di->name : "";
        if (!key.empty() && nm.find(key) != std::string::npos) { best = i; break; }
    }
    if (best < 0) best = output ? Pa_GetDefaultOutputDevice() : Pa_GetDefaultInputDevice();
    Pa_Terminate();
    return best;
}

static double pickSupportedRate(bool output, PaDeviceIndex dev, double preferred) {
    const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    double def = (di && di->defaultSampleRate > 0) ? di->defaultSampleRate : 48000.0;
    double cand[] = { preferred, def, 48000.0, 44100.0, 32000.0, 22050.0, 16000.0 };
    PaStreamParameters inP{}; PaStreamParameters outP{};
    inP.device=dev; inP.channelCount=1; inP.sampleFormat=paInt16;
    outP.device=dev; outP.channelCount=1; outP.sampleFormat=paInt16;
    for (double r : cand) {
        if (r <= 0) continue;
        PaError ok = output ? Pa_IsFormatSupported(nullptr, &outP, r)
                            : Pa_IsFormatSupported(&inP, nullptr, r);
        if (ok == paFormatIsSupported) return r;
    }
    return -1.0;
}

static std::vector<int16_t> recordSeconds(int seconds, int deviceIndex) {
    std::vector<int16_t> buf;
    PaError err = Pa_Initialize();
    if (err != paNoError) { std::cerr << "[audio] Pa_Initialize(in): " << Pa_GetErrorText(err) << "\n"; return {}; }
    PaDeviceIndex dev = (deviceIndex >= 0) ? deviceIndex : Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) { std::cerr << "[audio] Aucun périphérique d'entrée\n"; Pa_Terminate(); return {}; }

    double rate = pickSupportedRate(false, dev, g_lastSampleRate);
    if (rate <= 0) { std::cerr << "[audio] Aucun sample-rate supporté (in)\n"; Pa_Terminate(); return {}; }
    g_lastSampleRate = rate;

    int frames = seconds * (int)rate;
    buf.assign(frames, 0);

    PaStream* stream = nullptr;
    PaStreamParameters inParams{};
    const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    inParams.device = dev; inParams.channelCount = 1; inParams.sampleFormat = paInt16;
    inParams.suggestedLatency = di ? di->defaultLowInputLatency : 0.050;

    err = Pa_OpenStream(&stream, &inParams, nullptr, rate, paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) { std::cerr << "[audio] OpenStream(in): " << Pa_GetErrorText(err) << "\n"; Pa_Terminate(); return {}; }
    if ((err = Pa_StartStream(stream)) != paNoError) { std::cerr << "[audio] StartStream(in): " << Pa_GetErrorText(err) << "\n"; Pa_CloseStream(stream); Pa_Terminate(); return {}; }

    err = Pa_ReadStream(stream, buf.data(), (unsigned long)frames);
    if (err != paNoError) std::cerr << "[audio] ReadStream: " << Pa_GetErrorText(err) << "\n";

    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
    return buf;
}

static void playbackPcm(const std::vector<int16_t>& pcm, int deviceIndex) {
    if (pcm.empty()) { std::cerr << "[audio] PCM vide\n"; return; }
    PaError err = Pa_Initialize();
    if (err != paNoError) { std::cerr << "[audio] Pa_Initialize(out): " << Pa_GetErrorText(err) << "\n"; return; }
    PaDeviceIndex dev = (deviceIndex >= 0) ? deviceIndex : Pa_GetDefaultOutputDevice();
    if (dev == paNoDevice) { std::cerr << "[audio] Aucun périphérique de sortie\n"; Pa_Terminate(); return; }

    double rate = pickSupportedRate(true, dev, g_lastSampleRate);
    if (rate <= 0) { std::cerr << "[audio] Aucun sample-rate supporté (out)\n"; Pa_Terminate(); return; }

    PaStream* stream = nullptr;
    PaStreamParameters outParams{};
    const PaDeviceInfo* di = Pa_GetDeviceInfo(dev);
    outParams.device = dev; outParams.channelCount = 1; outParams.sampleFormat = paInt16;
    outParams.suggestedLatency = di ? di->defaultLowOutputLatency : 0.050;

    err = Pa_OpenStream(&stream, nullptr, &outParams, rate, paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) { std::cerr << "[audio] OpenStream(out): " << Pa_GetErrorText(err) << "\n"; Pa_Terminate(); return; }
    if ((err = Pa_StartStream(stream)) != paNoError) { std::cerr << "[audio] StartStream(out): " << Pa_GetErrorText(err) << "\n"; Pa_CloseStream(stream); Pa_Terminate(); return; }

    err = Pa_WriteStream(stream, pcm.data(), (unsigned long)pcm.size());
    if (err != paNoError) std::cerr << "[audio] WriteStream: " << Pa_GetErrorText(err) << "\n";

    Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();
}
#endif // WITH_AUDIO

struct Args {
    bool offline=false;
    bool withAudio=false;
    bool listDevices=false;
    int  recordSeconds=5;
    std::string inKey, outKey;
};
static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i=1;i<argc;i++) {
        std::string s = argv[i];
        auto next = [&](std::string& dst){ if (i+1<argc) dst=argv[++i]; };
        if (s=="--offline") a.offline=true;
        else if (s=="--with-audio") a.withAudio=true;
        else if (s=="--list-devices") a.listDevices=true;
        else if (s=="--record-seconds") { std::string v; next(v); a.recordSeconds = std::max(1, std::atoi(v.c_str())); }
        else if (s=="--input-device") next(a.inKey);
        else if (s=="--output-device") next(a.outKey);
    }
    return a;
}
static bool isNumber(const std::string& s){ if(s.empty())return false; for(char c:s) if(!std::isdigit((unsigned char)c)) return false; return true; }

int main(int argc, char** argv) {
    AppCfg cfg = loadCfg("config/app.env");
    const char* e;
    if ((e=getenv("API_BASE"))) cfg.apiBase = e;
    if ((e=getenv("API_KEY" ))) cfg.apiKey  = e;
    if ((e=getenv("MODEL"   ))) cfg.model   = e;

    Args args = parseArgs(argc, argv);

    std::cout << "[assistant] prêt. tape /exit pour quitter.\n";
    std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

#ifdef WITH_AUDIO
    if (args.listDevices) {
        auto devs = listPaDevices();
        for (auto& d : devs) std::cout << d.index << " : " << d.name << " [" << d.host << "]\n";
        return 0;
    }
#endif

    OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model);

#ifdef WITH_AUDIO
    if (args.withAudio) {
        std::cout << "[audio] Appuie Entrée pour parler (" << args.recordSeconds << "s)…";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        int inIdx  = -1, outIdx = -1;
        if (!args.inKey.empty())  inIdx  = isNumber(args.inKey)  ? std::atoi(args.inKey.c_str())  : findDeviceIndex(args.inKey, false);
        if (!args.outKey.empty()) outIdx = isNumber(args.outKey) ? std::atoi(args.outKey.c_str()) : findDeviceIndex(args.outKey, true);

        auto pcm = recordSeconds(args.recordSeconds, inIdx);
        std::string userText = "[audio] (capture " + std::to_string(args.recordSeconds) + "s)";
        std::string reply;
        if (args.offline) reply = "(offline) Echo: " + userText;
        else {
            auto r = client.chatOnce(userText);
            reply = r.ok ? r.text : std::string("[error] ")+(!r.error.empty()?r.error:r.text);
        }
        std::cout << "assistant> " << reply << "\n";
        playbackPcm(pcm, outIdx);
        return 0;
    }
#endif

    std::string line;
    while (true) {
        std::cout << "you> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "/exit") { std::cout << "[assistant] au revoir.\n"; break; }
        if (args.offline) { std::cout << "assistant> (offline) Echo: " << line << "\n"; continue; }
        ChatResult r = client.chatOnce(line);
        if (!r.ok) std::cout << "assistant> [error] " << (!r.error.empty()?r.error:r.text) << "\n";
        else       std::cout << "assistant> " << r.text << "\n";
    }
    return 0;
}
