#include <bendcore/kernel/reference_mesh.hpp>

#include "../occt_failure.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

#include <Standard_Failure.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>

namespace bendcore::kernel {

namespace {

using Vec = std::array<double, 3>;

Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec add(const Vec& a, const Vec& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec mul(const Vec& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
Vec cross(const Vec& a, const Vec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const Vec& a) { return std::sqrt(dot(a, a)); }

std::string megabytes(std::size_t bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", mb);
    return buf;
}

std::uint32_t readLe32(std::string_view d, std::size_t at) {
    std::uint32_t v = 0;
    std::memcpy(&v, d.data() + at, 4);
    return v;
}

bool startsWithSolid(std::string_view d) {
    std::size_t i = 0;
    while (i < d.size() && std::isspace(static_cast<unsigned char>(d[i]))) ++i;
    return d.substr(i, 5) == "solid";
}

struct KeyHash {
    std::size_t operator()(const std::array<std::uint64_t, 3>& k) const noexcept {
        std::uint64_t h = 1469598103934665603ull;
        for (std::uint64_t v : k) {
            h ^= v;
            h *= 1099511628211ull;
        }
        return static_cast<std::size_t>(h);
    }
};

// Triangle soup (corner coordinates) -> welded indexed mesh without degenerate triangles.
struct SoupBuilder {
    std::vector<Vec> corners;  // 3 per triangle

    Result<ReferenceMeshData> build(MeshReadReport report) const {
        using R = Result<ReferenceMeshData>;
        Vec lo{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
               std::numeric_limits<double>::infinity()};
        Vec hi = mul(lo, -1.0);
        for (std::size_t i = 0; i < corners.size(); ++i) {
            for (int k = 0; k < 3; ++k) {
                const double c = corners[i][k];
                if (!std::isfinite(c)) {
                    return R::failure(ErrorCode::ParseError, "mesh triangle " + std::to_string(i / 3 + 1) +
                                                                 ": coordinate is not finite");
                }
                lo[k] = std::min(lo[k], c);
                hi[k] = std::max(hi[k], c);
            }
        }
        const Vec diag = sub(hi, lo);
        const double minDoubleArea = 2.0e-12 * dot(diag, diag);
        ReferenceMeshData out;
        out.report = std::move(report);
        std::unordered_map<std::array<std::uint64_t, 3>, std::uint32_t, KeyHash> index;
        index.reserve(corners.size() / 2 + 1);
        auto weld = [&](const Vec& p) {
            std::array<std::uint64_t, 3> key{};
            for (int k = 0; k < 3; ++k) {
                const double c = p[k] == 0.0 ? 0.0 : p[k];  // -0 and +0 weld
                std::memcpy(&key[k], &c, 8);
            }
            auto [it, inserted] = index.try_emplace(key, static_cast<std::uint32_t>(out.mesh.vertices.size()));
            if (inserted) out.mesh.vertices.push_back(p);
            return it->second;
        };
        for (std::size_t i = 0; i + 2 < corners.size(); i += 3) {
            const Vec& a = corners[i];
            const Vec& b = corners[i + 1];
            const Vec& c = corners[i + 2];
            if (!(norm(cross(sub(b, a), sub(c, a))) > minDoubleArea)) {
                ++out.report.degenerateDropped;
                continue;
            }
            const std::uint32_t ia = weld(a), ib = weld(b), ic = weld(c);
            if (ia == ib || ib == ic || ia == ic) {
                ++out.report.degenerateDropped;
                continue;
            }
            out.mesh.triangles.push_back({ia, ib, ic});
        }
        if (out.mesh.triangles.empty()) return R::failure(ErrorCode::InvalidInput, "the mesh has no triangle of non-zero area");
        return out;
    }
};

Result<ReferenceMeshData> readStlMesh(std::string_view data, const MeshReadLimits& limits) {
    using R = Result<ReferenceMeshData>;
    MeshReadReport report;
    const bool binaryLayout = data.size() >= 84 && 84ull + 50ull * readLe32(data, 80) == data.size();
    if (!binaryLayout && !startsWithSolid(data)) {
        if (data.size() < 84) {
            return R::failure(ErrorCode::ParseError, "STL: the file is " + std::to_string(data.size()) +
                                                         " bytes, shorter than a binary STL header (84 bytes)");
        }
        const std::uint64_t n = readLe32(data, 80);
        return R::failure(ErrorCode::ParseError, "binary STL: the header declares " + std::to_string(n) + " triangles (" +
                                                     std::to_string(84ull + 50ull * n) + " bytes), the file has " +
                                                     std::to_string(data.size()) + " bytes");
    }
    if (binaryLayout) {
        const std::uint64_t n = readLe32(data, 80);
        if (n > limits.maxTriangles) {
            return R::failure(ErrorCode::InvalidInput, "the mesh has " + std::to_string(n) + " triangles, the limit is " +
                                                           std::to_string(limits.maxTriangles));
        }
    }
    auto facets = readStl(data);
    if (!facets) return facets.error();
    report.encoding = binaryLayout ? "binary" : "ascii";
    report.facetsRead = facets.value().size();
    if (report.facetsRead > limits.maxTriangles) {
        return R::failure(ErrorCode::InvalidInput, "the mesh has " + std::to_string(report.facetsRead) +
                                                       " triangles, the limit is " + std::to_string(limits.maxTriangles));
    }
    if (report.facetsRead == 0) return R::failure(ErrorCode::InvalidInput, "the mesh has no triangle of non-zero area");
    SoupBuilder soup;
    soup.corners.reserve(report.facetsRead * 3);
    for (const StlFacet& f : facets.value())
        for (const auto& v : f.vertices) soup.corners.push_back({v[0], v[1], v[2]});
    return soup.build(std::move(report));
}

// ------------------------------------------------------------------ 3MF

// One start or empty-element tag of the model XML with its attributes.
struct XmlTag {
    std::string_view name;  // without a namespace prefix
    bool closing = false;
    bool selfClosing = false;
    std::vector<std::pair<std::string_view, std::string_view>> attributes;

    std::optional<std::string_view> attr(std::string_view key) const {
        for (const auto& [k, v] : attributes) {
            const auto colon = k.find(':');
            if (k == key || (colon != std::string_view::npos && k.substr(colon + 1) == key)) return v;
        }
        return std::nullopt;
    }
};

class XmlScanner {
public:
    explicit XmlScanner(std::string_view text) : t_(text) {}

    // Next tag; nullopt at the end. `error` is set on malformed markup.
    std::optional<XmlTag> next(std::string& error) {
        for (;;) {
            const std::size_t lt = t_.find('<', pos_);
            if (lt == std::string_view::npos) return std::nullopt;
            pos_ = lt + 1;
            if (t_.substr(pos_, 3) == "!--") {
                const std::size_t end = t_.find("-->", pos_);
                if (end == std::string_view::npos) return fail(error, "unterminated comment");
                pos_ = end + 3;
                continue;
            }
            if (pos_ < t_.size() && (t_[pos_] == '?' || t_[pos_] == '!')) {
                const std::size_t end = t_.find('>', pos_);
                if (end == std::string_view::npos) return fail(error, "unterminated declaration");
                pos_ = end + 1;
                continue;
            }
            XmlTag tag;
            if (pos_ < t_.size() && t_[pos_] == '/') {
                tag.closing = true;
                ++pos_;
            }
            const std::size_t nameStart = pos_;
            while (pos_ < t_.size() && !std::isspace(static_cast<unsigned char>(t_[pos_])) && t_[pos_] != '>' && t_[pos_] != '/') ++pos_;
            std::string_view name = t_.substr(nameStart, pos_ - nameStart);
            if (name.empty()) return fail(error, "empty tag name");
            if (const auto colon = name.find(':'); colon != std::string_view::npos) name = name.substr(colon + 1);
            tag.name = name;
            for (;;) {
                while (pos_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[pos_]))) ++pos_;
                if (pos_ >= t_.size()) return fail(error, "unterminated tag <" + std::string(name) + ">");
                if (t_[pos_] == '>') {
                    ++pos_;
                    return tag;
                }
                if (t_[pos_] == '/') {
                    if (pos_ + 1 >= t_.size() || t_[pos_ + 1] != '>') return fail(error, "bad tag end");
                    tag.selfClosing = true;
                    pos_ += 2;
                    return tag;
                }
                const std::size_t keyStart = pos_;
                while (pos_ < t_.size() && t_[pos_] != '=' && !std::isspace(static_cast<unsigned char>(t_[pos_])) && t_[pos_] != '>') ++pos_;
                const std::string_view key = t_.substr(keyStart, pos_ - keyStart);
                while (pos_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[pos_]))) ++pos_;
                if (pos_ >= t_.size() || t_[pos_] != '=') return fail(error, "attribute '" + std::string(key) + "' without a value");
                ++pos_;
                while (pos_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[pos_]))) ++pos_;
                if (pos_ >= t_.size() || (t_[pos_] != '"' && t_[pos_] != '\'')) return fail(error, "unquoted attribute '" + std::string(key) + "'");
                const char quote = t_[pos_++];
                const std::size_t end = t_.find(quote, pos_);
                if (end == std::string_view::npos) return fail(error, "unterminated attribute '" + std::string(key) + "'");
                tag.attributes.emplace_back(key, t_.substr(pos_, end - pos_));
                pos_ = end + 1;
            }
        }
    }

private:
    std::optional<XmlTag> fail(std::string& error, const std::string& what) {
        error = what;
        return std::nullopt;
    }
    std::string_view t_;
    std::size_t pos_ = 0;
};

bool parseNumber(std::string_view s, double& out) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (!s.empty() && s.front() == '+') s.remove_prefix(1);
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() && r.ptr == s.data() + s.size() && std::isfinite(out);
}

std::optional<double> threeMfUnitScale(std::string_view unit) {
    if (unit.empty() || unit == "millimeter") return 1.0;
    if (unit == "micron") return 0.001;
    if (unit == "centimeter") return 10.0;
    if (unit == "meter") return 1000.0;
    if (unit == "inch") return 25.4;
    if (unit == "foot") return 304.8;
    return std::nullopt;
}

Result<ReferenceMeshData> read3mfMesh(std::string_view data, const MeshReadLimits& limits) {
    using R = Result<ReferenceMeshData>;
    auto entries = readZip(data);
    if (!entries) {
        if (entries.error().code == ErrorCode::Unsupported) return entries.error();
        return R::failure(ErrorCode::ParseError, "3MF: " + entries.error().message);
    }
    const ZipEntry* model = nullptr;
    for (const ZipEntry& e : entries.value()) {
        std::string lower = e.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (lower.size() > 6 && lower.compare(lower.size() - 6, 6, ".model") == 0) {
            if (!model || lower == "3d/3dmodel.model") model = &e;
        }
    }
    if (!model) return R::failure(ErrorCode::ParseError, "3MF: the package has no 3D model part");

    struct Object {
        std::vector<Vec> vertices;
        std::vector<std::array<std::uint32_t, 3>> triangles;
    };
    std::map<std::string, Object, std::less<>> objects;
    std::vector<std::pair<std::string, std::array<double, 12>>> items;
    double scale = 1.0;
    Object* current = nullptr;
    std::size_t triangleTotal = 0;
    XmlScanner scan(model->data);
    std::string error;
    auto fail = [&](const std::string& what) { return R::failure(ErrorCode::ParseError, "3MF model: " + what); };
    while (auto tag = scan.next(error)) {
        if (tag->closing) {
            if (tag->name == "object") current = nullptr;
            continue;
        }
        if (tag->name == "model") {
            const auto u = threeMfUnitScale(tag->attr("unit").value_or(""));
            if (!u) return fail("unknown unit '" + std::string(tag->attr("unit").value_or("")) + "'");
            scale = *u;
        } else if (tag->name == "object") {
            const auto id = tag->attr("id");
            if (!id) return fail("object without an id");
            current = &objects[std::string(*id)];
        } else if (tag->name == "components" || tag->name == "component") {
            return R::failure(ErrorCode::Unsupported, "3MF: objects made of components are not supported");
        } else if (tag->name == "vertex") {
            if (!current) return fail("vertex outside an object");
            Vec p{};
            const char* names[3] = {"x", "y", "z"};
            for (int k = 0; k < 3; ++k) {
                const auto v = tag->attr(names[k]);
                if (!v || !parseNumber(*v, p[k])) return fail("vertex " + std::to_string(current->vertices.size()) + ": bad coordinate " + names[k]);
            }
            current->vertices.push_back(p);
        } else if (tag->name == "triangle") {
            if (!current) return fail("triangle outside an object");
            std::array<std::uint32_t, 3> t{};
            const char* names[3] = {"v1", "v2", "v3"};
            for (int k = 0; k < 3; ++k) {
                const auto v = tag->attr(names[k]);
                double idx = -1;
                if (!v || !parseNumber(*v, idx) || idx < 0 || idx != std::floor(idx) || idx >= double(current->vertices.size())) {
                    return fail("triangle " + std::to_string(current->triangles.size()) + ": bad vertex index " + names[k]);
                }
                t[k] = static_cast<std::uint32_t>(idx);
            }
            current->triangles.push_back(t);
            if (++triangleTotal > limits.maxTriangles) {
                return R::failure(ErrorCode::InvalidInput, "the mesh has more than " + std::to_string(limits.maxTriangles) +
                                                               " triangles, the limit is " + std::to_string(limits.maxTriangles));
            }
        } else if (tag->name == "item") {
            const auto id = tag->attr("objectid");
            if (!id) return fail("build item without an objectid");
            std::array<double, 12> m{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
            if (const auto tr = tag->attr("transform")) {
                std::string_view s = *tr;
                for (int k = 0; k < 12; ++k) {
                    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
                    std::size_t end = 0;
                    while (end < s.size() && !std::isspace(static_cast<unsigned char>(s[end]))) ++end;
                    if (!parseNumber(s.substr(0, end), m[k])) return fail("bad transform of item '" + std::string(*id) + "'");
                    s.remove_prefix(end);
                }
            }
            items.emplace_back(std::string(*id), m);
        }
    }
    if (!error.empty()) return fail(error);
    if (items.empty()) return fail("the build has no items");
    MeshReadReport report;
    report.encoding = "3mf";
    report.objects = items.size();
    SoupBuilder soup;
    for (const auto& [id, m] : items) {
        const auto it = objects.find(id);
        if (it == objects.end()) return fail("build item refers to a missing object '" + id + "'");
        const Object& o = it->second;
        report.facetsRead += o.triangles.size();
        for (const auto& t : o.triangles) {
            for (std::uint32_t vi : t) {
                const Vec& p = o.vertices[vi];
                // 3MF row-vector convention: p' = [x y z 1] * M (4x3).
                soup.corners.push_back({scale * (p[0] * m[0] + p[1] * m[3] + p[2] * m[6] + m[9]),
                                        scale * (p[0] * m[1] + p[1] * m[4] + p[2] * m[7] + m[10]),
                                        scale * (p[0] * m[2] + p[1] * m[5] + p[2] * m[8] + m[11])});
            }
        }
    }
    if (report.facetsRead == 0) return R::failure(ErrorCode::InvalidInput, "the mesh has no triangle of non-zero area");
    return soup.build(std::move(report));
}

// Closest point of triangle abc to p (Ericson, Real-Time Collision Detection, 5.1.5).
Vec closestOnTriangle(const Vec& p, const Vec& a, const Vec& b, const Vec& c) {
    const Vec ab = sub(b, a), ac = sub(c, a), ap = sub(p, a);
    const double d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const Vec bp = sub(p, b);
    const double d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return add(a, mul(ab, d1 / (d1 - d3)));
    const Vec cp = sub(p, c);
    const double d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return add(a, mul(ac, d2 / (d2 - d6)));
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return add(b, mul(sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    const double denom = 1.0 / (va + vb + vc);
    return add(a, add(mul(ab, vb * denom), mul(ac, vc * denom)));
}

}  // namespace

const char* toString(MeshFileFormat format) noexcept { return format == MeshFileFormat::ThreeMf ? "3mf" : "stl"; }

std::optional<MeshFileFormat> meshFileFormatFromString(std::string_view text) noexcept {
    if (text == "stl") return MeshFileFormat::Stl;
    if (text == "3mf") return MeshFileFormat::ThreeMf;
    return std::nullopt;
}

std::optional<MeshFileFormat> meshFileFormatOfName(std::string_view fileName) noexcept {
    const auto dot = fileName.rfind('.');
    if (dot == std::string_view::npos) return std::nullopt;
    std::string ext(fileName.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return meshFileFormatFromString(ext);
}

Result<ReferenceMeshData> readReferenceMesh(std::string_view data, MeshFileFormat format, const MeshReadLimits& limits) {
    using R = Result<ReferenceMeshData>;
    if (data.empty()) return R::failure(ErrorCode::InvalidInput, "the mesh file is empty");
    if (data.size() > limits.maxBytes) {
        return R::failure(ErrorCode::InvalidInput, "the mesh file is " + megabytes(data.size()) + " MB, the limit is " +
                                                       megabytes(limits.maxBytes) + " MB");
    }
    try {
        return format == MeshFileFormat::ThreeMf ? read3mfMesh(data, limits) : readStlMesh(data, limits);
    } catch (const std::bad_alloc&) {
        return R::failure(ErrorCode::InvalidInput, "the mesh file is " + megabytes(data.size()) + " MB, the limit is " +
                                                       megabytes(limits.maxBytes) + " MB");
    }
}

std::optional<double> meshUnitScale(std::string_view unit) noexcept {
    if (unit == "mm") return 1.0;
    if (unit == "cm") return 10.0;
    if (unit == "m") return 1000.0;
    if (unit == "in") return 25.4;
    return std::nullopt;
}

Result<TriangleMesh> placeMesh(const TriangleMesh& mesh, const MeshPlacement& placement) {
    using R = Result<TriangleMesh>;
    if (!std::isfinite(placement.unitScale) || !(placement.unitScale > 0.0)) {
        return R::failure(ErrorCode::InvalidInput, "mesh scale must be positive");
    }
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(placement.rotationDegrees[k]) || !std::isfinite(placement.translation[k])) {
            return R::failure(ErrorCode::InvalidInput, "mesh placement must be finite");
        }
    }
    try {
        constexpr double kDeg = 3.14159265358979323846 / 180.0;
        gp_Trsf t;
        t.SetTranslation(gp_Vec(placement.translation[0], placement.translation[1], placement.translation[2]));
        const gp_Dir axes[3] = {gp_Dir(1, 0, 0), gp_Dir(0, 1, 0), gp_Dir(0, 0, 1)};
        for (int k = 2; k >= 0; --k) {
            if (placement.rotationDegrees[k] == 0.0) continue;
            gp_Trsf r;
            r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), axes[k]), placement.rotationDegrees[k] * kDeg);
            t.Multiply(r);
        }
        if (placement.unitScale != 1.0) {
            gp_Trsf s;
            s.SetScale(gp_Pnt(0, 0, 0), placement.unitScale);
            t.Multiply(s);
        }
        TriangleMesh out;
        out.triangles = mesh.triangles;
        out.vertices.reserve(mesh.vertices.size());
        for (const Vec& p : mesh.vertices) {
            gp_XYZ x(p[0], p[1], p[2]);
            t.Transforms(x);
            out.vertices.push_back({x.X(), x.Y(), x.Z()});
        }
        return out;
    } catch (const Standard_Failure& e) {
        return R::failure(ErrorCode::OperationFailed, "OCCT exception: " + detail::failureText(e));
    }
}

MeshProperties meshProperties(const TriangleMesh& mesh) {
    MeshProperties p;
    p.vertices = mesh.vertices.size();
    p.triangles = mesh.triangles.size();
    if (!mesh.vertices.empty()) {
        p.min = p.max = mesh.vertices.front();
        for (const Vec& v : mesh.vertices)
            for (int k = 0; k < 3; ++k) {
                p.min[k] = std::min(p.min[k], v[k]);
                p.max[k] = std::max(p.max[k], v[k]);
            }
    }
    for (const auto& t : mesh.triangles)
        p.area += 0.5 * norm(cross(sub(mesh.vertices[t[1]], mesh.vertices[t[0]]), sub(mesh.vertices[t[2]], mesh.vertices[t[0]])));
    p.volume = meshVolume(mesh);
    const MeshClosureReport c = checkClosure(mesh);
    p.openEdges = c.boundaryOrNonManifoldEdges + c.misorientedEdges;
    p.closed = c.closed() && !mesh.triangles.empty();
    return p;
}

Result<MeshPoint> pointOnTriangle(const TriangleMesh& mesh, std::size_t triangle, const Point3& hint, double snapDistance) {
    using R = Result<MeshPoint>;
    if (triangle >= mesh.triangles.size()) {
        return R::failure(ErrorCode::InvalidInput, "mesh triangle " + std::to_string(triangle) + " does not exist");
    }
    for (double c : hint)
        if (!std::isfinite(c)) return R::failure(ErrorCode::InvalidInput, "mesh point must be finite");
    const auto& t = mesh.triangles[triangle];
    MeshPoint out;
    out.triangle = triangle;
    out.point = closestOnTriangle(hint, mesh.vertices[t[0]], mesh.vertices[t[1]], mesh.vertices[t[2]]);
    double best = snapDistance;
    for (std::uint32_t vi : t) {
        const double d = norm(sub(mesh.vertices[vi], out.point));
        if (d <= best) {
            best = d;
            out.vertex = vi;
        }
    }
    if (out.vertex) out.point = mesh.vertices[*out.vertex];
    return out;
}

Result<MeshPoint> nearestPointOnMesh(const TriangleMesh& mesh, const Point3& p) {
    using R = Result<MeshPoint>;
    if (mesh.triangles.empty()) return R::failure(ErrorCode::InvalidInput, "the mesh has no triangles");
    MeshPoint best;
    double bestD = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& t = mesh.triangles[i];
        const Vec q = closestOnTriangle(p, mesh.vertices[t[0]], mesh.vertices[t[1]], mesh.vertices[t[2]]);
        const double d = norm(sub(q, p));
        if (d < bestD) {
            bestD = d;
            best.point = q;
            best.triangle = i;
        }
    }
    return best;
}

std::optional<MeshPoint> raycastMesh(const TriangleMesh& mesh, const Point3& origin, const Point3& direction) {
    std::optional<MeshPoint> hit;
    double bestT = std::numeric_limits<double>::infinity();
    const double dl = norm(direction);
    if (!(dl > 0.0)) return hit;
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& t = mesh.triangles[i];
        const Vec& a = mesh.vertices[t[0]];
        const Vec e1 = sub(mesh.vertices[t[1]], a), e2 = sub(mesh.vertices[t[2]], a);
        const Vec pv = cross(direction, e2);
        const double det = dot(e1, pv);
        if (std::abs(det) < 1e-300) continue;
        const double inv = 1.0 / det;
        const Vec tv = sub(origin, a);
        const double u = dot(tv, pv) * inv;
        if (u < 0.0 || u > 1.0) continue;
        const Vec qv = cross(tv, e1);
        const double v = dot(direction, qv) * inv;
        if (v < 0.0 || u + v > 1.0) continue;
        const double tt = dot(e2, qv) * inv;
        if (tt >= 0.0 && tt < bestT) {
            bestT = tt;
            MeshPoint m;
            m.triangle = i;
            m.point = add(origin, mul(direction, tt));
            hit = m;
        }
    }
    return hit;
}

Result<MeshSection> sectionMesh(const TriangleMesh& mesh, const SectionPlane& plane, double tolerance) {
    using R = Result<MeshSection>;
    const double nl = norm(plane.normal);
    if (!(nl > 1e-12) || !std::isfinite(nl)) return R::failure(ErrorCode::InvalidInput, "section plane normal must not be zero");
    const Vec n = mul(plane.normal, 1.0 / nl);
    const std::uint64_t nv = mesh.vertices.size();
    std::vector<double> dist(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) dist[i] = dot(sub(mesh.vertices[i], plane.origin), n);

    // A crossing point is keyed by the mesh edge (a < b: a * nv + b) or, when it lies on a
    // vertex, by that vertex (v * nv + v), so the segments of neighbouring triangles meet exactly.
    std::unordered_map<std::uint64_t, Vec> pointOf;
    struct Segment {
        std::uint64_t s, e;
    };
    std::vector<Segment> segments;
    auto crossing = [&](std::uint32_t u, std::uint32_t v) -> std::uint64_t {
        const double du = dist[u], dv = dist[v];
        std::uint64_t key;
        Vec p;
        if (du == 0.0) {
            key = u * nv + u;
            p = mesh.vertices[u];
        } else if (dv == 0.0) {
            key = v * nv + v;
            p = mesh.vertices[v];
        } else {
            const std::uint64_t lo = std::min(u, v), hi = std::max(u, v);
            key = lo * nv + hi;
            const double t = du / (du - dv);
            p = add(mesh.vertices[u], mul(sub(mesh.vertices[v], mesh.vertices[u]), t));
        }
        pointOf.try_emplace(key, p);
        return key;
    };
    double area2 = 0.0;
    for (const auto& t : mesh.triangles) {
        const bool pos[3] = {dist[t[0]] >= 0.0, dist[t[1]] >= 0.0, dist[t[2]] >= 0.0};
        if (pos[0] == pos[1] && pos[1] == pos[2]) continue;
        int lone = 0;
        if (pos[1] != pos[0] && pos[1] != pos[2]) lone = 1;
        else if (pos[2] != pos[0] && pos[2] != pos[1]) lone = 2;
        const std::uint32_t L = t[lone], A = t[(lone + 1) % 3], B = t[(lone + 2) % 3];
        std::uint64_t k1 = crossing(L, A), k2 = crossing(L, B);
        if (k1 == k2) continue;
        // Orientation: the material (inside of the triangle's outward normal) on the left of the
        // segment seen from +n, so outer contours run counter-clockwise.
        const Vec fn = cross(sub(mesh.vertices[t[1]], mesh.vertices[t[0]]), sub(mesh.vertices[t[2]], mesh.vertices[t[0]]));
        const Vec d = sub(pointOf[k2], pointOf[k1]);
        if (dot(cross(n, fn), d) < 0.0) std::swap(k1, k2);
        segments.push_back({k1, k2});
        area2 += dot(cross(sub(pointOf[k1], plane.origin), sub(pointOf[k2], plane.origin)), n);
    }
    MeshSection out;
    out.area = 0.5 * area2;
    if (segments.empty()) return out;

    std::unordered_map<std::uint64_t, std::vector<std::size_t>> incident;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        incident[segments[i].s].push_back(i);
        incident[segments[i].e].push_back(i);
    }
    std::vector<bool> used(segments.size(), false);
    auto walk = [&](std::uint64_t startKey, std::size_t firstSeg) {
        MeshSectionContour c;
        std::vector<std::uint64_t> keys{startKey};
        std::uint64_t cur = startKey;
        std::size_t seg = firstSeg;
        for (;;) {
            used[seg] = true;
            cur = segments[seg].s == cur ? segments[seg].e : segments[seg].s;
            if (cur == startKey) {
                c.closed = true;
                break;
            }
            keys.push_back(cur);
            std::optional<std::size_t> nextSeg;
            for (std::size_t s : incident[cur])
                if (!used[s]) {
                    nextSeg = s;
                    break;
                }
            if (!nextSeg) break;
            seg = *nextSeg;
        }
        for (std::uint64_t k : keys) c.points.push_back(pointOf[k]);
        return c;
    };
    // Open chains first (from keys used by one segment), then the loops.
    std::vector<std::uint64_t> ends;
    for (const auto& [key, segs] : incident)
        if (segs.size() % 2 == 1) ends.push_back(key);
    std::sort(ends.begin(), ends.end());
    std::vector<MeshSectionContour> contours;
    for (std::uint64_t key : ends) {
        for (std::size_t s : incident[key])
            if (!used[s]) {
                contours.push_back(walk(key, s));
                break;
            }
    }
    for (std::size_t i = 0; i < segments.size(); ++i)
        if (!used[i]) contours.push_back(walk(segments[i].s, i));

    // Drop points on the line of their neighbours.
    const double tol = std::max(tolerance, 0.0);
    for (MeshSectionContour& c : contours) {
        auto offLine = [&](const Vec& a, const Vec& p, const Vec& b) {
            const Vec ab = sub(b, a);
            const double l = norm(ab);
            if (!(l > 0.0)) return norm(sub(p, a)) > tol;
            return norm(cross(ab, sub(p, a))) / l > tol;
        };
        bool changed = true;
        while (changed && c.points.size() > (c.closed ? 3u : 2u)) {
            changed = false;
            std::vector<Vec> kept;
            const std::size_t m = c.points.size();
            for (std::size_t i = 0; i < m; ++i) {
                const bool endpoint = !c.closed && (i == 0 || i + 1 == m);
                const Vec& prev = kept.empty() ? c.points[(i + m - 1) % m] : kept.back();
                const Vec& next = c.points[(i + 1) % m];
                if (endpoint || offLine(prev, c.points[i], next)) kept.push_back(c.points[i]);
                else changed = true;
            }
            if (kept.size() < (c.closed ? 3u : 2u)) break;
            c.points = std::move(kept);
        }
        for (std::size_t i = 0; i + 1 < c.points.size(); ++i) out.length += norm(sub(c.points[i + 1], c.points[i]));
        if (c.closed && c.points.size() > 1) out.length += norm(sub(c.points.front(), c.points.back()));
    }
    out.contours = std::move(contours);
    return out;
}

}  // namespace bendcore::kernel
