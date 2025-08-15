#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "nlohmann/json.hpp"

struct Note { std::string id; std::string text; std::string created_at; };
struct Reminder { std::string id; std::string text; std::string when_iso; bool done=false; };

class MemoryStore {
public:
  explicit MemoryStore(const std::string& path = "data/memory.json");
  bool load();                  // creates file if missing
  bool save();                  // atomic: write tmp + rename
  // facts (key/value)
  void set(const std::string& key, const std::string& value);
  bool get(const std::string& key, std::string& value) const;
  bool del(const std::string& key);
  std::vector<std::pair<std::string,std::string>> listFacts() const;

  // notes
  std::string addNote(const std::string& text);
  bool deleteNote(const std::string& id);
  std::vector<Note> listNotes() const;

  // reminders
  std::string addReminder(const std::string& text, const std::string& when_iso);
  bool completeReminder(const std::string& id);
  std::vector<Reminder> listReminders(bool includeDone=true) const;

  // Http methods
  nlohmann::json toJson() const { return j_; }
  void fromJson(const nlohmann::json& j, bool merge = false) {
      if (merge) j_.merge_patch(j);
      else j_ = j;
  }
  void clear() { j_.clear(); }

private:
  std::string path_;
  nlohmann::json j_; // { "version":1, "facts":{...}, "notes":[...], "reminders":[...] }
  bool ensureParentDir() const; // create data/ if missing
  static std::string genId();   // e.g. timestamp + random
};

// Simple intent parsing (very small heuristic, FR/EN mixed)
enum class IntentType { NONE, NOTE_ADD, REMINDER_ADD, FACT_SET };
struct Intent {
  IntentType type = IntentType::NONE;
  std::string text;
  std::string when_iso;     // for reminder
  std::string key, value;   // for fact
};

Intent parseIntent(const std::string& utterance);
