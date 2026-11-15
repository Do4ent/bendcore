#pragma once

// Аннотации чертежа (фаза 4, п. 6): шероховатость ГОСТ 2.309-73 (изм. 2002), допуски формы и
// расположения ГОСТ 2.308-2011 / ISO 1101, обозначение баз, линии-выноски и технические
// требования ГОСТ 2.316-2008, номера позиций ГОСТ 2.109-73 / ISO 6433.
// Документация — docs/drawing/ANNOTATIONS.md.

#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/drawing_v2/dimensions.hpp>
#include <bendcore/drawing_v2/title_block.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Геометрия знака в координатах листа.
struct SymbolGeometry {
    std::vector<LineSeg> lines;
    std::vector<Arc> arcs;
    std::vector<Circle> circles;
    std::vector<Polyline> polylines;
    std::vector<SolidFill> fills;
    std::vector<DimensionLabel> labels;
    void append(const SymbolGeometry& other);
};

// Вывод знака: линии — сплошная тонкая (ГОСТ 2.309, 2.308, 2.316 — знаки и рамки тонкими
// линиями), заливки, тексты шрифтом ГОСТ 2.304 на слое style.layer.
Status renderSymbol(Drawing& drawing, const SymbolGeometry& g, const AnnotationStyle& style = {});

// --- Шероховатость ГОСТ 2.309-73 ---

enum class RoughnessMethod {
    Unspecified,        // вид обработки не устанавливается: знак √
    MaterialRemoval,    // с удалением слоя материала: знак с горизонтальной чертой
    NoMaterialRemoval,  // без удаления слоя: знак с окружностью
};

struct RoughnessSpec {
    RoughnessMethod method = RoughnessMethod::MaterialRemoval;
    std::string parameter = "Ra";  // Ra, Rz, Rmax; пусто — знак без параметра
    std::string value;             // «1,6»; пусто — знак без параметра
    std::string process;           // способ обработки (над полкой), может быть пусто
    double heightFactor = 2.5;     // H = k·h, ГОСТ 2.309: H = (1,5…5)·h
};

// Знак вершиной в точке tip на линии поверхности, направленной под углом surfaceAngleDeg;
// знак — с левой стороны от направления линии. Короткая сторона высотой h, длинная — H,
// угол каждой стороны с поверхностью 60°. Параметр «Ra 1,6» — под полкой длинной стороны.
// Ошибки: H вне 1,5…5 h.
Result<SymbolGeometry> roughnessSign(Point2 tip, double surfaceAngleDeg, const RoughnessSpec& spec,
                                     const AnnotationStyle& style = {});

// Знак «остальное» (ГОСТ 2.309, изм. 2002): в правом верхнем углу рамки — знак с параметром
// и знак √ в скобках; размеры в 1,5 раза больше знаков на изображении. frame — рамка листа.
Result<SymbolGeometry> roughnessRemainder(const Rect& frame, const RoughnessSpec& spec,
                                          const AnnotationStyle& style = {});

// --- Допуски формы и расположения ГОСТ 2.308-2011 / ISO 1101 ---

enum class GeometricCharacteristic {
    Straightness,      // прямолинейность
    Flatness,          // плоскостность
    Roundness,         // круглость
    Cylindricity,      // цилиндричность
    LineProfile,       // форма заданного профиля
    SurfaceProfile,    // форма заданной поверхности
    Parallelism,       // параллельность
    Perpendicularity,  // перпендикулярность
    Angularity,        // наклон
    Position,          // позиционный
    Coaxiality,        // соосность (концентричность)
    Symmetry,          // симметричность
    CircularRunout,    // радиальное / торцовое биение
    TotalRunout,       // полное биение
};

// Нужна ли характеристике база (ISO 1101, табл. 2: допуски формы — без баз).
bool requiresDatum(GeometricCharacteristic c);

struct FeatureControlFrame {
    GeometricCharacteristic characteristic = GeometricCharacteristic::Flatness;
    std::string tolerance;           // «0,02»
    bool diameterZone = false;       // знак ⌀ перед значением (цилиндрическое поле)
    std::vector<std::string> datums; // буквы баз по порядку: «А», «Б»
};

struct FrameLayout {
    SymbolGeometry geometry;
    Rect box;  // габарит рамки
};

// Рамка: высота 2h (h — style.textHeight), первая клетка квадратная со знаком, вторая —
// значение допуска, далее по клетке на базу. origin — левый нижний угол рамки.
// Ошибки: пустое значение; допуск формы с базами; допуск расположения без баз.
Result<FrameLayout> featureControlFrame(Point2 origin, const FeatureControlFrame& f,
                                        const AnnotationStyle& style = {});

// Знак характеристики в квадрате со стороной size с центром center (для тестов и легенд).
SymbolGeometry characteristicSymbol(GeometricCharacteristic c, Point2 center, double size);

// Соединительная линия от рамки к элементу со стрелкой на конце (ГОСТ 2.308): ломаная
// points (первая точка — на рамке, последняя — на элементе).
Result<SymbolGeometry> frameLeader(const std::vector<Point2>& points, const AnnotationStyle& style = {});

// Обозначение базы: зачернённый равносторонний треугольник высотой h на поверхности (основание
// на линии поверхности под углом surfaceAngleDeg, вершина — с левой стороны от направления
// линии), линия длиной leaderLength и квадратная рамка 2h×2h с буквой.
Result<SymbolGeometry> datumSymbol(Point2 foot, double surfaceAngleDeg, const std::string& letter,
                                   double leaderLength = 5.0, const AnnotationStyle& style = {});

// --- Линии-выноски ГОСТ 2.316-2008 ---

enum class LeaderEnd {
    Arrow,  // на линии контура
    Dot,    // внутри контура
    None,   // на линии (без стрелки и точки)
};

// Выноска: от target до knee, полка в сторону от target, надпись над полкой.
Result<SymbolGeometry> leaderNote(Point2 target, Point2 knee, const std::string& text, LeaderEnd end,
                                  const AnnotationStyle& style = {});

// --- Технические требования ГОСТ 2.316-2008 ---

struct TechnicalRequirementsOptions {
    double textHeight = 5.0;      // 3,5 или 5 (ГОСТ 2.304)
    double width = 185.0;         // поле над основной надписью, не шире её (ГОСТ 2.316)
    double gapAboveTitle = 10.0;  // от основной надписи до текста
    double lineSpacingFactor = 1.7;  // расстояние между основаниями строк, в h
    // Зазор между начертанием строки и правой границей поля: граница поля шириной 185 мм
    // совпадает с внутренней кромкой рамки, линия рамки (s до 1,4 мм) знак касаться не должен.
    double rightClearance = 1.0;
};

struct TechnicalRequirementsLayout {
    Rect box;                        // поле: x0 основной надписи … x0 + width
    std::vector<std::string> lines;  // строки после переноса (с номерами пунктов)
    double textX = 0.0;              // точка начала строк (левая кромка начертания не левее box.x0)
    std::vector<double> inkRight;    // правая кромка начертания каждой строки, мм листа
};

// Горизонтальные границы начертания строки относительно точки начала (основание, левый край):
// для шрифта ГОСТ 2.304 (style.font == nullptr или gostStrokeFont()) — точно по линиям знаков
// с наклоном 75° и толщиной линии d = h/10; для другого шрифта — ширина по метрикам плюс
// вынос наклона h·tg 15°.
struct InkExtent {
    double left = 0.0;
    double right = 0.0;
};
Result<InkExtent> textInkExtent(const std::string& text, double h, bool slanted, const AnnotationStyle& style);

// Строки технических требований после переноса: пункты «1. …» (один пункт не нумеруется),
// перенос по словам так, что начертание строки (textInkExtent) помещается в поле шириной
// width − rightClearance. Ошибки: нет пунктов, пустой пункт, слово шире поля, ширина вне 0…185.
Result<std::vector<std::string>> technicalRequirementsLines(const std::vector<std::string>& items,
                                                            const TechnicalRequirementsOptions& options = {},
                                                            const AnnotationStyle& style = {});

// Нумерованный текст над основной надписью: строки technicalRequirementsLines; левая кромка
// начертания — x0 основной надписи, правая — не дальше x0 + width − rightClearance.
// Ошибки: как у technicalRequirementsLines; текст выше рамки.
Result<TechnicalRequirementsLayout> addTechnicalRequirements(Drawing& drawing, const TitleBlockLayout& layout,
                                                             const std::vector<std::string>& items,
                                                             const TechnicalRequirementsOptions& options = {},
                                                             const AnnotationStyle& style = {});

// Перенос строки по словам: ширина строк по метрикам шрифта (estimateTextWidth) ≤ width при
// высоте h. inkSlanted задан — мерится правая кромка начертания (textInkExtent) с этим наклоном
// от точки начала строки.
Result<std::vector<std::string>> wrapText(const std::string& text, double width, double h,
                                          const AnnotationStyle& style, std::optional<bool> inkSlanted = std::nullopt);

// --- Номера позиций ГОСТ 2.109-73 / ISO 6433 ---

enum class BalloonStyle {
    Shelf,   // ЕСКД: номер на полке линии-выноски
    Circle,  // ISO 6433: номер в кружке
};

struct Balloon {
    std::uint32_t definition = 0;  // определение сборки (assembly_v2::DefinitionId), чей номер ставится
    Point2 target;                 // точка на изображении составной части (точка выноски)
    Point2 at;                     // начало полки / центр кружка
    std::string text;              // номер позиции (заполняется synchronizeBalloons)
};

struct BalloonOptions {
    BalloonStyle style = BalloonStyle::Shelf;
    // Номера позиций — на 1–2 номера шрифта больше размерных чисел (ГОСТ 2.109).
    double textHeight = 5.0;
    double dotRadius = 0.75;  // точка на конце линии-выноски (требует сверки)
};

Result<SymbolGeometry> balloonGeometry(const Balloon& b, const BalloonOptions& options = {},
                                       const AnnotationStyle& style = {});

// Полки позиций выравниваются в строку (y) или колонну (x) — ГОСТ 2.109.
void alignBalloonsInRow(std::vector<Balloon>& balloons, double y);
void alignBalloonsInColumn(std::vector<Balloon>& balloons, double x);

}  // namespace bendcore::drawing_v2::annotations
