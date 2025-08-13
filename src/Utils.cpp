#include "Utils.h"
#include <cctype>
#include <algorithm>

static inline bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && is_space(s[i])) ++i;
    return s.substr(i);
}

std::string rtrim(const std::string& s) {
    size_t i = s.size();
    while (i > 0 && is_space(s[i - 1])) --i;
    return s.substr(0, i);
}

std::string trim(const std::string& s) {
    return rtrim(ltrim(s));
}

// naïf: cherche "field":"..."; à remplacer par nlohmann/json plus tard
std::string extractJsonStringField(const std::string& json, const std::string& field) {
    std::string pat = "\"" + field + "\":\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return std::string();
    pos += pat.size();

    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\') {
            if (pos < json.size()) {
                char next = json[pos++];
                switch (next) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'r':  out.push_back('\r'); break;
                    default:   out.push_back(next); break;
                }
            }
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& items, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += sep;
        out += items[i];
    }
    return out;
}
