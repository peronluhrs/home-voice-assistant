
#include <iostream>
#include <string>
#include <vector>
#include "Env.h"
#include "OpenAIClient.h"

int main(int argc, char* argv[]) {
    // Load config
    const std::string envPath = "config/app.env";
    EnvConfig cfg = loadEnvFile(envPath);
    bool offline = false;

    // Parse command-line arguments
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--api-base" && i + 1 < args.size()) {
            cfg.apiBase = args[++i];
        } else if (args[i] == "--model" && i + 1 < args.size()) {
            cfg.model = args[++i];
        } else if (args[i] == "--api-key" && i + 1 < args.size()) {
            cfg.apiKey = args[++i];
        } else if (args[i] == "--offline") {
            offline = true;
        }
    }

    std::cout << "[assistant] prêt. tape /exit pour quitter.\n";
    std::cout << "[cfg] API_BASE=" << cfg.apiBase << " MODEL=" << cfg.model << "\n";

    OpenAIClient client(cfg.apiBase, cfg.apiKey, cfg.model, offline);

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
