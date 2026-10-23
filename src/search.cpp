// Advanced Search and Filtering Engine Implementation
// Task 3: Search & Filters for BendCore CAD (Q3 2027)

#include "bendcore/search.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

namespace bendcore::search {

using json = nlohmann::json;

// =========== UTILITY FUNCTIONS ===========

std::string operator_to_string(Operator op) {
    switch (op) {
        case Operator::Equals: return "==";
        case Operator::NotEquals: return "!=";
        case Operator::LessThan: return "<";
        case Operator::GreaterThan: return ">";
        case Operator::LessThanOrEqual: return "<=";
        case Operator::GreaterThanOrEqual: return ">=";
        case Operator::Contains: return "contains";
        case Operator::Regex: return "regex";
        default: return "unknown";
    }
}

std::string field_to_string(SearchField field) {
    switch (field) {
        case SearchField::Designation: return "designation";
        case SearchField::Material: return "material";
        case SearchField::OD: return "od";
        case SearchField::WallThickness: return "wall_thickness";
        case SearchField::Length: return "length";
        case SearchField::ManufacturingMethod: return "manufacturing_method";
        case SearchField::Cost: return "cost_usd";
        case SearchField::CreatedDate: return "created_date";
        case SearchField::Author: return "author";
        case SearchField::FullText: return "full_text";
        default: return "unknown";
    }
}

std::string material_to_string(Material m) {
    switch (m) {
        case Material::Steel: return "Steel";
        case Material::Aluminum: return "Aluminum";
        case Material::Copper: return "Copper";
        case Material::Titanium: return "Titanium";
        case Material::Custom: return "Custom";
        default: return "Unknown";
    }
}

std::string logical_op_to_string(LogicalOperator op) {
    return (op == LogicalOperator::AND) ? "AND" : "OR";
}

Operator string_to_operator(const std::string& s) {
    if (s == "==") return Operator::Equals;
    if (s == "!=") return Operator::NotEquals;
    if (s == "<") return Operator::LessThan;
    if (s == ">") return Operator::GreaterThan;
    if (s == "<=") return Operator::LessThanOrEqual;
    if (s == ">=") return Operator::GreaterThanOrEqual;
    if (s == "contains") return Operator::Contains;
    if (s == "regex") return Operator::Regex;
    throw std::invalid_argument("Unknown operator: " + s);
}

SearchField string_to_field(const std::string& s) {
    if (s == "designation") return SearchField::Designation;
    if (s == "material") return SearchField::Material;
    if (s == "od") return SearchField::OD;
    if (s == "wall_thickness") return SearchField::WallThickness;
    if (s == "wall") return SearchField::WallThickness;
    if (s == "length") return SearchField::Length;
    if (s == "manufacturing_method") return SearchField::ManufacturingMethod;
    if (s == "method") return SearchField::ManufacturingMethod;
    if (s == "cost_usd") return SearchField::Cost;
    if (s == "cost") return SearchField::Cost;
    if (s == "created_date") return SearchField::CreatedDate;
    if (s == "author") return SearchField::Author;
    if (s == "full_text") return SearchField::FullText;
    throw std::invalid_argument("Unknown field: " + s);
}

Material string_to_material(const std::string& s) {
    if (s == "Steel") return Material::Steel;
    if (s == "Aluminum") return Material::Aluminum;
    if (s == "Copper") return Material::Copper;
    if (s == "Titanium") return Material::Titanium;
    if (s == "Custom") return Material::Custom;
    return Material::Custom;
}

LogicalOperator string_to_logical_op(const std::string& s) {
    return (s == "AND") ? LogicalOperator::AND : LogicalOperator::OR;
}

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::string uuid;
    const char* hex = "0123456789abcdef";

    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            uuid += '-';
        }
        uuid += hex[dis(gen)];
    }

    return uuid;
}

// =========== SEARCH INDEX IMPLEMENTATION ===========

void SearchIndex::build_index(const std::vector<TubeSearchRecord>& tubes) {
    clear();

    double total_doc_length = 0.0;

    for (const auto& tube : tubes) {
        tube_records_[tube.id] = tube;

        // Build full text from all fields
        std::string full_text = tube.designation + " " + tube.author + " " +
                               material_to_string(tube.material) + " " +
                               tube.manufacturing_method;

        // Tokenize and index
        auto tokens = tokenize(full_text);
        total_doc_length += tokens.size();

        // Build inverted index and track document frequency
        std::unordered_set<std::string> unique_tokens;
        for (const auto& token : tokens) {
            inverted_index_[token].insert(tube.id);
            unique_tokens.insert(token);
        }

        // Update document frequency
        for (const auto& token : unique_tokens) {
            document_frequency_[token]++;
        }
    }

    // Calculate average document length for BM25
    if (!tubes.empty()) {
        avg_doc_length_ = total_doc_length / static_cast<double>(tubes.size());
    }
}

void SearchIndex::update_tube(const TubeSearchRecord& tube) {
    remove_tube(tube.id);

    std::vector<TubeSearchRecord> tubes;
    for (const auto& [id, rec] : tube_records_) {
        tubes.push_back(rec);
    }
    tubes.push_back(tube);

    build_index(tubes);
}

void SearchIndex::remove_tube(const std::string& tube_id) {
    tube_records_.erase(tube_id);

    // Remove from inverted index
    for (auto& [token, ids] : inverted_index_) {
        ids.erase(tube_id);
    }
}

void SearchIndex::clear() {
    inverted_index_.clear();
    tube_records_.clear();
    document_frequency_.clear();
    avg_doc_length_ = 0.0;
}

std::vector<std::string> SearchIndex::search_token(const std::string& token) const {
    std::string lower_token = token;
    std::transform(lower_token.begin(), lower_token.end(),
                  lower_token.begin(), ::tolower);

    auto it = inverted_index_.find(lower_token);
    if (it != inverted_index_.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

int SearchIndex::levenshtein_distance(const std::string& s1,
                                      const std::string& s2) const {
    const auto len1 = s1.size();
    const auto len2 = s2.size();

    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i) d[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) d[0][j] = j;

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1,      // deletion
                               d[i][j - 1] + 1,      // insertion
                               d[i - 1][j - 1] + cost});  // substitution
        }
    }

    return d[len1][len2];
}

std::vector<std::string> SearchIndex::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string token;
    std::string lower_text = text;

    // Convert to lowercase
    std::transform(lower_text.begin(), lower_text.end(),
                  lower_text.begin(), ::tolower);

    // Split by non-alphanumeric characters
    for (char c : lower_text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            token += c;
        } else {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<std::pair<std::string, double>> SearchIndex::fuzzy_search(
    const std::string& search_term,
    const std::vector<std::string>& candidates,
    double min_similarity) const {

    std::vector<std::pair<std::string, double>> results;
    std::string lower_term = search_term;
    std::transform(lower_term.begin(), lower_term.end(),
                  lower_term.begin(), ::tolower);

    for (const auto& candidate : candidates) {
        std::string lower_candidate = candidate;
        std::transform(lower_candidate.begin(), lower_candidate.end(),
                      lower_candidate.begin(), ::tolower);

        int dist = levenshtein_distance(lower_term, lower_candidate);
        double max_len = std::max(lower_term.length(), lower_candidate.length());

        if (max_len > 0) {
            double similarity = 1.0 - (static_cast<double>(dist) / max_len);

            if (similarity >= min_similarity) {
                results.emplace_back(candidate, similarity);
            }
        }
    }

    // Sort by similarity descending
    std::sort(results.begin(), results.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

    return results;
}

// =========== SEARCH ENGINE IMPLEMENTATION ===========

void SearchEngine::initialize(const std::vector<TubeSearchRecord>& tubes) {
    tubes_ = tubes;
    index_.build_index(tubes);
}

SearchResult SearchEngine::search(const CompoundQuery& query) const {
    auto start = std::chrono::high_resolution_clock::now();

    SearchResult result;

    for (const auto& tube : tubes_) {
        if (evaluate_query(query, tube)) {
            result.tubes.push_back(tube);
        }
    }

    result.result_count = result.tubes.size();

    auto end = std::chrono::high_resolution_clock::now();
    result.search_time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

SearchResult SearchEngine::search_by_field(SearchField field, Operator op,
                                          const std::string& value) const {
    CompoundQuery query;
    query.add_filter(QueryFilter(field, op, value));
    return search(query);
}

bool SearchEngine::evaluate_filter(const QueryFilter& filter,
                                   const TubeSearchRecord& tube) const {
    switch (filter.field) {
        case SearchField::Designation: {
            if (filter.op == Operator::Contains) {
                return fuzzy_match(filter.value, tube.designation);
            } else if (filter.op == Operator::Equals) {
                return tube.designation == filter.value;
            }
            break;
        }

        case SearchField::Material: {
            Material m = string_to_material(filter.value);
            if (filter.op == Operator::Equals) {
                return tube.material == m;
            } else if (filter.op == Operator::NotEquals) {
                return tube.material != m;
            }
            break;
        }

        case SearchField::OD: {
            // Support both single value and range queries
            auto range_opt = parse_range(filter.value);
            if (range_opt.first && range_opt.second) {
                // Range query (e.g., "25-50")
                return tube.od >= *range_opt.first && tube.od <= *range_opt.second;
            }

            auto value_opt = parse_number(filter.value);
            if (!value_opt) return false;

            double value = *value_opt;
            switch (filter.op) {
                case Operator::Equals: return tube.od == value;
                case Operator::NotEquals: return tube.od != value;
                case Operator::LessThan: return tube.od < value;
                case Operator::GreaterThan: return tube.od > value;
                case Operator::LessThanOrEqual: return tube.od <= value;
                case Operator::GreaterThanOrEqual: return tube.od >= value;
                default: break;
            }
            break;
        }

        case SearchField::WallThickness: {
            // Support both single value and range queries
            auto range_opt = parse_range(filter.value);
            if (range_opt.first && range_opt.second) {
                // Range query (e.g., "1-2.5")
                return tube.wall_thickness >= *range_opt.first &&
                       tube.wall_thickness <= *range_opt.second;
            }

            auto value_opt = parse_number(filter.value);
            if (!value_opt) return false;

            double value = *value_opt;
            switch (filter.op) {
                case Operator::Equals: return tube.wall_thickness == value;
                case Operator::NotEquals: return tube.wall_thickness != value;
                case Operator::LessThan: return tube.wall_thickness < value;
                case Operator::GreaterThan: return tube.wall_thickness > value;
                case Operator::LessThanOrEqual: return tube.wall_thickness <= value;
                case Operator::GreaterThanOrEqual: return tube.wall_thickness >= value;
                default: break;
            }
            break;
        }

        case SearchField::Length: {
            // Support both single value and range queries
            auto range_opt = parse_range(filter.value);
            if (range_opt.first && range_opt.second) {
                // Range query (e.g., "100-500")
                return tube.length >= *range_opt.first && tube.length <= *range_opt.second;
            }

            auto value_opt = parse_number(filter.value);
            if (!value_opt) return false;

            double value = *value_opt;
            switch (filter.op) {
                case Operator::Equals: return tube.length == value;
                case Operator::NotEquals: return tube.length != value;
                case Operator::LessThan: return tube.length < value;
                case Operator::GreaterThan: return tube.length > value;
                case Operator::LessThanOrEqual: return tube.length <= value;
                case Operator::GreaterThanOrEqual: return tube.length >= value;
                default: break;
            }
            break;
        }

        case SearchField::Cost: {
            // Support both single value and range queries
            auto range_opt = parse_range(filter.value);
            if (range_opt.first && range_opt.second) {
                // Range query (e.g., "50-200")
                return tube.cost_usd >= *range_opt.first &&
                       tube.cost_usd <= *range_opt.second;
            }

            auto value_opt = parse_number(filter.value);
            if (!value_opt) return false;

            double value = *value_opt;
            switch (filter.op) {
                case Operator::Equals: return tube.cost_usd == value;
                case Operator::NotEquals: return tube.cost_usd != value;
                case Operator::LessThan: return tube.cost_usd < value;
                case Operator::GreaterThan: return tube.cost_usd > value;
                case Operator::LessThanOrEqual: return tube.cost_usd <= value;
                case Operator::GreaterThanOrEqual: return tube.cost_usd >= value;
                default: break;
            }
            break;
        }

        case SearchField::ManufacturingMethod: {
            if (filter.op == Operator::Equals) {
                return tube.manufacturing_method == filter.value;
            } else if (filter.op == Operator::NotEquals) {
                return tube.manufacturing_method != filter.value;
            } else if (filter.op == Operator::Contains) {
                return tube.manufacturing_method.find(filter.value) != std::string::npos;
            }
            break;
        }

        case SearchField::CreatedDate: {
            // Support both single date and date range queries
            auto date_range_opt = parse_date_range(filter.value);
            if (date_range_opt.first && date_range_opt.second) {
                // Date range query (e.g., "2024-01-01/2024-12-31")
                auto tube_date_opt = parse_date(tube.created_date);
                if (!tube_date_opt) return false;
                auto tube_date = *tube_date_opt;
                return tube_date >= *date_range_opt.first &&
                       tube_date <= *date_range_opt.second;
            }

            auto date_opt = parse_date(filter.value);
            if (!date_opt) return false;

            auto tube_date_opt = parse_date(tube.created_date);
            if (!tube_date_opt) return false;

            auto date = *date_opt;
            auto tube_date = *tube_date_opt;

            switch (filter.op) {
                case Operator::Equals: return tube_date == date;
                case Operator::NotEquals: return tube_date != date;
                case Operator::LessThan: return tube_date < date;
                case Operator::GreaterThan: return tube_date > date;
                case Operator::LessThanOrEqual: return tube_date <= date;
                case Operator::GreaterThanOrEqual: return tube_date >= date;
                default: break;
            }
            break;
        }

        case SearchField::Author: {
            if (filter.op == Operator::Equals) {
                return tube.author == filter.value;
            } else if (filter.op == Operator::NotEquals) {
                return tube.author != filter.value;
            } else if (filter.op == Operator::Contains) {
                return tube.author.find(filter.value) != std::string::npos;
            }
            break;
        }

        case SearchField::FullText: {
            // Full-text search across all fields using fuzzy matching
            std::string lower_value = filter.value;
            std::transform(lower_value.begin(), lower_value.end(),
                          lower_value.begin(), ::tolower);

            std::string full_text = tube.designation + " " + tube.author + " " +
                                   material_to_string(tube.material) + " " +
                                   tube.manufacturing_method;
            std::string lower_full_text = full_text;
            std::transform(lower_full_text.begin(), lower_full_text.end(),
                          lower_full_text.begin(), ::tolower);

            if (filter.op == Operator::Contains) {
                return lower_full_text.find(lower_value) != std::string::npos;
            }
            break;
        }

        default:
            break;
    }

    return false;
}

bool SearchEngine::evaluate_query(const CompoundQuery& query,
                                  const TubeSearchRecord& tube) const {
    // Evaluate filters
    bool result = true;
    bool first = true;

    for (const auto& filter : query.filters) {
        bool filter_result = evaluate_filter(filter, tube);

        if (first) {
            result = filter_result;
            first = false;
        } else {
            if (query.logical_op == LogicalOperator::AND) {
                result = result && filter_result;
            } else {
                result = result || filter_result;
            }
        }
    }

    // Evaluate nested queries
    for (const auto& nested : query.nested_queries) {
        bool nested_result = evaluate_query(nested, tube);

        if (first) {
            result = nested_result;
            first = false;
        } else {
            if (query.logical_op == LogicalOperator::AND) {
                result = result && nested_result;
            } else {
                result = result || nested_result;
            }
        }
    }

    return result;
}

std::optional<double> SearchEngine::parse_number(const std::string& value) const {
    try {
        size_t idx = 0;
        double result = std::stod(value, &idx);

        // Check if entire string was parsed
        if (idx == value.length()) {
            return result;
        }

        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::chrono::system_clock::time_point>
SearchEngine::parse_date(const std::string& value) const {
    try {
        std::istringstream ss(value);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%d");

        if (ss.fail()) {
            return std::nullopt;
        }

        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        return tp;
    } catch (...) {
        return std::nullopt;
    }
}

Material SearchEngine::parse_material(const std::string& value) const {
    return string_to_material(value);
}

bool SearchEngine::fuzzy_match(const std::string& pattern,
                               const std::string& text,
                               int max_distance) const {
    int dist = index_.levenshtein_distance(pattern, text);
    return dist <= max_distance;
}

void SearchEngine::clear() {
    tubes_.clear();
    index_.clear();
}

// =========== SAVED SEARCHES MANAGER IMPLEMENTATION ===========

std::string SavedSearchesManager::create_saved_search(
    const std::string& name,
    const CompoundQuery& query,
    const std::string& author) {

    SavedSearch search;
    search.id = generate_uuid();
    search.name = name;
    search.query_json = query_to_json(query);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    search.created_at = ss.str();
    search.created_by = author;

    searches_[search.id] = search;
    return search.id;
}

std::vector<SavedSearch> SavedSearchesManager::get_all() const {
    std::vector<SavedSearch> result;
    for (const auto& [id, search] : searches_) {
        result.push_back(search);
    }
    return result;
}

std::optional<SavedSearch> SavedSearchesManager::get_by_id(
    const std::string& id) const {

    auto it = searches_.find(id);
    if (it != searches_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<SavedSearch> SavedSearchesManager::get_by_name(
    const std::string& name) const {

    for (const auto& [id, search] : searches_) {
        if (search.name == name) {
            return search;
        }
    }
    return std::nullopt;
}

bool SavedSearchesManager::delete_saved_search(const std::string& id) {
    return searches_.erase(id) > 0;
}

void SavedSearchesManager::mark_as_used(const std::string& id) {
    auto it = searches_.find(id);
    if (it != searches_.end()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        it->second.last_used_at = ss.str();
    }
}

int SavedSearchesManager::load_from_json(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);

        if (!j.is_array()) {
            return 0;
        }

        int count = 0;
        for (const auto& item : j) {
            SavedSearch search;
            search.id = item["id"];
            search.name = item["name"];
            search.query_json = item["query_json"];
            search.created_at = item["created_at"];
            search.created_by = item["created_by"];

            if (item.contains("last_used_at")) {
                search.last_used_at = item["last_used_at"];
            }

            searches_[search.id] = search;
            count++;
        }

        return count;
    } catch (...) {
        return 0;
    }
}

std::string SavedSearchesManager::export_to_json() const {
    json arr = json::array();

    for (const auto& [id, search] : searches_) {
        json obj;
        obj["id"] = search.id;
        obj["name"] = search.name;
        obj["query_json"] = search.query_json;
        obj["created_at"] = search.created_at;
        obj["created_by"] = search.created_by;

        if (!search.last_used_at.empty()) {
            obj["last_used_at"] = search.last_used_at;
        }

        arr.push_back(obj);
    }

    return arr.dump();
}

void SavedSearchesManager::clear() {
    searches_.clear();
}

// =========== SERIALIZATION FUNCTIONS ===========

std::string query_to_json(const CompoundQuery& query) {
    json j;

    j["logical_op"] = logical_op_to_string(query.logical_op);

    json filters_arr = json::array();
    for (const auto& filter : query.filters) {
        json f;
        f["field"] = field_to_string(filter.field);
        f["op"] = operator_to_string(filter.op);
        f["value"] = filter.value;
        filters_arr.push_back(f);
    }
    j["filters"] = filters_arr;

    // TODO: Implement nested queries serialization if needed
    j["nested_queries"] = json::array();

    return j.dump();
}

CompoundQuery json_to_query(const std::string& json_str) {
    CompoundQuery query;

    try {
        auto j = json::parse(json_str);

        if (j.contains("logical_op")) {
            query.logical_op = string_to_logical_op(j["logical_op"]);
        }

        if (j.contains("filters")) {
            for (const auto& f : j["filters"]) {
                QueryFilter filter;
                filter.field = string_to_field(f["field"]);
                filter.op = string_to_operator(f["op"]);
                filter.value = f["value"];
                query.add_filter(filter);
            }
        }

        // TODO: Handle nested queries deserialization if needed
    } catch (...) {
        // Return empty query on error
    }

    return query;
}

// =========== BM25 AND FULL-TEXT SEARCH ===========

std::string SearchIndex::normalize_token(const std::string& token) const {
    std::string normalized = token;
    std::transform(normalized.begin(), normalized.end(),
                  normalized.begin(), ::tolower);
    return normalized;
}

double SearchIndex::calculate_bm25_score(
    const std::string& doc_id,
    const std::vector<std::string>& query_tokens,
    double k1, double b) const {

    auto it = tube_records_.find(doc_id);
    if (it == tube_records_.end()) return 0.0;

    double score = 0.0;
    size_t n_docs = tube_records_.size();

    // Build full text from tube record
    const auto& tube = it->second;
    std::string full_text = tube.designation + " " + tube.author + " " +
                           material_to_string(tube.material) + " " +
                           tube.manufacturing_method;

    auto doc_tokens = tokenize(full_text);
    double doc_length = static_cast<double>(doc_tokens.size());

    for (const auto& token : query_tokens) {
        std::string norm_token = normalize_token(token);

        // Count occurrences of token in document
        int term_freq = 0;
        for (const auto& doc_token : doc_tokens) {
            if (normalize_token(doc_token) == norm_token) {
                term_freq++;
            }
        }

        if (term_freq > 0) {
            // BM25 formula
            size_t df = document_frequency_.count(norm_token) ?
                       document_frequency_.at(norm_token) : 1;

            double idf = std::log(1.0 + (static_cast<double>(n_docs) - df + 0.5) /
                                      (df + 0.5));

            double norm_tf = (term_freq * (k1 + 1.0)) /
                           (term_freq + k1 * (1.0 - b + b * (doc_length / avg_doc_length_)));

            score += idf * norm_tf;
        }
    }

    return score;
}

std::vector<std::pair<std::string, double>> SearchIndex::full_text_search(
    const std::string& query,
    size_t top_k) const {

    auto query_tokens = tokenize(query);
    if (query_tokens.empty()) return {};

    std::vector<std::pair<std::string, double>> results;

    // Calculate BM25 score for each document
    for (const auto& [doc_id, _] : tube_records_) {
        double score = calculate_bm25_score(doc_id, query_tokens);
        if (score > 0.0) {
            results.emplace_back(doc_id, score);
        }
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

    // Return top k results
    if (results.size() > top_k) {
        results.resize(top_k);
    }

    return results;
}

// =========== HELPER METHODS ===========

std::pair<std::optional<double>, std::optional<double>>
SearchEngine::parse_range(const std::string& value) const {
    size_t dash_pos = value.find('-');
    if (dash_pos == std::string::npos) {
        return {std::nullopt, std::nullopt};
    }

    try {
        std::string min_str = value.substr(0, dash_pos);
        std::string max_str = value.substr(dash_pos + 1);

        double min_val = std::stod(min_str);
        double max_val = std::stod(max_str);

        return {min_val, max_val};
    } catch (...) {
        return {std::nullopt, std::nullopt};
    }
}

std::pair<std::optional<std::chrono::system_clock::time_point>,
          std::optional<std::chrono::system_clock::time_point>>
SearchEngine::parse_date_range(const std::string& value) const {
    size_t slash_pos = value.find('/');
    if (slash_pos == std::string::npos) {
        return {std::nullopt, std::nullopt};
    }

    std::string start_str = value.substr(0, slash_pos);
    std::string end_str = value.substr(slash_pos + 1);

    auto start_opt = parse_date(start_str);
    auto end_opt = parse_date(end_str);

    return {start_opt, end_opt};
}

}  // namespace bendcore::search
