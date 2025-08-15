#include "Memory.h"
#include <fstream>
#if __has_include(<filesystem>)
#include <filesystem>
#else
#include <experimental/filesystem>
namespace std { namespace filesystem = experimental::filesystem; }
#endif
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>

// Helper to get current time as ISO 8601 string
static std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    // Use GMT for consistency
#ifdef _WIN32
    tm tm_buf;
    gmtime_s(&tm_buf, &in_time_t);
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
#else
    std::tm* tm_gmt = std::gmtime(&in_time_t);
    ss << std::put_time(tm_gmt, "%Y-%m-%dT%H:%M:%SZ");
#endif
    return ss.str();
}

// Helper to convert string to lower case
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

// --- MemoryStore implementation ---

MemoryStore::MemoryStore(const std::string& path) : path_(path) {
    // Ensure the j_ object has the basic structure even before loading.
    j_ = {
        {"version", 1},
        {"facts", nlohmann::json::object()},
        {"notes", nlohmann::json::array()},
        {"reminders", nlohmann::json::array()}
    };
}

bool MemoryStore::ensureParentDir() const {
    try {
        std::filesystem::path p(path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        // This could be a permission error, etc. For now, we don't log.
        return false;
    }
}

bool MemoryStore::load() {
    ensureParentDir();
    if (!std::filesystem::exists(path_)) {
        return true; // Use default empty structure, will be saved on first write
    }
    std::ifstream f(path_);
    if (!f) return false;
    try {
        f >> j_;
        // Basic validation: ensure top-level keys exist
        if (!j_.contains("version") || !j_.contains("facts") || !j_.contains("notes") || !j_.contains("reminders")) {
           // Re-initialize if structure is corrupt
            j_ = { {"version", 1}, {"facts", nlohmann::json::object()}, {"notes", nlohmann::json::array()}, {"reminders", nlohmann::json::array()} };
        }
    } catch (const nlohmann::json::parse_error& e) {
        // Handle case where file is corrupt or empty
        return false;
    }
    return true;
}

bool MemoryStore::save() {
    ensureParentDir();
    std::string tmp_path = path_ + ".tmp";
    std::ofstream o(tmp_path);
    if (!o) return false;
    o << j_.dump(2); // pretty print with 2 spaces
    o.close();
    if (o.fail()) return false;

    try {
        std::filesystem::rename(tmp_path, path_);
    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }
    return true;
}

std::string MemoryStore::genId() {
    auto now = std::chrono::high_resolution_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 0xFFFF);

    std::stringstream ss;
    ss << std::hex << micros << "-" << distrib(gen);
    return ss.str();
}

void MemoryStore::set(const std::string& key, const std::string& value) {
    j_["facts"][key] = value;
}

bool MemoryStore::get(const std::string& key, std::string& value) const {
    if (j_.contains("facts") && j_["facts"].contains(key)) {
        value = j_["facts"][key].get<std::string>();
        return true;
    }
    return false;
}

bool MemoryStore::del(const std::string& key) {
    if (j_.contains("facts") && j_["facts"].contains(key)) {
        j_["facts"].erase(key);
        return true;
    }
    return false;
}

std::vector<std::pair<std::string, std::string>> MemoryStore::listFacts() const {
    std::vector<std::pair<std::string, std::string>> result;
    if (!j_.contains("facts")) return result;
    for (auto const& [key, val] : j_["facts"].items()) {
        result.emplace_back(key, val.get<std::string>());
    }
    return result;
}

std::string MemoryStore::addNote(const std::string& text) {
    std::string id = genId();
    nlohmann::json note = {
        {"id", id},
        {"text", text},
        {"created_at", getCurrentTimestamp()}
    };
    j_["notes"].push_back(note);
    return id;
}

bool MemoryStore::deleteNote(const std::string& id) {
    auto& notes = j_["notes"];
    for (auto it = notes.begin(); it != notes.end(); ++it) {
        if ((*it)["id"].get<std::string>() == id) {
            notes.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<Note> MemoryStore::listNotes() const {
    std::vector<Note> result;
    if (!j_.contains("notes")) return result;
    for (const auto& item : j_["notes"]) {
        result.push_back({
            item.value("id", ""),
            item.value("text", ""),
            item.value("created_at", "")
        });
    }
    return result;
}

std::string MemoryStore::addReminder(const std::string& text, const std::string& when_iso) {
    std::string id = genId();
    nlohmann::json reminder = {
        {"id", id},
        {"text", text},
        {"when_iso", when_iso},
        {"done", false}
    };
    j_["reminders"].push_back(reminder);
    return id;
}

bool MemoryStore::completeReminder(const std::string& id) {
    auto& reminders = j_["reminders"];
    for (auto it = reminders.begin(); it != reminders.end(); ++it) {
        if ((*it)["id"].get<std::string>() == id) {
            (*it)["done"] = true;
            return true;
        }
    }
    return false;
}

std::vector<Reminder> MemoryStore::listReminders(bool includeDone) const {
    std::vector<Reminder> result;
    if (!j_.contains("reminders")) return result;
    for (const auto& item : j_["reminders"]) {
        bool isDone = item.value("done", false);
        if (includeDone || !isDone) {
            result.push_back({
                item.value("id", ""),
                item.value("text", ""),
                item.value("when_iso", ""),
                isDone
            });
        }
    }
    return result;
}


// --- Intent parsing implementation ---

Intent parseIntent(const std::string& utterance) {
    Intent intent;
    std::string lower_utterance = toLower(utterance);

    auto startsWith = [&](const std::string& prefix) {
        return lower_utterance.rfind(prefix, 0) == 0;
    };

    auto trim = [](std::string s){
        auto issp=[](unsigned char ch){return std::isspace(ch);};
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [issp](unsigned char ch){ return !issp(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [issp](unsigned char ch){ return !issp(ch); }).base(), s.end());
        return s;
    };

    // NOTE_ADD: "note: ...", "add note ...", "ajoute une note ..."
    const std::vector<std::string> note_kw = {"note:", "add note", "ajoute une note", "memo:"};
    for(const auto& kw : note_kw) {
        if (startsWith(kw)) {
            intent.type = IntentType::NOTE_ADD;
            intent.text = trim(utterance.substr(kw.length()));
            return intent;
        }
    }

    // REMINDER_ADD: "remind me ...", "rappel ...", "rappelle-moi ..."
    const std::vector<std::string> rem_kw = {"remind me to ", "remind me ", "rappel ", "rappelle-moi de ", "rappelle-moi "};
     for(const auto& kw : rem_kw) {
        if (startsWith(kw)) {
            intent.type = IntentType::REMINDER_ADD;
            intent.text = trim(utterance.substr(kw.length()));
            // Naive date parsing would go here, for now when_iso is empty
            return intent;
        }
    }

    // FACT_SET: "remember X=Y", "set key=value"
    const std::vector<std::string> fact_kw = {"remember ", "souviens-toi ", "set "};
    for(const auto& kw : fact_kw) {
        if (startsWith(kw)) {
            std::string content = trim(utterance.substr(kw.length()));
            size_t p = content.find('=');
            if (p != std::string::npos) {
                intent.type = IntentType::FACT_SET;
                intent.key = trim(content.substr(0, p));
                intent.value = trim(content.substr(p + 1));
                if (!intent.key.empty() && !intent.value.empty()) {
                    return intent;
                }
            } else if (kw == "set ") { // "set key value" variant
                 size_t p_space = content.find(' ');
                 if (p_space != std::string::npos) {
                    intent.type = IntentType::FACT_SET;
                    intent.key = trim(content.substr(0, p_space));
                    intent.value = trim(content.substr(p_space + 1));
                    if (!intent.key.empty() && !intent.value.empty()) {
                        return intent;
                    }
                 }
            }
        }
    }

    return intent; // NONE
}
