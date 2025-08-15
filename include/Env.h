
#pragma once
#include <string>
#include <unordered_map>

struct EnvConfig {
    std::string apiBase = "http://localhost:8000/v1";
    std::string apiKey = "EMPTY";
    std::string model  = "gpt-4o-mini";
    std::string lang   = "fr-FR";
    std::string wakeWord = "jarvis";
    std::string asrEngine = "disabled";
    std::string ttsEngine = "disabled";
};

// Charge un fichier .env (format KEY=VALUE, lignes, # pour commentaires).
EnvConfig loadEnvFile(const std::string& path);
