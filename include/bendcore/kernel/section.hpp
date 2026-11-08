#pragma once

// Planar section of a model for the "section view" of the viewport: the body clipped by a
// half-space, the section contour (BRepAlgoAPI_Section) as edges, closed wires and planar
// faces, and the section area (docs/kernel/ANALYSIS.md).

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <vector>

namespace bendcore::kernel {

struct SectionPlane {
    Point3 origin{};
    Point3 normal{0.0, 0.0, 1.0};  // need not be unit; must not be zero
};

// Removes the material on the side the normal points to: keeps {p : (p - origin) . n <= 0}.
// BRepAlgoAPI_Cut with a box that covers the shape's bounding box on the positive side.
// Returns an empty compound when the whole shape is on the removed side.
Result<Shape> clipByPlane(const Shape& shape, const SectionPlane& plane);

struct SectionResult {
    Shape edges;   // compound of the section edges (BRepAlgoAPI_Section)
    Shape wires;   // compound of closed and open wires built from the edges
    Shape faces;   // compound of planar faces bounded by the closed wires, holes included
    double area = 0.0;  // mm^2, area of `faces`
    std::vector<std::vector<Point3>> polylines;  // edges discretised for display, mm
};

// Section of the solids of `shape` by the plane. A plane that misses the shape gives an empty
// result with area 0. Sections of shells or faces give edges and wires but no faces.
// `deflection` controls the display polylines (GCPnts_QuasiUniformDeflection), mm.
Result<SectionResult> sectionByPlane(const Shape& shape, const SectionPlane& plane, double deflection = 0.01);

}  // namespace bendcore::kernel
