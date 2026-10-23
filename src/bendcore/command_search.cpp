/**
 * BendCore Command Search Implementation
 * Fuzzy search and command indexing
 */

#include "command_search.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace bendcore {

// ============ CommandIndexEntry ============

json CommandIndexEntry::to_json() const {
  json j;
  j["id"] = id;
  j["name"] = name;
  j["category"] = category;
  j["tab"] = tab;
  if (!shortcut.empty()) {
    j["shortcut"] = shortcut;
  }
  j["keywords"] = keywords;
  j["iconColor"] = icon_color;
  j["description"] = description;
  return j;
}

// ============ CommandIndex ============

CommandIndex::CommandIndex() {
  build_index();
}

void CommandIndex::build_index() {
  // FILE OPERATIONS (Home tab)
  entries_.push_back({
      "new-document", "New Document", "create", "Home", "Ctrl+N",
      {"new", "document", "file", "create"}, "#22C55E",
      "Create a new BendCore document"});
  entries_.push_back({
      "open", "Open", "open", "Home", "Ctrl+O",
      {"open", "file", "load"}, "#22C55E",
      "Open an existing document"});
  entries_.push_back({
      "save", "Save", "save", "Home", "Ctrl+S",
      {"save", "document", "file"}, "#22C55E",
      "Save the current document"});
  entries_.push_back({
      "save-as", "Save As", "save", "Home", "Ctrl+Shift+S",
      {"save", "as", "export", "document"}, "#22C55E",
      "Save with a new filename"});
  entries_.push_back({
      "print", "Print", "output", "Home", "Ctrl+P",
      {"print", "output", "document"}, "#22C55E",
      "Print the document"});
  entries_.push_back({
      "options", "Options", "settings", "Home", "",
      {"options", "settings", "preferences"}, "#8B5CF6",
      "Configure application settings"});

  // PART DESIGN (Part tab)
  entries_.push_back({
      "new-sketch", "New Sketch", "create", "Part", "Ctrl+Shift+K",
      {"sketch", "new", "2d", "drawing"}, "#3B82F6",
      "Create a new 2D sketch"});
  entries_.push_back({
      "edit-sketch", "Edit Sketch", "edit", "Part", "Ctrl+K",
      {"sketch", "edit", "modify"}, "#3B82F6",
      "Edit an existing sketch"});
  entries_.push_back({
      "finish-sketch", "Finish Sketch", "edit", "Part", "",
      {"sketch", "finish", "done", "exit"}, "#3B82F6",
      "Exit sketch editing mode"});
  entries_.push_back({
      "new-tube", "New Tube", "create", "Part", "Ctrl+T",
      {"tube", "new", "create", "pipe"}, "#F59E0B",
      "Create a new tube element"});
  entries_.push_back({
      "edit-tube", "Edit Tube", "edit", "Part", "Ctrl+E",
      {"tube", "edit", "modify", "pipe"}, "#F59E0B",
      "Edit an existing tube"});
  entries_.push_back({
      "reference-plane", "Reference Plane", "auxiliary", "Part", "",
      {"plane", "reference", "datum"}, "#A78BFA",
      "Create a reference plane"});
  entries_.push_back({
      "reference-axis", "Reference Axis", "auxiliary", "Part", "",
      {"axis", "reference", "datum"}, "#A78BFA",
      "Create a reference axis"});
  entries_.push_back({
      "reference-point", "Reference Point", "auxiliary", "Part", "",
      {"point", "reference", "datum"}, "#A78BFA",
      "Create a reference point"});
  entries_.push_back({
      "coordinate-system", "Coordinate System", "auxiliary", "Part", "",
      {"coordinate", "system", "cs", "datum"}, "#A78BFA",
      "Create a coordinate system"});

  // ASSEMBLY (Assembly tab)
  entries_.push_back({
      "insert-component", "Insert Component", "create", "Assembly", "",
      {"insert", "component", "part", "add"}, "#06B6D4",
      "Add a component to assembly"});
  entries_.push_back({
      "import-step", "Import STEP", "import", "Assembly", "",
      {"import", "step", "file", "3d"}, "#14B8A6",
      "Import a STEP file"});
  entries_.push_back({
      "import-dwfx", "Import DWFx", "import", "Assembly", "",
      {"import", "dwfx", "file", "drawing"}, "#14B8A6",
      "Import a DWFx file"});
  entries_.push_back({
      "new-port", "New Port", "create", "Assembly", "",
      {"port", "new", "connection"}, "#06B6D4",
      "Create a new connection port"});
  entries_.push_back({
      "trace-tube", "Trace Tube", "create", "Assembly", "",
      {"trace", "tube", "path", "routing"}, "#06B6D4",
      "Trace tube path in assembly"});
  entries_.push_back({
      "add-support", "Add Support", "create", "Assembly", "",
      {"support", "add", "clamp", "fixture"}, "#06B6D4",
      "Add a support element"});
  entries_.push_back({
      "recognize-geometry", "Recognize Geometry", "analyze", "Assembly", "",
      {"recognize", "geometry", "detect", "auto"}, "#06B6D4",
      "Auto-detect geometric features"});

  // ANALYSIS (Analysis tab)
  entries_.push_back({
      "check-all", "Check All", "analyze", "Analysis", "",
      {"check", "all", "validate", "test"}, "#EF4444",
      "Run all checks and analyses"});
  entries_.push_back({
      "check-collisions", "Check Collisions", "analyze", "Analysis", "Ctrl+Alt+C",
      {"collision", "check", "intersect", "overlap"}, "#EF4444",
      "Check for collisions"});
  entries_.push_back({
      "check-gaps", "Check Gaps", "analyze", "Analysis", "Ctrl+G",
      {"gap", "check", "spacing", "distance"}, "#EF4444",
      "Check minimum gap requirements"});
  entries_.push_back({
      "measure", "Measure", "analyze", "Analysis", "",
      {"measure", "dimension", "distance", "length"}, "#10B981",
      "Measure distances and angles"});
  entries_.push_back({
      "calculate-mass", "Calculate Mass", "analyze", "Analysis", "Ctrl+M",
      {"mass", "weight", "calculate", "properties"}, "#10B981",
      "Calculate component mass"});
  entries_.push_back({
      "generate-report", "Generate Report", "output", "Analysis", "",
      {"report", "generate", "output", "export"}, "#8B5CF6",
      "Generate analysis report"});

  // DRAWING (Drawing tab)
  entries_.push_back({
      "new-sheet", "New Sheet", "create", "Drawing", "Ctrl+D",
      {"sheet", "new", "drawing", "page"}, "#EC4899",
      "Create a new drawing sheet"});
  entries_.push_back({
      "create-view", "Create View", "create", "Drawing", "",
      {"view", "create", "projection", "orthographic"}, "#EC4899",
      "Create a drawing view"});
  entries_.push_back({
      "bend-table", "Bend Table", "create", "Drawing", "",
      {"table", "bend", "list", "schedule"}, "#EC4899",
      "Insert a bend schedule table"});
  entries_.push_back({
      "unfold-drawing", "Unfold Drawing", "create", "Drawing", "",
      {"unfold", "flat", "pattern", "development"}, "#EC4899",
      "Create an unfolded/flat pattern drawing"});
  entries_.push_back({
      "add-dimension", "Add Dimension", "create", "Drawing", "",
      {"dimension", "add", "annotation", "text"}, "#EC4899",
      "Add a dimension to drawing"});
  entries_.push_back({
      "add-text", "Add Text", "create", "Drawing", "",
      {"text", "add", "annotation", "label"}, "#EC4899",
      "Add text annotation"});

  // Build ID map
  for (auto& entry : entries_) {
    id_map_[entry.id] = &entry;
  }
}

std::vector<CommandIndexEntry> CommandIndex::search(
    const std::string& query,
    int max_results) const {
  if (query.empty()) {
    // Return most common commands
    std::vector<CommandIndexEntry> result;
    for (const auto& entry : entries_) {
      if (result.size() < static_cast<size_t>(max_results)) {
        result.push_back(entry);
      }
    }
    return result;
  }

  // Score each command
  struct ScoredEntry {
    CommandIndexEntry entry;
    int score = 0;
  };

  std::vector<ScoredEntry> scored;
  for (const auto& entry : entries_) {
    int score = 0;

    // Name match (highest weight)
    score += fuzzy_match_score(query, entry.name) * 3;

    // ID match (medium weight)
    score += fuzzy_match_score(query, entry.id) * 2;

    // Keyword match (medium weight)
    score += keyword_match_score(query, entry.keywords) * 2;

    // Category match (low weight)
    score += fuzzy_match_score(query, entry.category);

    // Tab match (low weight)
    score += fuzzy_match_score(query, entry.tab);

    if (score > 0) {
      scored.push_back({entry, score});
    }
  }

  // Sort by score (descending)
  std::sort(scored.begin(), scored.end(),
            [](const ScoredEntry& a, const ScoredEntry& b) {
              return a.score > b.score;
            });

  // Extract results
  std::vector<CommandIndexEntry> result;
  for (size_t i = 0; i < scored.size() && i < static_cast<size_t>(max_results);
       ++i) {
    result.push_back(scored[i].entry);
  }

  return result;
}

std::vector<CommandIndexEntry> CommandIndex::search_by_category(
    const std::string& category) const {
  std::vector<CommandIndexEntry> result;
  for (const auto& entry : entries_) {
    if (entry.category == category) {
      result.push_back(entry);
    }
  }
  return result;
}

std::vector<CommandIndexEntry> CommandIndex::search_by_tab(
    const std::string& tab) const {
  std::vector<CommandIndexEntry> result;
  for (const auto& entry : entries_) {
    if (entry.tab == tab) {
      result.push_back(entry);
    }
  }
  return result;
}

CommandIndexEntry* CommandIndex::get_command(const std::string& id) {
  auto it = id_map_.find(id);
  if (it != id_map_.end()) {
    return it->second;
  }
  return nullptr;
}

json CommandIndex::to_json() const {
  json j = json::array();
  for (const auto& entry : entries_) {
    j.push_back(entry.to_json());
  }
  return j;
}

// Fuzzy matching algorithm (Levenshtein-like)
int CommandIndex::fuzzy_match_score(const std::string& query,
                                     const std::string& target) const {
  // Convert to lowercase for comparison
  std::string q = query;
  std::string t = target;
  std::transform(q.begin(), q.end(), q.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::transform(t.begin(), t.end(), t.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // Exact match
  if (q == t) return 100;

  // Substring match
  if (t.find(q) != std::string::npos) return 80;

  // Character sequence match (e.g., "nd" matches "new document")
  int matched = 0;
  size_t t_pos = 0;
  for (char qc : q) {
    size_t pos = t.find(qc, t_pos);
    if (pos != std::string::npos) {
      matched++;
      t_pos = pos + 1;
    }
  }

  if (matched == static_cast<int>(q.length())) {
    return 40 + (matched * 5);  // Proportional bonus
  }

  return 0;
}

int CommandIndex::keyword_match_score(
    const std::string& query,
    const std::vector<std::string>& keywords) const {
  int max_score = 0;
  for (const auto& keyword : keywords) {
    int score = fuzzy_match_score(query, keyword);
    max_score = std::max(max_score, score);
  }
  return max_score;
}

}  // namespace bendcore
