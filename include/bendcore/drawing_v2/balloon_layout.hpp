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
};

struct BalloonArrangement {
    std::vector<Balloon> balloons;  // в порядке кандидатов: definition, target, at, text = номер
    BalloonLine line = BalloonLine::Right;
    bool ordered = true;       // номера по возрастанию вдоль линии полок
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
// Слоты упорядочены по номерам; для каждой позиции выбирается точка выноски из targets, при которой
// линия-выноска не пересекает другие и короче (локальный поиск до отсутствия улучшений). Если
// пересечения остаются, порядок номеров уступает: слоты назначаются по положению выбранных точек и
// пары пересекающихся выносок обмениваются слотами (каждый обмен укорачивает сумму длин) —
// ordered = false. Препятствия (options.obstacles): слоты сдвигаются вдоль линии (промежутки
// между полками больше шага) так, чтобы полки с номерами не задевали препятствий, с наименьшей
// суммой сдвигов (динамическое программирование по сетке 0,5 мм); если и так не выходит или
// сдвиг большой — линия полок отодвигается от изображения за препятствие (стоимость сдвига
// линии — n · расстояние). Точки выносок выбираются по (пересечения выносок, пересечения с
// надписями размеров, с линиями размеров, длина). Ошибки InvalidInput: кандидат без точек,
// полки не помещаются в area.
Result<BalloonArrangement> arrangeBalloons(const std::vector<BalloonCandidate>& candidates,
                                           const BalloonArrangeOptions& options);

// Пересекаются ли линии-выноски (отрезки target → at) двух позиций (общие концы не считаются).
bool leadersCross(const Balloon& a, const Balloon& b);
std::size_t leaderCrossings(const std::vector<Balloon>& balloons);

}  // namespace bendcore::drawing_v2::annotations
