#pragma once

// Shared helpers of the part document features (sketch_features.cpp, part_features.cpp).

#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <gp_Ax1.hxx>
#include <gp_Pln.hxx>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::modeling::features {

namespace detail {

inline Result<FeatureOutput> fail(ErrorCode code, std::string message) {
    return Result<FeatureOutput>::failure(code, std::move(message));
}

inline std::string num(double v) {
    std::ostringstream os;
    os << v;
    return os.str();
}

}  // namespace detail

// Plane chosen by an option value: "XY" (normal +Z, X = +X), "XZ" (normal -Y, X = +X),
// "YZ" (normal +X, X = +Y), "face" (planar face of reference slot faceSlot, outward
// normal), "datum" (datum plane of link datumLink).
Result<gp_Pln> planeFromOption(const FeatureContext& ctx, const std::string& choice, const char* faceSlot,
                               const char* datumLink);
// Axis chosen by an option value: "X", "Y", "Z" (through the origin), "face" (axis of a
// cylindrical/conical face), "edge" (straight edge), "datum" (datum axis of link datumLink).
Result<gp_Ax1> axisFromOption(const FeatureContext& ctx, const std::string& choice, const char* faceSlot,
                              const char* edgeSlot, const char* datumLink);

void addSketchFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all);
void addBodyFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all);
// Sweep, Loft, Rib, ThreadedHole, BodyBoolean, DeleteBody, BodyLinearPattern,
// BodyCircularPattern, BodyMirror (advanced_features.cpp).
void addAdvancedFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all);

}  // namespace bendcore::modeling::features
