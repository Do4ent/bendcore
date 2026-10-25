#include "bendcore/format_exporters.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace bendcore::io {

std::string STEP_Exporter::generateSTEPHeader() {
    std::ostringstream header;
    header << "ISO-10303-21;\n";
    header << "HEADER;\n";
    header << "FILE_DESCRIPTION(('BendCore CAD Model'),\n";
    header << "  '2;1');\n";
    header << "FILE_NAME('bendcore_model.stp',\n";
    header << "  2026-10-25T13:00:00,(''),(''),\n";
    header << "  'BendCore CAD System v1.0','','');\n";
    header << "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\n";
    header << "ENDSEC;\n";
    return header.str();
}

std::string STEP_Exporter::generateProductDefinition(const GeometryData& geom) {
    std::ostringstream step;
    step << std::fixed << std::setprecision(6);

    // Simplified STEP output - real implementation would be much more complex
    unsigned int index = 1;

    // Product definition
    step << "#" << index << "=PRODUCT('" << geom.name << "','','',(#"
         << (index + 1) << "));\n";
    index += 2;
    step << "#" << index << "=PRODUCT_CONTEXT('design',#" << (index + 1) << ",'');\n";

    return step.str();
}

bool STEP_Exporter::exportToSTEP(const std::vector<GeometryData>& geometry,
                                 const ExportOptions& options)
{
    std::ofstream file(options.outputPath);
    if (!file.is_open()) return false;

    // Write STEP header
    file << generateSTEPHeader();

    // Write DATA section
    file << "DATA;\n";

    for (const auto& geom : geometry) {
        file << generateProductDefinition(geom);
    }

    file << "ENDSEC;\n";
    file << "END-ISO-10303-21;\n";

    file.close();
    return true;
}

bool IGES_Exporter::exportToIGES(const std::vector<GeometryData>& geometry,
                                 const ExportOptions& options)
{
    std::ofstream file(options.outputPath);
    if (!file.is_open()) return false;

    std::string header = generateIGESHeader();
    file << header;

    // Write geometry data
    int lineNumber = 1;
    for (size_t i = 0; i < geometry.size(); ++i) {
        // IGES format: 128-character lines
        std::ostringstream line;
        line << std::fixed << std::setprecision(6);
        line << "     116       " << lineNumber << "       ";
        file << line.str() << "\n";
        lineNumber++;
    }

    file.close();
    return true;
}

std::string IGES_Exporter::generateIGESHeader() {
    std::ostringstream header;
    header << "                                                                        S      1\n";
    header << "1H,,1HZ,15H3D PIPE MODEL,13HBendCore CAD,32,308,15,308,15,\n";
    header << "          ,1.,2HMM,1,0.01,15H20261025.13000,1E-07,100,\n";
    header << "          1H,1H,1H,1H,1H,1H,22HBendCore CAD System   ,32H,                    G      1\n";
    return header.str();
}

bool DXF_Exporter::exportToDXF(const std::vector<GeometryData>& geometry,
                               const ExportOptions& options)
{
    std::ofstream file(options.outputPath);
    if (!file.is_open()) return false;

    file << "999\nDXF created by BendCore CAD\n";
    file << "0\nSECTION\n";
    file << "2\nHEADER\n";
    file << "9\n$ACADVER\n";
    file << "1\nAC1021\n";
    file << "0\nENDSEC\n";

    file << "0\nSECTION\n";
    file << "2\nENTITIES\n";

    // Write 2D projections
    for (const auto& geom : geometry) {
        auto proj = generate2DProjection(geom);
        file << proj;
    }

    file << "0\nENDSEC\n";
    file << "0\nEOF\n";

    file.close();
    return true;
}

std::string DXF_Exporter::generate2DProjection(const GeometryData& geom) {
    std::ostringstream dxf;
    dxf << std::fixed << std::setprecision(4);

    // Generate LINE entities from vertices
    for (size_t i = 0; i + 1 < geom.vertices.size(); i += 3) {
        if (i + 4 < geom.vertices.size()) {
            dxf << "0\nLINE\n";
            dxf << "8\nDEFAULT\n";
            dxf << "10\n" << geom.vertices[i] << "\n";
            dxf << "20\n" << geom.vertices[i+1] << "\n";
            dxf << "11\n" << geom.vertices[i+3] << "\n";
            dxf << "21\n" << geom.vertices[i+4] << "\n";
        }
    }

    return dxf.str();
}

bool STL_Exporter::exportToSTL(const std::vector<GeometryData>& geometry,
                               const ExportOptions& options,
                               bool isBinary)
{
    if (isBinary) {
        // Binary STL format
        std::ofstream file(options.outputPath, std::ios::binary);
        if (!file.is_open()) return false;

        // Write header (80 bytes)
        std::string header = "BendCore CAD STL Model                                                   ";
        file.write(header.c_str(), 80);

        // Count total triangles
        uint32_t triangleCount = 0;
        for (const auto& geom : geometry) {
            triangleCount += geom.faces.size() / 3;
        }
        file.write(reinterpret_cast<char*>(&triangleCount), 4);

        // Write triangles
        for (const auto& geom : geometry) {
            for (size_t i = 0; i + 2 < geom.faces.size(); i += 3) {
                // Normal vector
                float normal[3] = {0, 0, 1};
                file.write(reinterpret_cast<char*>(normal), 12);

                // Vertices
                for (int j = 0; j < 3; j++) {
                    unsigned int idx = geom.faces[i + j] * 3;
                    if (idx + 2 < geom.vertices.size()) {
                        float v[3] = {
                            (float)geom.vertices[idx],
                            (float)geom.vertices[idx+1],
                            (float)geom.vertices[idx+2]
                        };
                        file.write(reinterpret_cast<char*>(v), 12);
                    }
                }

                // Attribute count
                uint16_t attr = 0;
                file.write(reinterpret_cast<char*>(&attr), 2);
            }
        }

        file.close();
    } else {
        // ASCII STL format
        std::ofstream file(options.outputPath);
        if (!file.is_open()) return false;

        file << "solid BendCore_Model\n";
        file << std::fixed << std::setprecision(6);

        for (const auto& geom : geometry) {
            for (size_t i = 0; i + 2 < geom.faces.size(); i += 3) {
                file << "facet normal 0 0 1\n";
                file << "  outer loop\n";

                for (int j = 0; j < 3; j++) {
                    unsigned int idx = geom.faces[i + j] * 3;
                    if (idx + 2 < geom.vertices.size()) {
                        file << "    vertex "
                             << geom.vertices[idx] << " "
                             << geom.vertices[idx+1] << " "
                             << geom.vertices[idx+2] << "\n";
                    }
                }

                file << "  endloop\n";
                file << "endfacet\n";
            }
        }

        file << "endsolid BendCore_Model\n";
        file.close();
    }

    return true;
}

bool OBJ_Exporter::exportToOBJ(const std::vector<GeometryData>& geometry,
                              const ExportOptions& options)
{
    std::ofstream file(options.outputPath);
    if (!file.is_open()) return false;

    file << "# OBJ file exported from BendCore CAD\n";
    file << std::fixed << std::setprecision(6);

    unsigned int vertexOffset = 1;

    for (const auto& geom : geometry) {
        file << "g " << geom.name << "\n";

        // Write vertices
        for (size_t i = 0; i + 2 < geom.vertices.size(); i += 3) {
            file << "v " << geom.vertices[i] << " "
                 << geom.vertices[i+1] << " "
                 << geom.vertices[i+2] << "\n";
        }

        // Write normals if available
        if (options.includeNormals && !geom.normals.empty()) {
            for (size_t i = 0; i + 2 < geom.normals.size(); i += 3) {
                file << "vn " << geom.normals[i] << " "
                     << geom.normals[i+1] << " "
                     << geom.normals[i+2] << "\n";
            }
        }

        // Write faces
        for (size_t i = 0; i + 2 < geom.faces.size(); i += 3) {
            file << "f";
            for (int j = 0; j < 3; j++) {
                unsigned int idx = geom.faces[i+j] + vertexOffset;
                file << " " << idx;
                if (options.includeNormals) {
                    file << "//" << idx;
                }
            }
            file << "\n";
        }

        vertexOffset += geom.vertices.size() / 3;
    }

    file.close();
    return true;
}

bool JSON_Exporter::exportToJSON(const std::vector<GeometryData>& geometry,
                                const ExportOptions& options)
{
    std::ofstream file(options.outputPath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"bendcore_model\": {\n";
    file << "    \"version\": \"1.0\",\n";
    file << "    \"exported\": \"2026-10-25T13:00:00Z\",\n";
    file << "    \"geometry_count\": " << geometry.size() << ",\n";
    file << "    \"geometries\": [\n";

    for (size_t g = 0; g < geometry.size(); g++) {
        file << geometryToJSON(geometry[g]);
        if (g < geometry.size() - 1) file << ",";
        file << "\n";
    }

    file << "    ]\n";
    file << "  }\n";
    file << "}\n";

    file.close();
    return true;
}

std::string JSON_Exporter::geometryToJSON(const GeometryData& geom) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);

    json << "      {\n";
    json << "        \"name\": \"" << geom.name << "\",\n";
    json << "        \"vertex_count\": " << (geom.vertices.size() / 3) << ",\n";
    json << "        \"face_count\": " << (geom.faces.size() / 3) << ",\n";
    json << "        \"vertices\": [";

    for (size_t i = 0; i < std::min(size_t(12), geom.vertices.size()); i++) {
        if (i > 0) json << ",";
        json << geom.vertices[i];
    }

    if (geom.vertices.size() > 12) json << ",...";
    json << "],\n";
    json << "        \"material\": \"" << (geom.materials.empty() ? "default" : geom.materials[0]) << "\"\n";
    json << "      }";

    return json.str();
}

bool UniversalExporter::exportGeometry(const std::vector<GeometryData>& geometry,
                                      const ExportOptions& options,
                                      Format format)
{
    switch (format) {
        case Format::STEP:
            return STEP_Exporter::exportToSTEP(geometry, options);
        case Format::IGES:
            return IGES_Exporter::exportToIGES(geometry, options);
        case Format::DXF:
            return DXF_Exporter::exportToDXF(geometry, options);
        case Format::STL:
            return STL_Exporter::exportToSTL(geometry, options, true);
        case Format::OBJ:
            return OBJ_Exporter::exportToOBJ(geometry, options);
        case Format::JSON:
            return JSON_Exporter::exportToJSON(geometry, options);
        default:
            return false;
    }
}

} // namespace bendcore::io
