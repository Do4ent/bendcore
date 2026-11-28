#pragma once

// Data exchange shared by the kernel, part and assembly WebAssembly modules
// (docs/ui/DATA_EXCHANGE.md): export of a shape to STEP / IGES / STL / 3MF with options from
// JS, mass properties to JS, and the byte conversions. Header-only, so each module compiles
// it with its own bindings; every function returns k::Result or a plain emscripten::val and
// lets the caller wrap it into its {ok, value} convention.

#include <bendcore/kernel/mass_properties.hpp>
#include <bendcore/kernel/mesh_export.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/xcaf_io.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace bendcore::wasm_exchange {

using emscripten::val;
namespace k = bendcore::kernel;

// Malformed option from JS; the bindings report it as InvalidInput.
struct OptionError : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

inline bool given(const val& v) { return !v.isUndefined() && !v.isNull(); }

inline std::string stringOption(const val& options, const char* name, const std::string& fallback) {
    if (!given(options) || !given(options[name])) return fallback;
    if (!options[name].isString()) throw OptionError(std::string("options.") + name + " must be a string");
    return options[name].as<std::string>();
}

inline double numberOption(const val& options, const char* name, double fallback, double min, double max) {
    if (!given(options) || !given(options[name])) return fallback;
    if (!options[name].isNumber()) throw OptionError(std::string("options.") + name + " must be a number");
    const double v = options[name].as<double>();
    if (!std::isfinite(v) || v < min || v > max) {
        throw OptionError(std::string("options.") + name + " = " + std::to_string(v) + " is outside [" +
                          std::to_string(min) + ", " + std::to_string(max) + "]");
    }
    return v;
}

// Formats a JS caller can ask for; "step" and "iges" keep the B-rep, "stl" and "3mf" are meshes.
enum class ExportFormat { Step, Iges, Stl, ThreeMf };

inline ExportFormat parseFormat(const std::string& format) {
    if (format == "step") return ExportFormat::Step;
    if (format == "iges") return ExportFormat::Iges;
    if (format == "stl") return ExportFormat::Stl;
    if (format == "3mf") return ExportFormat::ThreeMf;
    throw OptionError("format must be 'step', 'iges', 'stl' or '3mf', got '" + format + "'");
}

inline k::StepLengthUnit unitOption(const val& options) {
    const std::string unit = stringOption(options, "unit", "mm");
    if (unit == "mm") return k::StepLengthUnit::Millimetre;
    if (unit == "m") return k::StepLengthUnit::Metre;
    if (unit == "inch") return k::StepLengthUnit::Inch;
    throw OptionError("options.unit must be 'mm', 'm' or 'inch'");
}

inline k::AssemblyWriteOptions stepOptions(const val& options) {
    k::AssemblyWriteOptions o;
    const std::string schema = stringOption(options, "schema", "AP214");
    if (schema == "AP242") o.schema = k::StepSchema::AP242;
    else if (schema != "AP214") throw OptionError("options.schema must be 'AP214' or 'AP242'");
    o.fileUnit = unitOption(options);
    return o;
}

inline k::IgesWriteOptions igesOptions(const val& options) {
    k::IgesWriteOptions o;
    const std::string mode = stringOption(options, "igesMode", "brep");
    if (mode == "faces") o.mode = k::IgesWriteMode::Faces;
    else if (mode != "brep") throw OptionError("options.igesMode must be 'brep' or 'faces'");
    o.fileUnit = unitOption(options);
    return o;
}

// Chordal deflection (mm) and angle (degrees in JS, radians in the kernel) of mesh formats.
inline k::MeshExportParams meshOptions(const val& options) {
    k::MeshExportParams p;
    p.linearDeflection = numberOption(options, "linearDeflection", 0.05, 1e-4, 100.0);
    p.angularDeflection = numberOption(options, "angularDeflectionDeg", 20.0, 0.5, 90.0) * M_PI / 180.0;
    return p;
}

inline bool stlBinary(const val& options) {
    const std::string format = stringOption(options, "stlFormat", "binary");
    if (format == "binary") return true;
    if (format == "ascii") return false;
    throw OptionError("options.stlFormat must be 'binary' or 'ascii'");
}

inline k::Rgb toRgb(const k::AssemblyColor& c) {
    auto byte = [](double v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    return k::Rgb{byte(c.r), byte(c.g), byte(c.b)};
}

// One part: a single-definition XCAF document, so STEP and IGES carry the name and colour.
inline k::AssemblyDocument partDocument(const k::Shape& shape, const std::string& name,
                                        const std::optional<k::AssemblyColor>& color) {
    k::AssemblyDocument doc;
    k::AssemblyDefinition def;
    def.kind = k::DefinitionKind::Part;
    def.name = name.empty() ? std::string("Part") : name;
    def.shape = shape;
    def.color = color;
    doc.definitions.push_back(std::move(def));
    doc.roots = {0};
    return doc;
}

// Export of an XCAF structure (part or assembly). STL merges all placed parts into one mesh;
// 3MF writes one object per placed part with its effective colour.
inline k::Result<std::string> exportDocument(const k::AssemblyDocument& doc, const std::string& format,
                                             const val& options) {
    switch (parseFormat(format)) {
        case ExportFormat::Step:
            return k::writeStepAssembly(doc, stepOptions(options));
        case ExportFormat::Iges:
            return k::writeIgesAssembly(doc, igesOptions(options));
        case ExportFormat::Stl: {
            auto placed = k::flattenAssembly(doc);
            if (!placed) return placed.error();
            const bool binary = stlBinary(options);
            const k::MeshExportParams params = meshOptions(options);
            k::TriangleMesh all;
            for (const k::PlacedPart& p : placed.value()) {
                auto mesh = k::meshForExport(p.shape, params);
                if (!mesh) return mesh.error();
                const auto base = static_cast<std::uint32_t>(all.vertices.size());
                all.vertices.insert(all.vertices.end(), mesh.value().vertices.begin(), mesh.value().vertices.end());
                for (const auto& t : mesh.value().triangles) all.triangles.push_back({t[0] + base, t[1] + base, t[2] + base});
            }
            const std::string name = doc.definitions[doc.roots.front()].name;
            return binary ? k::writeStlBinary(all, "bendcore " + name) : k::writeStlAscii(all, name);
        }
        case ExportFormat::ThreeMf: {
            auto placed = k::flattenAssembly(doc);
            if (!placed) return placed.error();
            std::vector<k::ThreeMfObject> objects;
            for (const k::PlacedPart& p : placed.value()) {
                k::ThreeMfObject o;
                o.name = p.path;
                o.shape = p.shape;
                if (p.color) o.color = toRgb(*p.color);
                objects.push_back(std::move(o));
            }
            return k::export3mf(objects, meshOptions(options));
        }
    }
    return k::Error{k::ErrorCode::InvalidInput, "unknown export format"};
}

inline val bytesToJs(const std::string& bytes) {
    return val::global("Uint8Array")
        .new_(emscripten::typed_memory_view(bytes.size(), reinterpret_cast<const unsigned char*>(bytes.data())));
}

inline std::string bytesFromJs(const val& v, const char* what) {
    if (v.isString()) return v.as<std::string>();
    if (!v.instanceof(val::global("Uint8Array"))) {
        throw OptionError(std::string(what) + " must be a Uint8Array or a string");
    }
    const auto n = v["length"].as<std::size_t>();
    std::string out(n, '\0');
    if (n > 0) {
        // The memory view aliases `out`; set() copies the JS bytes into it.
        val(emscripten::typed_memory_view(n, reinterpret_cast<unsigned char*>(out.data()))).call<void>("set", v);
    }
    return out;
}

inline val vec3(const std::array<double, 3>& p) {
    val out = val::array();
    for (double x : p) out.call<void>("push", x);
    return out;
}

inline val massToJs(const k::MassProperties& p) {
    val o = val::object();
    o.set("mass", p.mass);
    o.set("volume", p.volume);
    o.set("area", p.area);
    o.set("centerOfMass", vec3(p.centerOfMass));
    val inertia = val::array();
    for (const auto& row : p.inertiaAtCenter) inertia.call<void>("push", vec3(row));
    o.set("inertiaAtCenter", inertia);
    o.set("principalMoments", vec3(p.principal.moments));
    val axes = val::array();
    for (const auto& a : p.principal.axes) axes.call<void>("push", vec3(a));
    o.set("principalAxes", axes);
    o.set("bodies", static_cast<double>(p.bodyCount));
    return o;
}

inline double densityArgument(const val& density) {
    if (!density.isNumber()) throw OptionError("density must be a number (kg/m^3)");
    const double rho = density.as<double>();
    if (!std::isfinite(rho) || !(rho > 0.0) || rho > 30000.0) {
        throw OptionError("density must be in (0, 30000] kg/m^3, got " + std::to_string(rho));
    }
    return rho;
}

// Structure of an imported file for the UI: number of placed parts, names, whether it is an
// assembly (several parts or an assembly root), warnings of the reader.
inline val structureToJs(const k::AssemblyDocument& doc, const std::vector<k::PlacedPart>& placed) {
    val o = val::object();
    bool assembly = placed.size() > 1;
    for (std::size_t r : doc.roots)
        if (doc.definitions[r].kind == k::DefinitionKind::Assembly) assembly = true;
    o.set("assembly", assembly);
    o.set("parts", static_cast<double>(placed.size()));
    o.set("definitions", static_cast<double>(doc.definitions.size()));
    val names = val::array();
    for (const k::PlacedPart& p : placed) names.call<void>("push", p.path);
    o.set("paths", names);
    val warnings = val::array();
    for (const std::string& w : doc.warnings) warnings.call<void>("push", w);
    o.set("warnings", warnings);
    o.set("name", doc.roots.empty() ? std::string() : doc.definitions[doc.roots.front()].name);
    o.set("fileUnitMm", doc.fileUnitMm ? val(*doc.fileUnitMm) : val::null());
    return o;
}

inline k::Result<k::AssemblyDocument> readDocument(const std::string& format, const std::string& data) {
    if (format == "step") return k::readStepAssembly(data);
    if (format == "iges") return k::readIgesAssembly(data);
    throw OptionError("import format must be 'step' or 'iges', got '" + format + "'");
}

}  // namespace bendcore::wasm_exchange
