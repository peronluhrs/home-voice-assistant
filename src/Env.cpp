
#include "Env.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

EnvConfig loadEnvFile(const std::string& path) {
    EnvConfig cfg;
    std::ifstream in(path);
    if (!in) {
        // fallback to environment variables if present
        if (const char* p = std::getenv("API_BASE")) cfg.apiBase = p;
        if (const char* p = std::getenv("API_KEY"))  cfg.apiKey  = p;
        if (const char* p = std::getenv("MODEL"))    cfg.model   = p;
        if (const char* p = std::getenv("LANG"))     cfg.lang    = p;
        if (const char* p = std::getenv("WAKE_WORD")) cfg.wakeWord = p;
        if (const char* p = std::getenv("ASR_ENGINE")) cfg.asrEngine = p;
        if (const char* p = std::getenv("TTS_ENGINE")) cfg.ttsEngine = p;
        return cfg;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        auto eqpos = line.find('=');
        if (eqpos == std::string::npos) continue;
        std::string key = trim(line.substr(0, eqpos));
        std::string val = trim(line.substr(eqpos + 1));
        if      (key == "API_BASE") cfg.apiBase = val;
        else if (key == "API_KEY")  cfg.apiKey  = val;
        else if (key == "MODEL")    cfg.model   = val;
        else if (key == "LANG")     cfg.lang    = val;
        else if (key == "WAKE_WORD") cfg.wakeWord = val;
        else if (key == "ASR_ENGINE") cfg.asrEngine = val;
        else if (key == "TTS_ENGINE") cfg.ttsEngine = val;
    }
    return cfg;
}
