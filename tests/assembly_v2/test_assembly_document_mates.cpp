// Сущности сопряжений из граней OCCT, вал во втулке, перетаскивание, замена компонента.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/occt_entities.hpp>
#include <bendcore/modeling/basic_features.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>

#include <filesystem>
#include <fstream>

#include <unistd.h>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;
using bendcore::kernel::TopologyIndex;
namespace modeling = bendcore::modeling;

namespace {

TopologyIndex indexOf(const TopoDS_Shape& s) { return TopologyIndex::build(bendcore::kernel::Shape(s)).value(); }

// Документ детали «вал»: операция Cylinder с ID "Shaft" (грани Shaft:Side/Top/Bottom).
modeling::PartDocument shaftDocument(double radius, double height) {
    modeling::PartDocument doc(modeling::basic::basicRegistry());
    modeling::FeatureSpec spec;
    spec.type = "Cylinder";
    spec.id = "Shaft";
    spec.parameters = {{"radius", std::to_string(radius)}, {"height", std::to_string(height)}};
    EXPECT_TRUE(doc.addFeature(spec).ok());
    EXPECT_TRUE(doc.rebuild().failed.empty());
    return doc;
}

PartData shaftPart(double radius, double height) { return partDataFromDocument(shaftDocument(radius, height)).value(); }

GeometryRef named(OccurrenceId o, const std::string& name) {
    return {{o}, ElementRef::named(ElementKind::Face, name), EntityUse::Auto};
}

struct ShaftInBushing {
    AssemblyDocument doc{"ShaftAssembly"};
    TopoDS_Shape bushingShape = bushing(20, 10, 30);
    DefinitionId bushingDef = 0, shaftDef = 0;
    OccurrenceId bushingOcc = 0, shaftOcc = 0;
    MateId concentric = 0, faces = 0;

    explicit ShaftInBushing(double shaftHeight = 50) {
        bushingDef = doc.addPart("Bushing", {bushingShape, nullptr}).value();
        shaftDef = doc.addPart("Shaft", shaftPart(10, shaftHeight)).value();
        bushingOcc = doc.addOccurrence(bushingDef, Transform::identity()).value();
        EXPECT_TRUE(doc.setFixed(bushingOcc, true).ok());
        shaftOcc = doc.addOccurrence(shaftDef, Transform::fromAxisAngle(Vec3(1, 1, 0), 0.4, Vec3(30, -15, 40))).value();
        auto m1 = doc.addMate(MateSpec::concentric(named(shaftOcc, "Shaft:Side"),
                                                   faceRef(bushingOcc, cylinderFace(bushingShape, 10))));
        EXPECT_TRUE(m1.ok()) << (m1 ? "" : m1.error().message);
        auto m2 = doc.addMate(MateSpec::coincident(named(shaftOcc, "Shaft:Bottom"),
                                                   faceRef(bushingOcc, horizontalFace(bushingShape, 30, 1)),
                                                   Alignment::AntiAligned));
        EXPECT_TRUE(m2.ok()) << (m2 ? "" : m2.error().message);
        concentric = m1 ? m1.value() : 0;
        faces = m2 ? m2.value() : 0;
    }
};

void expectShaftSeated(const AssemblyDocument& doc, OccurrenceId shaft) {
    const Transform& p = occAt(doc, shaft).placement;
    // Торец вала (локально z = 0, нормаль −Z) на верхнем торце втулки z = 30, ось вала — +Z.
    expectVecNear(p.applyToPoint(Vec3::Zero()), Vec3(0, 0, 30), 1e-8);
    expectVecNear(p.applyToDirection(Vec3::UnitZ()), Vec3::UnitZ(), 1e-9);
}

}  // namespace

TEST(AssemblyDocumentEntities, FacesEdgesVerticesToSolverEntities) {
    // Плоская грань коробки: нормаль наружу, точка — центр грани.
    const TopoDS_Shape b = box(0, 0, 0, 10, 20, 30);
    auto top = entityFromIndex(indexOf(b), ElementKind::Face, horizontalFace(b, 30, 1));
    ASSERT_TRUE(top.ok());
    EXPECT_EQ(top.value().type, EntityType::Plane);
    expectVecNear(top.value().direction, Vec3::UnitZ(), 1e-12);
    expectVecNear(top.value().point, Vec3(5, 10, 30), 1e-9);
    auto bottom = entityFromIndex(indexOf(b), ElementKind::Face, horizontalFace(b, 0, -1));
    ASSERT_TRUE(bottom.ok());
    expectVecNear(bottom.value().direction, -Vec3::UnitZ(), 1e-12);

    // Цилиндр: ось, радиус, точка на оси в середине грани; Axis — та же ось без радиуса.
    const TopoDS_Shape c = cylinder(3, 4, 5, 7, 20);
    const TopologyIndex ci = indexOf(c);
    auto cyl = entityFromIndex(ci, ElementKind::Face, cylinderFace(c, 7, 3, 4));
    ASSERT_TRUE(cyl.ok());
    EXPECT_EQ(cyl.value().type, EntityType::Cylinder);
    EXPECT_NEAR(cyl.value().radius, 7, 1e-12);
    expectVecNear(cyl.value().point, Vec3(3, 4, 15), 1e-9);
    EXPECT_NEAR(std::abs(cyl.value().direction.z()), 1.0, 1e-12);
    auto axis = entityFromIndex(ci, ElementKind::Face, cylinderFace(c, 7, 3, 4), EntityUse::Axis);
    ASSERT_TRUE(axis.ok());
    EXPECT_EQ(axis.value().type, EntityType::Axis);
    EXPECT_EQ(entityFromIndex(ci, ElementKind::Face, cylinderFace(c, 7, 3, 4), EntityUse::Point).error().code,
              ErrorCode::Unsupported);

    // Круговое ребро: ось + центр; Point — центр.
    for (std::uint32_t e = 0; e < ci.edges().size(); ++e) {
        if (ci.edges()[e].curve != bendcore::kernel::CurveType::Circle) continue;
        auto ax = entityFromIndex(ci, ElementKind::Edge, e);
        auto pt = entityFromIndex(ci, ElementKind::Edge, e, EntityUse::Point);
        ASSERT_TRUE(ax.ok() && pt.ok());
        EXPECT_EQ(ax.value().type, EntityType::Axis);
        EXPECT_EQ(pt.value().type, EntityType::Point);
        EXPECT_NEAR(pt.value().point.x(), 3, 1e-12);
        EXPECT_NEAR(pt.value().point.y(), 4, 1e-12);
        EXPECT_TRUE(std::abs(pt.value().point.z() - 5) < 1e-12 || std::abs(pt.value().point.z() - 25) < 1e-12);
        expectVecNear(ax.value().point, pt.value().point, 1e-12);
    }
    // Прямое ребро коробки: ось через середину ребра; вершина — точка.
    const TopologyIndex bi = indexOf(b);
    bool sawLine = false;
    for (std::uint32_t e = 0; e < bi.edges().size(); ++e) {
        auto ax = entityFromIndex(bi, ElementKind::Edge, e);
        ASSERT_TRUE(ax.ok());
        EXPECT_EQ(ax.value().type, EntityType::Axis);
        const Vec3 d = ax.value().direction.cwiseAbs();
        EXPECT_NEAR(d.maxCoeff(), 1.0, 1e-12);
        // Середина ребра коробки: координата вдоль ребра равна половине размера.
        const Eigen::Index k = [&] { Eigen::Index i; d.maxCoeff(&i); return i; }();
        const double half[] = {5, 10, 15};
        EXPECT_NEAR(ax.value().point[k], half[k], 1e-9);
        sawLine = true;
    }
    EXPECT_TRUE(sawLine);
    for (std::uint32_t v = 0; v < bi.vertices().size(); ++v) {
        auto p = entityFromIndex(bi, ElementKind::Vertex, v);
        ASSERT_TRUE(p.ok());
        const auto& q = bi.vertices()[v].point;
        expectVecNear(p.value().point, Vec3(q[0], q[1], q[2]), 1e-12);
    }

    // Конус → ось; сфера → сфера (центр, радиус) и точка.
    const TopoDS_Shape cone = BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(1, 2, 3), gp::DZ()), 10, 4, 12).Shape();
    auto coneFace = findFace(cone, [](const BRepAdaptor_Surface& s, const TopoDS_Face&) { return s.GetType() == GeomAbs_Cone; });
    ASSERT_TRUE(coneFace);
    auto coneAxis = entityFromIndex(indexOf(cone), ElementKind::Face, *coneFace);
    ASSERT_TRUE(coneAxis.ok());
    EXPECT_EQ(coneAxis.value().type, EntityType::Axis);
    EXPECT_NEAR(coneAxis.value().point.x(), 1, 1e-9);
    EXPECT_NEAR(coneAxis.value().point.y(), 2, 1e-9);
    const TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(gp_Pnt(4, 5, 6), 9).Shape();
    auto sph = entityFromIndex(indexOf(sphere), ElementKind::Face, 0);
    ASSERT_TRUE(sph.ok());
    EXPECT_EQ(sph.value().type, EntityType::Sphere);
    EXPECT_NEAR(sph.value().radius, 9, 1e-12);
    expectVecNear(sph.value().point, Vec3(4, 5, 6), 1e-12);

    // Неподдерживаемая геометрия — явная ошибка.
    const TopoDS_Shape torus = BRepPrimAPI_MakeTorus(20, 5).Shape();
    auto tor = entityFromIndex(indexOf(torus), ElementKind::Face, 0);
    ASSERT_FALSE(tor.ok());
    EXPECT_EQ(tor.error().code, ErrorCode::Unsupported);
    EXPECT_NE(tor.error().message.find("Torus"), std::string::npos);
    const TopoDS_Shape nurbs = BRepBuilderAPI_NurbsConvert(b).Shape();
    auto ns = entityFromIndex(indexOf(nurbs), ElementKind::Face, 0);
    ASSERT_FALSE(ns.ok());
    EXPECT_EQ(ns.error().code, ErrorCode::Unsupported);
    EXPECT_EQ(entityFromIndex(bi, ElementKind::Face, 99).error().code, ErrorCode::InvalidInput);
}

TEST(AssemblyDocumentMates, ShaftInBushingSeatsWithOneDof) {
    ShaftInBushing m;
    auto r = m.doc.solve();
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r.value().success);
    EXPECT_EQ(r.value().totalDof, 1);
    EXPECT_LT(r.value().residualNorm, 1e-9);
    ASSERT_EQ(r.value().subsystems.size(), 1u);
    EXPECT_EQ(r.value().subsystems[0].occurrences, std::vector<OccurrenceId>{m.shaftOcc});
    EXPECT_TRUE(r.value().subsystems[0].redundantMates.empty());
    expectShaftSeated(m.doc, m.shaftOcc);
    // Ось боковой грани вала в мировой СК совпадает с осью отверстия.
    auto side = m.doc.worldEntity(named(m.shaftOcc, "Shaft:Side"));
    ASSERT_TRUE(side.ok());
    EXPECT_NEAR(side.value().point.x(), 0, 1e-8);
    EXPECT_NEAR(side.value().point.y(), 0, 1e-8);
    EXPECT_NEAR(side.value().point.z(), 55, 1e-8);  // середина вала: 30 + 50/2
    // Фиксированная втулка не сдвинулась.
    expectVecNear(occAt(m.doc, m.bushingOcc).placement.position, Vec3::Zero(), 0);
}

TEST(AssemblyDocumentMates, DragRotatesShaftAboutItsAxis) {
    ShaftInBushing m;
    ASSERT_TRUE(m.doc.solve().ok());
    auto r = m.doc.drag(m.shaftOcc, Vec3(10, 0, 0), Vec3(0, 50, 30));
    ASSERT_TRUE(r.ok()) << r.error().message;
    expectShaftSeated(m.doc, m.shaftOcc);
    const Vec3 p = occAt(m.doc, m.shaftOcc).placement.applyToPoint(Vec3(10, 0, 0));
    EXPECT_NEAR(std::atan2(p.y(), p.x()), kPi / 2, 1e-6);
    EXPECT_FALSE(m.doc.drag(m.bushingOcc, Vec3::Zero(), Vec3::Ones()).ok());
}

TEST(AssemblyDocumentMates, InvalidReferencesAreRejected) {
    ShaftInBushing m;
    EXPECT_EQ(m.doc.addMate(MateSpec::concentric(named(m.shaftOcc, "Shaft:Nope"), faceRef(m.bushingOcc, 0))).error().code,
              ErrorCode::InvalidInput);
    // Имя без карты элементов.
    EXPECT_EQ(m.doc.addMate(MateSpec::concentric(named(m.bushingOcc, "X"), faceRef(m.shaftOcc, 0))).error().code,
              ErrorCode::InvalidInput);
    EXPECT_EQ(m.doc.addMate(MateSpec::concentric(faceRef(m.bushingOcc, 1000), faceRef(m.shaftOcc, 0))).error().code,
              ErrorCode::InvalidInput);
    EXPECT_FALSE(m.doc.addMate(MateSpec::concentric(faceRef(999, 0), faceRef(m.shaftOcc, 0))).ok());
    // Плоскость не может быть соосна цилиндру: ошибка решателя.
    EXPECT_FALSE(m.doc.addMate(MateSpec::concentric(named(m.shaftOcc, "Shaft:Top"),
                                                    faceRef(m.bushingOcc, cylinderFace(m.bushingShape, 10))))
                     .ok());
    EXPECT_EQ(m.doc.mates().size(), 2u);
}

TEST(AssemblyDocumentReplace, LongerShaftKeepsMatesByName) {
    ShaftInBushing m(50);
    ASSERT_TRUE(m.doc.solve().ok());
    const DefinitionId longer = m.doc.addPart("ShaftLong", shaftPart(10, 80)).value();
    auto rep = m.doc.replaceOccurrenceDefinition(m.shaftOcc, longer);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().broken.empty());
    EXPECT_EQ(rep.value().remapped.size(), 2u);
    auto r = m.doc.solve();
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().success);
    EXPECT_EQ(r.value().totalDof, 1);
    expectShaftSeated(m.doc, m.shaftOcc);
    auto side = m.doc.worldEntity(named(m.shaftOcc, "Shaft:Side"));
    ASSERT_TRUE(side.ok());
    EXPECT_NEAR(side.value().point.z(), 70, 1e-8);  // 30 + 80/2
}

TEST(AssemblyDocumentReplace, BushingGeometryByIndexAndBrokenReference) {
    ShaftInBushing m;
    ASSERT_TRUE(m.doc.solve().ok());
    // Новая втулка той же топологии, выше и толще: ссылки по индексу сохраняются, торец на z = 40.
    auto rep = m.doc.replacePartGeometry(m.bushingDef, {bushing(25, 10, 40), nullptr});
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().broken.empty());
    auto r = m.doc.solve();
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value().totalDof, 1);
    expectVecNear(occAt(m.doc, m.shaftOcc).placement.applyToPoint(Vec3::Zero()), Vec3(0, 0, 40), 1e-8);

    // Замена вала коробкой: у коробки нет цилиндра — соосность сломана с причиной.
    const DefinitionId boxDef = m.doc.addPart("Block", {box(-5, -5, 0, 10, 10, 10), nullptr}).value();
    auto rep2 = m.doc.replaceOccurrenceDefinition(m.shaftOcc, boxDef);
    ASSERT_TRUE(rep2.ok());
    const DocumentMate* conc = m.doc.findMate(m.concentric);
    ASSERT_NE(conc, nullptr);
    EXPECT_TRUE(conc->broken());
    bool reported = false;
    for (const auto& [id, why] : rep2.value().broken) reported = reported || (id == m.concentric && !why.empty());
    EXPECT_TRUE(reported);
    auto r2 = m.doc.solve();
    ASSERT_TRUE(r2.ok());
    EXPECT_NE(std::find(r2.value().brokenMates.begin(), r2.value().brokenMates.end(), m.concentric),
              r2.value().brokenMates.end());

    // Возврат вала восстанавливает сопряжение.
    auto rep3 = m.doc.replaceOccurrenceDefinition(m.shaftOcc, m.shaftDef);
    ASSERT_TRUE(rep3.ok());
    EXPECT_FALSE(m.doc.findMate(m.concentric)->broken());
    auto r3 = m.doc.solve();
    ASSERT_TRUE(r3.ok());
    EXPECT_TRUE(r3.value().success);
    EXPECT_EQ(r3.value().totalDof, 1);
}

TEST(AssemblyDocumentReplace, PartDocumentFromFileReloads) {
    const auto dir = std::filesystem::temp_directory_path() / ("bendcore_asm_doc_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    auto write = [&](double height) {
        std::ofstream(dir / "shaft.json") << shaftDocument(10, height).toJson().value();
    };
    write(50);
    auto resolver = filePartResolver(modeling::basic::basicRegistry(), dir.string());
    AssemblyDocument doc;
    const TopoDS_Shape bush = bushing(20, 10, 30);
    const OccurrenceId b = doc.addOccurrence(doc.addPart("Bushing", {bush, nullptr}).value(), Transform::identity()).value();
    ASSERT_TRUE(doc.setFixed(b, true).ok());
    auto shaftDef = doc.addPartFromSource("Shaft", "shaft.json", resolver);
    ASSERT_TRUE(shaftDef.ok()) << shaftDef.error().message;
    EXPECT_EQ(doc.definition(shaftDef.value()).source, "shaft.json");
    ASSERT_NE(doc.definition(shaftDef.value()).elementMap, nullptr);
    const OccurrenceId s = doc.addOccurrence(shaftDef.value(), Transform::fromTranslation(Vec3(5, 5, 50))).value();
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(named(s, "Shaft:Side"), faceRef(b, cylinderFace(bush, 10)))).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(named(s, "Shaft:Bottom"), faceRef(b, horizontalFace(bush, 30, 1)),
                                                 Alignment::AntiAligned))
                    .ok());
    ASSERT_TRUE(doc.solve().ok());
    EXPECT_NEAR(volumeOf(doc.definition(shaftDef.value()).shape), kPi * 100 * 50, 1e-6);

    write(80);
    auto rep = doc.reloadPart(shaftDef.value(), resolver);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().broken.empty());
    EXPECT_EQ(doc.definition(shaftDef.value()).revision, 1u);
    EXPECT_NEAR(volumeOf(doc.definition(shaftDef.value()).shape), kPi * 100 * 80, 1e-6);
    auto r = doc.solve();
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value().totalDof, 1);
    expectShaftSeated(doc, s);

    EXPECT_EQ(resolver("missing.json").error().code, ErrorCode::IoError);
    std::ofstream(dir / "part.xyz") << "x";
    EXPECT_EQ(resolver("part.xyz").error().code, ErrorCode::Unsupported);
    std::filesystem::remove_all(dir);
}
