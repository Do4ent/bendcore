#pragma once

// Compatibility header for code written against the first part document registry
// (Box, Cylinder, Fillet, Chamfer): these types are now part of the full part feature set
// (part_features.hpp) with the same parameters and element role names. The parametric prism
// cut of that registry is replaced by Sketch + Extrude; files with it are migrated on load
// (docs/modeling/PART_DOCUMENT.md, "Миграция v2 → v3").

#include <bendcore/modeling/part_features.hpp>

#include <memory>

namespace bendcore::modeling::basic {

// The part feature registry (partRegistry()).
inline std::shared_ptr<const FeatureRegistry> basicRegistry() { return partRegistry(); }

}  // namespace bendcore::modeling::basic
