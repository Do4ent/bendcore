{
  "targets": [
    {
      "target_name": "occt_binding",
      "sources": [
        "src/addon.cc",
        "src/shape_wrapper.cc",
        "src/step_reader.cc",
        "src/geometry_kernel.cc"
      ],
      "include_dirs": [
        "include",
        "<!(node -p \"require('node-addon-api').include_dir\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-fPIC"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "variables": {
              "occt_home": "<!(if defined OCCT_PATH ( echo %OCCT_PATH% ) else ( echo C:\\occt ))"
            },
            "include_dirs": [
              "<(occt_home)/inc"
            ],
            "libraries": [
              "<(occt_home)/win64/vc14/lib/TKernel.lib",
              "<(occt_home)/win64/vc14/lib/TKMath.lib",
              "<(occt_home)/win64/vc14/lib/TKG2d.lib",
              "<(occt_home)/win64/vc14/lib/TKG3d.lib",
              "<(occt_home)/win64/vc14/lib/TKGeomBase.lib",
              "<(occt_home)/win64/vc14/lib/TKGeomAlgo.lib",
              "<(occt_home)/win64/vc14/lib/TKBRep.lib",
              "<(occt_home)/win64/vc14/lib/TKTopAlgo.lib",
              "<(occt_home)/win64/vc14/lib/TKPrim.lib",
              "<(occt_home)/win64/vc14/lib/TKSTEP.lib",
              "<(occt_home)/win64/vc14/lib/TKSTEPBase.lib",
              "<(occt_home)/win64/vc14/lib/TKSTEP209.lib"
            ]
          }
        ],
        [
          "OS=='linux'",
          {
            "variables": {
              "occt_home": "<!(echo $OCCT_PATH || echo /usr/local/occt)"
            },
            "include_dirs": [
              "<(occt_home)/inc"
            ],
            "libraries": [
              "-L<(occt_home)/lib",
              "-lTKernel",
              "-lTKMath",
              "-lTKG2d",
              "-lTKG3d",
              "-lTKGeomBase",
              "-lTKGeomAlgo",
              "-lTKBRep",
              "-lTKTopAlgo",
              "-lTKPrim",
              "-lTKSTEP",
              "-lTKSTEPBase",
              "-lTKSTEP209"
            ]
          }
        ]
      ],
      "msbuild_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      }
    }
  ]
}
