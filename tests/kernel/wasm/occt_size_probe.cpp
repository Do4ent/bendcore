// OCCT size/function probe for the WebAssembly build (docs/kernel/OCCT_BUILD.md).
//
// Exercises the path the browser kernel needs end to end, entirely in memory:
//   1. builds a solid from primitives: box 40x30x20 minus a through hole of radius 5
//      (BRepPrimAPI + BRepAlgoAPI_Cut);
//   2. writes it as STEP AP214 to a std::ostringstream (STEPControl_Writer::WriteStream);
//   3. reads the STEP text back from a memory buffer (STEPControl_Reader::ReadStream);
//   4. checks the read solid against analytic values (volume, face count);
//   5. tessellates it (BRepMesh_IncrementalMesh), prints the triangle count and checks
//      the mesh area against the analytic surface area.
// Exit code 0 only if every check passes. Built natively and with Emscripten by
// third_party/build_occt_wasm.sh; the .wasm size of this program is the measured
// lower bound of the kernel download.

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_PrinterOStream.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

namespace {

constexpr double kBoxX = 40.0;
constexpr double kBoxY = 30.0;
constexpr double kBoxZ = 20.0;
constexpr double kHoleRadius = 5.0;
constexpr double kPi = 3.14159265358979323846;

int Fail(const char* what) {
  std::printf("FAIL: %s\n", what);
  return 1;
}

int CountFaces(const TopoDS_Shape& shape) {
  int n = 0;
  for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) ++n;
  return n;
}

int Run() {
  std::printf("OCCT %s\n", OCC_VERSION_COMPLETE);
  // Translator statistics go to the default messenger (stdout); keep the output to the
  // probe's own lines.
  Message::DefaultMessenger()->RemovePrinters(STANDARD_TYPE(Message_PrinterOStream));

  // 1. Primitive solid: box with a vertical through hole in its centre.
  TopoDS_Shape box = BRepPrimAPI_MakeBox(kBoxX, kBoxY, kBoxZ).Shape();
  // The cylinder sticks out of the box on both sides so the cut is a clean through hole.
  gp_Ax2 axis(gp_Pnt(kBoxX / 2, kBoxY / 2, -1.0), gp_Dir(0, 0, 1));
  TopoDS_Shape tool = BRepPrimAPI_MakeCylinder(axis, kHoleRadius, kBoxZ + 2.0).Shape();
  BRepAlgoAPI_Cut cut(box, tool);
  if (!cut.IsDone()) return Fail("BRepAlgoAPI_Cut did not finish");
  const TopoDS_Shape part = cut.Shape();

  // 2. STEP into memory.
  STEPControl_Writer writer;
  if (writer.Transfer(part, STEPControl_AsIs) != IFSelect_RetDone)
    return Fail("STEPControl_Writer::Transfer");
  std::ostringstream out;
  if (writer.WriteStream(out) != IFSelect_RetDone) return Fail("STEPControl_Writer::WriteStream");
  const std::string step = out.str();
  std::printf("STEP bytes: %zu\n", step.size());
  if (step.rfind("ISO-10303-21;", 0) != 0) return Fail("STEP text has no ISO-10303-21 header");

  // 3. STEP from the memory buffer.
  std::istringstream in(step);
  STEPControl_Reader reader;
  if (reader.ReadStream("probe.step", in) != IFSelect_RetDone)
    return Fail("STEPControl_Reader::ReadStream");
  if (reader.TransferRoots() <= 0) return Fail("STEPControl_Reader::TransferRoots");
  const TopoDS_Shape read = reader.OneShape();
  if (read.IsNull()) return Fail("read shape is null");

  // 4. Analytic checks on what came back from STEP.
  if (!BRepCheck_Analyzer(read).IsValid()) return Fail("BRepCheck_Analyzer: read shape invalid");
  GProp_GProps props;
  BRepGProp::VolumeProperties(read, props);
  const double volume = props.Mass();
  const double expected = kBoxX * kBoxY * kBoxZ - kPi * kHoleRadius * kHoleRadius * kBoxZ;
  std::printf("volume: %.6f (analytic %.6f)\n", volume, expected);
  if (std::fabs(volume - expected) > 1e-6 * expected) return Fail("volume differs from analytic");
  // 6 box faces + 1 cylindrical hole face; OCCT keeps the periodic cylinder as one face.
  const int faces = CountFaces(read);
  std::printf("faces: %d\n", faces);
  if (faces != 7) return Fail("expected 7 faces (6 planar + 1 cylindrical)");

  // 5. Tessellation.
  const double linearDeflection = 0.1;
  const double angularDeflection = 0.5;
  BRepMesh_IncrementalMesh mesh(read, linearDeflection, false, angularDeflection, false);
  if (!mesh.IsDone()) return Fail("BRepMesh_IncrementalMesh did not finish");
  long triangles = 0;
  long nodes = 0;
  double meshArea = 0.0;
  for (TopExp_Explorer ex(read, TopAbs_FACE); ex.More(); ex.Next()) {
    TopLoc_Location loc;
    const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(ex.Current()), loc);
    if (tri.IsNull()) return Fail("face without triangulation after BRepMesh");
    triangles += tri->NbTriangles();
    nodes += tri->NbNodes();
    for (int i = 1; i <= tri->NbTriangles(); ++i) {
      int a = 0, b = 0, c = 0;
      tri->Triangle(i).Get(a, b, c);
      const gp_Vec ab(tri->Node(a), tri->Node(b));
      const gp_Vec ac(tri->Node(a), tri->Node(c));
      meshArea += 0.5 * ab.Crossed(ac).Magnitude();
    }
  }
  std::printf("triangles: %ld\nnodes: %ld\n", triangles, nodes);
  if (triangles == 0) return Fail("no triangles");
  // Mesh area vs analytic surface area. Planar faces are exact; the hole wall is an
  // inscribed polygon, so the mesh area is slightly smaller, bounded by the deflection.
  const double analyticArea = 2 * (kBoxX * kBoxY - kPi * kHoleRadius * kHoleRadius) +
                              2 * (kBoxX + kBoxY) * kBoxZ + 2 * kPi * kHoleRadius * kBoxZ;
  std::printf("mesh area: %.6f (analytic %.6f)\n", meshArea, analyticArea);
  if (std::fabs(meshArea - analyticArea) > 1e-3 * analyticArea)
    return Fail("mesh area differs from analytic surface area by more than 0.1%");
  std::printf("OK\n");
  return 0;
}

}  // namespace

int main() {
  try {
    return Run();
  } catch (const Standard_Failure& e) {
    std::printf("FAIL: OCCT exception: %s\n", e.what());
    return 1;
  }
}
