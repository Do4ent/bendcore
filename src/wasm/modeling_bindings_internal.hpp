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
m::SketchData sketchFromJs(const val& v);

}  // namespace bendcore::wasm_modeling
