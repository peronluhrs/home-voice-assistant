
#include "OpenAIClient.h"
#include "Utils.h"
#include <string>
#include <sstream>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

OpenAIClient::OpenAIClient(std::string apiBase, std::string apiKey, std::string model)
: m_apiBase(std::move(apiBase)), m_apiKey(std::move(apiKey)), m_model(std::move(model)) {}

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
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ChatResult{false, "", "curl_easy_init failed"};
    }

    std::string url = m_apiBase;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/chat/completions";

    std::string payload;
    {
        // Build minimal JSON by hand.
        // {"model":"...","messages":[{"role":"system","content":"You are a helpful French voice assistant."},{"role":"user","content":"..."}]}
        std::ostringstream oss;
        oss << "{";
        oss << ""model":"" << m_model << "",";
        oss << ""stream":false,";
        oss << ""messages":["
            << "{"role":"system","content":"You are a concise assistant. Reply in the user's language."},"
            << "{"role":"user","content":"" << userMessage << ""}"
            << "]";
        oss << "}";
        payload = oss.str();
    }

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
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
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
    if (code < 200 || code >= 300) {
        return ChatResult{false, response, "HTTP status " + std::to_string(code)};
    }

    // Try to extract the first choices[0].message.content
    // This is a naive extraction; replace with a proper JSON library later.
    // Look for "content":"..."
    std::string content = extractJsonStringField(response, "content");
    if (content.empty()) {
        // fallback: return raw JSON
        return ChatResult{true, response, ""};
    }
    return ChatResult{true, content, ""};
#endif
}
