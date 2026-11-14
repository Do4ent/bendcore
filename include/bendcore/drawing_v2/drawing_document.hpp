#pragma once

// Документ чертежа (фаза 4 docs/ROADMAP_GENERAL_CAD.md — сквозной результат): ссылка на
// документ детали или сборки (путь + хэш результата), листы ГОСТ 2.301 с основной надписью
// ГОСТ 2.104 (поля — из свойств модели), виды ГОСТ 2.305 (основные, проекционные,
// аксонометрия ГОСТ 2.317, разрезы, выносные элементы), размеры ГОСТ 2.307 с привязкой к
// элементам модели, шероховатость ГОСТ 2.309, технические требования ГОСТ 2.316, позиции
// ГОСТ 2.109 по составу сборки; спецификация ГОСТ 2.106 — отдельный документ.
// Обновление при изменении модели (виды через кэш, переразрешение ссылок, отчёт об устаревших
// и висячих элементах), формат JSON с версией и миграцией, экспорт комплекта PDF/DXF/SVG.
// Документация — docs/drawing/DRAWING_DOCUMENT.md.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/annotations.hpp>
#include <bendcore/drawing_v2/dimensions.hpp>
#include <bendcore/drawing_v2/specification.hpp>
#include <bendcore/drawing_v2/threads.hpp>
#include <bendcore/drawing_v2/title_block.hpp>
#include <bendcore/drawing_v2/view_cache.hpp>
#include <bendcore/drawing_v2/view_sheet.hpp>
#include <bendcore/modeling/naming.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::drawing_v2::document {

// ---------------------------------------------------------------- модель

enum class ModelKind { Part, Assembly };
const char* toString(ModelKind kind) noexcept;

// Свойства изделия для основной надписи (графы 1, 2, 3, 5 ГОСТ 2.104).
struct ModelProperties {
    std::string designation;
    std::string name;
    std::string material;
    std::optional<double> density;  // кг/м³; без плотности масса детали не определяется
};

// Ссылка документа чертежа на модель.
struct ModelLink {
    ModelKind kind = ModelKind::Part;
    std::string path;          // файл документа детали/сборки (относительно файла чертежа)
    std::string resultHash;    // хэш формы результата модели при последнем обновлении (16 hex)
    std::uint32_t revision = 0;  // число обновлений, при которых модель изменилась
    // Деталь: свойства для основной надписи (у документа детали их нет). Сборка: не
    // используются — берутся из свойств корневого определения документа сборки.
    ModelProperties properties;
};

// Состояние модели для обновления чертежа.
struct ModelSnapshot {
    ModelKind kind = ModelKind::Part;
    kernel::Shape shape;  // деталь — итоговое тело; сборка — компаунд деталей в мировой СК
    std::shared_ptr<const modeling::NamedShape> named;  // карта имён (индексы = TopologyIndex)
    bool historyNames = false;  // имена из истории операций (деталь) или только сигнатуры (сборка)
    ModelProperties properties;
    std::optional<double> massKg;
    std::uint64_t contentHash = 0;  // views::shapeContentHash(shape)
    // Сборка:
    std::shared_ptr<const assembly_v2::AssemblyDocument> assembly;
    // Для каждого ребра формы (индекс TopologyIndex) — определение детали, которой оно
    // принадлежит (по развёртке placedParts).
    std::vector<assembly_v2::DefinitionId> edgeOwner;
    // Для каждой дочерней формы компаунда (TopoDS_Iterator, порядок развёртки) — определение
    // детали (условности разрезов сборок, ГОСТ 2.305, разд. 9).
    std::vector<assembly_v2::DefinitionId> childOwner;
};

// Снимок документа детали: итоговая форма с картой имён; масса = объём · плотность.
// Ошибки: InvalidInput — у документа нет тела.
Result<ModelSnapshot> snapshotPart(const modeling::PartDocument& document, const ModelProperties& properties);

// Снимок документа сборки: компаунд деталей (подавленные вхождения не входят), свойства
// корневого определения, масса — по BOM (если известна плотность всех деталей).
Result<ModelSnapshot> snapshotAssembly(std::shared_ptr<const assembly_v2::AssemblyDocument> document);

using ModelLoader = std::function<Result<ModelSnapshot>(const ModelLink&)>;

// Загрузка модели по ModelLink::path относительно baseDirectory: деталь — JSON документа
// детали (modeling::partRegistry()), сборка — AssemblyDocument::loadFile.
ModelLoader fileModelLoader(std::string baseDirectory);

std::string hashHex(std::uint64_t hash);

// Условности разрезов сборок (ГОСТ 2.305-2008, разд. 9; ГОСТ 2.109-73): как деталь показывается
// в разрезе.
enum class SectionRule {
    Cut,                    // рассекается
    WholeInLongitudinal,    // при продольном разрезе — нерассечённой (крепёж, валы, оси)
    NeverCut,               // всегда нерассечённой (шарики)
};
// Правило детали: PartProperties::sectionCut (true — Cut, false — WholeInLongitudinal), иначе по
// умолчанию: наименование или имя начинается со слова «Шарик»/«Ball» — NeverCut; стандартное
// изделие (обозначение пустое — так в спецификацию попадают изделия библиотеки крепежа) или
// наименование «Болт», «Винт», «Шпилька», «Гайка», «Шайба», «Штифт», «Шпонка», «Заклёпка», «Вал»,
// «Ось», «Шпиндель» (и англ. bolt, screw, stud, nut, washer, pin, key, rivet, shaft, axle,
// spindle) — WholeInLongitudinal; прочие — Cut.
SectionRule sectionRule(const assembly_v2::PartProperties& properties, const std::string& definitionName);

// Разрез продольный для детали: ось детали (ось цилиндрической грани наибольшей площади) лежит в
// секущей плоскости (|ось · нормаль| ≤ 0,1). Деталь без цилиндров — продольный (true).
bool longitudinalSection(const TopoDS_Shape& part, views::Vec3 sight);

// Части снимка для разреза с направлением взгляда sight: деталь — одна часть; сборка — дочерние
// формы компаунда по childOwner, keepWhole — по sectionRule и longitudinalSection.
views::SectionParts sectionParts(const ModelSnapshot& model, views::Vec3 sight);

// Ссылка на элемент модели по индексу TopologyIndex её формы (имя + сигнатура; у сборки — только
// сигнатура). Ошибка InvalidInput — индекс вне формы.
Result<modeling::TopoReference> elementReference(const ModelSnapshot& model, modeling::ElementKind kind,
                                                 std::uint32_t index);

// ---------------------------------------------------------------- листы и виды

struct SheetFormat {
    GostFormat base = GostFormat::A3;
    int multiplier = 1;
    Orientation orientation = Orientation::Landscape;
};

// Графы основной надписи, не связанные с моделью. Пустые designation/name/material/mass
// заполняются из модели; масштаб, «Лист», «Листов» — всегда вычисляются.
struct TitleFields {
    std::string designation;
    std::string name;
    std::string material;
    std::string mass;
    std::string litera;
    std::string organization;
    SignatureRow developer, checker, techControl, normControl, approver;
};

enum class ViewKind {
    Base,         // главный вид: ориентация toward/right
    Projected,    // проекционный вид от parent со стороны side (ГОСТ 2.305, разд. 5)
    Axonometric,  // аксонометрия ГОСТ 2.317 относительно parent
    Section,      // разрез/сечение (ГОСТ 2.305, разд. 6, 7)
    Detail,       // выносной элемент parent (ГОСТ 2.305, разд. 8)
};
const char* toString(ViewKind kind) noexcept;

struct ViewDef {
    std::string id;
    ViewKind kind = ViewKind::Base;
    // Base и Section без side: ориентация (toward — к наблюдателю).
    views::Vec3 toward{0, -1, 0};
    views::Vec3 right{1, 0, 0};
    std::string parent;
    // Projected — обязательно; Section — вид в проекционной связи с parent (ориентация как у
    // проекционного вида), без side — вне связи (ориентация toward/right).
    std::optional<views::SheetSide> side;
    views::Axonometry axonometry = views::Axonometry::Isometric;
    // Section: точка секущей плоскости; направление взгляда — −toward вида разреза.
    views::Vec3 planePoint{0, 0, 0};
    views::SectionKind sectionKind = views::SectionKind::Cut;
    std::string markView;  // Section: вид, на котором обозначается секущая плоскость
    // Section: ступенчатый разрез (ГОСТ 2.305, разд. 6) — ещё точки плоскостей, параллельных
    // первой (planePoint); уступы — посередине между соседними точками вдоль следа на markView.
    std::vector<views::Vec3> stepPoints;
    // Base/Projected: местные разрезы (ГОСТ 2.305, разд. 6) — замкнутые области в координатах
    // этого вида (мм модели) и глубина от ближайшей к наблюдателю точки модели.
    std::vector<views::LocalSection> localSections;
    // Detail: окружность в координатах вида parent (мм модели).
    Point2 detailCenter;
    double detailRadius = 0.0;
    std::string letter;  // Section/Detail: пусто — по порядку букв ГОСТ 2.316
    // Масштаб вида вне проекционной связи (аксонометрия, разрез без side, выносной элемент);
    // пусто — масштаб листа (выносной — ближайший ряда ГОСТ 2.302 не меньше 2× листа), если
    // вид не помещается — наибольший помещающийся.
    std::optional<views::ScaleRatio> scale;
    // Центр габарита вида на листе (мм); для главного вида — центр проекционной группы.
    // Пусто — автоматическая компоновка.
    std::optional<Point2> position;
    bool hidden = true;       // невидимые линии
    bool centerLines = true;  // осевые и центровые
    // Условная резьба модели по ГОСТ 2.311 (annotations::threadImages): на видах, где ось резьбы
    // параллельна или перпендикулярна плоскости вида (кроме выносных элементов и аксонометрии).
    bool threads = true;
    bool autoBalloons = false;  // позиции ГОСТ 2.109 по составу сборки на этом виде
    views::HlrMethod method = views::HlrMethod::Exact;
    bool autoMethod = false;  // выбор алгоритма HLR по геометрии (views::chooseHlrMethod)
};

// Размер на виде. Ссылки — ModelRef::persistent (имя + сигнатура); индекс вычисляется при
// обновлении (annotations::rebindRef).
struct DimensionItem {
    std::string id;
    std::string view;
    annotations::DimensionDef def;
    std::optional<double> lastValue;  // значение при предыдущем обновлении (сохраняется в файле)
};

// Знак шероховатости на линии вида — изображении ребра модели (ГОСТ 2.309).
struct RoughnessItem {
    std::string id;
    std::string view;
    modeling::TopoReference edge;
    double at = 0.5;  // положение вершины знака вдоль линии, 0…1
    annotations::RoughnessSpec spec;
};

// Допуск формы или расположения ГОСТ 2.308 на линии вида — изображении ребра модели: рамка
// снаружи вида, соединительная линия со стрелкой на линии поверхности в доле at её длины.
struct ToleranceItem {
    std::string id;
    std::string view;
    modeling::TopoReference edge;
    double at = 0.5;
    annotations::FeatureControlFrame frame;
};

// Обозначение базы ГОСТ 2.308 на линии вида: зачернённый треугольник на линии поверхности
// (снаружи вида) в доле at её длины, рамка с буквой.
struct DatumItem {
    std::string id;
    std::string view;
    modeling::TopoReference edge;
    double at = 0.5;
    std::string letter;
};

// Номер позиции ГОСТ 2.109, поставленный вручную на изображение составной части сборки: точка
// выноски — на видимой линии ребра edge (середина самого длинного прямого участка, середина
// дуги), полка — в точке at листа (пусто — на 12 мм правее и 8 мм выше точки выноски). Номер —
// позиция определения детали, которой принадлежит ребро, по спецификации (при каждом
// обновлении). Автоматические позиции вида (ViewDef::autoBalloons) не ставятся для
// определений, у которых на листе есть ручная позиция.
struct BalloonItem {
    std::string id;
    std::string view;
    modeling::TopoReference edge;
    std::optional<Point2> at;
};

// Размер резьбы с обозначением (ГОСТ 2.311, п. 5) на виде: ссылка — грань резьбы (стенка
// резьбового отверстия, ThreadInfo::name); надпись — threadDesignationText с полем допуска.
struct ThreadNoteItem {
    std::string id;
    std::string view;
    modeling::TopoReference face;
    annotations::ThreadDimensionDef def;
};

struct SheetDef {
    std::string id;
    SheetFormat format;
    std::optional<views::ScaleRatio> scale;  // масштаб листа; пусто — наибольший помещающийся
    TitleFields title;
    std::vector<ViewDef> views;
    std::vector<DimensionItem> dimensions;
    std::vector<RoughnessItem> roughness;
    std::vector<ToleranceItem> tolerances;
    std::vector<DatumItem> datums;
    std::vector<BalloonItem> balloons;  // ручные позиции (сборка)
    std::vector<ThreadNoteItem> threadNotes;  // размеры резьбы
    std::optional<annotations::RoughnessSpec> roughnessRemainder;  // знак в правом верхнем углу
    std::vector<std::string> technicalRequirements;
    bool partsList = false;  // перечень составных частей ISO 7573 над основной надписью (сборка)
};

struct DrawingSettings {
    double lineThickness = 0.5;  // s, ГОСТ 2.303
    views::ProjectionMethod projection = views::ProjectionMethod::FirstAngle;
    double viewGap = 30.0;   // зазор между видами в проекционной связи, мм листа (место размеров)
    double margin = 15.0;    // отступ области видов от рамки, мм
    views::ScaleRatio maxScale{5, 1};  // наибольший масштаб при автоматическом выборе
    bool reducedAxonometry = true;     // приведённые коэффициенты ГОСТ 2.317 (1 по осям)
};

// ---------------------------------------------------------------- результат обновления

struct ComputedView {
    ViewDef def;  // определение (родитель, плоскость, окружность выносного элемента)
    std::string id;
    ViewKind kind = ViewKind::Base;
    views::SheetView sheetView;  // рёбра вида, масштаб, привязка на листе
    std::shared_ptr<const views::SectionView> section;  // для Section
    views::ScaleRatio scale;
    std::string label;  // «А–А», «Б (4:1)»; пусто — без надписи
    std::string letter;
    views::CenterGeometry centers;
    std::vector<annotations::ThreadImage> threads;  // резьбы ГОСТ 2.311 на виде
    bool fromCache = false;
};

struct ComputedSheet {
    std::string id;
    Sheet sheet;
    views::ScaleRatio scale;
    int index = 1;  // номер листа
    int count = 1;  // листов в чертеже
    TitleBlockData title;
    std::vector<ComputedView> views;
    std::vector<annotations::DimensionResult> dimensions;  // по SheetDef::dimensions
    std::vector<std::optional<annotations::SymbolGeometry>> roughness;  // nullopt — висячий
    std::vector<std::optional<annotations::SymbolGeometry>> tolerances;  // по SheetDef::tolerances
    std::vector<std::optional<annotations::SymbolGeometry>> datums;      // по SheetDef::datums
    std::vector<annotations::Balloon> balloons;  // автоматические позиции видов
    // По SheetDef::balloons: номер, точка выноски и полка; nullopt — висячая позиция (ребро не
    // найдено или не видно, у его детали нет позиции в спецификации).
    std::vector<std::optional<annotations::Balloon>> manualBalloons;
    // По SheetDef::threadNotes; Dangling — грань не найдена, не резьба или резьба не на виде.
    std::vector<annotations::DimensionResult> threadNotes;
    std::optional<annotations::Specification> specification;  // для перечня ISO 7573
};

struct DimensionReport {
    std::string sheet;
    std::string id;
    annotations::DimensionState state = annotations::DimensionState::Dangling;
    std::optional<double> oldValue;
    double newValue = 0.0;
    bool changed = false;  // значение отличается от предыдущего (> 1e-9)
    modeling::ResolveMethod method = modeling::ResolveMethod::Broken;  // худший способ разрешения ссылок
    std::string reason;
};

struct BalloonReport {
    std::string sheet;
    std::string view;
    std::vector<std::pair<int, assembly_v2::DefinitionId>> placed;  // номер позиции → определение
    std::vector<assembly_v2::DefinitionId> withoutBalloon;  // составные части, не видимые на виде
};

struct UpdateReport {
    bool modelChanged = false;
    std::string previousHash;
    std::string currentHash;
    std::uint32_t revision = 0;
    std::size_t viewsComputed = 0;   // HLR/разрезы, рассчитанные заново
    std::size_t viewsFromCache = 0;  // взятые из кэша
    std::vector<DimensionReport> dimensions;
    std::vector<std::string> stale;     // «S1/L: 60 → 80» — размеры с изменившимся значением
    std::vector<std::string> dangling;  // «S1/D: причина» — размеры и знаки без элемента модели
    std::vector<BalloonReport> balloons;
    std::vector<std::string> warnings;
};

// Проверка без обновления: изменилась ли модель с последнего обновления.
struct StalenessReport {
    bool modelChanged = false;
    std::string storedHash;
    std::string currentHash;
    std::vector<std::string> staleViews;  // «S1/F» — виды, рассчитанные по прежней модели
    std::vector<std::string> dimensions;  // размеры, которые будут пересчитаны
};

// ---------------------------------------------------------------- документ

class DrawingDocument {
public:
    // Версия 2: модель — объект {kind, path, resultHash, revision, properties}, формат листа —
    // объект; версия 1 (миграция при чтении): модель — строка пути + "modelKind", формат —
    // строка «A3L»/«A4P», ссылки размеров — только имена.
    static constexpr int kFormatVersion = 2;

    ModelLink& model() { return model_; }
    const ModelLink& model() const { return model_; }
    DrawingSettings& settings() { return settings_; }
    const DrawingSettings& settings() const { return settings_; }
    std::vector<SheetDef>& sheets() { return sheets_; }
    const std::vector<SheetDef>& sheets() const { return sheets_; }
    SheetDef* findSheet(const std::string& id);

    // Проверка определений: уникальность идентификаторов, связи видов (parent/markView —
    // существующий вид того же листа, объявленный раньше), вид размера существует.
    Status validate() const;

    StalenessReport checkModel(const ModelSnapshot& model) const;

    // Шрифт для ширины текстов размеров и знаков при обновлении (полки, размещение числа);
    // тот же шрифт следует передавать в render. Не сохраняется в файле.
    void setFont(const TrueTypeFont* font) { font_ = font; }

    // Пересчёт: виды — через кэш (HLR — ViewCache, разрезы — кэш документа по хэшу формы и
    // параметрам), компоновка и масштабы, переразрешение ссылок размеров и знаков, позиции по
    // спецификации (spec; для сборки без spec — спецификация по умолчанию), основная надпись.
    Result<UpdateReport> update(const ModelSnapshot& model, views::ViewCache& cache,
                                const annotations::Specification* spec = nullptr);
    Result<UpdateReport> update(const ModelLoader& loader, views::ViewCache& cache,
                                const annotations::Specification* spec = nullptr);

    bool computed() const { return !computed_.empty(); }
    const std::vector<ComputedSheet>& computedSheets() const { return computed_; }

    // Листы чертежа (рамка, основная надпись форм 1/2а, виды, обозначения, размеры, знаки,
    // позиции, технические требования). Ошибка InvalidInput — документ не обновлён.
    Result<std::vector<Drawing>> render(const TrueTypeFont* font) const;

    Result<std::string> toJson() const;
    static Result<DrawingDocument> fromJson(std::string_view json, int* loadedVersion = nullptr);
    Status saveFile(const std::string& path) const;
    static Result<DrawingDocument> loadFile(const std::string& path, int* loadedVersion = nullptr);

private:
    ModelLink model_;
    DrawingSettings settings_;
    std::vector<SheetDef> sheets_;
    // Вычисленное состояние (не сохраняется).
    std::vector<ComputedSheet> computed_;
    std::map<std::uint64_t, std::shared_ptr<const views::SectionView>> sectionCache_;
    const TrueTypeFont* font_ = nullptr;
};

// ---------------------------------------------------------------- отмена и повтор

// Состояние правки документа чертежа: всё, что пользователь меняет в чертеже (листы, виды,
// размеры и знаки, основная надпись, настройки, свойства изделия для основной надписи). Модель
// (параметры детали, состав сборки) — не входит: её изменение приходит обновлением по модели.
struct DrawingEditState {
    std::vector<SheetDef> sheets;
    DrawingSettings settings;
    ModelProperties properties;
};
DrawingEditState captureEditState(const DrawingDocument& doc);
void restoreEditState(DrawingDocument& doc, const DrawingEditState& state);

// История правок (транзакций) документа чертежа: record — состояние до успешной правки и её
// название; undo/redo — восстановление определений (после него нужен update). Новая правка
// очищает повтор; глубина ограничена limit (старые записи отбрасываются).
class DrawingHistory {
public:
    explicit DrawingHistory(std::size_t limit = 100) : limit_(limit == 0 ? 1 : limit) {}
    void record(DrawingEditState before, std::string label);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    std::string undoLabel() const { return undo_.empty() ? std::string() : undo_.back().label; }
    std::string redoLabel() const { return redo_.empty() ? std::string() : redo_.back().label; }
    std::size_t undoDepth() const { return undo_.size(); }
    std::size_t redoDepth() const { return redo_.size(); }
    // Восстанавливает состояние до последней правки; текущее уходит в повтор. Возвращает название
    // отменённой правки. Ошибка InvalidInput — отменять нечего. Если обновление после undo не
    // удалось, redo возвращает документ в прежнее состояние (и наоборот).
    Result<std::string> undo(DrawingDocument& doc);
    Result<std::string> redo(DrawingDocument& doc);
    void clear();

private:
    struct Entry {
        DrawingEditState state;
        std::string label;
    };
    std::size_t limit_;
    std::vector<Entry> undo_, redo_;
};

// ---------------------------------------------------------------- спецификация

// Настройки спецификации; ключ составной части — её обозначение, а если его нет, наименование.
struct SpecificationSettings {
    std::map<std::string, std::string> formats;
    std::map<std::string, std::string> notes;
    std::map<std::string, annotations::SpecSection> sections;
    std::vector<annotations::SpecItem> documents;
    bool addAssemblyDrawing = true;
    std::string assemblyDrawingFormat = "А3";
    int reservePositions = 0;
};

// Спецификация ГОСТ 2.106 как отдельный документ (свой файл "bendcore-specification").
class SpecificationDocument {
public:
    static constexpr int kFormatVersion = 1;

    ModelLink& model() { return model_; }
    const ModelLink& model() const { return model_; }
    SpecificationSettings& settings() { return settings_; }
    const SpecificationSettings& settings() const { return settings_; }
    TitleFields& title() { return title_; }
    const TitleFields& title() const { return title_; }

    // Состав из BOM сборки. Ошибки: модель — не сборка; ошибки buildSpecification.
    // Возвращает true, если состав (позиции, обозначения, количества) изменился.
    Result<bool> update(const ModelSnapshot& model);
    const annotations::Specification* specification() const { return spec_ ? &*spec_ : nullptr; }

    // Листы формы 1 (А4, основная надпись форм 2 и 2а). Ошибка — документ не обновлён.
    Result<annotations::SpecDocument> render(const TrueTypeFont* font) const;

    Result<std::string> toJson() const;
    static Result<SpecificationDocument> fromJson(std::string_view json);

private:
    ModelLink model_;
    SpecificationSettings settings_;
    TitleFields title_;
    std::optional<annotations::Specification> spec_;
};

// ---------------------------------------------------------------- экспорт комплекта

struct ExportKitOptions {
    std::string directory;
    std::string baseName = "drawing";
    const TrueTypeFont* font = nullptr;  // обязателен (тексты основной надписи)
    bool pdf = true;
    bool dxf = true;
    bool svg = true;
};

struct ExportedFile {
    std::string path;
    std::string format;  // "pdf", "dxf", "svg"
    std::string sheet;   // идентификатор листа чертежа, «SP<n>» — лист спецификации, пусто — PDF
    std::size_t bytes = 0;
};

struct ExportKitResult {
    std::vector<ExportedFile> files;
    int drawingPages = 0;
    int specificationPages = 0;
};

// Комплект: один многостраничный PDF (листы чертежа, затем листы спецификации) и по файлу DXF
// и SVG на каждый лист: <base>.pdf, <base>_<лист>.dxf/.svg, <base>_SP<n>.dxf/.svg.
Result<ExportKitResult> exportKit(const DrawingDocument& drawing, const SpecificationDocument* specification,
                                  const ExportKitOptions& options);

}  // namespace bendcore::drawing_v2::document
