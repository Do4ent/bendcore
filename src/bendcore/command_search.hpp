/**
 * BendCore Command Search & Indexing
 * Search-as-you-type command finder
 * Part of Phase 31 foundational work
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * CommandIndexEntry: Single command in the searchable index
 */
struct CommandIndexEntry {
  std::string id;              // Command ID (e.g., "new-document")
  std::string name;            // Display name (e.g., "New Document")
  std::string category;        // create, edit, delete, import, export, etc.
  std::string tab;             // Home, Part, Assembly, Analysis, Drawing
  std::string shortcut;        // Optional keyboard shortcut (e.g., "Ctrl+N")
  std::vector<std::string> keywords;  // Search keywords
  std::string icon_color;      // Hex color code
  std::string description;     // Tooltip/description

  json to_json() const;
};

/**
 * CommandIndex: Indexed database of all 34 commands
 */
class CommandIndex {
 public:
  static CommandIndex& instance() {
    static CommandIndex index;
    return index;
  }

  // Search operations (fuzzy matching)
  std::vector<CommandIndexEntry> search(const std::string& query,
                                         int max_results = 10) const;

  // Category search
  std::vector<CommandIndexEntry> search_by_category(
      const std::string& category) const;

  // Tab search
  std::vector<CommandIndexEntry> search_by_tab(const std::string& tab) const;

  // Get single command
  CommandIndexEntry* get_command(const std::string& id);

  // Get all commands
  const std::vector<CommandIndexEntry>& get_all_commands() const {
    return entries_;
  }

  // Serialize to JSON
  json to_json() const;

 private:
  CommandIndex();
  void build_index();

  // Fuzzy match score (0-100)
  int fuzzy_match_score(const std::string& query,
                        const std::string& target) const;

  // Keyword match score
  int keyword_match_score(const std::string& query,
                          const std::vector<std::string>& keywords) const;

  std::vector<CommandIndexEntry> entries_;
  std::map<std::string, CommandIndexEntry*> id_map_;
};

}  // namespace bendcore
