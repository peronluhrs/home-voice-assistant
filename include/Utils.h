
#pragma once
#include <string>
#include <vector>

// trim helpers
std::string ltrim(const std::string& s);
std::string rtrim(const std::string& s);
std::string trim(const std::string& s);

// naive helper to extract the first occurrence of a JSON field value: "field":"VALUE"
// This is not a real JSON parser; replace with nlohmann/json in production.
std::string extractJsonStringField(const std::string& json, const std::string& field);

// Tiny join for building JSON arrays manually
std::string join(const std::vector<std::string>& items, const std::string& sep);
