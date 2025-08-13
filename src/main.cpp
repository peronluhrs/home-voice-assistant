
#include <iostream>
#include <string>
#include <vector>
#include "Env.h"
#include "OpenAIClient.h"

int main() {
    // Load config
    const std::string envPath = "config/app.env";
    EnvConfig cfg = loadEnvFile(envPath);

    std::cout << "[assistant] prêt. tape /exit pour quitter.\n";
    std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

    OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model);

    std::string line;
    while (true) {
        std::cout << "you> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "/exit") break;
        if (line.empty()) continue;

        ChatResult r = client.chatOnce(line);
        if (!r.ok && !r.error.empty()) {
            std::cout << "assistant> [error] " << r.error << "\n";
            if (!r.text.empty()) {
                std::cout << "assistant> [raw] " << r.text.substr(0, 4000) << "\n";
            }
            continue;
        }
        std::cout << "assistant> " << r.text << "\n";
    }
    std::cout << "[assistant] au revoir.\n";
    return 0;
}
