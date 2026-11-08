#pragma once

// Feature types of the part document (docs/modeling/PART_DOCUMENT.md, section "Операции").
// Each type wraps an operation of src/modeling/features/ (features.hpp) or of the sketch and
// reference geometry modules (sketch_profile.hpp, reference_geometry.hpp):
//   Sketch, DatumPlane, DatumAxis                       — nodes without a body
//   Extrude, Revolve, Hole, Fillet, Chamfer, LinearPattern, CircularPattern, Mirror,
//   Shell, Draft                                        — body operations
//   Box, Cylinder                                       — primitives (rectangle / circle
//                                                         profile extruded by extrude())

#include <bendcore/modeling/part_document.hpp>

#include <memory>

namespace bendcore::modeling {

// Registers every type listed above. Fails if a type is already registered.
Status registerPartFeatures(FeatureRegistry& registry);

// A registry with the part features.
std::shared_ptr<const FeatureRegistry> partRegistry();

}  // namespace bendcore::modeling
