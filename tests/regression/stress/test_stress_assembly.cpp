// Нагрузка: сборка из 300 компонентов (ROADMAP_GENERAL_CAD.md, фаза 5, п. 1).
//
// Плита 240 × 180 × 10 с сеткой 20 × 15 отверстий ⌀5 (шаг 12, без последнего узла — 299
// отверстий) неподвижна; 299 штифтов (стержень ⌀4 × 10, головка ⌀8 × 3) стоят со смещением
// и наклоном. На каждый штифт — соосность стержня с отверстием и совмещение нижнего торца
// головки с верхней гранью плиты: 598 сопряжений, 299 независимых подсистем.
//
// Эталон — по построению: после решения начало СК штифта i — (x_i, y_i, 10), ось — +Z;
// пересечений нет (зазор 0,5 мм по радиусу), каждый штифт касается плиты головкой, пары
// штифтов отсеиваются по габаритам (головки ⌀8 при шаге 12) — кандидатов ровно 299 из
// 300·299/2; BOM: плита × 1, штифт × 299, массы — объёмы по формулам × 7850 кг/м³.

#include "stress_util.hpp"

#include "../../assembly_v2/assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/interference.hpp>

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <vector>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::qa::stress::timed;

namespace {

constexpr int kCols = 20, kRows = 15, kPins = kCols * kRows - 1;
constexpr double kPitch = 12.0, kStart = 6.0, kHoleR = 2.5, kT = 10.0;
constexpr double kLen = kStart * 2 + kPitch * (kCols - 1), kWid = kStart * 2 + kPitch * (kRows - 1);
constexpr double kShaftR = 2.0, kHeadR = 4.0, kHeadH = 3.0, kDensity = 7850.0;

double holeX(int n) { return kStart + kPitch * (n % kCols); }
double holeY(int n) { return kStart + kPitch * (n / kCols); }

TopoDS_Shape drilledPlate() {
    BRep_Builder bb;
    TopoDS_Compound tools;
    bb.MakeCompound(tools);
    for (int n = 0; n < kPins; ++n) bb.Add(tools, cylinder(holeX(n), holeY(n), -1, kHoleR, kT + 2));
    return cut(box(0, 0, 0, kLen, kWid, kT), tools);
}

}  // namespace

TEST(StressAssembly, ThreeHundredComponentsSolveInterferenceBom) {
    AssemblyDocument doc("Stress300");
    const TopoDS_Shape plateShape = drilledPlate();
    const TopoDS_Shape pinShape = bolt(kShaftR, kT, kHeadR, kHeadH);
    const DefinitionId plateDef =
        doc.addPart("Plate", {plateShape, nullptr}, {"ST.300.001", "Plate", "Steel", kDensity}).value();
    const DefinitionId pinDef =
        doc.addPart("Pin", {pinShape, nullptr}, {"ST.300.002", "Pin", "Steel", kDensity}).value();
    const OccurrenceId plate = doc.addOccurrence(plateDef, Transform::identity(), "Plate").value();
    ASSERT_TRUE(doc.setFixed(plate, true).ok());

    const std::uint32_t plateTop = horizontalFace(plateShape, kT, 1);
    const std::uint32_t pinShaft = cylinderFace(pinShape, kShaftR);
    const std::uint32_t pinHeadBottom = horizontalFace(pinShape, 0, -1);
    std::vector<std::uint32_t> holes(kPins);
    timed("assembly_300", "find_hole_faces", 60, [&] {
        for (int n = 0; n < kPins; ++n) holes[n] = cylinderFace(plateShape, kHoleR, holeX(n), holeY(n));
    });

    std::vector<OccurrenceId> pins(kPins);
    timed("assembly_300", "add_components_and_mates", 30, [&] {
        for (int n = 0; n < kPins; ++n) {
            // Смещение до 1 мм, подъём 2 мм и наклон 0,05 рад — начальное положение не решение.
            const Transform start = Transform::fromAxisAngle(Vec3(1, n % 3, 0), 0.05,
                                                             Vec3(holeX(n) + 0.6, holeY(n) - 0.4, kT + 2));
            pins[n] = doc.addOccurrence(pinDef, start, "Pin" + std::to_string(n)).value();
            ASSERT_TRUE(doc.addMate(MateSpec::concentric(faceRef(pins[n], pinShaft), faceRef(plate, holes[n]))).ok());
            ASSERT_TRUE(
                doc.addMate(MateSpec::coincident(faceRef(pins[n], pinHeadBottom), faceRef(plate, plateTop))).ok());
        }
    });
    ASSERT_EQ(doc.topLevel().size(), 300u);
    ASSERT_EQ(doc.mates().size(), 598u);

    DocumentSolveReport solved;
    timed("assembly_300", "solve_598_mates", 15, [&] {
        auto r = doc.solve();
        ASSERT_TRUE(r.ok()) << r.error().message;
        solved = r.value();
    });
    EXPECT_TRUE(solved.success);
    EXPECT_TRUE(solved.brokenMates.empty());
    EXPECT_EQ(solved.subsystems.size(), std::size_t(kPins)) << "each pin is its own subsystem";
    EXPECT_EQ(solved.totalDof, kPins) << "rotation about its axis stays free for every pin";
    for (int n = 0; n < kPins; ++n) {
        auto w = doc.worldPlacement({pins[n]});
        ASSERT_TRUE(w.ok());
        // Начало СК штифта — на оси отверстия на верхней грани; ось штифта — +Z.
        expectVecNear(w.value().applyToPoint(Vec3::Zero()), Vec3(holeX(n), holeY(n), kT), 1e-6);
        expectVecNear(w.value().applyToDirection(Vec3::UnitZ()), Vec3::UnitZ(), 1e-9);
    }

    InterferenceReport inter;
    timed("assembly_300", "interference_bbox_prefilter", 240, [&] {
        auto r = checkInterference(doc);
        ASSERT_TRUE(r.ok()) << r.error().message;
        inter = std::move(r.value());
    });
    EXPECT_EQ(inter.totalPairs, 300u * 299u / 2u);
    EXPECT_EQ(inter.candidatePairs, std::size_t(kPins)) << "only pin-plate boxes overlap";
    std::size_t touching = 0, interfering = 0;
    for (const auto& p : inter.pairs) (p.kind == ContactKind::Touching ? touching : interfering)++;
    EXPECT_EQ(interfering, 0u);
    EXPECT_EQ(touching, std::size_t(kPins));
    EXPECT_NEAR(inter.totalVolume, 0.0, 1e-6);

    Bom bom;
    timed("assembly_300", "flat_bom", 30, [&] {
        auto r = flatBom(doc);
        ASSERT_TRUE(r.ok()) << r.error().message;
        bom = std::move(r.value());
    });
    ASSERT_EQ(bom.rows.size(), 2u);
    EXPECT_EQ(bom.totalParts, 300);
    const double plateVolume = kLen * kWid * kT - kPins * kPi * kHoleR * kHoleR * kT;
    const double pinVolume = kPi * kShaftR * kShaftR * kT + kPi * kHeadR * kHeadR * kHeadH;
    for (const BomRow& row : bom.rows) {
        const bool isPin = row.name == "Pin";
        EXPECT_EQ(row.quantity, isPin ? kPins : 1) << row.name;
        const double v = isPin ? pinVolume : plateVolume;
        EXPECT_NEAR(row.unitVolume, v, 1e-6 * v) << row.name;
        ASSERT_TRUE(row.totalMass.has_value());
        EXPECT_NEAR(*row.totalMass, v * 1e-9 * kDensity * row.quantity, 1e-9 * v * row.quantity) << row.name;
    }
    ASSERT_TRUE(bom.totalMass.has_value());
    EXPECT_NEAR(*bom.totalMass, (plateVolume + kPins * pinVolume) * 1e-9 * kDensity, 1e-6);

    // Сохранение с встроенной геометрией и загрузка: состав и сопряжения не теряются.
    std::string json;
    timed("assembly_300", "save_json_embedded", 60, [&] {
        auto t = doc.toJson();
        ASSERT_TRUE(t.ok()) << t.error().message;
        json = std::move(t.value());
    });
    timed("assembly_300", "load_json_embedded", 60, [&] {
        auto l = AssemblyDocument::fromJson(json, LoadOptions{});
        ASSERT_TRUE(l.ok()) << l.error().message;
        EXPECT_EQ(l.value().document.topLevel().size(), 300u);
        EXPECT_EQ(l.value().document.mates().size(), 598u);
    });
}
