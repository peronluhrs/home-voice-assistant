
#pragma once
#include <string>

struct ChatResult {
    bool ok = false;
    std::string text;     // assistant content or raw response
    std::string error;    // error message if any
};

class OpenAIClient {
public:
    OpenAIClient(std::string apiBase, std::string apiKey, std::string model, bool offline = false);

    // one-shot prompt => assistant reply (non-streaming)
    ChatResult chatOnce(const std::string& userMessage) const;

private:
    std::string m_apiBase;
    std::string m_apiKey;
    std::string m_model;
    bool m_offline;
};
