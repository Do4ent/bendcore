// Чертёжная среда (раздел 7): стандарты и форматы, масштабы, виды трубы, развёртка с размерами,
// таблицы (гибов, координат вершин, карта заготовки), компоновка листа и проверки выпуска.
//
// Геометрия видов — в миллиметрах модели; размеры шрифта и вынос размерных линий — в миллиметрах
// листа. Размещённый вид хранит масштаб и сдвиг: точка листа = origin + scale · точка вида.
// Ось Y листа направлена вверх (как в чертеже); переворот для SVG делает интерфейс.
#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/blank.hpp"
#include "bendcore/bend_output.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore::drawing {

using Vec2 = std::array<double, 2>;

// ---------- Стандарт, метод проецирования, масштабы, форматы (6.11, 7.1, 7.3) ----------
enum class Standard { GOST, ISO, DIN, ANSI };
enum class Projection { FirstAngle, ThirdAngle };

Projection projection_method(Standard s);           // ГОСТ, ISO, DIN — первый угол; ANSI — третий
std::vector<double> scale_series(Standard s);        // масштабы по убыванию (2.0 = 2:1, 0.5 = 1:2)
std::string scale_label(double scale, Standard s);   // «1:2», «2,5:1», «1:1»
bool is_standard_scale(double scale, Standard s);
// Наибольший масштаб ряда, при котором w×h (мм модели) помещается в avail_w×avail_h (мм листа);
// nullopt — не помещается даже в самом мелком (нужен больший формат или разрыв вида).
std::optional<double> pick_scale(double w, double h, double avail_w, double avail_h, Standard s);

struct Rect {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double w() const { return x1 - x0; }
    double h() const { return y1 - y0; }
};

struct Format {
    std::string name;  // A4, A3, A2, A1, A0, ANSI A…D
    double w = 0, h = 0;
};
// Формат по имени; landscape меняет стороны. Бросает std::invalid_argument для неизвестного.
Format format_by_name(const std::string& name, bool landscape = true);

struct SheetFrame {
    Format format;
    Rect border;       // внутренняя рамка
    Rect title_block;  // основная надпись (штамп)
    Rect drawable;     // поле чертежа над штампом и слева от колонки таблиц
};
// first_sheet = false — последующий лист (по ГОСТ 2.104 основная надпись по форме 2а, 185×15).
SheetFrame make_sheet(Standard s, const Format& f, bool first_sheet = true);

// ---------- Примитивы ----------
enum class LineStyle { Contour, Thin, Axis, Cut, Hidden };

struct Polyline2 {
    std::vector<Vec2> pts;
    LineStyle style = LineStyle::Contour;
    bool closed = false;
};
struct Circle2 {
    Vec2 c{};
    double r = 0;
    LineStyle style = LineStyle::Contour;
};
struct Text2 {
    Vec2 p{};
    std::string text;
    double height = 3.5;        // мм листа
    std::string anchor = "middle";  // start | middle | end
    bool frame = false;         // текст в рамке (номер гиба)
    Vec2 shift{0, 0};           // сдвиг после масштабирования, мм листа
};
// Линейный размер, параллельный отрезку a→b; линия смещена влево от a→b на offset (мм листа).
struct Dim2 {
    Vec2 a{}, b{};
    double offset = 10.0;       // мм листа
    double model_offset = 0.0;  // добавка в мм модели (например, радиус трубы), умножается на масштаб
    std::string text;
    bool reference = false;     // справочный (со «*» по ГОСТ 2.307)
};
struct Hatch2 {
    std::vector<Vec2> outline;  // замкнутый контур штриховки
};

struct Box2 {
    Vec2 lo{0, 0}, hi{0, 0};
    bool empty = true;
    void add(const Vec2& p);
    double w() const { return empty ? 0 : hi[0] - lo[0]; }
    double h() const { return empty ? 0 : hi[1] - lo[1]; }
};

struct Geometry {
    std::vector<Polyline2> lines;
    std::vector<Circle2> circles;
    std::vector<Text2> texts;
    std::vector<Dim2> dims;
    std::vector<Hatch2> hatches;
    Box2 bbox() const;  // по линиям, окружностям и штриховке (без текстов и размеров)
};

// ---------- Виды трубы (7.3, 7.5) ----------
enum class ViewKind { Front, Top, Left, Right, Iso, BendPlane, Unfold };
const char* view_name(ViewKind k);

// Режим простановки размеров на видах (7.5): S (по умолчанию), L между вершинами, только таблица.
enum class DimMode { S, L, TableOnly };

struct ViewOptions {
    bool show_axis = true;
    bool bend_labels = true;
    DimMode dims = DimMode::S;
    int bend_no = 0;            // для вида по плоскости гиба
    double E_A = 0, E_B = 0;    // деталь после подрезки: концевые участки с глубиной вставки
    int precision = 1;          // знаков после запятой в размерах
    char decimal = ',';         // десятичный разделитель (ANSI — точка)
};

// Ортогональная проекция детали. Контур прямых участков точный (две линии на ±r), гибов —
// смещение проекции оси (точно, когда плоскость гиба параллельна плоскости вида). Участок,
// направленный на наблюдателя, показывается окружностью. Невидимые линии не удаляются (версия 1).
Geometry tube_view(const Path& path, double od, ViewKind kind, const ViewOptions& opt = {});

// Направление взгляда и оси экрана вида (для проверок и интерфейса).
struct ViewBasis {
    Vec3 dir, right, up;
};
ViewBasis view_basis(ViewKind kind, const Path& path = {}, int bend_no = 0);

// ---------- Развёртка (7.3) ----------
enum class MarkBase { ArcStart, Vertex, ArcMid };

struct UnfoldOptions {
    char feed = 'A';            // подача с конца: размеры от торца заготовки этого конца
    EndSpec end_A, end_B;       // глубина вставки и ручные участки
    double N_first = 0, N_last = 0;
    bool cut_allowed = true;
    double dmin = 0;
    bool point_end = true;      // точка конца гиба (по умолчанию включена)
    bool point_mid = false;     // середина дуги
    bool chain = false;         // цепочкой вместо от базы
    bool angles = true;         // подписи B/A
    bool numbers = true;        // номера гибов
    bool arc_lengths = false;   // длины дуг
    Profile direction = Profile::Right;
    bool minimize = true;
    std::optional<double> springback_K;  // компенсация: развёртка по значениям карты
    MarkBase mark_base = MarkBase::ArcStart;
    double mark_offset = 0;
    double round_step = 0.1;    // шаг округления профиля (мм) для размеров развёртки и меток
    int precision = 1;
    char decimal = ',';
};

struct MarkRow {
    int bend_no = 0;
    double position = 0;   // от торца заготовки начального конца
    double B = 0, A = 0;
};

struct UnfoldResult {
    Geometry geom;
    std::optional<double> blank_length;  // nullopt — ошибка ручного участка
    double delta_start = 0, delta_end = 0;  // Δ на начальном и конечном конце этой развёртки
    double trimmed_length = 0;           // длина после подрезки (готовая + E_А + E_Б)
    std::vector<MarkRow> marks;
    std::vector<Issue> issues;
    std::string caption;                 // «Подача с конца А»
};
UnfoldResult unfold_view(const Path& path, double od, const UnfoldOptions& opt = {});

// ---------- Таблицы (7.6) ----------
struct Table {
    std::string title;
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    std::string note;  // строка под шапкой («R = 40»)
};
Table bend_table(const Path& path, double E_A, double E_B, Profile direction, bool minimize, int precision,
                 char decimal = ',');
Table vertex_table(const Path& path, double E_A, double E_B, const output::Frame& frame, int precision,
                   char decimal = ',');
Table blank_map(const Path& path, const UnfoldOptions& opt);
// Таблица меток гибов рядом с развёрткой (7.3): положения от торца заготовки, B и A.
Table marks_table(const UnfoldResult& u, const UnfoldOptions& opt);

std::string fmt(double v, int precision, char decimal);
double round_to(double v, double step);

// ---------- Выпуск (7.7) ----------
enum class Strictness { Strict, Soft };
struct ReleaseInputs {
    bool tech_or_geom_errors = false;
    int lost_dim_refs = 0;
    bool unfold_sheet_changed = false;
    int stale_views = 0;
    int missing_required_props = 0;
};
struct ReleaseCheck {
    std::vector<std::string> blocking;
    std::vector<std::string> warnings;
    bool can_release = false;
    bool not_for_production = false;  // пометка «Не для производства»
    bool offer_update_and_release = false;
};
ReleaseCheck release_check(const ReleaseInputs& in, Strictness s = Strictness::Strict);
// Снять выпуск может роль «Выпускающий» или администратор.
bool can_unrelease(const std::string& role);

// ---------- Чертёж детали ----------
enum class Preset { Minimal, Full };

struct DrawingInput {
    Path path;
    double od = 0, wall = 0;
    Standard standard = Standard::GOST;
    std::string format = "A3";
    bool landscape = true;
    Preset preset = Preset::Minimal;
    bool bend_plane_views = false;   // виды по плоскостям гибов (для пространственных труб)
    DimMode dims = DimMode::S;
    UnfoldOptions unfold;
    int precision = 1;
    std::map<std::string, std::string> title;  // обозначение, наименование, материал, …
    bool has_errors = false;         // технологические или геометрические ошибки трубы
};

struct PlacedView {
    std::string id;
    ViewKind kind;
    std::string label;      // «Развёртка», «А (1:1)»
    double scale = 1.0;
    Vec2 origin{};          // точка листа для начала координат вида
    Geometry geom;
};
struct PlacedTable {
    Table table;
    Vec2 top_left{};        // мм листа
    double width = 0;
};

// ---------- Слои и управление видимостью (Q2 2027) ----------
// Слои группируют элементы чертежа по типам: контур, размеры, оси, разметка, вспомогательная геометрия
// Каждый слой имеет независимую видимость, блокировку, цвет и имя

enum class LayerType {
    Contour = 0,               // контур трубы (Polyline2, Circle2)
    Dimensions = 1,            // размеры (Dim2)
    Axis = 2,                  // оси и оси вращения
    Markup = 3,                // разметка (метки гибов, номера)
    ConstructionGeometry = 4   // вспомогательная геометрия (штриховка, базовые линии)
};

const char* layer_name(LayerType type);  // "Контур", "Размеры", "Оси", "Разметка", "Вспомогательное"
const char* layer_color(LayerType type); // "#000000", "#0000FF", "#FF0000", etc

struct Layer {
    LayerType type;
    std::string name;
    bool visible = true;
    bool locked = false;
    std::string color = "#000000";  // hex color
    int element_count = 0;          // сколько элементов в слое
};

// Статус видимости слоёв для чертежа
struct LayerVisibility {
    std::map<int, bool> layer_visibility;  // LayerType -> visible

    bool is_visible(LayerType type) const {
        auto it = layer_visibility.find(static_cast<int>(type));
        return it == layer_visibility.end() ? true : it->second;
    }

    void set_visible(LayerType type, bool visible) {
        layer_visibility[static_cast<int>(type)] = visible;
    }
};

// Слои листа по умолчанию (все типы LayerType, видимы) и соответствующая карта видимости.
std::vector<Layer> make_default_layers();
LayerVisibility make_layer_visibility();

struct DrawingSheet {
    int number = 1;           // номер листа
    int count = 1;            // листов в чертеже
    Standard standard;
    Projection projection;
    SheetFrame frame;
    std::vector<PlacedView> views;
    std::vector<PlacedTable> tables;
    std::map<std::string, std::string> title;  // поля штампа, включая «Масштаб»
    bool not_for_production = false;
    std::vector<std::string> warnings;         // нестандартный масштаб, нужен разрыв, больший формат

    // Слои и видимость (Q2 2027, Task 3)
    std::vector<Layer> layers;         // все слои на листе
    LayerVisibility layer_visibility;  // текущее состояние видимости
};

// ---------- 2D-эскизы на чертежах (v7.9) ----------
// Эскизы — это пользовательские геометрические элементы, нарисованные на листе чертежа
// Координаты эскизов — в миллиметрах листа (как текст и размеры)

enum class SketchElementType { Line, Arc, Text, Dimension };

// Базовый элемент эскиза
struct SketchElement {
    std::string id;  // уникальный идентификатор
    SketchElementType type;
    Vec2 p0{};  // начальная точка (для линии, дуги, размера)
    Vec2 p1{};  // конечная точка (для линии, размера)
    std::string text;  // для Text и Dimension
    double radius = 0.0;  // для Arc
    bool arc_large = false;  // для Arc: большая дуга?
    bool arc_sweep = false;  // для Arc: направление развёртки
    double text_height = 3.5;  // для Text и Dimension (мм листа)
    bool is_construction = false;  // вспомогательная линия (штриховка на чертеже)
    LineStyle style = LineStyle::Contour;
};

// Размер в эскизе (более сложный, чем обычный Dim2)
struct SketchDimension {
    std::string id;
    Vec2 p0{}, p1{};  // точки элемента, к которому относится размер
    Vec2 offset_point{};  // где разместить текст размера
    std::string text;  // значение размера (может быть формула)
    double offset = 10.0;  // мм листа
    bool reference = false;  // справочный размер
    bool driven = false;  // ведомый размер (из модели)
};

// Эскиз на листе
struct Sketch2D {
    std::string id;  // уникальный идентификатор эскиза
    int sheet_number = 1;  // на каком листе расположен эскиз
    std::string name;  // "Эскиз 1", "Схема монтажа", etc.
    std::vector<SketchElement> elements;  // все элементы эскиза
    std::vector<SketchDimension> dimensions;  // размеры в эскизе
    bool visible = true;
    bool locked = false;  // заблокирован для редактирования
    std::string created_by;  // автор
    std::string created_at;  // ISO 8601 timestamp
    std::string modified_at;
};

// Опции валидации эскиза
struct SketchValidationOptions {
    bool check_element_ids = true;
    bool check_intersections = false;  // проверить пересечения элементов
    double element_tolerance = 0.01;  // мм листа
};

struct SketchValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ---------- Комментарии на чертежах (v7.10) ----------
// Система замечаний и аннотаций, привязанные к позициям на листах
// Комментарии хранятся в DrawingVersion и восстанавливаются при откате версии

enum class CommentStatus { Open, Resolved, NeedsRework, Archived };

const char* comment_status_name(CommentStatus s);
CommentStatus comment_status_from(const std::string& s);

// Ответ на комментарий в цепочке обсуждения
struct CommentReply {
    std::string id;           // уникальный идентификатор ответа (UUID)
    std::string text;         // текст ответа
    std::string author;       // автор ответа (username)
    std::string timestamp;    // ISO 8601 timestamp создания
};

// Комментарий на чертеже, привязан к позиции на листе
// Может содержать несколько ответов (цепочка обсуждения)
struct Comment {
    std::string id;                      // уникальный идентификатор (UUID)
    int sheet_number = 1;                // номер листа, на котором находится комментарий
    Vec2 position{0, 0};                 // позиция на листе (мм, от начала координат листа)
    std::string text;                    // текст комментария
    std::string author;                  // автор комментария (username)
    std::string created_at;              // ISO 8601 timestamp создания
    std::string modified_at;             // ISO 8601 timestamp последнего изменения
    CommentStatus status = CommentStatus::Open;   // статус: открыт/разрешён/требует правки
    std::vector<CommentReply> replies;   // история ответов и обсуждений
};

// Вспомогательные функции для работы с комментариями
Comment make_comment(int sheet_number, const Vec2& position, const std::string& text,
                     const std::string& author, const std::string& id = "");
CommentReply make_reply(const std::string& text, const std::string& author,
                       const std::string& id = "");
void add_reply_to_comment(Comment& comment, const CommentReply& reply);
void change_comment_status(Comment& comment, CommentStatus new_status);

// Фильтрование комментариев по статусу
std::vector<Comment> filter_comments_by_status(
    const std::vector<Comment>& comments,
    CommentStatus status);
std::vector<Comment> filter_comments_by_sheet(
    const std::vector<Comment>& comments,
    int sheet_number);

// Ассоциативность (версионирование и история) ----------
// Отпечаток пути для детектирования изменений (SHA256)
struct PathFingerprint {
    std::string sha256;

    bool operator==(const PathFingerprint& other) const { return sha256 == other.sha256; }
    bool operator!=(const PathFingerprint& other) const { return !(*this == other); }
};

// Статус ассоциативного чертежа
enum class DrawingAssociativity { Fresh, Stale, Broken, NotApplicable };
const char* associativity_name(DrawingAssociativity a);

// ---------- Ручные размеры (2.1-2.7) ----------
// Запись о ручном размере (не пересчитывается при изменении пути)
struct ManualDimension {
    std::string id;                  // уникальный идентификатор
    Vec2 position{};                 // позиция на листе (мм)
    std::string text;                // текст размера
    bool reference = false;          // справочный размер (со «*»)
    std::string author;              // кто создал
    std::string created_at;          // ISO 8601 timestamp
};

// Таблица ручных размеров на листе чертежа
struct ManualDimensionTable {
    std::vector<ManualDimension> dimensions;

    // Найти размер по id
    ManualDimension* find(const std::string& id) {
        for (auto& d : dimensions) {
            if (d.id == id) return &d;
        }
        return nullptr;
    }

    // Удалить размер
    bool remove(const std::string& id) {
        auto it = std::find_if(dimensions.begin(), dimensions.end(),
                              [&](const ManualDimension& d) { return d.id == id; });
        if (it != dimensions.end()) {
            dimensions.erase(it);
            return true;
        }
        return false;
    }
};

// Результат валидации новых размеров
struct ManualDimensionValidation {
    bool valid = true;
    std::vector<std::string> conflicts;     // список конфликтов с авто-размерами
    std::vector<std::string> out_of_bounds;  // размеры за пределами листа
};

// Состояние ассоциативного чертежа
struct AssociativeDrawingState {
    DrawingAssociativity status = DrawingAssociativity::Fresh;
    PathFingerprint fingerprint;
    std::string last_update_time;    // ISO 8601
    std::string last_updated_by;     // username
    std::vector<std::string> change_log;

    bool needs_recalc(const PathFingerprint& current) const { return fingerprint != current; }
};

// Версия чертежа (историческая запись)
struct DrawingVersion {
    int revision = 1;
    PathFingerprint fingerprint;
    std::vector<DrawingSheet> sheets;
    std::vector<Sketch2D> sketches;  // 2D эскизы на чертежах
    std::map<int, ManualDimensionTable> manual_dimensions;  // ручные размеры по номерам листов
    std::vector<Comment> comments;   // комментарии и замечания на чертежах (v7.10)
    std::string timestamp;           // ISO 8601
    std::string author;
    std::string comment;             // "исправлена развёртка", "добавлены виды"
};

// Ассоциативный чертёж с историей версий
struct AssociativeDrawing {
    std::vector<DrawingVersion> versions;  // version[0] = самая свежая
    AssociativeDrawingState state;

    DrawingVersion& current() { return versions.front(); }
    const DrawingVersion& current() const { return versions.front(); }

    // Различия между двумя версиями
    struct Diff {
        std::vector<std::string> changed_sheets;
        std::vector<std::string> changed_views;
        std::vector<std::string> changed_tables;
    };

    // Сравнить две версии
    Diff compare(const DrawingVersion& v1, const DrawingVersion& v2) const;

    // Откатиться на версию N
    void revert_to(int revision_number);
};

// Типы изменений
struct DrawingChanges {
    bool path_changed = false;
    bool od_changed = false;
    bool wall_changed = false;
    bool preset_changed = false;
    bool format_changed = false;
};

// Вычислить отпечаток пути (SHA256)
PathFingerprint compute_fingerprint(
    const Path& path, double od, double wall,
    Standard standard, const std::string& format);

// Создать ассоциативный чертёж (с первой версией)
AssociativeDrawing make_associative_drawing(
    const DrawingInput& in);

// Обновить ассоциативный чертёж при изменении входных параметров
AssociativeDrawing update_associative_drawing(
    const AssociativeDrawing& previous,
    const DrawingInput& updated_input,
    const std::string& author,
    const std::string& comment);

// Детектировать, что изменилось
DrawingChanges detect_changes(
    const DrawingInput& old_input,
    const DrawingInput& new_input);

// Чертёж детали: лист 1 — виды и таблица гибов; развёртка с картой заготовки и метками — на листе 1,
// если помещается без уменьшения масштаба видов, иначе на листе 2 («Виды раскладываются по листам», 7.1).
std::vector<DrawingSheet> make_part_drawing(const DrawingInput& in);

// ---------- Пакетный вывод (сборочный чертёж) ----------
// БОМ строка (позиция, обозначение, ОД, стенка, кол-во, трубы)
struct BOMRow {
    int position = 0;
    std::string designation;
    double od = 0, wall = 0;
    int quantity = 1;
    std::vector<std::string> tube_ids;  // какие трубы в этой позиции
};

// Входные данные для сборочного чертежа (все трубы)
struct AssemblyDrawingInput {
    std::vector<std::pair<std::string, DrawingInput>> tubes;  // (id, input)
    Standard standard = Standard::GOST;
    std::string format = "A3";
    int tubes_per_unfold_sheet = 4;  // 2x2 сетка
    bool sort_by_position = true;
    bool group_by_material = false;
};

// Строка спецификации (позиция, обозначение, материал, кол-во, примечание)
struct SpecificationRow {
    int position = 0;
    std::string designation;
    std::string description;
    std::string material;
    double quantity = 1;
    std::string remark;
};

// Опции спецификации
struct SpecificationOptions {
    bool group_by_material = false;
    bool sort_by_od = false;
    std::string title = "Спецификация";
};

// Сборочный чертёж со всеми трубами: лист 1 = БОМ, листы 2+ = развёртки
std::vector<DrawingSheet> make_assembly_drawing(const AssemblyDrawingInput& in);

// Таблица спецификации (позиции с обозначениями и кол-вом)
Table create_specification_table(
    const std::vector<BOMRow>& bom,
    const SpecificationOptions& opt = {});

// ---------- Экспорт в PDF ----------
// Опции для экспорта в PDF
struct PDFExportOptions {
    std::string filename = "drawing.pdf";
    std::string author = "BendCore";
    std::string subject;
    std::string title;
    bool include_watermark = false;
    std::string watermark_text;  // "Выпущено", "НЕ для производства", etc
    int dpi = 300;
    bool landscape = true;
    // Ручные размеры по номерам листов (DrawingVersion::manual_dimensions); выводятся в SVG листа.
    std::map<int, ManualDimensionTable> manual_dimensions;
};

// Подготовить SVG с метаданными для PDF экспорта
// (рендеринг в PDF происходит на клиенте)
struct PDFExportData {
    std::vector<std::string> svg_pages;      // SVG для каждого листа
    std::string title;
    std::string author;
    std::string subject;
    int page_count = 0;
};

// Подготовить данные для экспорта в PDF
PDFExportData prepare_pdf_export(
    const std::vector<DrawingSheet>& sheets,
    const PDFExportOptions& opts);

// ---------- Функции для работы с 2D эскизами ----------

// Создать новый элемент эскиза (линия)
SketchElement make_sketch_line(const Vec2& p0, const Vec2& p1, const std::string& id = "");

// Создать новый элемент эскиза (дуга)
SketchElement make_sketch_arc(const Vec2& p0, const Vec2& p1, double radius,
                              bool large = false, bool sweep = false, const std::string& id = "");

// Создать новый элемент эскиза (текст)
SketchElement make_sketch_text(const Vec2& p, const std::string& text, double height = 3.5,
                               const std::string& id = "");

// Создать новый размер в эскизе
SketchDimension make_sketch_dimension(const Vec2& p0, const Vec2& p1, const std::string& text,
                                      const Vec2& offset = {0, 10.0}, const std::string& id = "");

// Валидировать эскиз
SketchValidationResult validate_sketch(const Sketch2D& sketch,
                                       const SketchValidationOptions& opt = {});

// Редактирование эскиза: добавить элемент
void sketch_add_element(Sketch2D& sketch, const SketchElement& elem);

// Редактирование эскиза: удалить элемент по id
bool sketch_remove_element(Sketch2D& sketch, const std::string& elem_id);

// Редактирование эскиза: обновить элемент
bool sketch_update_element(Sketch2D& sketch, const std::string& elem_id, const SketchElement& elem);

// Редактирование эскиза: добавить размер
void sketch_add_dimension(Sketch2D& sketch, const SketchDimension& dim);

// Редактирование эскиза: удалить размер по id
bool sketch_remove_dimension(Sketch2D& sketch, const std::string& dim_id);

// Создать новый пустой эскиз
Sketch2D make_sketch(int sheet_number = 1, const std::string& name = "Sketch",
                     const std::string& author = "");

// Получить границы (bbox) эскиза в миллиметрах листа
Box2 sketch_bbox(const Sketch2D& sketch);

// ---------- Функции для работы с ручными размерами (2.1-2.7) ----------

// Валидировать новый ручной размер (проверить конфликты и границы)
ManualDimensionValidation validate_manual_dimension(
    const ManualDimension& dim,
    const DrawingSheet& sheet,
    const ManualDimensionTable& existing);

// Добавить ручной размер на лист (с валидацией)
// Возвращает id нового размера или пустую строку при ошибке
std::string add_manual_dimension(
    DrawingSheet& sheet,
    ManualDimensionTable& table,
    const Vec2& position,
    const std::string& text,
    bool reference,
    const std::string& author);

// Обновить текст ручного размера
bool update_manual_dimension_text(
    ManualDimensionTable& table,
    const std::string& id,
    const std::string& new_text);

// Переместить ручной размер
bool move_manual_dimension(
    DrawingSheet& sheet,
    ManualDimensionTable& table,
    const std::string& id,
    const Vec2& new_position);

// Удалить ручной размер
bool remove_manual_dimension(
    ManualDimensionTable& table,
    const std::string& id);

}  // namespace bendcore::drawing
