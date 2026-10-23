// Advanced Search and Filtering Engine for BendCore CAD
// Task 3: Search & Filters implementation (Q3 2027)
// Provides multi-field search, compound queries, full-text indexing, and saved searches

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bendcore::search {

// =========== ENUMS AND CONSTANTS ===========

enum class Operator {
    Equals,         // ==
    NotEquals,      // !=
    LessThan,       // <
    GreaterThan,    // >
    LessThanOrEqual,    // <=
    GreaterThanOrEqual, // >=
    Contains,       // contains (for strings)
    Regex           // regex match
};

enum class LogicalOperator {
    AND,
    OR
};

enum class Material {
    Steel,
    Aluminum,
    Copper,
    Titanium,
    Custom
};

enum class SearchField {
    Designation,        // string, fuzzy match
    Material,           // enum
    OD,                 // number (outer diameter)
    WallThickness,      // number
    Length,             // number (total length)
    ManufacturingMethod,// string
    Cost,               // number (cost in USD)
    CreatedDate,        // date range
    Author,             // string, exact match
    FullText            // full-text search across all fields
};

// =========== STRUCTURES ===========

// Represents a single filter condition
struct QueryFilter {
    SearchField field;
    Operator op;
    std::string value;  // string representation of value

    QueryFilter() = default;
    QueryFilter(SearchField f, Operator o, const std::string& v)
        : field(f), op(o), value(v) {}
};

// Compound query with multiple filters and logical operators
struct CompoundQuery {
    std::vector<QueryFilter> filters;
    LogicalOperator logical_op = LogicalOperator::AND;
    std::vector<CompoundQuery> nested_queries;

    // Helper to add a filter
    void add_filter(const QueryFilter& f) {
        filters.push_back(f);
    }

    // Helper to add a nested query
    void add_nested(const CompoundQuery& q) {
        nested_queries.push_back(q);
    }
};

// A single tube record for search results
struct TubeSearchRecord {
    std::string id;
    std::string designation;
    Material material;
    double od;                      // outer diameter in mm
    double wall_thickness;          // in mm
    double length;                  // total length in mm
    std::string manufacturing_method; // e.g., "Cold Drawn", "Hot Rolled"
    double cost_usd;                // cost in USD
    std::string created_date;       // ISO 8601
    std::string author;
};

// Search results
struct SearchResult {
    std::vector<TubeSearchRecord> tubes;
    int result_count = 0;
    double search_time_ms = 0.0;  // elapsed time
    std::string error_message;
};

// Saved search record
struct SavedSearch {
    std::string id;
    std::string name;
    std::string query_json;  // serialized CompoundQuery as JSON
    std::string created_at;  // ISO 8601
    std::string last_used_at;  // ISO 8601, optional
    std::string created_by;
};

// =========== SEARCH INDEX ===========

// Full-text search index with BM25 ranking
class SearchIndex {
public:
    SearchIndex() = default;
    ~SearchIndex() = default;

    // Build index from tubes
    void build_index(const std::vector<TubeSearchRecord>& tubes);

    // Update index when a tube changes
    void update_tube(const TubeSearchRecord& tube);

    // Remove tube from index
    void remove_tube(const std::string& tube_id);

    // Clear entire index
    void clear();

    // Search by token (inverted index)
    std::vector<std::string> search_token(const std::string& token) const;

    // Fuzzy search with Levenshtein distance and minimum similarity threshold
    std::vector<std::pair<std::string, double>> fuzzy_search(
        const std::string& search_term,
        const std::vector<std::string>& candidates,
        double min_similarity = 0.8) const;

    // Full-text search with BM25 ranking
    std::vector<std::pair<std::string, double>> full_text_search(
        const std::string& query,
        size_t top_k = 100) const;

    // Check if index is built
    bool is_built() const { return !inverted_index_.empty(); }

    // Get index statistics
    size_t get_index_size() const { return tube_records_.size(); }
    size_t get_vocabulary_size() const { return inverted_index_.size(); }

private:
    // Tokenize string for indexing
    std::vector<std::string> tokenize(const std::string& text) const;

    // Normalize token (lowercase, remove accents)
    std::string normalize_token(const std::string& token) const;

    // Calculate Levenshtein distance
    int levenshtein_distance(const std::string& s1, const std::string& s2) const;

    // Calculate BM25 score
    double calculate_bm25_score(
        const std::string& doc_id,
        const std::vector<std::string>& query_tokens,
        double k1 = 1.5, double b = 0.75) const;

    // Inverted index: token -> set of tube IDs
    std::unordered_map<std::string, std::unordered_set<std::string>> inverted_index_;

    // Reverse index: tube ID -> tube record
    std::unordered_map<std::string, TubeSearchRecord> tube_records_;

    // Document frequency for BM25
    std::unordered_map<std::string, size_t> document_frequency_;

    // Average document length
    double avg_doc_length_ = 0.0;
};

// =========== SEARCH ENGINE ===========

class SearchEngine {
public:
    SearchEngine() = default;
    ~SearchEngine() = default;

    // Initialize with tubes
    void initialize(const std::vector<TubeSearchRecord>& tubes);

    // Execute a search query
    SearchResult search(const CompoundQuery& query) const;

    // Simple search by single field
    SearchResult search_by_field(SearchField field, Operator op,
                                 const std::string& value) const;

    // Get the search index
    const SearchIndex& index() const { return index_; }
    SearchIndex& index() { return index_; }

    // Clear internal state
    void clear();

private:
    // Evaluate a single filter against a tube
    bool evaluate_filter(const QueryFilter& filter,
                        const TubeSearchRecord& tube) const;

    // Evaluate a compound query against a tube
    bool evaluate_query(const CompoundQuery& query,
                       const TubeSearchRecord& tube) const;

    // Parse numeric value
    std::optional<double> parse_number(const std::string& value) const;

    // Parse date value (YYYY-MM-DD format)
    std::optional<std::chrono::system_clock::time_point>
    parse_date(const std::string& value) const;

    // Parse material enum
    Material parse_material(const std::string& value) const;

    // String comparison for designation (fuzzy match)
    bool fuzzy_match(const std::string& pattern, const std::string& text,
                    int max_distance = 3) const;

    // Parse range value (min-max format like "25-50")
    std::pair<std::optional<double>, std::optional<double>> parse_range(
        const std::string& value) const;

    // Parse date range value
    std::pair<std::optional<std::chrono::system_clock::time_point>,
              std::optional<std::chrono::system_clock::time_point>>
    parse_date_range(const std::string& value) const;

    SearchIndex index_;
    std::vector<TubeSearchRecord> tubes_;  // cached tubes
};

// =========== SAVED SEARCHES MANAGER ===========

class SavedSearchesManager {
public:
    SavedSearchesManager() = default;
    ~SavedSearchesManager() = default;

    // Create a new saved search
    std::string create_saved_search(
        const std::string& name,
        const CompoundQuery& query,
        const std::string& author);

    // Get all saved searches
    std::vector<SavedSearch> get_all() const;

    // Get saved search by ID
    std::optional<SavedSearch> get_by_id(const std::string& id) const;

    // Get saved search by name
    std::optional<SavedSearch> get_by_name(const std::string& name) const;

    // Delete a saved search
    bool delete_saved_search(const std::string& id);

    // Update last_used_at timestamp
    void mark_as_used(const std::string& id);

    // Load from JSON (returns count of loaded searches)
    int load_from_json(const std::string& json);

    // Export to JSON
    std::string export_to_json() const;

    // Clear all saved searches
    void clear();

private:
    std::unordered_map<std::string, SavedSearch> searches_;
};

// =========== UTILITY FUNCTIONS ===========

// Convert enum to string
std::string operator_to_string(Operator op);
std::string field_to_string(SearchField field);
std::string material_to_string(Material m);
std::string logical_op_to_string(LogicalOperator op);

// Convert string to enum
Operator string_to_operator(const std::string& s);
SearchField string_to_field(const std::string& s);
Material string_to_material(const std::string& s);
LogicalOperator string_to_logical_op(const std::string& s);

// Serialize/deserialize CompoundQuery to/from JSON
std::string query_to_json(const CompoundQuery& query);
CompoundQuery json_to_query(const std::string& json);

// Generate UUID for searches
std::string generate_uuid();

}  // namespace bendcore::search
