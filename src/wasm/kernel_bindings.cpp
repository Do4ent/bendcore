// WebAssembly module of the CAD kernel (bendcore_kernel_wasm, docs/ui/KERNEL_VIEWPORT.md).
//
// Shapes live in a registry inside the module and are addressed from JS by an integer handle;
// JS releases them explicitly with disposeShape(handle). Every entry point returns a plain JS
// object: {ok: true, value} on success or {ok: false, code, message} with the kernel ErrorCode,
// so no C++ exception crosses into JS. The ES wrapper (bendcore_kernel_api.mjs) turns failures
// into KernelError exceptions.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/kernel/tessellation.hpp>

#include "exchange_common.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <BRepAdaptor_Surface.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>

#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <utility>

using emscripten::val;
namespace k = bendcore::kernel;

namespace {

struct Entry {
    k::Shape shape;
    std::optional<k::TopologyIndex> index;  // built on first use
    // Structure of an imported STEP / IGES file (names, colours, occurrences): export writes it
    // instead of the bare shape. Empty for readStep() shapes.
    std::optional<k::AssemblyDocument> source;
};

std::map<std::uint32_t, Entry>& registry() {
    static std::map<std::uint32_t, Entry> shapes;
    return shapes;
}

std::uint32_t& nextHandle() {
    static std::uint32_t next = 1;
    return next;
}

val success(val value) {
    val out = val::object();
    out.set("ok", true);
    out.set("value", std::move(value));
    return out;
}

val failure(const std::string& code, const std::string& message) {
    val out = val::object();
    out.set("ok", false);
    out.set("code", code);
    out.set("message", message);
    return out;
}

val failure(const k::Error& error) {
    return failure(k::toString(error.code), error.message);
}

val point(const k::Point3& p) {
    val out = val::array();
    out.call<void>("push", p[0]);
    out.call<void>("push", p[1]);
    out.call<void>("push", p[2]);
    return out;
}

val point(const gp_XYZ& p) {
    return point(k::Point3{p.X(), p.Y(), p.Z()});
}

template <typename T>
val typedArray(const char* constructor, const std::vector<T>& data) {
    // The view aliases wasm memory; constructing a new typed array from it copies the data,
    // so the result stays valid after memory growth or after the vector is freed.
    return val::global(constructor).new_(emscripten::typed_memory_view(data.size(), data.data()));
}

// Runs body(entry) for a live handle; OCCT and std exceptions become OperationFailed.
template <typename Body>
val withShape(std::uint32_t handle, Body&& body) {
    auto it = registry().find(handle);
    if (it == registry().end()) {
        return failure("InvalidInput", "unknown shape handle " + std::to_string(handle) +
                                           " (never created or already disposed)");
    }
    try {
        return body(it->second);
    } catch (const Standard_Failure& e) {
        return failure("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failure("OperationFailed", e.what());
    }
}

const k::TopologyIndex* topology(Entry& entry, val& error) {
    if (!entry.index) {
        auto built = k::TopologyIndex::build(entry.shape);
        if (!built) {
            error = failure(built.error());
            return nullptr;
        }
        entry.index = std::move(built).value();
    }
    return &*entry.index;
}

val kernelInfo() {
    val out = val::object();
    out.set("occtVersion", std::string(OCC_VERSION_COMPLETE));
    out.set("liveShapes", static_cast<double>(registry().size()));
    return out;
}

val readStep(val bytes) {
    if (!bytes.instanceof(val::global("Uint8Array"))) {
        return failure("InvalidInput", "readStep expects a Uint8Array");
    }
    const auto length = bytes["length"].as<std::size_t>();
    std::string data(length, '\0');
    val(emscripten::typed_memory_view(length, data.data())).call<void>("set", bytes);
    try {
        auto shape = k::readStep(data);
        if (!shape) return failure(shape.error());
        const std::uint32_t handle = nextHandle()++;
        registry().emplace(handle, Entry{std::move(shape).value(), std::nullopt, std::nullopt});
        return success(val(handle));
    } catch (const Standard_Failure& e) {
        return failure("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failure("OperationFailed", e.what());
    }
}

val disposeShape(std::uint32_t handle) {
    if (registry().erase(handle) == 0) {
        return failure("InvalidInput", "unknown shape handle " + std::to_string(handle) +
                                           " (never created or already disposed)");
    }
    return success(val::undefined());
}

val counts(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        const k::TopologyCounts c = e.shape.counts();
        val out = val::object();
        out.set("solids", static_cast<double>(c.solids));
        out.set("shells", static_cast<double>(c.shells));
        out.set("faces", static_cast<double>(c.faces));
        out.set("edges", static_cast<double>(c.edges));
        out.set("vertices", static_cast<double>(c.vertices));
        return success(out);
    });
}

val volumeProperties(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        auto v = e.shape.volumeProperties();
        if (!v) return failure(v.error());
        val out = val::object();
        out.set("volume", v.value().volume);
        out.set("centerOfMass", point(v.value().centerOfMass));
        return success(out);
    });
}

val surfaceProperties(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        auto s = e.shape.surfaceProperties();
        if (!s) return failure(s.error());
        val out = val::object();
        out.set("area", s.value().area);
        out.set("centroid", point(s.value().centroid));
        return success(out);
    });
}

val boundingBox(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        auto b = e.shape.boundingBox();
        if (!b) return failure(b.error());
        val out = val::object();
        out.set("min", point(b.value().min));
        out.set("max", point(b.value().max));
        return success(out);
    });
}

val checkValidity(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        const k::ValidityReport report = e.shape.checkValidity();
        val issues = val::array();
        for (const std::string& issue : report.issues) issues.call<void>("push", issue);
        val out = val::object();
        out.set("valid", report.valid);
        out.set("issues", issues);
        return success(out);
    });
}

val faceTypes(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        val error = val::undefined();
        const k::TopologyIndex* index = topology(e, error);
        if (!index) return error;
        val out = val::array();
        for (const k::FaceInfo& f : index->faces()) out.call<void>("push", std::string(k::toString(f.surface)));
        return success(out);
    });
}

val edgeTypes(std::uint32_t handle) {
    return withShape(handle, [](Entry& e) {
        val error = val::undefined();
        const k::TopologyIndex* index = topology(e, error);
        if (!index) return error;
        val out = val::array();
        for (const k::EdgeInfo& info : index->edges()) out.call<void>("push", std::string(k::toString(info.curve)));
        return success(out);
    });
}

// Analytic parameters of the face's surface in global coordinates (BRepAdaptor_Surface applies
// the face location). The plane normal points out of the material (flipped for REVERSED faces).
val surfaceParameters(const k::FaceInfo& info) {
    const BRepAdaptor_Surface surface(info.face);
    const bool reversed = info.face.Orientation() == TopAbs_REVERSED;
    val out = val::object();
    auto axis = [&](const gp_Ax1& a) {
        out.set("axisOrigin", point(a.Location().XYZ()));
        out.set("axisDirection", point(a.Direction().XYZ()));
    };
    switch (info.surface) {
        case k::SurfaceType::Plane: {
            const gp_Pln plane = surface.Plane();
            gp_XYZ normal = plane.Axis().Direction().XYZ();
            if (reversed) normal.Reverse();
            out.set("origin", point(plane.Location().XYZ()));
            out.set("normal", point(normal));
            break;
        }
        case k::SurfaceType::Cylinder: {
            const gp_Cylinder c = surface.Cylinder();
            out.set("radius", c.Radius());
            axis(c.Axis());
            break;
        }
        case k::SurfaceType::Cone: {
            const gp_Cone c = surface.Cone();
            out.set("referenceRadius", c.RefRadius());
            out.set("semiAngle", c.SemiAngle());
            out.set("apex", point(c.Apex().XYZ()));
            axis(c.Axis());
            break;
        }
        case k::SurfaceType::Sphere: {
            const gp_Sphere s = surface.Sphere();
            out.set("radius", s.Radius());
            out.set("center", point(s.Location().XYZ()));
            break;
        }
        case k::SurfaceType::Torus: {
            const gp_Torus t = surface.Torus();
            out.set("majorRadius", t.MajorRadius());
            out.set("minorRadius", t.MinorRadius());
            axis(t.Axis());
            break;
        }
        case k::SurfaceType::BSpline: {
            out.set("uDegree", surface.UDegree());
            out.set("vDegree", surface.VDegree());
            out.set("uPoles", surface.NbUPoles());
            out.set("vPoles", surface.NbVPoles());
            break;
        }
        case k::SurfaceType::Other:
            break;
    }
    return out;
}

val faceInfo(std::uint32_t handle, std::uint32_t faceIndex) {
    return withShape(handle, [faceIndex](Entry& e) {
        val error = val::undefined();
        const k::TopologyIndex* index = topology(e, error);
        if (!index) return error;
        if (faceIndex >= index->faces().size()) {
            return failure("InvalidInput", "face index " + std::to_string(faceIndex) + " is out of range (shape has " +
                                               std::to_string(index->faces().size()) + " faces)");
        }
        const k::FaceInfo& info = index->faces()[faceIndex];
        auto props = k::Shape(info.face).surfaceProperties();
        if (!props) return failure(props.error());
        val out = val::object();
        out.set("index", faceIndex);
        out.set("surfaceType", std::string(k::toString(info.surface)));
        out.set("orientation", std::string(info.face.Orientation() == TopAbs_REVERSED ? "Reversed" : "Forward"));
        out.set("area", props.value().area);
        out.set("centroid", point(props.value().centroid));
        out.set("parameters", surfaceParameters(info));
        return success(out);
    });
}

double numberField(const val& params, const char* name, double fallback) {
    const val v = params[name];
    return v.isNumber() ? v.as<double>() : fallback;
}

bool boolField(const val& params, const char* name, bool fallback) {
    const val v = params[name];
    return v.isTrue() ? true : (v.isFalse() ? false : fallback);
}

val tessellate(std::uint32_t handle, val params) {
    return withShape(handle, [&params](Entry& e) {
        k::TessellationParams p;
        if (!params.isUndefined() && !params.isNull()) {
            p.linearDeflection = numberField(params, "linearDeflection", p.linearDeflection);
            p.angularDeflection = numberField(params, "angularDeflection", p.angularDeflection);
            p.relative = boolField(params, "relative", p.relative);
            p.requireAllFaces = boolField(params, "requireAllFaces", p.requireAllFaces);
        }
        // Single-threaded build (no -pthread): BRepMesh parallel mode is not available.
        p.inParallel = false;
        auto mesh = k::tessellate(e.shape, p);
        if (!mesh) return failure(mesh.error());
        const k::Mesh& m = mesh.value();

        std::vector<std::uint32_t> spans;
        spans.reserve(m.faceSpans.size() * 3);
        for (const k::FaceSpan& s : m.faceSpans) {
            spans.push_back(s.faceIndex);
            spans.push_back(s.firstIndex);
            spans.push_back(s.indexCount);
        }
        val edges = val::array();
        for (const k::EdgePolyline& poly : m.edgePolylines) {
            val edge = val::object();
            edge.set("edgeIndex", poly.edgeIndex);
            edge.set("points", typedArray("Float32Array", poly.points));
            edges.call<void>("push", edge);
        }
        val out = val::object();
        out.set("positions", typedArray("Float32Array", m.positions));
        out.set("normals", typedArray("Float32Array", m.normals));
        out.set("indices", typedArray("Uint32Array", m.indices));
        out.set("faceSpans", typedArray("Uint32Array", spans));
        out.set("edges", edges);
        out.set("missingFaces", typedArray("Uint32Array", m.missingFaces));
        out.set("missingEdges", typedArray("Uint32Array", m.missingEdges));
        return success(out);
    });
}

namespace ex = bendcore::wasm_exchange;

// STEP or IGES with its XCAF structure -> {handle, structure}. The shape of the handle is the
// single placed part, or a compound of all placed parts in world coordinates.
val importModel(val bytes, std::string format) {
    try {
        const std::string data = ex::bytesFromJs(bytes, "importModel bytes");
        auto doc = ex::readDocument(format, data);
        if (!doc) return failure(doc.error());
        auto placed = k::flattenAssembly(doc.value());
        if (!placed) return failure(placed.error());
        if (placed.value().empty()) return failure("OperationFailed", "the file has no part with a shape");
        k::Shape shape = placed.value().front().shape;
        if (placed.value().size() > 1) {
            BRep_Builder builder;
            TopoDS_Compound all;
            builder.MakeCompound(all);
            for (const k::PlacedPart& p : placed.value()) builder.Add(all, p.shape.occt());
            shape = k::Shape(all);
        }
        val out = val::object();
        out.set("structure", ex::structureToJs(doc.value(), placed.value()));
        const std::uint32_t handle = nextHandle()++;
        registry().emplace(handle, Entry{std::move(shape), std::nullopt, std::move(doc).value()});
        out.set("handle", handle);
        return success(out);
    } catch (const ex::OptionError& e) {
        return failure("InvalidInput", e.what());
    } catch (const Standard_Failure& e) {
        return failure("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failure("OperationFailed", e.what());
    }
}

// {format: step|iges|stl|3mf, options} -> Uint8Array of the file.
val exportShape(std::uint32_t handle, std::string format, val options) {
    return withShape(handle, [&](Entry& e) {
        try {
            const std::string name = ex::stringOption(options, "name", "Part");
            const k::AssemblyDocument doc = e.source ? *e.source : ex::partDocument(e.shape, name, std::nullopt);
            auto bytes = ex::exportDocument(doc, format, options);
            if (!bytes) return failure(bytes.error());
            return success(ex::bytesToJs(bytes.value()));
        } catch (const ex::OptionError& err) {
            return failure("InvalidInput", err.what());
        }
    });
}

// Mass, centre of mass and inertia (bendcore_kernel_analysis) for one density, kg/m^3.
val massProperties(std::uint32_t handle, val density) {
    return withShape(handle, [&](Entry& e) {
        try {
            auto p = k::computeMassProperties(e.shape, ex::densityArgument(density));
            if (!p) return failure(p.error());
            return success(ex::massToJs(p.value()));
        } catch (const ex::OptionError& err) {
            return failure("InvalidInput", err.what());
        }
    });
}

// Byte layout of the formats in this build: {zipMethod: 'deflate' | 'stored'} (3MF).
val exchangeInfo() {
    val out = val::object();
    out.set("zipMethod", std::string(k::zipDeflateAvailable() ? "deflate" : "stored"));
    return out;
}

}  // namespace

EMSCRIPTEN_BINDINGS(bendcore_kernel) {
    emscripten::function("importModel", &importModel);
    emscripten::function("exportShape", &exportShape);
    emscripten::function("massProperties", &massProperties);
    emscripten::function("exchangeInfo", &exchangeInfo);
    emscripten::function("kernelInfo", &kernelInfo);
    emscripten::function("readStep", &readStep);
    emscripten::function("disposeShape", &disposeShape);
    emscripten::function("counts", &counts);
    emscripten::function("volumeProperties", &volumeProperties);
    emscripten::function("surfaceProperties", &surfaceProperties);
    emscripten::function("boundingBox", &boundingBox);
    emscripten::function("checkValidity", &checkValidity);
    emscripten::function("faceTypes", &faceTypes);
    emscripten::function("edgeTypes", &edgeTypes);
    emscripten::function("faceInfo", &faceInfo);
    emscripten::function("tessellate", &tessellate);
}
