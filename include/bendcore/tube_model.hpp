// Модель трубы: путь (если есть), ∅, стенка, порты. Импорт из CSV и JSON. Без OCCT.
// Концевые порты и тело трубы — в tube_body.hpp (нужен OCCT); STEP — там же.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bendcore/ports.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore {

struct TubeModel {
    std::string id;                  // имя трубы
    std::string source;              // "table", "csv", "json", "step"
    double od = 0.0;                 // мм, наружный ∅
    double wall = 0.0;               // мм, толщина стенки (0 — сплошная)
    std::optional<Path> path;        // осевая линия; у трубы из STEP пути нет
    std::vector<Port> ports;         // порты: ручные из файла и концевые из тела (после complete_model)
};

// Содержимое файла: участки пути и ручные порты.
struct ImportData {
    Path path;
    std::vector<Port> ports;
};

// CSV. Первая строка — заголовок. Строки с «#» в начале и пустые пропускаются.
// Колонки (лишние игнорируются, пустые ячейки — «не задано»):
//   kind      — straight | bend | spiral | port (обязательна)
//   length    — straight: длина S, мм
//   radius    — bend: радиус R, мм; spiral: радиус витка R, мм
//   angle     — bend: угол B, градусы, знак — направление гиба
//   rotation  — bend: поворот A вокруг касательной, градусы (по умолчанию 0)
//   pitch     — spiral: шаг p, мм
//   turns     — spiral: число витков n
//   direction — spiral: +1 правая, −1 левая (по умолчанию +1)
//   x, y, z   — port: положение, мм
//   dx, dy, dz — port: направление наружу (нормируется)
//   od        — port: ∅, мм (обязательна для порта)
//   name      — port: имя (необязательно; пустое получит авто-имя)
// Имена не должны содержать запятых. Строки идут в порядке участков пути.
// Исключение std::invalid_argument с номером строки при ошибке.
ImportData import_csv(const std::string& text);

// JSON-объект: {"path": [...], "ports": [...]}. В "path" элементы с теми же полями, что у CSV:
//   {"kind":"straight","length":50}
//   {"kind":"bend","radius":50,"angle":90,"rotation":0}
//   {"kind":"spiral","radius":30,"pitch":10,"turns":2,"direction":1}
// В "ports": {"name":"A","pos":[x,y,z],"direction":[dx,dy,dz],"od":20} (name необязательно).
// Исключение std::invalid_argument при синтаксической или смысловой ошибке.
ImportData import_json(const std::string& text);

// Модель из данных файла. Порты без имени получают авто-имя; повтор имени — ошибка.
// Пути и портов не должно быть одновременно пустыми.
TubeModel make_model(const std::string& id, const std::string& source, const ImportData& data, double od_mm,
                     double wall_mm = 0.0);

}  // namespace bendcore
