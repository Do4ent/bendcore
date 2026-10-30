#include "bendcore/tube_service.hpp"

#include "bendcore/geometry_analyzer.hpp"
#include "bendcore/occt/geometry_backend.hpp"
#include "bendcore/tube_body.hpp"
#include "bendcore/tube_model.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <cstdio>
#include <exception>
#include <sstream>
#include <stdexcept>

namespace bendcore::server {
namespace {

std::string num(double v) {
    if (!std::isfinite(v)) return "null";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string vec(const Vec3& v) {
    return "[" + num(v[0]) + "," + num(v[1]) + "," + num(v[2]) + "]";
}

std::string error_body(const std::string& message) {
    return "{\"ok\":false,\"error\":" + quote(message) + "}";
}

std::string mesh_json(const geom::TubeMesh& mesh) {
    std::string out = "{\"vertices\":[";
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (i) out += ",";
        out += num(mesh.vertices[i]);
    }
    out += "],\"indices\":[";
    for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
        if (i) out += ",";
        out += std::to_string(mesh.indices[i]);
    }
    return out + "]}";
}

// Временный файл для читателя STEP (OCCT читает по имени файла); удаляется деструктором.
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& content) {
        std::string tmpl = (std::filesystem::temp_directory_path() / "bendcore_import_XXXXXX").string();
        const int fd = mkstemp(tmpl.data());
        if (fd < 0) throw std::runtime_error("не удалось создать временный файл");
        close(fd);
        path = tmpl;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << content;
        if (!out) throw std::runtime_error("не удалось записать временный файл");
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

Response analyze_response(const std::string& format, const std::string& text, double deflection) {
    Response res;
    try {
        if (format != "step") throw std::invalid_argument("формат анализа: только step");
        if (text.empty()) throw std::invalid_argument("пустое тело запроса");
        if (!(deflection > 0.0 && deflection <= 10.0)) throw std::invalid_argument("deflection вне (0, 10]");
        const TempFile file(text);
        const geometry::BRepTubeFeatures features = occt::analyze_brep(geom::step_brep(file.path));
        if (!features.ok) throw std::invalid_argument("не распознано как труба: " + features.error);
        const geometry::ExtractedParams p = geometry::GeometryAnalyzer::params_from_features(features);
        const geom::TubeMesh mesh = geom::step_mesh(file.path, deflection);

        std::ostringstream out;
        out << "{\"ok\":true,\"tube\":{\"od\":" << num(p.outer_diameter) << ",\"wall\":" << num(p.wall_thickness)
            << ",\"inner_diameter\":" << num(p.inner_diameter) << ",\"length\":" << num(p.total_length)
            << ",\"bends\":[";
        for (std::size_t i = 0; i < p.bend_angles.size(); ++i) {
            if (i) out << ",";
            out << "{\"angle\":" << num(p.bend_angles[i]) << ",\"radius\":" << num(p.bend_radii[i]) << "}";
        }
        out << "],\"shape_type\":" << quote(p.shape_type) << ",\"confidence\":" << num(p.confidence)
            << ",\"notes\":" << quote(p.analysis_notes) << ",\"valid\":" << (p.is_valid() ? "true" : "false")
            << "},\"mesh\":" << mesh_json(mesh) << "}";
        res.body = out.str();
    } catch (const std::invalid_argument& e) {
        res.status = 400;
        res.body = error_body(e.what());
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = error_body(e.what());
    }
    return res;
}

Response tube_response(const std::string& format, const std::string& text, double od, double wall, double deflection) {
    Response res;
    try {
        if (format != "csv" && format != "json") throw std::invalid_argument("формат должен быть csv или json");
        if (!(deflection > 0.0) || deflection > 10.0) throw std::invalid_argument("отклонение сетки: от 0 до 10 мм");

        const ImportData data = format == "csv" ? import_csv(text) : import_json(text);
        TubeModel model = geom::complete_model(make_model("труба", format, data, od, wall));
        if (!model.path.has_value()) throw std::invalid_argument("в файле нет пути: тело не строится");

        const geom::TubeSolidResult solid = geom::make_tube(*model.path, od, wall);
        const geom::TubeMesh mesh = geom::make_mesh(*model.path, od, wall, deflection);

        std::ostringstream out;
        out << "{\"ok\":true"
            << ",\"od\":" << num(od) << ",\"wall\":" << num(wall)
            << ",\"volume\":" << num(solid.volume) << ",\"valid\":" << (solid.valid ? "true" : "false")
            << ",\"axis_length\":" << num(solid.axis_length) << ",\"ports\":[";
        for (std::size_t i = 0; i < model.ports.size(); ++i) {
            const Port& p = model.ports[i];
            if (i) out << ",";
            out << "{\"name\":" << quote(p.name) << ",\"pos\":" << vec(p.pos) << ",\"direction\":" << vec(p.direction)
                << ",\"od\":" << num(p.od) << "}";
        }
        out << "],\"mesh\":{\"vertices\":[";
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (i) out << ",";
            out << num(mesh.vertices[i]);
        }
        out << "],\"indices\":[";
        for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
            if (i) out << ",";
            out << mesh.indices[i];
        }
        out << "]}}";
        res.body = out.str();
    } catch (const std::invalid_argument& e) {
        res.status = 400;
        res.body = error_body(e.what());
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = error_body(e.what());
    }
    return res;
}

}  // namespace bendcore::server
