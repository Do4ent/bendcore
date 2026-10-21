#include <napi.h>
#include "tubegeom.hpp"
#include "tube_model.hpp"
#include "ports.hpp"
#include "refs.hpp"

namespace bendcore_node {

// Helper: Convert JS array of {x, y, z} to std::vector<bendcore::Vec3d>
static std::vector<bendcore::Vec3d> JSArrayToVec3dVector(
    const Napi::Array& arr) {
  std::vector<bendcore::Vec3d> result;
  for (size_t i = 0; i < arr.Length(); ++i) {
    auto obj = arr.Get(i).As<Napi::Object>();
    double x = obj.Get("x").As<Napi::Number>();
    double y = obj.Get("y").As<Napi::Number>();
    double z = obj.Get("z").As<Napi::Number>();
    result.push_back({x, y, z});
  }
  return result;
}

// Helper: Convert std::vector<bendcore::Vec3d> to JS array
static Napi::Array Vec3dVectorToJSArray(
    Napi::Env env, const std::vector<bendcore::Vec3d>& vec) {
  auto arr = Napi::Array::New(env, vec.size());
  for (size_t i = 0; i < vec.size(); ++i) {
    auto obj = Napi::Object::New(env);
    obj.Set("x", Napi::Number::New(env, vec[i].x));
    obj.Set("y", Napi::Number::New(env, vec[i].y));
    obj.Set("z", Napi::Number::New(env, vec[i].z));
    arr.Set(i, obj);
  }
  return arr;
}

// CreateTube: Create a tube from path vertices
// Args: {diameter, radius, vertices: [{x,y,z}...]}
// Returns: {success, tubes: [...], errors: [...]}
static Napi::Value CreateTube(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    throw Napi::Error::New(env, "CreateTube requires an object argument");
  }

  auto params = info[0].As<Napi::Object>();
  double diameter = params.Get("diameter").As<Napi::Number>();
  double radius = params.Get("radius").As<Napi::Number>();
  auto vertices = params.Get("vertices").As<Napi::Array>();

  try {
    auto path = JSArrayToVec3dVector(vertices);

    // Create tube geometry
    // This is a placeholder - actual implementation depends on bendcore API
    auto result = Napi::Object::New(env);
    result.Set("success", Napi::Boolean::New(env, true));
    result.Set("diameter", Napi::Number::New(env, diameter));
    result.Set("radius", Napi::Number::New(env, radius));
    result.Set("pathLength", Napi::Number::New(env, (double)path.size()));

    return result;
  } catch (const std::exception& e) {
    auto result = Napi::Object::New(env);
    result.Set("success", Napi::Boolean::New(env, false));
    result.Set("error", Napi::String::New(env, e.what()));
    return result;
  }
}

// ValidatePath: Check if a path is valid
// Args: {vertices: [{x,y,z}...], diameter, radius}
// Returns: {valid, errors: [...]}
static Napi::Value ValidatePath(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    throw Napi::Error::New(env, "ValidatePath requires an object argument");
  }

  auto params = info[0].As<Napi::Object>();
  auto vertices = params.Get("vertices").As<Napi::Array>();
  double diameter = params.Get("diameter").As<Napi::Number>();

  try {
    auto path = JSArrayToVec3dVector(vertices);

    auto result = Napi::Object::New(env);
    result.Set("valid", Napi::Boolean::New(env, path.size() >= 2));
    result.Set("vertexCount", Napi::Number::New(env, (double)path.size()));
    result.Set("diameter", Napi::Number::New(env, diameter));

    return result;
  } catch (const std::exception& e) {
    auto result = Napi::Object::New(env);
    result.Set("valid", Napi::Boolean::New(env, false));
    result.Set("error", Napi::String::New(env, e.what()));
    return result;
  }
}

// Initialize module
static Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(
    "createTube",
    Napi::Function::New(env, CreateTube)
  );

  exports.Set(
    "validatePath",
    Napi::Function::New(env, ValidatePath)
  );

  return exports;
}

NODE_API_MODULE(bendcore, Init)

}  // namespace bendcore_node
