#pragma once

// Mesh export from the kernel tessellation: STL (binary and ASCII) and 3MF (ZIP package with
// the 3D model XML, millimetres, colours, several objects), plus an STL reader and mesh
// checks used to verify the output (docs/kernel/ANALYSIS.md).

#include <bendcore/kernel/material.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::kernel {

struct MeshExportParams {
    double linearDeflection = 0.05;  // mm (or a fraction of the size when relative)
    double angularDeflection = 0.35; // radians
    bool relative = false;
};

// Indexed triangle mesh in mm. Vertices of one object are welded, so that a closed solid
// yields a closed mesh (every edge shared by exactly two triangles).
struct TriangleMesh {
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;  // counter-clockwise seen from outside
};

// Tessellates `shape` (BRepMesh through kernel::tessellate) and welds coincident vertices
// (distance <= weldTolerance, mm); triangles that collapse after welding are dropped.
Result<TriangleMesh> meshForExport(const Shape& shape, const MeshExportParams& params = {},
                                   double weldTolerance = 1.0e-6);

// Binary STL: 80-byte header, uint32 count, 50 bytes per facet (normal, 3 vertices, 0).
Result<std::string> writeStlBinary(const TriangleMesh& mesh, std::string_view header = "bendcore");
Result<std::string> writeStlAscii(const TriangleMesh& mesh, std::string_view solidName = "bendcore");
Result<std::string> exportStlBinary(const Shape& shape, const MeshExportParams& params = {});
Result<std::string> exportStlAscii(const Shape& shape, const MeshExportParams& params = {},
                                   std::string_view solidName = "bendcore");

struct StlFacet {
    std::array<float, 3> normal{};
    std::array<std::array<float, 3>, 3> vertices{};
};

// Reads binary or ASCII STL (detected from the size rule of the binary layout, then the
// "solid" keyword). ParseError with the offset or line of the first problem.
Result<std::vector<StlFacet>> readStl(std::string_view data);

// Signed volume of a triangle soup by the divergence theorem, mm^3.
double meshVolume(const std::vector<StlFacet>& facets);
double meshVolume(const TriangleMesh& mesh);

// Number of undirected edges not shared by exactly two triangles, and of edges whose two
// triangles traverse them in the same direction (inconsistent orientation).
struct MeshClosureReport {
    std::size_t boundaryOrNonManifoldEdges = 0;
    std::size_t misorientedEdges = 0;
    bool closed() const noexcept { return boundaryOrNonManifoldEdges == 0 && misorientedEdges == 0; }
};
MeshClosureReport checkClosure(const TriangleMesh& mesh);

struct ThreeMfObject {
    std::string name;
    Shape shape;
    std::optional<Rgb> color;  // written as a base material displaycolor
};

struct ZipEntry {
    std::string name;
    std::string data;
};

// Compression of ZIP entries. Deflate needs zlib, which only the native build links: the
// WebAssembly build has no zlib (the Emscripten port is downloaded from the network at build
// time), so it writes stored entries (docs/kernel/ANALYSIS.md, "ZIP и zlib").
enum class ZipMethod : std::uint16_t { Stored = 0, Deflate = 8 };

// true when this build links zlib (deflate on write, inflate on read).
bool zipDeflateAvailable() noexcept;
// Deflate where available, Stored otherwise; export3mf writes with this method.
ZipMethod zipDefaultMethod() noexcept;

// CRC-32 of ZIP (ISO 3309 / ITU-T V.42: reflected polynomial 0xEDB88320, initial value and
// final XOR 0xFFFFFFFF), table-driven, independent of zlib. `crc` continues a previous value.
std::uint32_t crc32(std::string_view data, std::uint32_t crc = 0) noexcept;

// ZIP archive (no ZIP64): every entry with the given method, CRC-32 of the uncompressed
// bytes, DOS date 1980-01-01 00:00 so that equal input gives byte-identical archives.
// Unsupported for Deflate without zlib and for archives that would need ZIP64.
Result<std::string> writeZip(const std::vector<ZipEntry>& entries, ZipMethod method);

// 3MF core specification 1.x package: [Content_Types].xml, _rels/.rels, 3D/3dmodel.model;
// unit="millimeter", one <object type="model"> and one build <item> per input object.
// ZIP entries use zipDefaultMethod() (deflate natively, stored in WebAssembly).
Result<std::string> export3mf(const std::vector<ThreeMfObject>& objects, const MeshExportParams& params = {});

// Minimal ZIP reader for the packages written here and by other tools (stored or deflated
// entries, no ZIP64, no encryption): entry name -> uncompressed bytes, CRC-32 checked.
// A deflated entry without zlib in the build is Unsupported.
Result<std::vector<ZipEntry>> readZip(std::string_view archive);

}  // namespace bendcore::kernel
