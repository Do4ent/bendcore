#pragma once

// Топологическое именование (фаза 1, п. 4 docs/ROADMAP_GENERAL_CAD.md).
//
// Каждой грани, ребру и вершине результата операции присваивается имя, выведенное из
// истории OCCT (BRepTools_History: Generated / Modified / IsDeleted) и из имён соседних
// элементов, плюс геометрическая сигнатура для разрешения неоднозначностей. Ссылка на
// элемент хранит имя и сигнатуру; после перестроения она разрешается по цепочке
// «точное имя → история → сигнатура → сломана (с причиной)».
// Алгоритм и сравнение с FreeCAD 1.0 и OCAF TNaming: docs/modeling/TOPOLOGICAL_NAMING.md.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <BRepTools_History.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Shape.hxx>

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bendcore::modeling {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

enum class ElementKind { Face = 0, Edge = 1, Vertex = 2 };

const char* toString(ElementKind kind) noexcept;
std::optional<ElementKind> elementKindFromString(std::string_view text) noexcept;
// Kind of a TopoDS sub-shape (FACE, EDGE, VERTEX); nullopt for other shape types.
std::optional<ElementKind> elementKindOf(const TopoDS_Shape& shape) noexcept;

// Geometry of one element, used to recognise it when its name no longer matches.
struct GeometricSignature {
    bool valid = false;  // false: no geometry recorded (e.g. reference migrated from format v1)
    ElementKind kind = ElementKind::Face;
    // Plane, Cylinder, Cone, Sphere, Torus, BSpline, Other (faces); Line, Circle, Ellipse,
    // BSpline, Other, Degenerated (edges); Point (vertices).
    std::string geometry;
    kernel::Point3 center{};      // face: area centroid; edge: length centroid; vertex: point
    bool hasDirection = false;
    // Plane: outward normal (face orientation applied). Cylinder, cone, torus, circle,
    // ellipse: axis; line: direction. Axes and lines use a canonical sign (first non-zero
    // component positive), since their sense carries no meaning.
    kernel::Point3 direction{};
    double radius = 0.0;          // cylinder, sphere, circle; cone reference radius; torus/ellipse major
    double size = 0.0;            // face area, edge length; 0 for vertices
};

Result<GeometricSignature> computeSignature(const TopoDS_Shape& element);

// Dissimilarity of two signatures, >= 0; +infinity when kinds, geometry types or directions
// are incompatible. `scale` (model size, mm) normalises the centre distance.
double signatureDistance(const GeometricSignature& a, const GeometricSignature& b, double scale);

// How the element got its name in the feature that produced the shape.
enum class NameOrigin {
    Role,       // named by the feature itself (primitive face, tool profile side)
    Unchanged,  // same sub-shape as an input element
    Modified,   // the single image of a modified input element
    Split,      // one of several images of a modified input element (suffix ;S<i>)
    Generated,  // generated from a lower- or same-dimension input element: G[<feature>](<source>)
    Derived,    // from the names of adjacent faces: E(...), V(...), or bounding edges: F(...)
};

const char* toString(NameOrigin origin) noexcept;
std::optional<NameOrigin> nameOriginFromString(std::string_view text) noexcept;

struct ElementRecord {
    std::string name;
    NameOrigin origin = NameOrigin::Derived;
    std::string feature;               // feature whose result first carried this name
    std::vector<std::string> aliases;  // other names that reached this element through history
    GeometricSignature signature;
};

// Input element that disappeared without a same-kind image.
struct DeletionRecord {
    std::string feature;                  // feature that removed it
    std::vector<std::string> successors;  // names of elements generated from it (any kind)
};

struct NamingRequest;
class NamedShape;
Result<NamedShape> buildNamedShape(const NamingRequest& request);

// A shape together with its element map. Element indices follow TopExp::MapShapes order,
// i.e. they coincide with kernel::TopologyIndex indices of the same shape.
class NamedShape {
public:
    NamedShape() = default;

    // Rebuilds a NamedShape from stored records (BRep cache). Fails when the record count
    // of a kind differs from the number of sub-shapes of that kind.
    static Result<NamedShape> fromRecords(TopoDS_Shape shape,
                                          std::array<std::vector<ElementRecord>, 3> records,
                                          std::map<std::string, DeletionRecord> deletions);

    bool isNull() const noexcept { return shape_.IsNull(); }
    const TopoDS_Shape& shape() const noexcept { return shape_; }

    std::size_t count(ElementKind kind) const noexcept;
    const TopoDS_Shape& element(ElementKind kind, std::size_t index) const;
    const ElementRecord& record(ElementKind kind, std::size_t index) const;
    const std::vector<ElementRecord>& records(ElementKind kind) const;

    // Index of a sub-shape (orientation ignored, location respected).
    std::optional<std::size_t> indexOf(ElementKind kind, const TopoDS_Shape& element) const;
    // Index of the element carrying `name` as its primary name.
    std::optional<std::size_t> find(ElementKind kind, std::string_view name) const;

    const std::map<std::string, DeletionRecord>& deletions() const noexcept { return deletions_; }

    // Bounding-box diagonal (mm), normalisation for signatureDistance; 1 for empty shapes.
    double scale() const noexcept { return scale_; }

private:
    friend Result<NamedShape> buildNamedShape(const NamingRequest& request);
    void indexShape();
    void rebuildNameIndex();

    TopoDS_Shape shape_;
    std::array<kernel::ShapeIndexMap, 3> maps_;
    std::array<std::vector<ElementRecord>, 3> records_;
    std::array<std::unordered_map<std::string, std::size_t>, 3> byName_;
    std::map<std::string, DeletionRecord> deletions_;
    double scale_ = 1.0;
};

// Element named by the feature itself: "<feature>:<role>".
struct ElementRole {
    TopoDS_Shape element;
    std::string role;
};

struct NamingRequest {
    std::string featureId;
    TopoDS_Shape result;
    // History from the elements of `inputs` and `roles` to the elements of `result`;
    // may be null when the result contains no input element (primitive without input).
    opencascade::handle<BRepTools_History> history;
    std::vector<const NamedShape*> inputs;
    std::vector<ElementRole> roles;
};

// Builds the element map of `result`. Errors: InvalidInput (null result, empty feature id).
Result<NamedShape> buildNamedShape(const NamingRequest& request);

// Name of an element as stored by a feature.
struct TopoReference {
    std::string name;
    ElementKind kind = ElementKind::Edge;
    GeometricSignature signature;
};

// Reference to `element` of `shape` with its current name and signature.
// Errors: InvalidInput (not a face/edge/vertex of the shape).
Result<TopoReference> makeReference(const NamedShape& shape, const TopoDS_Shape& element);

enum class ResolveMethod { Exact, History, Signature, Broken };

const char* toString(ResolveMethod method) noexcept;
std::optional<ResolveMethod> resolveMethodFromString(std::string_view text) noexcept;

struct Resolution {
    ResolveMethod method = ResolveMethod::Broken;
    std::vector<std::size_t> indices;  // element indices of `kind` in the shape
    std::vector<std::string> names;    // their current names
    std::string detail;                // how it was resolved, or why it is broken
};

struct ResolveOptions {
    // Largest signatureDistance accepted by the signature fallback.
    double maxSignatureDistance = 0.35;
    // The best signature candidate must be closer than ratio * second best.
    double ambiguityRatio = 0.6;
};

Resolution resolveReference(const TopoReference& reference, const NamedShape& shape,
                            const ResolveOptions& options = {});

}  // namespace bendcore::modeling
