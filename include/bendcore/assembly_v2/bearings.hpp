#pragma once

// Каталог подшипников качения библиотеки стандартных изделий (docs/assembly/FASTENERS.md,
// раздел 8): шариковые радиальные однорядные ГОСТ 8338-75 / ISO 15 серий 100, 200, 300
// (d = 10…60 мм). Модель — наружное и внутреннее кольца с дорожками качения и шарики (тела
// отдельными телами одной детали), атрибуты «bearing.*» на всех гранях — для условного
// изображения ГОСТ 2.420 на чертеже, свойства для BOM и спецификации, именованные грани для
// сопряжений.
//
// Размеры d, D, B, r записаны по памяти без доступа к официальному тексту стандартов и
// ТРЕБУЮТ СВЕРКИ (bearingTableInfo().verified == false, docs/STANDARDS_VERIFICATION.md).
// Внутренняя геометрия (диаметр и число шариков, дорожки) стандартами не нормируется —
// она условная (BallBearingInternals).

#include <bendcore/assembly_v2/fasteners.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::assembly_v2::fasteners {

// Серия по ГОСТ 8338 (третья цифра справа условного обозначения) и ISO 15 (серия размеров).
enum class BearingSeries {
    ExtraLight = 1,  // особо лёгкая, ГОСТ 1xx — ISO 60xx (серия размеров 10)
    Light = 2,       // лёгкая, ГОСТ 2xx — ISO 62xx (02)
    Medium = 3,      // средняя, ГОСТ 3xx — ISO 63xx (03)
};

struct BallBearingRow {
    int number = 0;           // условное обозначение ГОСТ 8338: 205
    BearingSeries series = BearingSeries::Light;
    std::string iso;          // обозначение ISO 15 / ISO 4: «6205»
    double d = 0;             // диаметр отверстия
    double D = 0;             // наружный диаметр
    double B = 0;             // ширина
    double r = 0;             // координата монтажной фаски колец r (r_s min по ISO 15)
};

// Строка каталога по условному обозначению (100…112, 200…212, 300…312: две последние цифры
// 00 → d = 10, 01 → 12, 02 → 15, 03 → 17, далее d = 5·n). Ошибки: Unsupported — обозначения
// нет в каталоге (сообщение перечисляет ряд).
kernel::Result<BallBearingRow> ballBearingRow(int number);
// По серии и диаметру отверстия. Ошибки: Unsupported.
kernel::Result<BallBearingRow> ballBearingRow(BearingSeries series, double d);
std::vector<int> ballBearingNumbers();

struct BearingTableInfo {
    std::string standard;      // «ГОСТ 8338-75 / ISO 15»
    std::string source;
    std::string verification;
    bool verified = false;
};
BearingTableInfo bearingTableInfo();

// Условная внутренняя геометрия (не табличная, docs/assembly/FASTENERS.md, 8.2):
// Dw = 0,3·(D − d); Dpw = (d + D)/2; Z = ⌊π·Dpw / (1,6·Dw)⌋; радиус желоба дорожки 0,52·Dw;
// борта колец — на Dpw/2 ∓ 0,3·Dw; радиальный зазор между шариком и дорожками — clearance.
struct BallBearingInternals {
    double Dw = 0;            // диаметр шарика
    double Dpw = 0;           // диаметр окружности центров шариков
    int Z = 0;                // число шариков
    double grooveRadius = 0;  // радиус профиля желоба дорожки
    double clearance = 0;     // радиальный зазор (сумма по двум дорожкам)
    double innerShoulder = 0; // диаметр борта внутреннего кольца
    double outerShoulder = 0; // диаметр борта наружного кольца
    double innerRaceway = 0;  // диаметр дна дорожки внутреннего кольца
    double outerRaceway = 0;  // диаметр дна дорожки наружного кольца
};
kernel::Result<BallBearingInternals> ballBearingInternals(const BallBearingRow& row);

// Роли граней подшипника: имя в карте элементов — "Bearing:<роль>".
namespace bearing_roles {
inline constexpr const char* kFeature = "Bearing";
inline constexpr const char* kBore = "Bore";                  // отверстие внутреннего кольца ⌀d
inline constexpr const char* kOuter = "Outer";                // наружная поверхность ⌀D
inline constexpr const char* kInnerFace = "InnerFace";        // торцы внутреннего кольца: InnerFace1 (z = 0), InnerFace2 (z = B)
inline constexpr const char* kOuterFace = "OuterFace";        // торцы наружного кольца: OuterFace1, OuterFace2
inline constexpr const char* kInnerRaceway = "InnerRaceway";  // желоб внутреннего кольца
inline constexpr const char* kOuterRaceway = "OuterRaceway";  // желоб наружного кольца
inline constexpr const char* kInnerShoulder = "InnerShoulder";  // борта: InnerShoulder1/2, OuterShoulder1/2 (по z)
inline constexpr const char* kOuterShoulder = "OuterShoulder";
inline constexpr const char* kBoreRound = "BoreRound";        // скругления r кромок отверстия: BoreRound1/2
inline constexpr const char* kOuterRound = "OuterRound";      // скругления r наружных кромок: OuterRound1/2
inline constexpr const char* kBall = "Ball";                  // шарики Ball1…BallZ (по углу от +X)
}  // namespace bearing_roles

// Полное имя грани: "Bearing:Bore".
std::string bearingFaceName(const std::string& role);

// Атрибуты «bearing.*» — на всех гранях модели (docs/assembly/FASTENERS.md, 8.4).
struct BearingAttributes {
    std::string type;         // «ball-radial» — шариковый радиальный однорядный
    std::string standard;     // «ГОСТ 8338-75»
    std::string designation;  // «Подшипник 205 ГОСТ 8338-75»
    std::string number;       // «205»
    std::string iso;          // «6205»
    double d = 0, D = 0, B = 0, r = 0;
    double Dw = 0, Dpw = 0;
    int Z = 0;
};
modeling::ElementAttributes bearingAttributes(const BearingAttributes& a);
// Ошибки: InvalidInput — нет ключа или значение не число.
kernel::Result<BearingAttributes> bearingFromAttributes(const modeling::ElementAttributes& attributes);
// Атрибуты подшипника формы (первая грань с атрибутами «bearing.*»); nullopt — не подшипник.
std::optional<BearingAttributes> bearingOf(const modeling::NamedShape& shape);

// Модель подшипника в собственной СК: ось — Z, торцы z = 0 и z = B, центр — на оси.
// Тела: наружное кольцо, внутреннее кольцо, Z шариков (составная форма, тела не касаются
// друг друга — зазор clearance/2 на каждой дорожке). Сепаратор не моделируется.
struct Bearing {
    BallBearingRow row;
    BallBearingInternals internals;
    TopoDS_Shape shape;
    std::shared_ptr<const modeling::NamedShape> elementMap;
    BearingAttributes attributes;
    FastenerProperties properties;  // name «Подшипник», designation «Подшипник 205 ГОСТ 8338-75»
    std::vector<std::pair<std::string, double>> dimensions;  // d, D, B, r, Dw, Dpw, Z

    std::optional<double> dimension(const std::string& key) const;
};

inline constexpr double kBearingSteelDensity = 7810.0;  // кг/м³, сталь ШХ15 (справочное)

// Ошибки: Unsupported — нет в каталоге; OperationFailed — OCCT.
kernel::Result<Bearing> makeBallBearing(int number);

PartData toPartData(const Bearing& bearing);
// Наименование — полное обозначение (графа «Обозначение» пустая, ГОСТ 2.106), материал и
// плотность стали ШХ15; в разрезах сборки рассекается (sectionCut = true).
PartProperties toPartProperties(const Bearing& bearing);
kernel::Result<DefinitionId> addBearingPart(AssemblyDocument& document, const Bearing& bearing);

}  // namespace bendcore::assembly_v2::fasteners
