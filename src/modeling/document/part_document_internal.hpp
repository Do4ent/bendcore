#pragma once

// Internal state of PartDocument shared by part_document.cpp and serialization.cpp.

#include <bendcore/modeling/part_document.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace bendcore::modeling {

struct CachedResult {
    std::uint64_t hash = 0;
    FeatureState state = FeatureState::Ok;
    std::vector<std::string> messages;
    std::map<std::string, double> values;
    std::vector<ReferenceReport> reports;
    std::shared_ptr<const NamedShape> output;
};

struct PartDocument::Node {
    std::string id;
    std::string type;
    std::string label;
    std::unique_ptr<Feature> feature;
    std::map<std::string, std::string> parameters;
    std::map<std::string, std::vector<TopoReference>> references;
    std::map<std::string, std::string> options;
    bool suppressed = false;

    // Results of the last rebuild.
    FeatureState state = FeatureState::RolledBack;
    std::vector<std::string> messages;
    std::map<std::string, double> values;
    std::vector<ReferenceReport> reports;
    std::shared_ptr<const NamedShape> output;
    std::optional<std::uint64_t> hash;  // input hash of the stored result

    std::optional<CachedResult> cache;  // from a loaded file, consumed by the first rebuild
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

}  // namespace bendcore::modeling
