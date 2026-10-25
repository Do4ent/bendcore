#pragma once

#include <string>
#include <vector>
#include <memory>

namespace bendcore::io {

struct ExportOptions {
    std::string outputPath;
    bool includeMetadata = true;
    bool includeNormals = true;
    bool includeColors = false;
    double tolerance = 0.01;  // mm
    unsigned int qualityLevel = 2;  // 0=low, 1=medium, 2=high
};

struct GeometryData {
    std::vector<double> vertices;       // x, y, z for each vertex
    std::vector<unsigned int> faces;    // triangle indices
    std::vector<double> normals;        // vertex normals
    std::vector<std::string> materials; // material names
    std::string name;
};

class STEP_Exporter {
public:
    static bool exportToSTEP(const std::vector<GeometryData>& geometry,
                            const ExportOptions& options);
    static std::string generateSTEPHeader();
    static std::string generateProductDefinition(const GeometryData& geom);
};

class IGES_Exporter {
public:
    static bool exportToIGES(const std::vector<GeometryData>& geometry,
                            const ExportOptions& options);
    static std::string generateIGESHeader();
};

class DXF_Exporter {
public:
    static bool exportToDXF(const std::vector<GeometryData>& geometry,
                           const ExportOptions& options);
    static std::string generate2DProjection(const GeometryData& geom);
};

class STL_Exporter {
public:
    static bool exportToSTL(const std::vector<GeometryData>& geometry,
                           const ExportOptions& options,
                           bool isBinary = false);
};

class OBJ_Exporter {
public:
    static bool exportToOBJ(const std::vector<GeometryData>& geometry,
                           const ExportOptions& options);
};

class JSON_Exporter {
public:
    static bool exportToJSON(const std::vector<GeometryData>& geometry,
                            const ExportOptions& options);
    static std::string geometryToJSON(const GeometryData& geom);
};

class UniversalExporter {
public:
    enum class Format {
        STEP,
        IGES,
        DXF,
        STL,
        OBJ,
        JSON
    };

    static bool exportGeometry(const std::vector<GeometryData>& geometry,
                              const ExportOptions& options,
                              Format format);
};

} // namespace bendcore::io
