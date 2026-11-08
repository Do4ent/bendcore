#pragma once

// Документ сборки на геометрии OCCT (фаза 3, docs/assembly/ASSEMBLY_DOCUMENT.md):
// определения деталей и подсборок, вхождения с положениями, фиксация, сопряжения по ссылкам
// на грани/рёбра/вершины компонентов, решение сопряжений через bendcore_assembly_v2, замена
// компонента с переносом сопряжений, массивы компонентов (линейный, круговой, по отверстиям
// детали), импорт и экспорт STEP через bendcore_kernel_xcaf.

#include <bendcore/assembly_v2/occt_entities.hpp>
#include <bendcore/assembly_v2/solver.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/xcaf_io.hpp>
#include <bendcore/modeling/naming.hpp>

#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bendcore::modeling {
class PartDocument;
class FeatureRegistry;
}  // namespace bendcore::modeling

namespace bendcore::assembly_v2 {

using DefinitionId = std::uint32_t;
using OccurrenceId = std::uint32_t;
using PatternId = std::uint32_t;
// Путь вхождения: path[0] — вхождение верхнего уровня, path[k+1] — вхождение внутри
// определения path[k]. Последнее вхождение пути ссылается на деталь.
using OccurrencePath = std::vector<OccurrenceId>;
using Status = kernel::Result<std::monostate>;

// ---- геометрия детали и источники

// Геометрия детали: форма в собственной СК и, если есть, карта имён элементов документа
// детали (индексы карты совпадают с kernel::TopologyIndex той же формы).
struct PartData {
    TopoDS_Shape shape;
    std::shared_ptr<const modeling::NamedShape> elementMap;
};

// Загрузка детали по пути или ID документа детали.
using PartResolver = std::function<kernel::Result<PartData>(const std::string& source)>;

// Итоговая форма документа детали с картой элементов. Ошибка InvalidInput — пустая форма.
kernel::Result<PartData> partDataFromDocument(const modeling::PartDocument& document);

// Источник — путь к файлу (относительный — от baseDirectory):
//  .json — документ детали (PartDocument::fromJson с registry, перестроение, карта имён);
//  .step/.stp — kernel::readStep; .brep — BRep текстовый или бинарный (по первому байту).
// Ошибки: IoError (файл не читается), Unsupported (расширение), ошибки чтения формата.
PartResolver filePartResolver(std::shared_ptr<const modeling::FeatureRegistry> registry,
                              std::string baseDirectory = {});

// ---- структура документа

enum class DefinitionKind { Part, Assembly };

struct PartProperties {
    std::string designation;        // обозначение
    std::string name;               // наименование; пусто — имя определения
    std::string material;
    std::optional<double> density;  // кг/м³; без плотности масса не считается
};

struct Occurrence {
    OccurrenceId id = 0;
    std::string name;
    DefinitionId definition = 0;
    Transform placement;  // относительно родительской сборки
    bool fixed = false;   // только верхний уровень
    std::optional<kernel::AssemblyColor> color;
    std::optional<PatternId> pattern;  // экземпляр массива: положение задаёт массив
};

struct Definition {
    DefinitionId id = 0;
    DefinitionKind kind = DefinitionKind::Part;
    std::string name;
    PartProperties properties;
    // Деталь:
    TopoDS_Shape shape;
    std::shared_ptr<const modeling::NamedShape> elementMap;  // может отсутствовать
    std::shared_ptr<const kernel::TopologyIndex> topology;
    std::string source;     // путь/ID документа детали; пусто — форма задана напрямую
    std::uint32_t revision = 0;  // растёт при каждой замене геометрии
    std::optional<kernel::AssemblyColor> color;
    std::vector<kernel::FaceColor> faceColors;
    // Сборка:
    std::vector<Occurrence> occurrences;
};

// ---- ссылки на геометрию и сопряжения

struct ElementRef {
    ElementKind kind = ElementKind::Face;
    std::optional<std::uint32_t> index;  // индекс TopologyIndex формы детали
    std::string name;                    // имя из карты элементов (если index не задан)

    static ElementRef face(std::uint32_t i) { return {ElementKind::Face, i, {}}; }
    static ElementRef edge(std::uint32_t i) { return {ElementKind::Edge, i, {}}; }
    static ElementRef vertex(std::uint32_t i) { return {ElementKind::Vertex, i, {}}; }
    static ElementRef named(ElementKind kind, std::string name) { return {kind, std::nullopt, std::move(name)}; }
};

struct GeometryRef {
    OccurrencePath path;
    ElementRef element;
    EntityUse use = EntityUse::Auto;
};

struct MateSpec {
    MateType type = MateType::Coincident;
    GeometryRef a;  // Rigid: используется только a.path[0]
    GeometryRef b;  // Rigid: используется только b.path[0]
    double value = 0.0;
    Alignment alignment = Alignment::Closest;
    bool internalTangency = false;
    std::string name;

    static MateSpec coincident(GeometryRef a, GeometryRef b, Alignment al = Alignment::Closest);
    static MateSpec concentric(GeometryRef a, GeometryRef b, Alignment al = Alignment::Closest);
    static MateSpec distance(GeometryRef a, GeometryRef b, double value, Alignment al = Alignment::Closest);
    static MateSpec angle(GeometryRef a, GeometryRef b, double radians);
    static MateSpec parallel(GeometryRef a, GeometryRef b, Alignment al = Alignment::Closest);
    static MateSpec perpendicular(GeometryRef a, GeometryRef b);
    static MateSpec tangent(GeometryRef a, GeometryRef b, Alignment side = Alignment::Closest, bool internal = false);
    static MateSpec rigid(OccurrenceId a, OccurrenceId b);
};

// Разрешённая сторона сопряжения: индекс элемента, его сигнатура (в СК детали) и
// определение/ревизия, к которым привязана ссылка.
struct BoundElement {
    std::uint32_t index = 0;
    modeling::GeometricSignature signature;
    DefinitionId definition = 0;
    std::uint32_t revision = 0;
};

struct DocumentMate {
    MateId id = 0;
    MateSpec spec;
    std::array<BoundElement, 2> bound;
    std::optional<Transform> rigidRelative;  // Rigid: положение b в СК a
    std::string brokenReason;                // не пусто: ссылка не разрешилась, сопряжение не решается
    bool broken() const { return !brokenReason.empty(); }
};

// ---- решение

struct DocumentSubsystem {
    std::vector<OccurrenceId> occurrences;  // нефиксированные вхождения верхнего уровня
    std::vector<MateId> mates;
    SubsystemStatus status = SubsystemStatus::Solved;
    int dof = 0;
    int rank = 0;
    std::vector<MateId> redundantMates;
    std::vector<MateId> conflictingMates;
};

struct DocumentSolveReport {
    SolveResult solver;  // ID компонентов и сопряжений — внутренние ID решателя
    std::vector<DocumentSubsystem> subsystems;
    std::vector<MateId> brokenMates;  // пропущены
    int totalDof = 0;
    double residualNorm = 0.0;
    bool success = false;
};

// ---- замена компонента

struct ReplaceReport {
    std::vector<MateId> kept;      // ссылки не затронуты
    std::vector<MateId> remapped;  // элемент найден заново (имя, история, индекс, сигнатура)
    std::vector<std::pair<MateId, std::string>> broken;  // сопряжение помечено сломанным
    std::vector<PatternId> patternsUpdated;
};

// ---- массивы компонентов

enum class PatternKind { Linear, Circular, Feature };

const char* toString(PatternKind kind) noexcept;

// Направления и оси массива задаются в СК опорного вхождения (reference) или в мировой СК.
struct LinearPatternSpec {
    OccurrenceId seed = 0;
    std::optional<OccurrenceId> reference;
    Vec3 direction = Vec3::UnitX();
    double spacing = 0.0;
    int count = 2;  // вместе с исходным
    // Второе направление (count2 > 1 — двумерный массив count × count2).
    Vec3 direction2 = Vec3::UnitY();
    double spacing2 = 0.0;
    int count2 = 1;
};

struct CircularPatternSpec {
    OccurrenceId seed = 0;
    std::optional<OccurrenceId> reference;
    Vec3 axisPoint = Vec3::Zero();
    Vec3 axisDirection = Vec3::UnitZ();
    double angleStep = 0.0;  // рад
    int count = 2;           // вместе с исходным
};

// Массив по отверстиям детали: seedHole — цилиндрическая грань отверстия, к которому
// сопряжён исходный компонент; экземпляры встают в другие отверстия той же детали с осью,
// параллельной оси seedHole, и тем же радиусом (radius, если задан).
struct FeaturePatternSpec {
    OccurrenceId seed = 0;
    GeometryRef seedHole;
    std::optional<double> radius;
    double tolerance = 1e-6;  // мм и рад
};

struct ComponentPattern {
    PatternId id = 0;
    PatternKind kind = PatternKind::Linear;
    OccurrenceId seed = 0;
    std::optional<OccurrenceId> reference;
    std::variant<LinearPatternSpec, CircularPatternSpec, FeaturePatternSpec> spec;
    std::vector<OccurrenceId> instances;
    // Преобразования экземпляров L_i в СК опорного вхождения:
    // поза экземпляра = P_ref · L_i · P_ref⁻¹ · P_seed.
    std::vector<Transform> transforms;
    // Массив по отверстиям: разрешённая грань исходного отверстия.
    std::optional<BoundElement> holeBound;
};

// ---- развёртка

struct PlacedOccurrence {
    OccurrencePath path;
    DefinitionId definition = 0;
    std::string pathName;  // имена вхождений через '/'
    Transform world;
    TopoDS_Shape shape;  // форма детали в мировой СК
};

// ---- разнесённый вид

struct ExplodeStep {
    std::vector<OccurrenceId> occurrences;  // вхождения верхнего уровня
    Vec3 direction = Vec3::UnitZ();         // мировая СК
    double distance = 0.0;                  // мм
};

struct ExplodedView {
    std::string name;
    std::vector<ExplodeStep> steps;
};

class AssemblyDocument {
public:
    explicit AssemblyDocument(std::string name = "Assembly");

    DefinitionId root() const noexcept { return 0; }
    const std::string& name() const { return definitions_[0].name; }

    // ---- определения
    // Ошибки InvalidInput: пустая форма; карта элементов не от этой формы.
    kernel::Result<DefinitionId> addPart(std::string name, PartData data, PartProperties properties = {});
    kernel::Result<DefinitionId> addPartFromSource(std::string name, std::string source,
                                                   const PartResolver& resolver, PartProperties properties = {});
    kernel::Result<DefinitionId> addSubassembly(std::string name, PartProperties properties = {});
    Status setProperties(DefinitionId id, PartProperties properties);
    Status setDefinitionColor(DefinitionId id, std::optional<kernel::AssemblyColor> color);

    const Definition& definition(DefinitionId id) const { return definitions_.at(id); }
    const std::vector<Definition>& definitions() const { return definitions_; }
    bool hasDefinition(DefinitionId id) const { return id < definitions_.size(); }

    // ---- вхождения
    // parent — сборка (по умолчанию верхний уровень). Ошибки InvalidInput: неизвестное
    // определение, родитель не сборка, вставка корня, цикл подсборок.
    kernel::Result<OccurrenceId> addOccurrence(DefinitionId definition, const Transform& placement,
                                               std::string name = {}, std::optional<DefinitionId> parent = {});
    // Удаляет вхождение, сопряжения со ссылками через него и массивы, где оно исходное или опорное.
    Status removeOccurrence(OccurrenceId id);
    Status setPlacement(OccurrenceId id, const Transform& placement);
    Status setFixed(OccurrenceId id, bool fixed);  // только верхний уровень
    Status setOccurrenceColor(OccurrenceId id, std::optional<kernel::AssemblyColor> color);

    const Occurrence* findOccurrence(OccurrenceId id) const;
    // Сборка, в которой лежит вхождение.
    std::optional<DefinitionId> parentOf(OccurrenceId id) const;
    const std::vector<Occurrence>& topLevel() const { return definitions_[0].occurrences; }

    // Мировое положение конца пути и положение конца пути в СК path[0].
    kernel::Result<Transform> worldPlacement(const OccurrencePath& path) const;
    kernel::Result<Transform> placementInTopLevel(const OccurrencePath& path) const;

    // ---- сопряжения
    // Ошибки: InvalidInput (путь, элемент, имя не найдено, экземпляр массива), Unsupported
    // (геометрия элемента), ошибки prepareMate решателя.
    kernel::Result<MateId> addMate(MateSpec spec);
    Status removeMate(MateId id);
    const std::vector<DocumentMate>& mates() const { return mates_; }
    const DocumentMate* findMate(MateId id) const;

    // Сущность ссылки в СК вхождения верхнего уровня path[0] и в мировой СК.
    kernel::Result<Entity> resolveEntity(const GeometryRef& ref) const;
    kernel::Result<Entity> worldEntity(const GeometryRef& ref) const;

    // Решает сопряжения, записывает положения вхождений верхнего уровня и пересчитывает массивы.
    kernel::Result<DocumentSolveReport> solve(const SolverOptions& options = {});
    // Перетаскивание точки localPoint (СК вхождения) к target (мировая СК) с учётом сопряжений.
    kernel::Result<DocumentSolveReport> drag(OccurrenceId occurrence, const Vec3& localPoint, const Vec3& target,
                                             const SolverOptions& options = {});
    // Модель решателя без решения: компонент i — вхождение occurrenceOf[i].
    struct SolverModel {
        Assembly assembly;
        std::vector<OccurrenceId> occurrenceOf;
        std::map<MateId, MateId> documentMateOf;  // ID решателя → ID документа
        std::vector<MateId> brokenMates;  // помечены сломанными и пропущены
        // Отвергнуты решателем при построении (геометрия перестала подходить сопряжению).
        std::vector<std::pair<MateId, std::string>> rejected;
    };
    kernel::Result<SolverModel> buildSolverModel() const;

    // ---- замена компонента
    // Вхождение начинает ссылаться на другое определение того же вида. Ссылки сопряжений на
    // его элементы ищутся заново: по имени (карта элементов, история, сигнатура), по тому же
    // индексу с той же геометрией, по ближайшей сигнатуре. Неразрешимые — помечаются сломанными.
    kernel::Result<ReplaceReport> replaceOccurrenceDefinition(OccurrenceId id, DefinitionId definition);
    // Новая геометрия определения детали (все вхождения).
    kernel::Result<ReplaceReport> replacePartGeometry(DefinitionId id, PartData data);
    // Повторная загрузка детали из её источника.
    kernel::Result<ReplaceReport> reloadPart(DefinitionId id, const PartResolver& resolver);

    // ---- массивы
    kernel::Result<PatternId> addLinearPattern(const LinearPatternSpec& spec);
    kernel::Result<PatternId> addCircularPattern(const CircularPatternSpec& spec);
    kernel::Result<PatternId> addFeaturePattern(const FeaturePatternSpec& spec);
    Status removePattern(PatternId id);  // вместе с экземплярами
    const std::vector<ComponentPattern>& patterns() const { return patterns_; }
    // Пересчитывает преобразования и положения экземпляров всех массивов (вызывается solve).
    Status updatePatterns();

    // ---- развёртка
    // overrides — мировые положения вхождений верхнего уровня вместо текущих (разнесённый вид).
    kernel::Result<std::vector<PlacedOccurrence>> placedParts(
        const std::map<OccurrenceId, Transform>* overrides = nullptr) const;

    // ---- разнесённые виды
    std::size_t addExplodedView(ExplodedView view);
    const std::vector<ExplodedView>& explodedViews() const { return views_; }

    // ---- обмен STEP (структура, имена, цвета, положения; сопряжения в STEP не пишутся)
    kernel::Result<kernel::AssemblyDocument> toXcaf() const;
    static kernel::Result<AssemblyDocument> fromXcaf(const kernel::AssemblyDocument& xcaf);
    kernel::Result<std::string> exportStep(const kernel::AssemblyWriteOptions& options = {}) const;
    static kernel::Result<AssemblyDocument> importStep(std::string_view step);

private:
    Occurrence* findOccurrenceMutable(OccurrenceId id);
    bool containsDefinition(DefinitionId container, DefinitionId target) const;
    kernel::Result<DefinitionId> terminalDefinition(const OccurrencePath& path) const;
    Status bindSide(const GeometryRef& ref, BoundElement& bound, bool requireElement) const;
    // Перепроверяет все сопряжения после изменения определений.
    void revalidateMates(ReplaceReport& report);
    kernel::Result<std::vector<Transform>> computePatternTransforms(const ComponentPattern& pattern) const;
    Status syncPatternInstances(ComponentPattern& pattern, std::vector<Transform> transforms);
    kernel::Result<PatternId> addPattern(PatternKind kind, OccurrenceId seed, std::optional<OccurrenceId> reference,
                                         std::variant<LinearPatternSpec, CircularPatternSpec, FeaturePatternSpec> spec);
    void collectPlaced(DefinitionId def, const Transform& world, OccurrencePath& path, const std::string& prefix,
                       const std::map<OccurrenceId, Transform>* overrides,
                       std::vector<PlacedOccurrence>& out) const;
    kernel::Result<std::uint32_t> resolveIndex(const GeometryRef& ref) const;
    kernel::Result<Entity> entityForBound(const GeometryRef& ref, const BoundElement& bound) const;
    kernel::Result<BoundElement> remapSide(const GeometryRef& ref, const BoundElement& old,
                                           DefinitionId newDefinition) const;
    kernel::Result<Mate> solverMate(const DocumentMate& mate, ComponentId a, ComponentId b) const;

    std::vector<Definition> definitions_;
    std::vector<DocumentMate> mates_;
    std::vector<ComponentPattern> patterns_;
    std::vector<ExplodedView> views_;
    OccurrenceId nextOccurrence_ = 1;
    MateId nextMate_ = 1;
    PatternId nextPattern_ = 1;
};

// Положение ↔ TopLoc_Location. Ошибка Unsupported — масштаб или отражение.
TopLoc_Location toLocation(const Transform& t);
kernel::Result<Transform> fromLocation(const TopLoc_Location& location);

// Мировые положения вхождений верхнего уровня в разнесённом виде: шаг s (с 0) выполнен на
// clamp(progress − s, 0, 1); смещения шагов суммируются. Документ не меняется.
kernel::Result<std::map<OccurrenceId, Transform>> explodedPlacements(const AssemblyDocument& document,
                                                                     const ExplodedView& view, double progress);

}  // namespace bendcore::assembly_v2
