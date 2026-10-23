// Вывод геометрии гибов (3.5, 3.6, 7.6): острые вершины, координаты XYZ, данные LRA,
// таблица гибов в технологическом порядке (подача с конца А или Б).
//
// Соглашения ядра (как trace): труба начинается в конце А, касательная +X, «верх» +Z.
// Номера гибов закреплены за гибами по пути (3.5): при подаче с конца Б строки идут в обратном
// порядке, номера остаются прежними, а повороты A пересчитываются из геометрии.
// Значения для XYZ и LRA — по детали после подрезки: концевые участки удлинены на глубину вставки E
// (3.6), без компенсации пружинения. Спираль в LRA не описывается.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "bendcore/tubegeom.hpp"

namespace bendcore::output {

// Ошибка вывода с понятным пользователю текстом (например, «LRA не описывает спираль»).
struct OutputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Система координат вывода (1.7): точка начала и три единичные оси.
// Координата точки = проекция (p − origin) на оси. По умолчанию — СК детали.
struct Frame {
    Vec3 origin{0.0, 0.0, 0.0};
    Vec3 x{1.0, 0.0, 0.0};
    Vec3 y{0.0, 1.0, 0.0};
    Vec3 z{0.0, 0.0, 1.0};
};
Vec3 to_frame(const Vec3& p, const Frame& f);
// Проверка, что оси единичные и взаимно перпендикулярные (допуск 1e-6). Бросает OutputError.
void validate_frame(const Frame& f);

// Ломаная острых вершин: торец А, вершины гибов 1..n, торец Б.
struct SharpPolyline {
    std::vector<Vec3> points;       // торец А, вершины, торец Б
    std::vector<double> radius;     // R для каждой вершины гиба (размер points.size() − 2)
    std::vector<int> bend_no;       // номер гиба по пути (1..n) для каждой вершины гиба
};

// Вершины пути. E_A и E_B удлиняют концевые участки (деталь после подрезки, с глубиной вставки).
// Вырожденные гибы (|B| < порога) пропускаются. Бросает OutputError для спирали и при S < 0.
SharpPolyline sharp_polyline(const Path& path, double E_A = 0.0, double E_B = 0.0);

// Строка LRA (YBC): прямой участок перед гибом L, поворот R, угол гиба A (всегда ≥ 0), радиус.
// Последняя строка — только L (последний прямой участок), is_end = true.
struct LraRow {
    int bend_no = 0;       // номер гиба по пути; 0 у концевой строки
    double L = 0.0;        // мм
    double rotation = 0.0; // градусы, (−180°, 180°], у первого гиба 0
    double angle = 0.0;    // градусы, ≥ 0
    double radius = 0.0;   // мм
    bool is_end = false;
};

enum class Feed { A, B };  // подача с конца (3.5)

// LRA из ломаной вершин в порядке подачи. Бросает OutputError, если прямой участок отрицательный.
std::vector<LraRow> lra_from_polyline(const SharpPolyline& poly, Feed feed = Feed::A);
std::vector<LraRow> lra(const Path& path, Feed feed = Feed::A, double E_A = 0.0, double E_B = 0.0);

// Путь S–A–B, восстановленный из LRA (для контроля и для режима «Таблица»).
Path path_from_lra(const std::vector<LraRow>& rows);

// Строка таблицы координат вершин (7.6): «А», номер гиба, «Б».
struct XyzRow {
    std::string label;
    int bend_no = 0;  // 0 у торцов
    Vec3 p{};
    std::optional<double> radius;  // у торцов нет
};
std::vector<XyzRow> xyz(const Path& path, Feed feed = Feed::A, double E_A = 0.0, double E_B = 0.0,
                        const Frame& frame = {});

// Таблица гибов (3.1, 3.4, 3.5) в выбранном порядке, готовые значения без припусков.
struct BendTableRow {
    int bend_no = 0;     // номер по пути
    double S = 0.0;      // прямой участок перед гибом
    double A = 0.0;      // поворот, (−180°, 180°]
    double B = 0.0;      // угол ≥ 0
    Side side = Side::None;
    double R = 0.0;
    double arc = 0.0;    // длина дуги R·B (справочно)
};
struct BendTable {
    std::vector<BendTableRow> rows;
    double last_S = 0.0;         // последний прямой участок (итоговая строка)
    double finished_length = 0.0;// длина готовой трубы
};
BendTable bend_table(const Path& path, Feed feed = Feed::A, Profile direction = Profile::Right,
                     bool minimize = true);

// CSV. Разделитель и число знаков задаются; десятичный разделитель — точка.
struct CsvOptions {
    char delimiter = ';';
    int precision = 3;
};
std::string xyz_csv(const std::vector<XyzRow>& rows, const CsvOptions& opt = {});
std::string lra_csv(const std::vector<LraRow>& rows, const CsvOptions& opt = {});

}  // namespace bendcore::output
