#pragma once

// Internal state of PartDocument shared by part_document.cpp, transactions.cpp and
// serialization.cpp.

#include <bendcore/modeling/part_document.hpp>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace bendcore::modeling {

// Everything a built node keeps; also the entries of the result pool and of the file cache.
struct CachedResult {
    std::uint64_t hash = 0;
    FeatureState state = FeatureState::Ok;
    std::vector<std::string> messages;
    std::map<std::string, double> values;
    std::vector<ReferenceReport> reports;
    std::shared_ptr<const NamedShape> output;
    TopoDS_Shape tool;
    std::optional<FeatureMode> toolMode;
    std::shared_ptr<const NamedShape> toolNamed;
    std::shared_ptr<const SketchResult> sketch;
    std::shared_ptr<const DatumResult> datum;
};

// Definition of a node: what the user edits, what undo restores and what the file stores.
struct NodeDefinition {
    std::string id;
    std::string type;
    std::string label;
    std::map<std::string, std::string> parameters;
    std::map<std::string, std::vector<TopoReference>> references;
    std::map<std::string, std::string> options;
    std::map<std::string, std::string> links;
    std::optional<SketchData> sketch;
    bool suppressed = false;
};

struct PartDocument::Node : NodeDefinition {
    std::unique_ptr<Feature> feature;

    // Results of the last rebuild.
    FeatureState state = FeatureState::RolledBack;
    std::vector<std::string> messages;
    std::map<std::string, double> values;
    std::vector<ReferenceReport> reports;
    std::shared_ptr<const NamedShape> output;
    TopoDS_Shape tool;
    std::optional<FeatureMode> toolMode;
    std::shared_ptr<const NamedShape> toolNamed;
    std::shared_ptr<const SketchResult> sketchResult;
    std::shared_ptr<const DatumResult> datum;
    std::optional<std::uint64_t> hash;  // input hash of the stored result

    std::optional<CachedResult> cache;  // from a loaded file, consumed by the first rebuild
    // Recent results of this node by input hash (most recent last): undo/redo and edits that
    // return to an earlier value reuse them instead of recomputing.
    std::deque<CachedResult> pool;
    static constexpr std::size_t kPoolSize = 6;

    CachedResult snapshot() const;
    void apply(const CachedResult& r);
    void clearResults();
};

struct PartDocument::State {
    std::vector<std::pair<std::string, std::string>> variables;
    std::optional<std::size_t> rollback;
    std::vector<NodeDefinition> nodes;
};

struct PartDocument::Transactions {
    struct Entry {
        std::string name;
        std::size_t edits = 0;
        State state;  // undo stack: state before; redo stack: state after
    };
    std::vector<Entry> undo;
    std::vector<Entry> redo;
    std::optional<Entry> open;  // state at beginTransaction
    static constexpr std::size_t kMaxEntries = 200;
};

// FNV-1a accumulator for input hashes.
class Hasher {
public:
    void add(std::string_view text) {
        for (unsigned char c : text) {
            h_ ^= c;
            h_ *= 1099511628211ULL;
        }
        h_ ^= 0xff;  // separator, so that ("ab","c") != ("a","bc")
        h_ *= 1099511628211ULL;
    }
    void add(double v);
    void add(std::uint64_t v);
    std::uint64_t value() const noexcept { return h_; }

private:
    std::uint64_t h_ = 1469598103934665603ULL;
};

std::optional<FeatureState> featureStateFromString(std::string_view text) noexcept;

// Tool of a feature with element names "<feature>:Tool.F<k>" (faces in a canonical geometric
// order: geometry type, direction, radius, centre), edges and vertices derived from the faces.
Result<NamedShape> nameTool(const std::string& featureId, const TopoDS_Shape& tool);

}  // namespace bendcore::modeling
