{
  "targets": [
    {
      "target_name": "bendcore",
      "sources": [
        "native/bendcore.cc"
      ],
      "include_dirs": [
        "include",
        "<!(node -p \"require('node-addon-api').include_dir\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++"
      },
      "msbuild_settings": {
        "VCCLCompilerTool": {"ExceptionHandling": 1}
      },
      "libraries": [
        "build/lib/libbendcore_core.a"
      ],
      "ldflags": [
        "-L../bendcore-remote/build/lib"
      ]
    }
  ]
}
