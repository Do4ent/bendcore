#pragma once

// Библиотека стандартного крепежа ГОСТ/ISO (фаза 3, п. 5 docs/ROADMAP_GENERAL_CAD.md;
// docs/assembly/FASTENERS.md): таблицы размеров, параметрические твердотельные модели OCCT
// (резьба условная: стержень номинального диаметра + атрибуты резьбы), свойства для BOM и
// спецификации, именованные грани для сопряжений и постановка пакета «болт — шайбы — гайка»
// в отверстия детали документа сборки.
//
// Размеры таблиц взяты по памяти из стандартов без доступа к их официальному тексту и
// ТРЕБУЮТ СВЕРКИ С ОФИЦИАЛЬНЫМ ИЗДАНИЕМ (tableInfo().verified == false).

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/modeling/naming.hpp>

#include <TopoDS_Shape.hxx>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::assembly_v2::fasteners {

enum class FastenerKind {
    HexBolt,          // болт с шестигранной головкой
    SocketHeadScrew,  // винт с цилиндрической головкой и шестигранным углублением
    HexNut,           // гайка шестигранная
    PlainWasher,      // шайба плоская
    SpringWasher,     // шайба пружинная
    ParallelPin,      // штифт цилиндрический
};

enum class FastenerStandard {
    Gost7798,   // Болт ГОСТ 7798-70
    Iso4014,    // Болт ISO 4014 (резьба на части стержня)
    Iso4017,    // Болт ISO 4017 (резьба на всю длину)
    Gost11738,  // Винт ГОСТ 11738-84
    Iso4762,    // Винт ISO 4762
    Gost5915,   // Гайка ГОСТ 5915-70
    Iso4032,    // Гайка ISO 4032
    Gost11371,  // Шайба ГОСТ 11371-78 (исполнение 1)
    Iso7089,    // Шайба ISO 7089
    Gost6402,   // Шайба пружинная ГОСТ 6402-70 (нормальная, Н)
    Gost3128,   // Штифт ГОСТ 3128-70
    Iso2338,    // Штифт ISO 2338
};

const char* toString(FastenerKind kind) noexcept;
// Обозначение стандарта: «ГОСТ 7798-70», «ISO 4014».
const char* toString(FastenerStandard standard) noexcept;
FastenerKind kindOf(FastenerStandard standard) noexcept;
bool isGost(FastenerStandard standard) noexcept;
// Стандарт другой системы с той же конструкцией (ГОСТ 7798 ↔ ISO 4014 и т. п.); у
// ГОСТ 6402 соответствия в ISO нет.
std::optional<FastenerStandard> counterpart(FastenerStandard standard) noexcept;
std::vector<FastenerStandard> allStandards();

// ---- таблицы размеров (мм)

// Шаг крупной резьбы по номинальному диаметру (ISO 261 / ГОСТ 8724).
std::optional<double> coarsePitch(int nominalDiameter) noexcept;
// Внутренний диаметр наружной резьбы d3 = d − 1,226869·P (ISO 68-1 / ГОСТ 9150, H = 0,866025·P).
double externalMinorDiameter(double d, double pitch) noexcept;

struct HexBoltRow {
    int d = 0;
    double pitch = 0;
    double s = 0;                  // размер под ключ
    double k = 0;                  // высота головки (номинальная)
    std::optional<double> eMin;    // наименьший диаметр описанной окружности (если указан в таблице)
};

struct SocketScrewRow {
    int d = 0;
    double pitch = 0;
    double dk = 0;  // диаметр головки (наибольший)
    double k = 0;   // высота головки (= d)
    double s = 0;   // размер шестигранного углубления под ключ
    double t = 0;   // глубина углубления (наименьшая)
};

struct HexNutRow {
    int d = 0;
    double pitch = 0;
    double s = 0;
    double m = 0;  // высота гайки
    std::optional<double> eMin;
};

struct WasherRow {
    int d = 0;      // номинальный диаметр резьбы крепежа
    double d1 = 0;  // внутренний диаметр
    double d2 = 0;  // наружный диаметр
    double h = 0;   // толщина
};

struct SpringWasherRow {
    int d = 0;
    double d1 = 0;  // внутренний диаметр
    double s = 0;   // толщина
    double b = 0;   // ширина сечения
};

struct PinRow {
    int d = 0;
    double c = 0;  // фаска на торцах (≈)
};

// Таблица стандарта: сведения об источнике и статус сверки.
struct TableInfo {
    FastenerStandard standard{};
    std::string source;        // откуда взяты значения
    std::string verification;  // статус сверки
    bool verified = false;     // сверено с официальным изданием
    std::vector<int> sizes;    // номинальные диаметры в таблице
};

TableInfo tableInfo(FastenerStandard standard);

// Ошибки: InvalidInput — стандарт другого вида; Unsupported — размера нет в таблице
// (сообщение перечисляет доступный ряд).
kernel::Result<HexBoltRow> hexBoltRow(FastenerStandard standard, int d);
kernel::Result<SocketScrewRow> socketScrewRow(FastenerStandard standard, int d);
kernel::Result<HexNutRow> hexNutRow(FastenerStandard standard, int d);
kernel::Result<WasherRow> washerRow(FastenerStandard standard, int d);
kernel::Result<SpringWasherRow> springWasherRow(FastenerStandard standard, int d);
kernel::Result<PinRow> pinRow(FastenerStandard standard, int d);

// ---- модели

struct FastenerSpec {
    FastenerStandard standard = FastenerStandard::Iso4014;
    int size = 0;         // номинальный диаметр резьбы (штифт — диаметр), мм
    double length = 0.0;  // болт, винт: длина стержня l без головки; штифт: длина; иначе не используется
};

// Условная резьба: геометрия — цилиндр номинального диаметра (наружная) или отверстие
// номинального диаметра (внутренняя); параметры — для чертежа и спецификации.
struct ThreadAttributes {
    bool present = false;
    bool internal = false;
    double nominalDiameter = 0.0;
    double pitch = 0.0;
    double minorDiameter = 0.0;  // наружная: d3; внутренняя: D1 = D − 1,082532·P
    double length = 0.0;         // длина резьбы от торца стержня (гайка — высота)
    std::string designation;     // «M10» (крупный шаг)
};

inline constexpr const char* kSpecificationSection = "Стандартные изделия";
inline constexpr double kSteelDensity = 7850.0;  // кг/м³

struct FastenerProperties {
    std::string name;            // «Болт», «Гайка», «Шайба», «Шайба пружинная», «Винт», «Штифт»
    std::string designation;     // по своему стандарту: «Болт М10×40 ГОСТ 7798-70», «ISO 4032 - M10»
    // По стандарту-соответствию, если его таблица даёт те же размеры для этого типоразмера;
    // иначе пусто (размеры различаются — изделия не взаимозаменяемы).
    std::string equivalentDesignation;
    std::string section = kSpecificationSection;
    std::string material = "Сталь";
    double density = kSteelDensity;
    double volume = 0.0;  // мм³ (BRepGProp)
    double mass = 0.0;    // кг: volume·1e-9·density
};

// Роли граней: имя в карте элементов — "Fastener:<роль>".
namespace roles {
inline constexpr const char* kFeature = "Fastener";
inline constexpr const char* kBearing = "Bearing";      // опорная поверхность (под головкой, торец гайки/шайбы z = 0)
inline constexpr const char* kTop = "Top";              // противоположный торец (головка, гайка, шайба)
inline constexpr const char* kShank = "Shank";          // цилиндр стержня (ось), болт/винт/штифт
inline constexpr const char* kBore = "Bore";            // отверстие гайки и шайбы
inline constexpr const char* kTip = "Tip";              // торец стержня
inline constexpr const char* kTipChamfer = "TipChamfer";
inline constexpr const char* kHeadChamfer = "HeadChamfer";
inline constexpr const char* kTopChamfer = "TopChamfer";      // гайка: фаска со стороны Top
inline constexpr const char* kBearingChamfer = "BearingChamfer";
inline constexpr const char* kOuter = "Outer";          // наружный цилиндр шайбы, головки винта
inline constexpr const char* kSocket = "Socket";        // боковые грани углубления: Socket1…6
inline constexpr const char* kSocketBottom = "SocketBottom";
inline constexpr const char* kFlat = "Flat";            // грани шестигранника: Flat1…6
inline constexpr const char* kSlot = "Slot";            // разрез пружинной шайбы: Slot1, Slot2
inline constexpr const char* kEnd = "End";              // штифт: торцы End1 (z = 0), End2 (z = l)
inline constexpr const char* kEndChamfer = "EndChamfer";  // штифт: EndChamfer1, EndChamfer2
}  // namespace roles

// Полное имя грани в карте элементов: "Fastener:Bearing".
std::string faceName(const std::string& role);

// Модель крепежа в собственной СК: ось — Z.
//  болт, винт: опорная поверхность головки z = 0 (нормаль −Z), головка z ∈ [0, k],
//              стержень z ∈ [−l, 0];
//  гайка, шайбы: z ∈ [0, высота], Bearing — торец z = 0, Top — z = высота;
//  штифт: z ∈ [0, l].
struct Fastener {
    FastenerSpec spec;
    FastenerKind kind = FastenerKind::HexBolt;
    TopoDS_Shape shape;
    std::shared_ptr<const modeling::NamedShape> elementMap;
    ThreadAttributes thread;
    FastenerProperties properties;
    // Размеры модели для чертежа: d, P, s, e, k, m, l, b, dk, t, d1, d2, h, c …
    std::vector<std::pair<std::string, double>> dimensions;

    std::optional<double> dimension(const std::string& key) const;
};

// Строит модель. Ошибки: InvalidInput (длина ≤ 0 или меньше допустимой; ISO 4014 при
// l ≤ b — нужен ISO 4017), Unsupported (размера нет в таблице), OperationFailed (OCCT).
kernel::Result<Fastener> makeFastener(const FastenerSpec& spec);

// Данные детали и свойства для документа сборки: наименование графы спецификации —
// полное обозначение по стандарту (раздел «Стандартные изделия», ГОСТ 2.106), графа
// «Обозначение» пустая; плотность — стали.
PartData toPartData(const Fastener& fastener);
PartProperties toPartProperties(const Fastener& fastener);
// addPart(обозначение, toPartData, toPartProperties).
kernel::Result<DefinitionId> addFastenerPart(AssemblyDocument& document, const Fastener& fastener);

// ---- постановка в отверстия

// Отверстие детали: цилиндрическая грань (материал снаружи цилиндра) и торцевые плоскости
// входа (со стороны головки) и выхода (со стороны гайки).
struct HoleSite {
    GeometryRef hole;
    GeometryRef entryFace;
    std::optional<GeometryRef> exitFace;
    double radius = 0.0;
    double depth = 0.0;  // расстояние между плоскостями входа и выхода (0 — выхода нет)
};

// Отверстия детали на пути `part` радиуса radius (допуск tolerance·max(1, r)), ось которых
// параллельна insertion (мировое направление от головки к концу стержня). Плоскость входа —
// смежная с отверстием плоская грань, наружная нормаль которой направлена против insertion,
// выхода — по insertion. Соосные грани одного отверстия объединяются. Порядок — по
// координатам точки входа (x, затем y, затем z, мировая СК).
// Ошибки: InvalidInput — путь, нулевое направление, отверстий нет.
kernel::Result<std::vector<HoleSite>> findHoleSites(const AssemblyDocument& document, const OccurrencePath& part,
                                                    const Vec3& insertion, double radius,
                                                    double tolerance = 1e-6);

// Пакет крепежа: болт/винт, шайбы под головку (по порядку от головки к детали),
// шайбы под гайку (от детали к гайке), гайка. Определения — детали крепежа документа
// (addFastenerPart): нужны грани Fastener:Bearing, Fastener:Shank/Bore, у шайб Fastener:Top.
struct FastenerStack {
    DefinitionId screw = 0;
    std::vector<DefinitionId> headWashers;
    std::vector<DefinitionId> nutWashers;
    std::optional<DefinitionId> nut;
};

struct StackPlacement {
    OccurrenceId screw = 0;
    std::vector<OccurrenceId> headWashers;
    std::vector<OccurrenceId> nutWashers;
    std::optional<OccurrenceId> nut;
    std::vector<MateId> mates;
    double grip = 0.0;        // от опорной поверхности головки до торца гайки (Top), мм
    double protrusion = 0.0;  // выступ стержня за торец гайки (с гайкой), мм
};

// Ставит пакет в каждое отверстие: вхождения в положении, где сопряжения уже выполнены, и
// сопряжения «соосность оси с отверстием» + «совпадение опорной поверхности с предыдущей
// поверхностью пакета (AntiAligned)». Каждая деталь пакета сохраняет поворот вокруг оси
// (1 степень свободы). Ошибки: InvalidInput — нет нужной грани у определения, ось отверстия
// не перпендикулярна плоскости входа/выхода, у отверстия нет выхода при гайке или шайбах
// под гайку, стержень короче пакета (protrusion < 0); при ошибке документ не меняется.
kernel::Result<std::vector<StackPlacement>> insertFastenerStacks(AssemblyDocument& document,
                                                                 const FastenerStack& stack,
                                                                 const std::vector<HoleSite>& sites);

}  // namespace bendcore::assembly_v2::fasteners
