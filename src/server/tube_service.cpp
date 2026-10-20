#include "bendcore/tube_service.hpp"

#include "bendcore/tube_body.hpp"
#include "bendcore/tube_model.hpp"

#include <cmath>
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

}  // namespace

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
