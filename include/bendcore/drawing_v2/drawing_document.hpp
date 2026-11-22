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
#include <bendcore/drawing_v2/gears.hpp>
#include <bendcore/drawing_v2/group_documents.hpp>
#include <bendcore/drawing_v2/bearings.hpp>
#include <bendcore/drawing_v2/balloon_layout.hpp>
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
#include <set>
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
    // Не сохраняется: загрузчик снимает все исполнения детали (snapshotPartConfigurations) —
    // DrawingDocument::update(loader) ставит по DrawingSettings::group.
    bool withConfigurations = false;
};

struct ModelSnapshot;

// Исполнение детали для группового чертежа ГОСТ 2.113 (строка таблицы параметров,
// docs/modeling/PART_DOCUMENT.md, «Таблица параметров»).
struct ConfigurationSnapshot {
    std::string designation;      // как в таблице («-01»)
    std::string fullDesignation;  // modeling::fullDesignation
    std::string name;             // наименование строки (пусто — как у детали)
    std::string material;         // материал исполнения (ячейка материала или материал детали)
    std::optional<double> massKg;
    std::vector<bool> suppressed;  // по ModelSnapshot::configurationFeatures: операция подавлена
    std::shared_ptr<const ModelSnapshot> model;  // снимок детали в этом исполнении
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
    // Для каждой дочерней формы — определения вхождений её пути от верхнего уровня до детали
    // (последнее = childOwner); для каждого ребра — индекс дочерней формы (UINT32_MAX — нет).
    // Номер позиции ребра — у первого определения цепочки, которое есть в спецификации:
    // подсборка-сборочная единица получает позицию по линиям своих деталей (ГОСТ 2.109).
    std::vector<std::vector<assembly_v2::DefinitionId>> childChain;
    std::vector<std::uint32_t> edgeChild;
    std::vector<std::uint32_t> faceChild;  // для каждой грани — индекс дочерней формы (UINT32_MAX — нет)
    // Зубчатые венцы детали (modeling::gearsOf исходной формы). shape и named — условная форма
    // ГОСТ 2.402 (впадины заполнены до окружности вершин, annotations::conventionalGearModel);
    // масса — по исходной форме с зубьями. Сборка — венцы всех деталей в мировой СК (ось
    // венца перенесена положением вхождения), gearChild — дочерняя форма каждого венца.
    std::vector<modeling::GearInfo> gears;
    std::vector<std::size_t> gearChild;
    // Сборка: подшипники качения (annotations::recognizeBearing — по телам качения в геометрии
    // или по наименованию «Подшипник …»), изображаются условно по ГОСТ 2.420.
    std::vector<annotations::BearingInfo> bearings;
    // Групповой чертёж (snapshotPartConfigurations): исполнения по порядку таблицы параметров
    // (первое — базовое; сам снимок — базовое исполнение) и операции столбцов подавления
    // таблицы (id, подпись).
    std::vector<ConfigurationSnapshot> configurations;
    std::vector<std::pair<std::string, std::string>> configurationFeatures;
};

// Определение, номер позиции которого ставится на ребре edge: первое определение цепочки
// вхождений (от верхнего уровня), имеющее позицию в spec; nullopt — такого нет (деталь «разборной»
// подсборки, которой нет и в спецификации, или ребро вне развёртки).
std::optional<assembly_v2::DefinitionId> positionOwner(const ModelSnapshot& model, std::uint32_t edge,
                                                       const annotations::Specification& spec);

// Снимок документа детали: итоговая форма с картой имён; масса = объём · плотность.
// Ошибки: InvalidInput — у документа нет тела.
Result<ModelSnapshot> snapshotPart(const modeling::PartDocument& document, const ModelProperties& properties);

// Снимок всех исполнений документа детали для группового чертежа ГОСТ 2.113: копия документа
// (сохранение с кэшем и загрузка; сам документ не меняется) перестраивается в каждом исполнении
// таблицы параметров. Результат — снимок базового исполнения (первая строка) с
// ModelSnapshot::configurations. Материал исполнения — ячейка материала (имя и плотность из
// kernel::builtInMaterials()), иначе properties. Деталь без таблицы — обычный снимок без
// исполнений. Ошибки: исполнение не строится («configuration '-01': feature 'Hole' failed»).
Result<ModelSnapshot> snapshotPartConfigurations(const modeling::PartDocument& document,
                                                 const ModelProperties& properties);

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
    // Поля основной надписи ISO 7200 (SheetStyle::Iso7200), которых нет в форме ГОСТ 2.104.
    // Остальные поля ISO берутся из граф ЕСКД: владелец — organization, обозначение — designation,
    // наименование — name, автор — developer.surname, утвердивший — approver.surname.
    std::string documentType;    // пусто — «Чертёж детали» / «Сборочный чертёж» (по языку)
    std::string documentStatus;
    std::string revision;
    std::string issueDate;       // ГГГГ-ММ-ДД (ISO 8601)
    std::string language;        // ISO 639; пусто — «ru»
    std::string department;
};

// Место над изображением под надпись разреза и выносного элемента («А–А», «Б (4:1)»), мм листа:
// ViewDef::position — центр габарита изображения вместе с этим местом.
inline constexpr double kViewLabelRoom = 14.0;

enum class ViewKind {
    Base,         // главный вид: ориентация toward/right
    Projected,    // проекционный вид от parent со стороны side (ГОСТ 2.305, разд. 5)
    Axonometric,  // аксонометрия ГОСТ 2.317 относительно parent
    Section,      // разрез/сечение (ГОСТ 2.305, разд. 6, 7)
    Detail,       // выносной элемент parent (ГОСТ 2.305, разд. 8)
    // Дополнительный вид (ГОСТ 2.305, п. 5.4–5.6; ISO 128-30): направление взгляда −toward,
    // стрелка с буквой на parent, надпись «А» над видом (повернутый — со знаком «повернуто»);
    // вне проекционной связи.
    Auxiliary,
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
    std::string letter;  // Section/Detail/Auxiliary: пусто — по порядку букв ГОСТ 2.316
    // Auxiliary: повернуть вид («вверх» parent — вверх листа, views::auxiliaryViewFrame).
    bool rotated = false;
    // Масштаб вида вне проекционной связи (аксонометрия, разрез без side, выносной элемент);
    // пусто — масштаб листа (выносной — ближайший ряда ГОСТ 2.302 не меньше 2× листа), если
    // вид не помещается — наибольший помещающийся.
    std::optional<views::ScaleRatio> scale;
    // Центр габарита вида на листе (мм); для главного вида — центр проекционной группы.
    // Пусто — автоматическая компоновка.
    std::optional<Point2> position;
    bool hidden = true;       // невидимые линии
    bool centerLines = true;  // осевые и центровые
    // Условная резьба модели по ГОСТ 2.311 (annotations::threadImages): виды сбоку и вдоль оси,
    // аксонометрия (эллипс у входа), выносной элемент (отсечение окружностью элемента).
    bool threads = true;
    // Условное изображение зубчатых венцов модели по ГОСТ 2.402 (annotations::gearImages):
    // виды сбоку и с торца, разрезы через ось (зуб не рассекается).
    bool gears = true;
    // Подшипники качения сборки (ModelSnapshot::bearings) в разрезе через ось — условно по
    // ГОСТ 2.420: контур сечения без штриховки с диагоналями или символом тела качения.
    bool bearings = true;
    annotations::BearingSymbol bearingSymbol = annotations::BearingSymbol::Diagonals;
    // Позиции ГОСТ 2.109 по составу сборки на этом виде: составная часть получает одну позицию на
    // листе — на первом таком виде, где она видна (линиями или фигурой сечения), а скрытая — на
    // ближайшем виде листа с разрезом, где видна. Полки — annotations::arrangeBalloons по линии
    // balloonLine.
    bool autoBalloons = false;
    annotations::BalloonLine balloonLine = annotations::BalloonLine::Auto;
    // Полки автоматических позиций этого вида, перенесённые мышью: определение → начало полки
    // (мм листа). Такая позиция не разносится на линию полок, точка выноски — ближайшая к полке
    // точка изображения части; DrawingDocument::arrangeBalloons очищает список.
    std::map<assembly_v2::DefinitionId, Point2> balloonShelves;
    views::HlrMethod method = views::HlrMethod::Exact;
    bool autoMethod = false;  // выбор алгоритма HLR по геометрии (views::chooseHlrMethod)
};

// Размер на виде. Ссылки — ModelRef::persistent (имя + сигнатура); индекс вычисляется при
// обновлении (annotations::rebindRef).
// Размеры цепочкой и от общей базы (ГОСТ 2.307-2011): размеры группы на одной размерной линии
// (цепочка) или с шагом groupSpacing от контура, меньшие ближе (от базы).
enum class DimensionGroupMode { Chain, Baseline };

const char* toString(DimensionGroupMode mode) noexcept;

struct DimensionItem {
    std::string id;
    std::string view;
    annotations::DimensionDef def;
    std::optional<double> lastValue;  // значение при предыдущем обновлении (сохраняется в файле)
    // Групповой чертёж ГОСТ 2.113: буква размера, различающегося у исполнений (пусто — по
    // порядку, annotations::groupDimensionLetter). У размера, одинакового во всех исполнениях,
    // не используется.
    std::string groupLetter;
    // Группа размеров (пусто — одиночный размер). Точки группы — def.offsetFrom (общий у всех
    // размеров группы; у группы от базы первая — база). При каждом обновлении группа
    // перестраивается по модели: цепочка — точки сортируются вдоль оси и размеры соседних пар
    // получают свои точки заново (после изменения модели звенья не перекрываются), от базы —
    // линии группы (|offset|) раздаются размерам по длине: меньший — ближе к контуру. Шаг
    // groupSpacing задаётся при создании (baselineDimensions); автокомпоновка (arrangeDimensions)
    // ставит размеры группы по рядам и сохраняет группу (цепочка — на самом дальнем из рядов звеньев).
    std::string group;
    DimensionGroupMode groupMode = DimensionGroupMode::Chain;
    double groupSpacing = 7.0;  // от базы: между параллельными размерными линиями, мм (≥ 7)
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
    // Полка ставится разнесением вместе с автоматическими позициями вида (arrangeBalloons,
    // DrawingDocument::arrangeBalloons); at при этом не используется.
    bool arranged = false;
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
    // Таблица параметров зубчатого венца ГОСТ 2.403 в правом верхнем углу (деталь с венцом).
    std::optional<annotations::GearTableDef> gearTable;
    std::optional<annotations::RoughnessSpec> roughnessRemainder;  // знак в правом верхнем углу
    std::vector<std::string> technicalRequirements;
    bool partsList = false;  // перечень составных частей ISO 7573 над основной надписью (сборка)
};

// Групповой рабочий чертёж детали ГОСТ 2.113 (способ с таблицей исполнений): изображение
// базового исполнения, размеры, различающиеся у исполнений, — буквами, таблица исполнений над
// основной надписью первого листа. docs/drawing/DRAWING_DOCUMENT.md, «Групповой чертёж».
struct GroupDrawingSettings {
    bool enabled = false;
    bool mass = true;       // графа «Масса, кг» (если масса известна у всех исполнений)
    bool material = false;  // графа «Материал»
    // Заголовки граф таблицы по ключу графы (GroupColumn::key); нет ключа или пусто — по умолчанию.
    std::map<std::string, std::string> headers;
};

// Оформление листа: ЕСКД (рамка ГОСТ 2.301/2.104, основная надпись форм 1 и 2а) или ISO (рамка
// ISO 5457, основная надпись ISO 7200 со знаком метода проекций ISO 5456-2).
enum class SheetStyle { Eskd, Iso7200 };
const char* toString(SheetStyle style) noexcept;  // "eskd", "iso7200"

struct DrawingSettings {
    double lineThickness = 0.5;  // s, ГОСТ 2.303
    // Метод проекций (ГОСТ 2.305 — первый угол; ISO 128-30: E — первый, A — третий). Менять —
    // DrawingDocument::setProjectionMethod: проекционные виды переходят на противоположную
    // сторону родителя и сохраняют своё содержание (вид сверху остаётся видом сверху).
    views::ProjectionMethod projection = views::ProjectionMethod::FirstAngle;
    SheetStyle sheetStyle = SheetStyle::Eskd;
    double viewGap = 30.0;   // зазор между видами в проекционной связи, мм листа (место размеров)
    double margin = 15.0;    // отступ области видов от рамки, мм
    views::ScaleRatio maxScale{5, 1};  // наибольший масштаб при автоматическом выборе
    bool reducedAxonometry = true;     // приведённые коэффициенты ГОСТ 2.317 (1 по осям)
    // Шрифт текстов листа: однолинейный тип Б ГОСТ 2.304 (линиями во всех форматах экспорта,
    // метрики — для основной надписи, размеров и полок) или TrueType (DejaVu Sans и др.).
    TextFont textFont = TextFont::Gost2304;
    GroupDrawingSettings group;
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
    std::vector<annotations::GearImage> gears;      // зубчатые венцы ГОСТ 2.402 на виде
    std::vector<annotations::GearMesh> gearMeshes;  // зацепления на виде (сборка)
    std::vector<annotations::BearingImage> bearings;  // подшипники ГОСТ 2.420 в разрезе
    // Auxiliary: поворот относительно ориентации по стрелке (0 — не повернут) и стрелка на parent.
    double rotationDeg = 0.0;
    std::optional<views::ViewArrowGeometry> arrow;
    bool fromCache = false;
};

struct ComputedSheet {
    std::string id;
    Sheet sheet;
    views::ScaleRatio scale;
    int index = 1;  // номер листа
    int count = 1;  // листов в чертеже
    TitleBlockData title;
    Iso7200Data isoTitle;  // основная надпись ISO 7200 (DrawingSettings::sheetStyle == Iso7200)
    std::vector<ComputedView> views;
    std::vector<annotations::DimensionResult> dimensions;  // по SheetDef::dimensions
    std::vector<std::optional<annotations::SymbolGeometry>> roughness;  // nullopt — висячий
    std::vector<std::optional<annotations::SymbolGeometry>> tolerances;  // по SheetDef::tolerances
    std::vector<std::optional<annotations::SymbolGeometry>> datums;      // по SheetDef::datums
    std::vector<annotations::Balloon> balloons;  // автоматические позиции видов
    std::vector<std::string> balloonViews;       // вид каждой автоматической позиции (по balloons)
    // По balloons: полка перенесена мышью (ViewDef::balloonShelves), не стоит на линии полок.
    std::vector<bool> balloonMoved;
    // По SheetDef::balloons: номер, точка выноски и полка; nullopt — висячая позиция (ребро не
    // найдено или не видно, у его детали нет позиции в спецификации).
    std::vector<std::optional<annotations::Balloon>> manualBalloons;
    // По SheetDef::threadNotes; Dangling — грань не найдена, не резьба или резьба не на виде.
    std::vector<annotations::DimensionResult> threadNotes;
    std::optional<annotations::Specification> specification;  // для перечня ISO 7573
    std::vector<annotations::GearTableRow> gearTable;  // строки таблицы ГОСТ 2.403 (SheetDef::gearTable)
};

// Графа таблицы исполнений. Ключи: «designation» — обозначение исполнения; «dim:<лист>/<размер>»
// — буквенный размер; «suppress:<операция>» — элемент, подавленный хотя бы в одном исполнении
// («есть»/«нет»); «mass» — масса, кг; «material» — материал.
struct GroupColumn {
    std::string key;
    std::string header;         // действующий заголовок
    std::string defaultHeader;  // заголовок по умолчанию
};

// Буквенный размер группового чертежа: значения по исполнениям (nullopt — размера нет в
// исполнении: элемент подавлен, ссылка не разрешилась).
struct GroupDimension {
    std::string sheet;
    std::string id;
    std::string letter;
    std::vector<std::optional<double>> values;
    std::vector<std::string> texts;  // ячейки таблицы («80», «–»)
};

// Результат группового чертежа (последнее обновление с DrawingSettings::group.enabled).
struct GroupDrawingResult {
    std::vector<std::string> designations;  // полные обозначения исполнений
    std::vector<GroupColumn> columns;
    annotations::GroupTable table;           // заголовки — действующие
    std::vector<GroupDimension> dimensions;  // только буквенные (различающиеся) размеры
    std::string baseDesignation;             // обозначение базового исполнения (основная надпись)
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
    std::vector<assembly_v2::DefinitionId> withoutBalloon;  // составные части, не видимые ни на одном виде листа
    // Составные части, скрытые на этом виде, позиции которых поставлены на другом виде листа
    // (ближайшем разрезе): определение → вид.
    std::vector<std::pair<assembly_v2::DefinitionId, std::string>> movedTo;
    annotations::BalloonLine line = annotations::BalloonLine::Right;  // линия полок (большинства позиций)
    std::size_t lineCount = 1;  // линий полок вида (стороны вида — при разбиении)
    bool ordered = true;        // номера на полках по порядку
    std::vector<int> outOfOrder;  // номера не на своём месте (порядок уступил непересечению)
    std::size_t crossings = 0;  // пересечения линий-выносок (0 — разнесено)
    // Полок, задевающих размеры листа (линии и надписи): 0 — линия полок обошла размеры.
    std::size_t shelfHits = 0;
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

    // Шрифт TrueType для ширины текстов размеров и знаков при обновлении (полки, размещение
    // числа), если DrawingSettings::textFont == TrueType; тот же шрифт следует передавать в
    // render. Не сохраняется в файле. Со шрифтом ГОСТ 2.304 метрики — gostStrokeFont().
    void setFont(const TrueTypeFont* font) { font_ = font; }
    // Метрики текста по настройке textFont: gostStrokeFont() или trueType.
    const TextMetrics* textMetrics(const TrueTypeFont* trueType) const;

    // Пересчёт: виды — через кэш (HLR — ViewCache, разрезы — кэш документа по хэшу формы и
    // параметрам), компоновка и масштабы, переразрешение ссылок размеров и знаков, позиции по
    // спецификации (spec; для сборки без spec — спецификация по умолчанию), основная надпись.
    Result<UpdateReport> update(const ModelSnapshot& model, views::ViewCache& cache,
                                const annotations::Specification* spec = nullptr);
    Result<UpdateReport> update(const ModelLoader& loader, views::ViewCache& cache,
                                const annotations::Specification* spec = nullptr);

    // Разнесение позиций листа (ГОСТ 2.109): ручные позиции листа (или только вида view, если он
    // задан) получают arranged = true — при обновлении их полки ставятся вместе с автоматическими
    // позициями своего вида на одну линию без пересечения выносок, по порядку номеров
    // (annotations::arrangeBalloons); полки автоматических позиций, перенесённые мышью
    // (ViewDef::balloonShelves), возвращаются на линию. Возвращает число ручных позиций и
    // возвращённых полок; после вызова нужен update. Ошибка InvalidInput — нет листа или вида.
    Result<std::size_t> arrangeBalloons(const std::string& sheet, const std::string& view = {});

    // Перенос полки автоматической позиции составной части definition на виде view (мышью):
    // at — начало полки на листе, nullopt — обратно на линию полок вида. Ошибка InvalidInput —
    // нет листа или вида, у вида нет позиций по спецификации (autoBalloons), точка не конечна.
    Status setAutoBalloon(const std::string& sheet, const std::string& view, assembly_v2::DefinitionId definition,
                          std::optional<Point2> at);

    // Автокомпоновка размеров вида (annotations::arrangeDimensions, ГОСТ 2.307): новые offset,
    // textShift и направление полок размеров вида view листа sheet по последнему обновлению;
    // model — тот же снимок, что в update. Определения меняются, после вызова нужен update.
    // Ошибка InvalidInput — документ не обновлён, нет листа или вида.
    Result<annotations::ArrangeResult> arrangeDimensions(const ModelSnapshot& model, const std::string& sheet,
                                                         const std::string& view,
                                                         const annotations::ArrangeOptions& options = {});

    // Перенос размера мышью (annotations::moveDimensionTo): число встаёт в точку target листа.
    // Размер группы (DimensionItem::group) переносит всю группу: цепочка — общая размерная линия
    // (offset всех размеров), от базы — все размерные линии на ту же величину, шаг сохраняется;
    // сдвиг вдоль линии (textShift) — только у перенесённого размера. Линии группы не переходят на
    // другую сторону (как у одиночного размера: линия, перешедшая бы на другую сторону, — в 2 мм
    // на прежней стороне). model — снимок последнего
    // обновления. Ошибка InvalidInput — документ не обновлён, нет листа или размера, размер висячий.
    Status moveDimension(const ModelSnapshot& model, const std::string& sheet, const std::string& id, Point2 target);

    // Метод проекций документа: проекционные виды и разрезы в проекционной связи (side) переходят
    // на противоположную сторону родителя (сверху ↔ снизу, слева ↔ справа) — изображение каждого
    // вида не меняется, меняется раскладка (ГОСТ 2.305, разд. 5; ISO 128-30, п. 5). Положения
    // видов (position) сохраняются. Возвращает число перенесённых видов; после вызова нужен update.
    std::size_t setProjectionMethod(views::ProjectionMethod method);

    bool computed() const { return !computed_.empty(); }
    // Групповой чертёж последнего обновления; nullptr — режим выключен или у модели нет исполнений.
    const GroupDrawingResult* groupDrawing() const { return group_ ? &*group_ : nullptr; }
    const std::vector<ComputedSheet>& computedSheets() const { return computed_; }

    // Листы чертежа (рамка, основная надпись форм 1/2а, виды, обозначения, размеры, знаки,
    // позиции, технические требования); Drawing::textFont() — по DrawingSettings::textFont.
    // font — шрифт TrueType (метрики при textFont == TrueType; со шрифтом ГОСТ не используется
    // и может быть nullptr). Ошибка InvalidInput — документ не обновлён.
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
    std::optional<GroupDrawingResult> group_;
    // Значения размеров каждого исполнения и буквы — до основного обновления (group_drawing.cpp).
    Result<GroupDrawingResult> computeGroup(const ModelSnapshot& model, views::ViewCache& cache) const;
    // Буква размера id листа sheet по group_ (пусто — числом).
    std::string groupLetterOf(const std::string& sheet, const std::string& id) const;
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

// Групповая спецификация ГОСТ 2.113 по составу всех исполнений сборки
// (assembly_v2::configurationBom, docs/drawing/SPECIFICATION.md, «Групповая спецификация»).
struct GroupSpecificationSettings {
    bool enabled = false;
    annotations::GroupSpecLayout layout = annotations::GroupSpecLayout::Columns;
    // Заголовки граф количества по исполнениям (по порядку таблицы); пусто — по умолчанию.
    std::vector<std::string> columnHeaders;
};

// Настройки спецификации; ключ составной части — её обозначение, а если его нет, наименование.
struct SpecificationSettings {
    std::map<std::string, std::string> formats;
    std::map<std::string, std::string> notes;
    std::map<std::string, annotations::SpecSection> sections;
    std::vector<annotations::SpecItem> documents;
    bool addAssemblyDrawing = true;
    std::string assemblyDrawingFormat = "А3";
    int reservePositions = 0;
    // «Разборные» подсборки (ключи составных частей): в спецификацию и позиции сборочного
    // чертежа входят их детали, а не сами подсборки (annotations::SpecificationOptions::dissolve).
    std::set<std::string> dissolved;
    TextFont textFont = TextFont::Gost2304;  // шрифт текстов листов спецификации
    GroupSpecificationSettings group;
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
    // Групповой режим (settings().group.enabled): состав всех исполнений — configurationBom с
    // loadOptions(); specification() — спецификация активного исполнения с номерами позиций
    // групповой (позиции сборочного чертежа), render() — листы групповой спецификации.
    Result<bool> update(const ModelSnapshot& model);
    const annotations::Specification* specification() const { return spec_ ? &*spec_ : nullptr; }
    const annotations::GroupSpecification* groupSpecification() const { return group_ ? &*group_ : nullptr; }
    // Загрузка исполнений компонентов для configurationBom (не сохраняется в файле).
    void setLoadOptions(assembly_v2::LoadOptions options) { loadOptions_ = std::move(options); }
    const assembly_v2::LoadOptions& loadOptions() const { return loadOptions_; }

    // Листы формы 1 (А4, основная надпись форм 2 и 2а); шрифт — SpecificationSettings::textFont
    // (font — TrueType, нужен только для textFont == TrueType). Ошибка — документ не обновлён.
    Result<annotations::SpecDocument> render(const TrueTypeFont* font) const;

    Result<std::string> toJson() const;
    static Result<SpecificationDocument> fromJson(std::string_view json);

private:
    ModelLink model_;
    SpecificationSettings settings_;
    TitleFields title_;
    std::optional<annotations::Specification> spec_;
    std::optional<annotations::GroupSpecification> group_;
    assembly_v2::LoadOptions loadOptions_;
    Result<bool> updateGroup(const ModelSnapshot& model, const annotations::SpecificationOptions& base);
};

// ---------------------------------------------------------------- экспорт комплекта

struct ExportKitOptions {
    std::string directory;
    std::string baseName = "drawing";
    // Шрифт TrueType: обязателен, если чертёж или спецификация выводятся шрифтом TrueType; при
    // шрифте ГОСТ 2.304 — необязателен (невидимый текстовый слой PDF для поиска).
    const TrueTypeFont* font = nullptr;
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
