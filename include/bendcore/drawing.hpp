// Чертёжная среда (раздел 7): стандарты и форматы, масштабы, виды трубы, развёртка с размерами,
// таблицы (гибов, координат вершин, карта заготовки), компоновка листа и проверки выпуска.
//
// Геометрия видов — в миллиметрах модели; размеры шрифта и вынос размерных линий — в миллиметрах
// листа. Размещённый вид хранит масштаб и сдвиг: точка листа = origin + scale · точка вида.
// Ось Y листа направлена вверх (как в чертеже); переворот для SVG делает интерфейс.
#pragma once

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
};

// ---------- Ассоциативность (версионирование и история) ----------
// Отпечаток пути для детектирования изменений (SHA256)
struct PathFingerprint {
    std::string sha256;

    bool operator==(const PathFingerprint& other) const { return sha256 == other.sha256; }
    bool operator!=(const PathFingerprint& other) const { return !(*this == other); }
};

// Статус ассоциативного чертежа
enum class DrawingAssociativity { Fresh, Stale, Broken, NotApplicable };
const char* associativity_name(DrawingAssociativity a);

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

}  // namespace bendcore::drawing
