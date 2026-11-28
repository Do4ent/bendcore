#pragma once

// Допуски и посадки гладких соединений: ISO 286-1:2010 / ГОСТ 25346-2013 (система допусков,
// основные отклонения, квалитеты IT) и ISO 286-2:2010 / ГОСТ 25347-2013 (поля допусков).
// Документация — docs/drawing/ANNOTATIONS.md, раздел «Допуски и посадки».
//
// Таблицы хранятся в мкм для номинальных размеров до 3150 мм (IT — до 3150, основные отклонения
// a…c, j, v…zc — до 500 мм, остальные — до 3150). Отклонения отверстий получаются
// из отклонений валов по общему и специальному правилам ISO 286-1 (п. 4.3.2.5–4.3.2.6) с
// поправкой Δ = ITn − IT(n−1).

#include <bendcore/drawing_v2/common.hpp>

#include <optional>
#include <string>

namespace bendcore::drawing_v2::annotations {

// Квалитет: 1…18 (IT01 и IT0 — значения −1 и 0).
// Номинальный размер — свыше 0 до 3150 мм включительно (интервалы ISO 286-1, табл. 1: 13 до
// 500 мм и 8 свыше). Результат — мкм. IT14…IT18 для размеров до 1 мм не применяются, IT01 и IT0
// свыше 500 мм не определены — ошибка InvalidInput.
Result<double> standardToleranceUm(double nominalMm, int grade);

// Допуск IT по формулам ISO 286-1, табл. 2, без округления (для независимой сверки таблиц):
// i = 0,45·∛D + 0,001·D (D ≤ 500), I = 0,004·D + 2,1 (D > 500), D — среднее геометрическое
// границ основного интервала; IT5…IT18 = k·i (k = 7, 10, 16, 25, 40, 64, 100, …), IT1 = 0,8 +
// 0,020D, IT2…IT4 — геометрический ряд между IT1 и IT5, IT01 = 0,3 + 0,008D, IT0 = 0,5 + 0,012D;
// для D > 500 IT1…IT4 = 2I, 2,7I, 3,7I, 5I.
Result<double> standardToleranceFormulaUm(double nominalMm, int grade);

// Основное отклонение вала по формуле ISO 286-1, табл. 4, без округления: es для a…g, ei для
// k…zc; D — среднее геометрическое границ промежуточного интервала. Для p, r (до 500 мм) и s
// (до 50 мм) формулы нет (в стандарте — табличные значения) — InvalidInput.
Result<double> shaftFundamentalFormulaUm(double nominalMm, const std::string& letter);

// Основная часть обозначения поля: буква основного отклонения и квалитет, например «H7»,
// «js6», «k6». Регистр буквы: прописная — отверстие, строчная — вал.
struct ToleranceClass {
    std::string letter;  // "H", "h", "js", "JS", "k", ...
    int grade = 7;
    bool hole() const;   // прописная буква
    std::string label() const;  // "H7"
};

// Разбор обозначения поля («H7», «js6», «K7»). Поддерживаемые буквы: валы a, b, c, d, e, f,
// g, h, js, j, k, m, n, p, r, s, t, u, v, x, y, z, za, zb, zc; отверстия — те же прописными.
// Не поддерживаются cd, ef, fg (CD, EF, FG).
// Ошибка InvalidInput — неизвестная буква или квалитет вне 1…18.
Result<ToleranceClass> parseToleranceClass(const std::string& text);

// Предельные отклонения, мкм: upper — верхнее (es / ES), lower — нижнее (ei / EI).
struct LimitDeviations {
    double upperUm = 0.0;
    double lowerUm = 0.0;
    double fundamentalUm = 0.0;  // основное отклонение (ближайшее к нулевой линии)
    double itUm = 0.0;           // допуск IT квалитета
};

// Отклонения поля для номинального размера. Ошибки InvalidInput: размер вне 0…3150 мм,
// комбинация, которой нет в ISO 286 (a, b, A, B — для размеров до 1 мм; N свыше IT8 для
// размеров до 1 мм; j — только IT5…IT7, J — IT6…IT8; свыше 500 мм не определены a, b, c, j, v, x,
// y, z, za, zb, zc и их прописные; t — только от 24 мм, v — от 14 мм, y — от 18 мм).
// Свыше 500 мм r, s, t, u — расчёт по формулам табл. 4 с округлением реализации (см.
// ANNOTATIONS.md, «Что требует сверки»).
Result<LimitDeviations> limitDeviations(double nominalMm, const ToleranceClass& cls);
Result<LimitDeviations> limitDeviations(double nominalMm, const std::string& cls);

// Посадка: отверстие/вал, например «H7/s6». Зазор положительный, натяг — отрицательный
// (ISO 286-1: зазор = размер отверстия − размер вала).
enum class FitType { Clearance, Transition, Interference };

struct FitResult {
    std::string label;          // "H7/s6"
    LimitDeviations hole;
    LimitDeviations shaft;
    double maxClearanceUm = 0;  // ES − ei: наибольший зазор (при < 0 — наименьший натяг со знаком)
    double minClearanceUm = 0;  // EI − es: наименьший зазор (при < 0 — наибольший натяг со знаком)
    double fitToleranceUm = 0;  // допуск посадки ITh + ITs
    FitType type = FitType::Clearance;  // зазор: min ≥ 0; натяг: max ≤ 0; иначе переходная
};

// Расчёт посадки: holeClass — прописная буква, shaftClass — строчная. Те же ошибки, что у
// limitDeviations; перепутанный регистр — InvalidInput.
Result<FitResult> fitLimits(double nominalMm, const std::string& holeClass, const std::string& shaftClass);
Result<FitResult> fitLimits(double nominalMm, const std::string& fit);  // "H7/s6"

// Запись отклонения в мм по ЕСКД: знак «+» / «−» (U+2212), десятичная запятая, без хвостовых
// нулей; 0 — «0». Например 21 мкм → «+0,021», −13 мкм → «−0,013».
std::string formatDeviationMm(double um);

// Запись размера с допуском (ГОСТ 2.307-2011, разд. 6 / ISO 129-1, п. 9).
enum class ToleranceDisplay {
    ClassOnly,           // «25H7»
    Deviations,          // «25 +0,021» (предельные отклонения)
    ClassAndDeviations,  // «25H7(+0,021)»
};

struct ToleranceText {
    std::string main;               // номинал с обозначением поля или «±»: «25H7», «25±0,0065»
    std::optional<std::string> upper;  // верхнее отклонение (над строкой), если пишется отдельно
    std::optional<std::string> lower;  // нижнее отклонение (под строкой)
    std::string suffixOpen;         // «(» перед отклонениями для ClassAndDeviations
    std::string suffixClose;        // «)» после
};

// Строки размера с допуском. Симметричные отклонения — «±» в одну строку той же высоты.
// omitZero — не писать нулевое отклонение (ЕСКД, ГОСТ 2.307: «отклонение, равное нулю, не
// указывают» — требует сверки), иначе «0» (ISO 129-1).
ToleranceText toleranceText(const std::string& nominal, const ToleranceClass& cls,
                            const LimitDeviations& dev, ToleranceDisplay display, bool omitZero = true);

}  // namespace bendcore::drawing_v2::annotations
