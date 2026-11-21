#pragma once

// Разнесение полок номеров позиций на сборочном чертеже (ГОСТ 2.109-73): полки — вне контура
// изображения, на одной вертикали (колонна) или горизонтали (строка), линии-выноски не
// пересекаются между собой, номера — по возрастанию сверху вниз (колонна) или слева направо
// (строка). Документация — docs/drawing/ANNOTATIONS.md, «Позиции».

#include <bendcore/drawing_v2/annotations.hpp>
#include <bendcore/drawing_v2/title_block.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Линия полок относительно изображения.
enum class BalloonLine {
    Auto,   // справа, если хватает места до рамки, иначе слева, сверху, снизу
    Right,  // колонна справа
    Left,   // колонна слева (полки — влево от линии)
    Above,  // строка над изображением
    Below,  // строка под изображением
};

const char* toString(BalloonLine line) noexcept;

// Составная часть, которой ставится номер позиции.
struct BalloonCandidate {
    std::uint32_t definition = 0;
    int number = 0;              // номер позиции (порядок полок)
    std::vector<Point2> targets;  // возможные точки выноски на изображении (мм листа), не пусто
};

// Препятствия для полок на листе (мм листа): размерные и выносные линии (дуги — отрезками),
// габариты надписей размеров, полки уже поставленных позиций других видов. Полка с номером не
// ставится на препятствие (зазор obstacleGap); линия-выноска по возможности не пересекает
// надписи и размерные линии (ГОСТ 2.316-2008, разд. 4; пункт требует сверки).
struct BalloonObstacles {
    std::vector<LineSeg> lines;
    std::vector<Rect> boxes;
    bool empty() const { return lines.empty() && boxes.empty(); }
};

// Линии и надписи размера как препятствия (labelBoxes — габариты надписей).
void addDimensionObstacles(BalloonObstacles& obstacles, const DimensionGeometry& geometry,
                           const AnnotationStyle& style = {});

// Место, занимаемое полкой с номером (Shelf): от начала полки at в сторону от точки выноски,
// по высоте — от полки до верха номера; без линии-выноски.
Rect balloonShelfBox(const Balloon& balloon, const BalloonOptions& options = {}, const AnnotationStyle& style = {});

struct BalloonArrangeOptions {
    BalloonLine line = BalloonLine::Auto;
    Rect view;                 // габарит изображения на листе
    Rect area;                 // область, в которой ставятся полки (внутри рамки)
    double clearance = 15.0;   // от габарита изображения до линии полок, мм
    double step = 10.0;        // шаг полок колонны, мм (не меньше textHeight + 2)
    double rowGap = 3.0;       // промежуток между полками строки, мм
    BalloonOptions balloon;    // высота номера (ширина полки)
    AnnotationStyle style;
    BalloonObstacles obstacles;
    double obstacleGap = 1.0;  // зазор полки и номера от препятствий, мм
    // Ближе всего линия полок подходит к изображению (когда исходная линия за размером даёт
    // пересечения выносок с размерными линиями), мм.
    double minClearance = 6.0;
    // Auto: если на одной линии номера по порядку без пересечений выносок не ставятся, позиции
    // разбиваются на несколько линий по сторонам вида (каждая упорядочена).
    bool split = true;
    double splitPenalty = 40.0;  // «стоимость» каждой следующей линии, мм длины выносок
};

struct BalloonArrangement {
    std::vector<Balloon> balloons;  // в порядке кандидатов: definition, target, at, text = номер
    BalloonLine line = BalloonLine::Right;  // линия полок (при нескольких — линия большинства)
    std::vector<BalloonLine> lines;  // линия полок каждой позиции (в порядке кандидатов)
    std::size_t lineCount = 1;       // число линий полок (сторон вида)
    bool ordered = true;       // номера по возрастанию вдоль каждой линии полок
    std::vector<int> outOfOrder;  // номера не на своём месте по порядку (ordered = false)
    std::size_t crossings = 0;  // пересечений линий-выносок между собой
    // Полок (с номером), задевающих препятствия: 0, если линию полок удалось сдвинуть (вдоль неё
    // с промежутками или наружу за размеры); иначе — полки на месте без учёта препятствий.
    std::size_t shelfHits = 0;
    // Пересечений линий-выносок с надписями размеров и с размерными/выносными линиями.
    std::size_t leaderTextHits = 0;
    std::size_t leaderLineHits = 0;
};

// Полки — на линии на расстоянии clearance от габарита изображения; колонна — шаг step, центр
// колонны — на средней высоте точек выносок (сдвигается и сжимается до шага textHeight + 2, чтобы
// остаться в area); строка — ячейки шириной в две полки, центрированные по средней абсциссе.
// Слоты каждой линии упорядочены по номерам. Для каждой позиции выбирается точка выноски из targets
// (локальный поиск по одной позиции и парами): лексикографически (пересечения выносок, с надписями
// и знаками, с размерными и выносными линиями, длина). Препятствия (options.obstacles) — полки
// сдвигаются вдоль линии (динамическое программирование по сетке 0,5 мм), линия пробуется на
// исходном месте, сразу за каждым препятствием наружу и перед ним (не ближе minClearance к
// изображению); выбирается линия с наименьшим (пересечения, полки на препятствиях, выноски через
// линии, через надписи, стоимость). Auto и split: если одна линия не даёт раскладки без
// пересечений, позиции распределяются по сторонам вида (каждая точка — к ближайшей стороне, затем
// перенос позиций между сторонами, пока раскладка улучшается). Порядок номеров уступает только
// если иначе выноски пересекаются: outOfOrder, ordered = false. Ошибки InvalidInput: кандидат без
// точек, полки не помещаются в area.
Result<BalloonArrangement> arrangeBalloons(const std::vector<BalloonCandidate>& candidates,
                                           const BalloonArrangeOptions& options);

// Пересекаются ли линии-выноски (отрезки target → at) двух позиций (общие концы не считаются).
bool leadersCross(const Balloon& a, const Balloon& b);
std::size_t leaderCrossings(const std::vector<Balloon>& balloons);

}  // namespace bendcore::drawing_v2::annotations
