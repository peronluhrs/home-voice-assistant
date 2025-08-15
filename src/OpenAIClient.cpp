
#include "OpenAIClient.h"
#include <string>
#include <sstream>
#include "nlohmann/json.hpp"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

OpenAIClient::OpenAIClient(std::string apiBase, std::string apiKey, std::string model, bool offline)
: m_apiBase(std::move(apiBase)), m_apiKey(std::move(apiKey)), m_model(std::move(model)), m_offline(offline) {}

#ifdef HAVE_CURL
static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = reinterpret_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
#endif

ChatResult OpenAIClient::chatOnce(const std::string& userMessage) const {
#ifndef HAVE_CURL
    // Offline echo mode
    ChatResult r;
    r.ok = true;
    r.text = "(offline) Echo: " + userMessage;
    return r;
#else
    if (m_offline) {
        ChatResult r;
        r.ok = true;
        r.text = "(offline) Echo: " + userMessage;
        return r;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return ChatResult{false, "", "curl_easy_init failed"};
    }

    std::string url = m_apiBase;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/chat/completions";

    nlohmann::json payload = {
        {"model", m_model},
        {"stream", false},
        {"messages", {
            {{"role", "system"}, {"content", "You are a concise assistant. Reply in the user's language."}},
            {{"role", "user"}, {"content", userMessage}}
        }}
    };
    const std::string payload_str = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!m_apiKey.empty() && m_apiKey != "EMPTY") {
        std::string auth = std::string("Authorization: Bearer ") + m_apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return ChatResult{false, "", std::string("curl error: ") + curl_easy_strerror(res)};
    }

    try {
        nlohmann::json j = nlohmann::json::parse(response);
        if (code < 200 || code >= 300) {
            std::string error_msg = "HTTP status " + std::to_string(code);
            if (j.contains("error") && j["error"].is_object() && j["error"].contains("message")) {
                error_msg += ": " + j["error"]["message"].get<std::string>();
            }
            return ChatResult{false, response, error_msg};
        }

        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            const auto& first_choice = j["choices"][0];
            if (first_choice.contains("message") && first_choice["message"].is_object() && first_choice["message"].contains("content")) {
                return ChatResult{true, first_choice["message"]["content"].get<std::string>(), ""};
            }
        }
    } catch (const nlohmann::json::parse_error& e) {
        return ChatResult{false, response, std::string("JSON parse error: ") + e.what()};
    }

    // Fallback if structure is not as expected
    return ChatResult{false, response, "Unexpected JSON structure from API"};
#endif
}
