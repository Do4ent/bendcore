#pragma once

// Shared helpers of the bendcore_modeling_wasm bindings: the document registry, the {ok, value} /
// {ok:false, code, message} result convention and the JS <-> C++ conversions used by both
// modeling_bindings.cpp (part document) and sketch_editor_bindings.cpp (sketch editor,
// docs/ui/SKETCH_EDITOR.md).

#include <bendcore/kernel/result.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <Standard_Failure.hxx>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace bendcore::wasm_modeling {

using emscripten::val;
namespace k = bendcore::kernel;
namespace m = bendcore::modeling;

// Malformed argument from JS (wrong type, missing field); reported as InvalidInput.
struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::map<std::uint32_t, std::unique_ptr<m::PartDocument>>& documents();

val success(val value);
val failure(const std::string& code, const std::string& message);
val failure(const k::Error& error);

// Runs body() and converts exceptions: InputError -> InvalidInput, OCCT and std -> OperationFailed.
template <typename Body>
val guarded(Body&& body) {
    try {
        return body();
    } catch (const InputError& e) {
        return failure("InvalidInput", e.what());
    } catch (const Standard_Failure& e) {
        return failure("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failure("OperationFailed", e.what());
    }
}

template <typename Body>
val withDocument(std::uint32_t handle, Body&& body) {
    auto it = documents().find(handle);
    if (it == documents().end()) {
        return failure("InvalidInput", "unknown document handle " + std::to_string(handle) +
                                           " (never created or already disposed)");
    }
    m::PartDocument& doc = *it->second;
    return guarded([&] { return body(doc); });
}

template <typename T>
val typedArray(const char* constructor, const std::vector<T>& data) {
    // The view aliases wasm memory; constructing a new typed array from it copies the data.
    return val::global(constructor).new_(emscripten::typed_memory_view(data.size(), data.data()));
}

bool present(const val& v);
bool isObject(const val& v);
std::string requireString(const val& v, const std::string& what);
std::string expression(const val& v, const std::string& what);
int requireInt(const val& v, const std::string& what);
bool optionalBool(const val& obj, const char* field, bool fallback, const std::string& what);
std::vector<val> arrayItems(const val& v, const std::string& what);
val point(const k::Point3& p);
// {origin, xDirection, yDirection, normal}: sketch frame in model space.
val placementJs(const m::SketchPlacement& p);

val toJs(const m::SketchData& d);
val toJs(const m::TopoReference& r);
m::TopoReference referenceFromJs(const val& v, const std::string& what);
// Shape addressed by a target string: final, before:<id>, after:<id>, sketch:<id>.
m::NamedShape targetShape(const m::PartDocument& doc, const val& targetValue);

// Decimal spelling of a value written into sketch expressions. The expression parser has no
// exponent notation, so tiny values (solver noise around 0) become 0 and the rest is written in
// plain positional form.
inline std::string decimalText(double v) {
    if (v < 1e-9 && v > -1e-9) return "0";
    std::string text = val::global("String")(val(v)).as<std::string>();
    if (text.find('e') == std::string::npos) return text;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12f", v);
    text = buf;
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}
m::SketchData sketchFromJs(const val& v);

}  // namespace bendcore::wasm_modeling
