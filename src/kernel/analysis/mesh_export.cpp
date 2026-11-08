#include <bendcore/kernel/mesh_export.hpp>
#include <bendcore/kernel/tessellation.hpp>

#include "../occt_failure.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>
#include <utility>

#include <Standard_Failure.hxx>

#include <zlib.h>

static_assert(std::endian::native == std::endian::little,
              "STL and ZIP writers copy IEEE floats and integers in little-endian byte order");

namespace bendcore::kernel {

namespace {

using Vec = std::array<double, 3>;

Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec cross(const Vec& a, const Vec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

Vec unitNormal(const Vec& a, const Vec& b, const Vec& c) {
    Vec n = cross(sub(b, a), sub(c, a));
    const double len = std::sqrt(dot(n, n));
    if (len > 0.0)
        for (double& x : n) x /= len;
    return n;
}

// Spatial hash welding: a point joins an existing vertex within `tol` in the 27 cells around it.
class Welder {
public:
    explicit Welder(double tol) : tol_(tol), cell_(std::max(tol, 1.0e-12) * 2.0) {}

    std::uint32_t add(const Vec& p, std::vector<Vec>& out) {
        const std::array<long long, 3> key = cellOf(p);
        for (long long dx = -1; dx <= 1; ++dx)
            for (long long dy = -1; dy <= 1; ++dy)
                for (long long dz = -1; dz <= 1; ++dz) {
                    auto it = cells_.find(hash({key[0] + dx, key[1] + dy, key[2] + dz}));
                    if (it == cells_.end()) continue;
                    for (std::uint32_t idx : it->second) {
                        const Vec d = sub(out[idx], p);
                        if (dot(d, d) <= tol_ * tol_) return idx;
                    }
                }
        const auto idx = static_cast<std::uint32_t>(out.size());
        out.push_back(p);
        cells_[hash(key)].push_back(idx);
        return idx;
    }

private:
    std::array<long long, 3> cellOf(const Vec& p) const {
        return {static_cast<long long>(std::floor(p[0] / cell_)), static_cast<long long>(std::floor(p[1] / cell_)),
                static_cast<long long>(std::floor(p[2] / cell_))};
    }
    static std::uint64_t hash(const std::array<long long, 3>& k) {
        std::uint64_t h = 1469598103934665603ull;
        for (long long v : k) {
            h ^= static_cast<std::uint64_t>(v);
            h *= 1099511628211ull;
        }
        return h;
    }
    double tol_;
    double cell_;
    // Hash collisions only add candidates; the distance test decides.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells_;
};

void appendLe32(std::string& out, std::uint32_t v) {
    char b[4];
    std::memcpy(b, &v, 4);
    out.append(b, 4);
}
void appendLe16(std::string& out, std::uint16_t v) {
    char b[2];
    std::memcpy(b, &v, 2);
    out.append(b, 2);
}
void appendFloat(std::string& out, float v) {
    char b[4];
    std::memcpy(b, &v, 4);
    out.append(b, 4);
}
std::uint32_t readLe32(std::string_view s, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, s.data() + at, 4);
    return v;
}
std::uint16_t readLe16(std::string_view s, std::size_t at) {
    std::uint16_t v;
    std::memcpy(&v, s.data() + at, 2);
    return v;
}

std::string formatNumber(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", v);
    return buf;
}

std::string xmlEscape(std::string_view text) {
    std::string out;
    for (char ch : text) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += ch;
        }
    }
    return out;
}

std::uint32_t crc32Of(std::string_view data) {
    uLong crc = crc32(0L, Z_NULL, 0);
    return static_cast<std::uint32_t>(
        crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size())));
}

Result<std::string> deflateRaw(std::string_view data) {
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib deflateInit2 failed");
    std::string out(deflateBound(&zs, static_cast<uLong>(data.size())), '\0');
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = deflate(&zs, Z_FINISH);
    const auto produced = zs.total_out;
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib deflate failed");
    out.resize(produced);
    return out;
}

Result<std::string> inflateRaw(std::string_view data, std::size_t expectedSize) {
    z_stream zs{};
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
        return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib inflateInit2 failed");
    std::string out(expectedSize, '\0');
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = inflate(&zs, Z_FINISH);
    const auto produced = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || produced != expectedSize)
        return Result<std::string>::failure(ErrorCode::ParseError, "zip: corrupt deflate stream");
    return out;
}

// ZIP with deflated entries. All entries carry the DOS date 1980-01-01 00:00 (the earliest
// representable), so that the same model always produces byte-identical packages.
Result<std::string> writeZip(const std::vector<ZipEntry>& entries) {
    constexpr std::uint16_t kDosTime = 0;
    constexpr std::uint16_t kDosDate = (0 << 9) | (1 << 5) | 1;
    std::string out;
    std::string central;
    for (const ZipEntry& e : entries) {
        auto packed = deflateRaw(e.data);
        if (!packed) return packed.error();
        const std::uint32_t crc = crc32Of(e.data);
        const auto offset = static_cast<std::uint32_t>(out.size());
        appendLe32(out, 0x04034b50);
        appendLe16(out, 20);  // version needed: 2.0 (deflate)
        appendLe16(out, 0);   // flags
        appendLe16(out, 8);   // deflate
        appendLe16(out, kDosTime);
        appendLe16(out, kDosDate);
        appendLe32(out, crc);
        appendLe32(out, static_cast<std::uint32_t>(packed.value().size()));
        appendLe32(out, static_cast<std::uint32_t>(e.data.size()));
        appendLe16(out, static_cast<std::uint16_t>(e.name.size()));
        appendLe16(out, 0);
        out += e.name;
        out += packed.value();

        appendLe32(central, 0x02014b50);
        appendLe16(central, 20);  // made by: MS-DOS / FAT, 2.0
        appendLe16(central, 20);
        appendLe16(central, 0);
        appendLe16(central, 8);
        appendLe16(central, kDosTime);
        appendLe16(central, kDosDate);
        appendLe32(central, crc);
        appendLe32(central, static_cast<std::uint32_t>(packed.value().size()));
        appendLe32(central, static_cast<std::uint32_t>(e.data.size()));
        appendLe16(central, static_cast<std::uint16_t>(e.name.size()));
        appendLe16(central, 0);  // extra
        appendLe16(central, 0);  // comment
        appendLe16(central, 0);  // disk
        appendLe16(central, 0);  // internal attributes
        appendLe32(central, 0);  // external attributes
        appendLe32(central, offset);
        central += e.name;
    }
    if (out.size() + central.size() > 0xFFFFFFFFull || entries.size() > 0xFFFF)
        return Result<std::string>::failure(ErrorCode::Unsupported, "zip: archive needs ZIP64");
    const auto centralOffset = static_cast<std::uint32_t>(out.size());
    out += central;
    appendLe32(out, 0x06054b50);
    appendLe16(out, 0);
    appendLe16(out, 0);
    appendLe16(out, static_cast<std::uint16_t>(entries.size()));
    appendLe16(out, static_cast<std::uint16_t>(entries.size()));
    appendLe32(out, static_cast<std::uint32_t>(central.size()));
    appendLe32(out, centralOffset);
    appendLe16(out, 0);
    return out;
}

template <typename T>
Result<T> guard(auto&& body) {
    try {
        return body();
    } catch (const Standard_Failure& failure) {
        return Result<T>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
    } catch (const std::exception& ex) {
        return Result<T>::failure(ErrorCode::OperationFailed, ex.what());
    }
}

}  // namespace

Result<TriangleMesh> meshForExport(const Shape& shape, const MeshExportParams& params, double weldTolerance) {
    if (shape.isNull()) return Result<TriangleMesh>::failure(ErrorCode::InvalidInput, "shape is null");
    if (!(params.linearDeflection > 0.0) || !(params.angularDeflection > 0.0))
        return Result<TriangleMesh>::failure(ErrorCode::InvalidInput, "deflections must be positive");
    if (!(weldTolerance >= 0.0))
        return Result<TriangleMesh>::failure(ErrorCode::InvalidInput, "weld tolerance must be non-negative");
    TessellationParams tp;
    tp.linearDeflection = params.linearDeflection;
    tp.angularDeflection = params.angularDeflection;
    tp.relative = params.relative;
    tp.requireAllFaces = true;
    auto mesh = tessellate(shape, tp);
    if (!mesh) return mesh.error();
    const Mesh& m = mesh.value();
    if (m.indices.empty()) return Result<TriangleMesh>::failure(ErrorCode::InvalidShape, "shape has no faces to mesh");

    TriangleMesh out;
    Welder welder(weldTolerance);
    std::vector<std::uint32_t> remap(m.positions.size() / 3);
    for (std::size_t i = 0; i < remap.size(); ++i)
        remap[i] = welder.add({m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]}, out.vertices);
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::array<std::uint32_t, 3> tri{remap[m.indices[t]], remap[m.indices[t + 1]], remap[m.indices[t + 2]]};
        if (tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2]) continue;
        out.triangles.push_back(tri);
    }
    return out;
}

Result<std::string> writeStlBinary(const TriangleMesh& mesh, std::string_view header) {
    if (mesh.triangles.size() > 0xFFFFFFFFull)
        return Result<std::string>::failure(ErrorCode::Unsupported, "too many triangles for STL");
    std::string out(80, ' ');
    // A binary header must not start with "solid", or readers take the file for ASCII.
    std::string head(header.substr(0, 80));
    if (head.rfind("solid", 0) == 0) head[0] = 'S';
    std::memcpy(out.data(), head.data(), head.size());
    appendLe32(out, static_cast<std::uint32_t>(mesh.triangles.size()));
    out.reserve(out.size() + 50 * mesh.triangles.size());
    for (const auto& t : mesh.triangles) {
        const Vec& a = mesh.vertices[t[0]];
        const Vec& b = mesh.vertices[t[1]];
        const Vec& c = mesh.vertices[t[2]];
        const Vec n = unitNormal(a, b, c);
        for (double x : n) appendFloat(out, static_cast<float>(x));
        for (const Vec* p : {&a, &b, &c})
            for (double x : *p) appendFloat(out, static_cast<float>(x));
        appendLe16(out, 0);
    }
    return out;
}

Result<std::string> writeStlAscii(const TriangleMesh& mesh, std::string_view solidName) {
    for (char ch : solidName)
        if (std::isspace(static_cast<unsigned char>(ch)))
            return Result<std::string>::failure(ErrorCode::InvalidInput, "STL solid name must not contain spaces");
    std::string out = "solid " + std::string(solidName) + "\n";
    for (const auto& t : mesh.triangles) {
        const Vec& a = mesh.vertices[t[0]];
        const Vec& b = mesh.vertices[t[1]];
        const Vec& c = mesh.vertices[t[2]];
        const Vec n = unitNormal(a, b, c);
        out += "  facet normal " + formatNumber(n[0]) + " " + formatNumber(n[1]) + " " + formatNumber(n[2]) + "\n";
        out += "    outer loop\n";
        for (const Vec* p : {&a, &b, &c})
            out += "      vertex " + formatNumber((*p)[0]) + " " + formatNumber((*p)[1]) + " " + formatNumber((*p)[2]) +
                   "\n";
        out += "    endloop\n  endfacet\n";
    }
    out += "endsolid " + std::string(solidName) + "\n";
    return out;
}

Result<std::string> exportStlBinary(const Shape& shape, const MeshExportParams& params) {
    auto mesh = meshForExport(shape, params);
    if (!mesh) return mesh.error();
    return writeStlBinary(mesh.value());
}

Result<std::string> exportStlAscii(const Shape& shape, const MeshExportParams& params, std::string_view solidName) {
    auto mesh = meshForExport(shape, params);
    if (!mesh) return mesh.error();
    return writeStlAscii(mesh.value(), solidName);
}

Result<std::vector<StlFacet>> readStl(std::string_view data) {
    using R = Result<std::vector<StlFacet>>;
    if (data.size() >= 84) {
        const std::uint32_t n = readLe32(data, 80);
        if (84ull + 50ull * n == data.size()) {
            std::vector<StlFacet> facets(n);
            for (std::uint32_t i = 0; i < n; ++i) {
                const std::size_t at = 84 + 50ull * i;
                std::memcpy(facets[i].normal.data(), data.data() + at, 12);
                for (int v = 0; v < 3; ++v) std::memcpy(facets[i].vertices[v].data(), data.data() + at + 12 + 12 * v, 12);
            }
            return facets;
        }
    }
    // ASCII
    std::size_t pos = 0;
    std::size_t line = 1;
    auto nextToken = [&]() -> std::string_view {
        while (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
            if (data[pos] == '\n') ++line;
            ++pos;
        }
        const std::size_t start = pos;
        while (pos < data.size() && !std::isspace(static_cast<unsigned char>(data[pos]))) ++pos;
        return data.substr(start, pos - start);
    };
    auto skipLine = [&]() {
        while (pos < data.size() && data[pos] != '\n') ++pos;
    };
    auto fail = [&](const std::string& what) { return R::failure(ErrorCode::ParseError, "STL line " + std::to_string(line) + ": " + what); };
    auto readFloat = [&](float& out) -> bool {
        std::string_view tok = nextToken();
        if (tok.empty()) return false;
        const auto res = std::from_chars(tok.data(), tok.data() + tok.size(), out);
        return res.ec == std::errc() && res.ptr == tok.data() + tok.size();
    };
    auto expect = [&](std::string_view word) { return nextToken() == word; };

    if (nextToken() != "solid") return R::failure(ErrorCode::ParseError, "STL: neither binary layout nor 'solid' keyword");
    skipLine();  // solid name
    std::vector<StlFacet> facets;
    for (;;) {
        std::string_view tok = nextToken();
        if (tok == "endsolid") return facets;
        if (tok != "facet") return fail(tok.empty() ? "unexpected end, 'endsolid' missing" : "expected 'facet'");
        StlFacet f;
        if (!expect("normal")) return fail("expected 'normal'");
        for (float& c : f.normal)
            if (!readFloat(c)) return fail("bad normal component");
        if (!expect("outer") || !expect("loop")) return fail("expected 'outer loop'");
        for (auto& v : f.vertices) {
            if (!expect("vertex")) return fail("expected 'vertex'");
            for (float& c : v)
                if (!readFloat(c)) return fail("bad vertex coordinate");
        }
        if (!expect("endloop")) return fail("expected 'endloop'");
        if (!expect("endfacet")) return fail("expected 'endfacet'");
        facets.push_back(f);
    }
}

double meshVolume(const std::vector<StlFacet>& facets) {
    double v = 0.0;
    for (const StlFacet& f : facets) {
        const Vec a{f.vertices[0][0], f.vertices[0][1], f.vertices[0][2]};
        const Vec b{f.vertices[1][0], f.vertices[1][1], f.vertices[1][2]};
        const Vec c{f.vertices[2][0], f.vertices[2][1], f.vertices[2][2]};
        v += dot(a, cross(b, c));
    }
    return v / 6.0;
}

double meshVolume(const TriangleMesh& mesh) {
    double v = 0.0;
    for (const auto& t : mesh.triangles)
        v += dot(mesh.vertices[t[0]], cross(mesh.vertices[t[1]], mesh.vertices[t[2]]));
    return v / 6.0;
}

MeshClosureReport checkClosure(const TriangleMesh& mesh) {
    // Undirected edge -> (uses, forward uses), forward meaning traversal from lower to higher index.
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<int, int>> edges;
    for (const auto& t : mesh.triangles)
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t a = t[k];
            const std::uint32_t b = t[(k + 1) % 3];
            auto& e = edges[{std::min(a, b), std::max(a, b)}];
            ++e.first;
            if (a < b) ++e.second;
        }
    MeshClosureReport report;
    for (const auto& [key, use] : edges) {
        if (use.first != 2)
            ++report.boundaryOrNonManifoldEdges;
        else if (use.second != 1)
            ++report.misorientedEdges;
    }
    return report;
}

Result<std::string> export3mf(const std::vector<ThreeMfObject>& objects, const MeshExportParams& params) {
    if (objects.empty()) return Result<std::string>::failure(ErrorCode::InvalidInput, "3MF: no objects");
    return guard<std::string>([&]() -> Result<std::string> {
        std::vector<TriangleMesh> meshes;
        for (const ThreeMfObject& o : objects) {
            auto mesh = meshForExport(o.shape, params);
            if (!mesh) return Error{mesh.error().code, "3MF object '" + o.name + "': " + mesh.error().message};
            meshes.push_back(std::move(mesh).value());
        }
        std::string model;
        model += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        model += "<model unit=\"millimeter\" xml:lang=\"en-US\" "
                 "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n";
        model += " <metadata name=\"Application\">bendcore</metadata>\n";
        model += " <resources>\n";
        // Base materials (id 1): one entry per coloured object.
        std::vector<int> colorIndex(objects.size(), -1);
        int nextColor = 0;
        std::string materials;
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (!objects[i].color) continue;
            const Rgb c = *objects[i].color;
            char hex[16];
            std::snprintf(hex, sizeof hex, "#%02X%02X%02XFF", c.r, c.g, c.b);
            materials += "  <base name=\"" + xmlEscape(objects[i].name) + "\" displaycolor=\"" + hex + "\"/>\n";
            colorIndex[i] = nextColor++;
        }
        const int materialsId = 1;
        if (nextColor > 0) model += "  <basematerials id=\"1\">\n" + materials + "  </basematerials>\n";
        for (std::size_t i = 0; i < objects.size(); ++i) {
            const int id = static_cast<int>(i) + 2;
            model += "  <object id=\"" + std::to_string(id) + "\" type=\"model\" name=\"" + xmlEscape(objects[i].name) + "\"";
            if (colorIndex[i] >= 0)
                model += " pid=\"" + std::to_string(materialsId) + "\" pindex=\"" + std::to_string(colorIndex[i]) + "\"";
            model += ">\n   <mesh>\n    <vertices>\n";
            for (const Vec& v : meshes[i].vertices)
                model += "     <vertex x=\"" + formatNumber(v[0]) + "\" y=\"" + formatNumber(v[1]) + "\" z=\"" +
                         formatNumber(v[2]) + "\"/>\n";
            model += "    </vertices>\n    <triangles>\n";
            for (const auto& t : meshes[i].triangles)
                model += "     <triangle v1=\"" + std::to_string(t[0]) + "\" v2=\"" + std::to_string(t[1]) + "\" v3=\"" +
                         std::to_string(t[2]) + "\"/>\n";
            model += "    </triangles>\n   </mesh>\n  </object>\n";
        }
        model += " </resources>\n <build>\n";
        for (std::size_t i = 0; i < objects.size(); ++i)
            model += "  <item objectid=\"" + std::to_string(i + 2) + "\"/>\n";
        model += " </build>\n</model>\n";

        const std::string contentTypes =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
            " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
            " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
            "</Types>\n";
        const std::string rels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            " <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
            "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
            "</Relationships>\n";
        return writeZip({{"[Content_Types].xml", contentTypes}, {"_rels/.rels", rels}, {"3D/3dmodel.model", model}});
    });
}

Result<std::vector<ZipEntry>> readZip(std::string_view z) {
    using R = Result<std::vector<ZipEntry>>;
    if (z.size() < 22) return R::failure(ErrorCode::ParseError, "zip: too short");
    std::size_t eocd = std::string_view::npos;
    const std::size_t lowest = z.size() >= 22 + 0xFFFF ? z.size() - 22 - 0xFFFF : 0;
    for (std::size_t at = z.size() - 22 + 1; at-- > lowest;) {
        if (readLe32(z, at) == 0x06054b50) {
            eocd = at;
            break;
        }
    }
    if (eocd == std::string_view::npos) return R::failure(ErrorCode::ParseError, "zip: end of central directory not found");
    const std::uint16_t count = readLe16(z, eocd + 10);
    const std::uint32_t cdSize = readLe32(z, eocd + 12);
    std::size_t cd = readLe32(z, eocd + 16);
    if (cd + cdSize > eocd) return R::failure(ErrorCode::ParseError, "zip: central directory out of range");
    std::vector<ZipEntry> entries;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (cd + 46 > eocd || readLe32(z, cd) != 0x02014b50)
            return R::failure(ErrorCode::ParseError, "zip: bad central directory header");
        const std::uint16_t flags = readLe16(z, cd + 8);
        const std::uint16_t method = readLe16(z, cd + 10);
        const std::uint32_t crc = readLe32(z, cd + 16);
        const std::uint32_t csize = readLe32(z, cd + 20);
        const std::uint32_t usize = readLe32(z, cd + 24);
        const std::uint16_t nameLen = readLe16(z, cd + 28);
        const std::uint16_t extraLen = readLe16(z, cd + 30);
        const std::uint16_t commentLen = readLe16(z, cd + 32);
        const std::uint32_t local = readLe32(z, cd + 42);
        if (cd + 46 + nameLen > eocd) return R::failure(ErrorCode::ParseError, "zip: name out of range");
        ZipEntry e;
        e.name = std::string(z.substr(cd + 46, nameLen));
        if (flags & 0x1) return R::failure(ErrorCode::Unsupported, "zip: encrypted entry " + e.name);
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local == 0xFFFFFFFFu)
            return R::failure(ErrorCode::Unsupported, "zip: ZIP64 entry " + e.name);
        if (local + 30 > z.size() || readLe32(z, local) != 0x04034b50)
            return R::failure(ErrorCode::ParseError, "zip: bad local header of " + e.name);
        const std::size_t dataAt = local + 30 + readLe16(z, local + 26) + readLe16(z, local + 28);
        if (dataAt + csize > z.size()) return R::failure(ErrorCode::ParseError, "zip: data of " + e.name + " out of range");
        const std::string_view packed = z.substr(dataAt, csize);
        if (method == 0) {
            if (csize != usize) return R::failure(ErrorCode::ParseError, "zip: stored size mismatch in " + e.name);
            e.data = std::string(packed);
        } else if (method == 8) {
            auto data = inflateRaw(packed, usize);
            if (!data) return Error{data.error().code, data.error().message + " (" + e.name + ")"};
            e.data = std::move(data).value();
        } else {
            return R::failure(ErrorCode::Unsupported, "zip: compression method " + std::to_string(method) + " in " + e.name);
        }
        if (crc32Of(e.data) != crc) return R::failure(ErrorCode::ParseError, "zip: CRC-32 mismatch in " + e.name);
        entries.push_back(std::move(e));
        cd += 46 + nameLen + extraLen + commentLen;
    }
    return entries;
}

}  // namespace bendcore::kernel
