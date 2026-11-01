/*
 * BendCore OCCT Node.js Binding
 * Exposes OpenCASCADE geometry kernel to JavaScript
 *
 * Usage: const occt = require('./build/Release/occt_binding');
 */

#include <napi.h>
#include <string>

// Forward declarations - implemented in other files
Napi::Object InitShapeWrapper(Napi::Env env, Napi::Object exports);
Napi::Object InitStepReader(Napi::Env env, Napi::Object exports);
Napi::Object InitGeometryKernel(Napi::Env env, Napi::Object exports);

/**
 * Initialize module
 * Called when require('occt_binding') is executed
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Export version info
  exports.Set(
    Napi::String::New(env, "version"),
    Napi::String::New(env, "0.1.0-alpha")
  );

  exports.Set(
    Napi::String::New(env, "occtVersion"),
    Napi::String::New(env, "7.7.0")
  );

  // Export main classes
  InitShapeWrapper(env, exports);
  InitStepReader(env, exports);
  InitGeometryKernel(env, exports);

  // Export utility functions
  exports.Set(
    Napi::String::New(env, "getOccVersion"),
    Napi::Function::New(env, [](const Napi::CallbackInfo& info) {
      return Napi::String::New(info.Env(), "OCCT 7.7.0");
    })
  );

  return exports;
}

// Export the module
NODE_API_MODULE(occt_binding, Init)
