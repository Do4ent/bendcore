// Element map and reference resolution on raw OCCT operations (no document).

#include "modeling_document_test_util.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

#include <set>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

NamedShape namedBox(double l, double w, double h, const std::string& id = "B") {
    BRepPrimAPI_MakeBox mk(l, w, h);
    mk.Build();
    NamingRequest req;
    req.featureId = id;
    req.result = mk.Shape();
    // Front = YMin, Back = YMax, Left = XMin, Right = XMax (OCCT's accessors name them differently).
    req.roles = {{mk.TopFace(), "Top"},    {mk.BottomFace(), "Bottom"}, {mk.LeftFace(), "Front"},
                 {mk.RightFace(), "Back"}, {mk.BackFace(), "Left"},     {mk.FrontFace(), "Right"}};
    auto r = buildNamedShape(req);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return std::move(r).value();
}

NamedShape fillet(const NamedShape& in, const TopoDS_Shape& edge, double r, const std::string& id) {
    BRepFilletAPI_MakeFillet mk(in.shape());
    mk.Add(r, TopoDS::Edge(edge));
    mk.Build();
    EXPECT_TRUE(mk.IsDone());
    NCollection_List<TopoDS_Shape> args;
    args.Append(in.shape());
    NamingRequest req;
    req.featureId = id;
    req.result = mk.Shape();
    req.history = new BRepTools_History(args, mk);
    req.inputs = {&in};
    auto out = buildNamedShape(req);
    EXPECT_TRUE(out.ok()) << (out.ok() ? "" : out.error().message);
    return std::move(out).value();
}

std::string nameOf(const NamedShape& s, const TopoDS_Shape& e) {
    auto k = elementKindOf(e);
    auto i = s.indexOf(*k, e);
    EXPECT_TRUE(i.has_value());
    return i ? s.record(*k, *i).name : std::string();
}

}  // namespace

TEST(ModelingNaming, BoxElementsHaveUniqueStableNames) {
    NamedShape a = namedBox(40, 30, 20);
    NamedShape b = namedBox(10, 70, 5);
    for (ElementKind k : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
        std::set<std::string> na, nb;
        for (const auto& r : a.records(k)) na.insert(r.name);
        for (const auto& r : b.records(k)) nb.insert(r.name);
        EXPECT_EQ(na.size(), a.count(k)) << "names are unique";
        EXPECT_EQ(na, nb) << "names do not depend on dimensions";
    }
    EXPECT_EQ(a.count(ElementKind::Face), 6u);
    EXPECT_EQ(a.count(ElementKind::Edge), 12u);
    EXPECT_EQ(a.count(ElementKind::Vertex), 8u);
    // The top-front edge (y = 0, z = H, along X) is named after its two faces in both boxes.
    EXPECT_EQ(nameOf(a, singleLine(a.shape(), 0, 0, 20)), "E(B:Front|B:Top)");
    EXPECT_EQ(nameOf(b, singleLine(b.shape(), 0, 0, 5)), "E(B:Front|B:Top)");
    EXPECT_EQ(nameOf(a, planarFace(a.shape(), gp::DZ(), 20)), "B:Top");
    EXPECT_EQ(a.record(ElementKind::Face, *a.find(ElementKind::Face, "B:Top")).origin, NameOrigin::Role);
}

TEST(ModelingNaming, FilletHistoryNamesGeneratedFaceAndKeepsTrimmedEdges) {
    NamedShape box = namedBox(40, 30, 20);
    NamedShape f = fillet(box, singleLine(box.shape(), 0, 0, 20), 3, "F");
    // The fillet face is generated from the edge.
    auto cyl = f.find(ElementKind::Face, "G[F](E(B:Front|B:Top))");
    ASSERT_TRUE(cyl.has_value());
    EXPECT_EQ(f.record(ElementKind::Face, *cyl).signature.geometry, "Cylinder");
    EXPECT_NEAR(f.record(ElementKind::Face, *cyl).signature.radius, 3.0, 1e-9);
    // The vertical front-left edge is trimmed to z in [0, 17] and keeps its name.
    auto left = f.find(ElementKind::Edge, "E(B:Front|B:Left)");
    ASSERT_TRUE(left.has_value());
    EXPECT_NEAR(f.record(ElementKind::Edge, *left).signature.size, 17.0, 1e-7);
    // The consumed edge is recorded as removed by F with the fillet face as successor.
    auto del = f.deletions().find("E(B:Front|B:Top)");
    ASSERT_NE(del, f.deletions().end());
    EXPECT_EQ(del->second.feature, "F");
    ASSERT_EQ(del->second.successors.size(), 1u);
    EXPECT_EQ(del->second.successors.front(), "G[F](E(B:Front|B:Top))");
}

TEST(ModelingNaming, SignatureDistance) {
    NamedShape box = namedBox(40, 30, 20);
    const auto& top = box.record(ElementKind::Face, *box.find(ElementKind::Face, "B:Top")).signature;
    const auto& bottom = box.record(ElementKind::Face, *box.find(ElementKind::Face, "B:Bottom")).signature;
    const auto& edge = box.record(ElementKind::Edge, 0).signature;
    EXPECT_DOUBLE_EQ(signatureDistance(top, top, box.scale()), 0.0);
    EXPECT_TRUE(std::isinf(signatureDistance(top, bottom, box.scale()))) << "opposite normals";
    EXPECT_TRUE(std::isinf(signatureDistance(top, edge, box.scale()))) << "different kinds";
    EXPECT_NEAR(top.size, 1200.0, 1e-9);
    EXPECT_NEAR(top.center[2], 20.0, 1e-9);
    EXPECT_NEAR(box.scale(), std::sqrt(40.0 * 40 + 30 * 30 + 20 * 20), 1e-3);
}

TEST(ModelingNaming, ResolveExactSplitDeletedAndSignature) {
    NamedShape box = namedBox(40, 30, 20);
    TopoReference topFront = refTo(box, singleLine(box.shape(), 0, 0, 20));

    // Exact.
    Resolution exact = resolveReference(topFront, box);
    EXPECT_EQ(exact.method, ResolveMethod::Exact);
    ASSERT_EQ(exact.indices.size(), 1u);

    // A slot across the edge (x in [15, 25], y in [-5, 5], z in [15, 25]) splits it in two.
    BRepPrimAPI_MakeBox tool(gp_Pnt(15, -5, 15), 10, 10, 10);
    BRepAlgoAPI_Cut cut;
    NCollection_List<TopoDS_Shape> args, tools;
    args.Append(box.shape());
    tools.Append(tool.Shape());
    cut.SetArguments(args);
    cut.SetTools(tools);
    cut.SetToFillHistory(true);
    cut.Build();
    ASSERT_TRUE(cut.IsDone());
    NamingRequest req;
    req.featureId = "C";
    req.result = cut.Shape();
    req.history = cut.History();
    req.inputs = {&box};
    auto cutNamed = buildNamedShape(req);
    ASSERT_TRUE(cutNamed.ok());
    Resolution split = resolveReference(topFront, cutNamed.value());
    EXPECT_EQ(split.method, ResolveMethod::History);
    ASSERT_EQ(split.indices.size(), 2u);
    double total = 0;
    for (auto i : split.indices) total += cutNamed.value().record(ElementKind::Edge, i).signature.size;
    EXPECT_NEAR(total, 30.0, 1e-7) << "both pieces: 40 - 10";

    // Deleted by a fillet: broken with the cause.
    NamedShape filleted = fillet(box, singleLine(box.shape(), 0, 0, 20), 2, "F");
    Resolution broken = resolveReference(topFront, filleted);
    EXPECT_EQ(broken.method, ResolveMethod::Broken);
    EXPECT_NE(broken.detail.find("removed by feature 'F'"), std::string::npos) << broken.detail;
    EXPECT_NE(broken.detail.find("G[F]"), std::string::npos) << broken.detail;

    // Same geometry under other names: found by signature.
    NamedShape other = namedBox(40, 30, 20, "B2");
    Resolution bySig = resolveReference(topFront, other);
    EXPECT_EQ(bySig.method, ResolveMethod::Signature);
    ASSERT_EQ(bySig.names.size(), 1u);
    EXPECT_EQ(bySig.names.front(), "E(B2:Front|B2:Top)");

    // No signature and no name: broken.
    TopoReference bare{"E(nothing)", ElementKind::Edge, {}};
    Resolution none = resolveReference(bare, box);
    EXPECT_EQ(none.method, ResolveMethod::Broken);
    EXPECT_NE(none.detail.find("no geometric signature"), std::string::npos);
}

TEST(ModelingNaming, MakeReferenceRejectsForeignElements) {
    NamedShape box = namedBox(40, 30, 20);
    BRepPrimAPI_MakeBox other(5, 5, 5);
    TopExp_Explorer ex(other.Shape(), TopAbs_FACE);
    auto r = makeReference(box, ex.Current());
    EXPECT_FALSE(r.ok());
    EXPECT_FALSE(makeReference(box, box.shape()).ok()) << "a solid is not an element";
    NamingRequest req;
    EXPECT_FALSE(buildNamedShape(req).ok()) << "null result";
}
