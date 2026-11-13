#pragma once

// Изображение резьбы на чертеже по ГОСТ 2.311-68 (ISO 6410-1) по условной резьбе модели —
// атрибутам «thread.*» граней (modeling::threadsOf, docs/modeling/FEATURES.md, «Резьбовое
// отверстие»): линии наружного диаметра, граница резьбы, дуга ≈ 3/4 окружности на виде вдоль
// оси, невидимая резьба штриховыми, обозначение резьбы в размере диаметра.
// Документация — docs/drawing/ANNOTATIONS.md, раздел «Резьба».

#include <bendcore/drawing_v2/dimensions.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Как резьба видна на виде.
enum class ThreadAspect {
    Side,  // ось резьбы параллельна плоскости проекции
    End,   // ось резьбы перпендикулярна плоскости проекции (вид вдоль оси)
};

// Резьба на виде. Координаты — вида (мм модели).
struct ThreadImage {
    std::vector<std::uint32_t> faces;  // грани резьбы (TopologyIndex формы вида)
    std::string name;                  // имя первой грани (ThreadInfo::name) — для ссылок
    modeling::ThreadSpec thread;
    ThreadAspect aspect = ThreadAspect::Side;
    views::Visibility visibility = views::Visibility::Visible;
    Point2 entry;            // центр входа резьбы на виде
    Point2 axis{0.0, 0.0};   // Side: единичное направление резьбы от входа на виде
    // Side: сплошные тонкие (невидимая резьба — штриховые) по наружному диаметру
    // thread.nominal от входа на длину thread.length (ГОСТ 2.311, п. 2.3 для отверстия).
    std::array<LineSeg, 2> majorLines{};
    // Side: граница резьбы — поперёк оси на конце длины резьбы, до линий наружного диаметра;
    // нет, если резьба доходит до конца стенки (сквозная резьба на всю длину).
    std::optional<LineSeg> boundary;
    // End: дуга по наружному диаметру ≈ 3/4 окружности (ГОСТ 2.311, п. 2.4 для отверстия).
    Arc majorArc{};
};

struct ThreadImageOptions {
    // Дуга вида вдоль оси: начало и раствор, градусы. Разрыв 270°…350° (правая нижняя
    // четверть) не начинается и не кончается на центровых линиях и не попадает под выносную
    // линию размера с углом 45°.
    double arcStartDeg = 350.0;
    double arcSweepDeg = 270.0;
    // Допуск «ось параллельна/перпендикулярна плоскости вида» (|cos|).
    double angularTolerance = 1e-6;
};

// Резьбы, изображаемые на виде: для каждой резьбы threads (грани с одной резьбой — один
// элемент) вид Side или End; на косых видах (аксонометрия) резьба не изображается. Видимость —
// по изображению стенки на виде: Side — прямые участки рёбер/очерков граней резьбы,
// параллельные оси (видим хотя бы один — резьба видима), End — окружность края стенки,
// ближайшая к наблюдателю, и резьба видна, только если доходит до этого края. Резьба, стенка
// которой на виде не изображена (вырезана разрезом), пропускается. faceIndex/edgeIndex вида
// должны относиться к topology (у разреза — к исходной форме, как в views::sectionShape).
// Ошибки: InvalidInput — индекс грани вне topology; Unsupported — наружная резьба (модель
// создаёт только внутреннюю).
Result<std::vector<ThreadImage>> threadImages(const std::vector<modeling::ThreadInfo>& threads,
                                              const kernel::TopologyIndex& topology,
                                              const views::ProjectedView& view,
                                              const ThreadImageOptions& options = {});

// Вывод резьб вида: линии наружного диаметра и дуга — сплошная тонкая (слой THIN), граница
// резьбы — сплошная основная (VISIBLE); невидимая резьба — штриховые (HIDDEN) (ГОСТ 2.311,
// разд. 2; ГОСТ 2.303, табл. 1). Сплошная тонкая проводится не ближе minThinGap (0,8 мм листа)
// к основной линии отверстия (thread.drill): при мелком масштабе линии наружного диаметра,
// дуга и граница отодвигаются до этого расстояния (ГОСТ 2.311: «не менее 0,8 мм и не более
// шага резьбы»).
Status renderThreads(Drawing& drawing, const views::SheetView& view, const std::vector<ThreadImage>& threads,
                     const views::RenderOptions& options = {}, double minThinGap = 0.8);

// Радиус линии наружного диаметра на листе с учётом minThinGap (мм листа).
double threadSheetRadius(const ThreadImage& image, double scale, double minThinGap = 0.8);

// Обозначение метрической резьбы для чертежа (ГОСТ 8724, ГОСТ 16093, ГОСТ 2.311, п. 5):
// «M10» (крупный шаг не пишется), «M10×1,25» (мелкий), «LH» для левой, «-6H» — поле допуска.
std::string threadDesignationText(const modeling::ThreadSpec& spec, const std::string& toleranceField = "");

struct ThreadDimensionDef {
    // Side: расстояние размерной линии от входа резьбы (наружу, против направления резьбы),
    // мм листа. End: не используется.
    double offset = 10.0;
    std::string toleranceField;  // «6H»; пусто — без поля допуска
    double leaderAngleDeg = 45.0;  // End: направление размерной линии через центр
    double textShift = 0.0;        // Side: сдвиг надписи вдоль размерной линии
};

// Размер наружного диаметра резьбы с обозначением вместо числа (ГОСТ 2.311, п. 5; ГОСТ 2.307):
// Side — выносные линии от концов линий наружного диаметра у входа, размерная линия поперёк
// оси; End — размерная линия через центр, стрелки на дуге 3/4. value — наружный диаметр,
// измеренный по изображению (мм модели).
Result<DimensionResult> threadDimension(const ThreadImage& image, const views::SheetView& view,
                                        const ThreadDimensionDef& def, const AnnotationStyle& style = {});

}  // namespace bendcore::drawing_v2::annotations
