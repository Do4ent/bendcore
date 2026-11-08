#pragma once

// Допуски и посадки гладких соединений: ISO 286-1:2010 / ГОСТ 25346-2013 (система допусков,
// основные отклонения, квалитеты IT) и ISO 286-2:2010 / ГОСТ 25347-2013 (поля допусков).
// Документация — docs/drawing/ANNOTATIONS.md, раздел «Допуски и посадки».
//
// Таблицы хранятся в мкм для номинальных размеров до 500 мм. Отклонения отверстий получаются
// из отклонений валов по общему и специальному правилам ISO 286-1 (п. 4.3.2.5–4.3.2.6) с
// поправкой Δ = ITn − IT(n−1).

#include <bendcore/drawing_v2/common.hpp>

#include <optional>
#include <string>

namespace bendcore::drawing_v2::annotations {

// Квалитет: 1…18 (IT01 и IT0 — значения −1 и 0).
// Номинальный размер — свыше 0 до 500 мм включительно (интервалы ISO 286-1, табл. 1).
// Результат — мкм. IT14…IT18 для размеров до 1 мм не применяются — ошибка InvalidInput.
Result<double> standardToleranceUm(double nominalMm, int grade);

// Основная часть обозначения поля: буква основного отклонения и квалитет, например «H7»,
// «js6», «k6». Регистр буквы: прописная — отверстие, строчная — вал.
struct ToleranceClass {
    std::string letter;  // "H", "h", "js", "JS", "k", ...
    int grade = 7;
    bool hole() const;   // прописная буква
    std::string label() const;  // "H7"
};

// Разбор обозначения поля («H7», «js6», «K7»). Поддерживаемые буквы: валы a, b, c, d, e, f,
// g, h, js, k, m, n, p, r, s; отверстия A, B, C, D, E, F, G, H, JS, K, M, N, P, R, S.
// Ошибка InvalidInput — неизвестная буква или квалитет вне 1…18.
Result<ToleranceClass> parseToleranceClass(const std::string& text);

// Предельные отклонения, мкм: upper — верхнее (es / ES), lower — нижнее (ei / EI).
struct LimitDeviations {
    double upperUm = 0.0;
    double lowerUm = 0.0;
    double fundamentalUm = 0.0;  // основное отклонение (ближайшее к нулевой линии)
    double itUm = 0.0;           // допуск IT квалитета
};

// Отклонения поля для номинального размера. Ошибки InvalidInput: размер вне 0…500 мм,
// комбинация, которой нет в ISO 286 (a, b, A, B — для размеров до 1 мм; K, M, N свыше IT8 до
// 3 мм — по правилам стандарта; N свыше IT8 для размеров до 1 мм).
Result<LimitDeviations> limitDeviations(double nominalMm, const ToleranceClass& cls);
Result<LimitDeviations> limitDeviations(double nominalMm, const std::string& cls);

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
