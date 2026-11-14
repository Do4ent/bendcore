#pragma once

// Sketch editing tools over BoundSketch (docs/modeling/SKETCH.md, section "Инструменты"):
// trim and extend to intersections, offset of a chain, mirror, linear and circular patterns,
// fillet and chamfer of a corner. Every tool changes the geometry of the sketch and adds the
// constraints that keep the result consistent when the sketch is solved again (point-on-object
// at cut points, parallel/concentric + distance for offsets, symmetry for mirrors, translation
// and rotation for patterns, tangency + radius for fillets). The sketch is not solved by the
// tools; points are placed at their exact positions so the sketch stays solved.

#include <bendcore/modeling/sketch_profile.hpp>

#include <vector>

namespace bendcore::modeling {

struct SketchEditResult {
    std::vector<SketchEntityId> created;      // new entities, in creation order
    std::vector<SketchEntityId> modified;     // entities whose end points changed
    std::vector<SketchEntityId> removed;      // entities removed
    std::vector<sketch2d::Id> points;         // new sketch points
    std::vector<sketch2d::Id> constraints;    // new constraints
};

// Removes the piece of a line, arc or circle that contains `pick`, bounded by the nearest
// intersections with the other live entities (construction ones included) or by the curve
// ends. A piece between two intersections splits the entity in two; a piece without
// intersections removes the entity. The new end points get point-on constraints with the
// cutting entities. A circle needs at least two intersections.
Result<SketchEditResult> trimEntity(BoundSketch& sketch, SketchEntityId entity, const Point2& pick);

// Extends the end of a line or arc nearest to `pick` up to the nearest intersection with another
// live entity; the new end point gets a point-on constraint with that entity.
Result<SketchEditResult> extendEntity(BoundSketch& sketch, SketchEntityId entity, const Point2& pick);

// Offsets a connected chain of lines and arcs (in chain order; closed when the last entity meets
// the first) by `distance` to the left of the chain direction (negative: to the right). The
// direction is that of the first entity towards the second. Corners are joined at the
// intersection of the offset curves (tangent joints share the end point). With `constrain`:
// lines get parallel + point-curve distance, arcs share the centre (concentric) and get the
// point-curve distance; joints share points.
Result<SketchEditResult> offsetChain(BoundSketch& sketch, const std::vector<SketchEntityId>& chain,
                                     double distance, bool constrain = true);

// Mirror copies about a line entity. Points on the axis are reused; the others get mirrored
// copies with symmetric constraints (and equal radius for circles).
Result<SketchEditResult> mirrorEntities(BoundSketch& sketch, const std::vector<SketchEntityId>& entities,
                                        SketchEntityId axis, bool constrain = true);

// count - 1 copies translated by k * step (k = 1 .. count - 1); copied points get DistanceX and
// DistanceY constraints from their originals (and equal radius for circles).
Result<SketchEditResult> linearPattern(BoundSketch& sketch, const std::vector<SketchEntityId>& entities,
                                       const Point2& step, int count, bool constrain = true);

// count - 1 copies rotated about point `center` by k * stepDegrees; copied points get rotation
// constraints from their originals (and equal radius for circles). The centre point is reused.
Result<SketchEditResult> circularPattern(BoundSketch& sketch, const std::vector<SketchEntityId>& entities,
                                         sketch2d::Id center, int count, double stepDegrees,
                                         bool constrain = true);

// Fillet of the corner where lines `a` and `b` meet (shared end point, or two coincident end
// points): tangent arc of `radius`, the lines are shortened to the tangent points. The corner
// point stays as a "virtual sharp" constrained onto both lines, so dimensions to it survive.
Result<SketchEditResult> filletCorner(BoundSketch& sketch, SketchEntityId a, SketchEntityId b, double radius);

// Chamfer of the same kind of corner: distances d1 along `a` and d2 along `b` from the corner.
Result<SketchEditResult> chamferCorner(BoundSketch& sketch, SketchEntityId a, SketchEntityId b, double d1, double d2);

}  // namespace bendcore::modeling
