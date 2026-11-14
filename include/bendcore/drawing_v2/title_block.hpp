#pragma once

// Рамка листа (ГОСТ 2.301, ГОСТ 2.104) и основная надпись ГОСТ 2.104-2006:
// форма 1 (185×55, первый лист чертежа), форма 2 (185×40, первый лист текстового
// документа — спецификации ГОСТ 2.106) и форма 2а (185×15, последующие листы).

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/truetype_font.hpp>

#include <map>
#include <string>

namespace bendcore::drawing_v2 {

enum class TitleBlockForm { Form1, Form2, Form2a };

struct SignatureRow {
    std::string surname;
    std::string date;
};

// Содержимое граф основной надписи (номера граф — по ГОСТ 2.104).
struct TitleBlockData {
    std::string designation;   // графа 2
    std::string name;          // графа 1
    std::string material;      // графа 3
    std::string litera;        // графа 4 (до трёх символов, по одному в клетке)
    std::string mass;          // графа 5
    std::string scale = "1:1"; // графа 6
    int sheet = 1;             // графа 7
    int sheets = 1;            // графа 8 (форма 1)
    std::string organization;  // графа 9
    SignatureRow developer;    // Разраб.
    SignatureRow checker;      // Пров.
    SignatureRow techControl;  // Т.контр.
    SignatureRow normControl;  // Н.контр.
    SignatureRow approver;     // Утв.
};

struct FrameOptions {
    TitleBlockForm form = TitleBlockForm::Form1;
    // Дополнительные графы 19–23 вдоль левой границы рамки (12×145 мм).
    bool storageStrip = true;
    // Шрифт для проверки, что текст помещается в графу. Если задан и текст не помещается
    // при данной высоте, сначала текст сжимается (коэффициент ширины до 0,6), затем высота уменьшается по ряду ГОСТ 2.304 до 2,5; не помещается
    // и при 2,5 — ошибка InvalidInput. Если не задан, ширина не проверяется.
    const TrueTypeFont* font = nullptr;
    std::string frameLayer = "FRAME";
    std::string textLayer = "TITLE";
};

struct Rect {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double width() const { return x1 - x0; }
    double height() const { return y1 - y0; }
};

struct TitleBlockLayout {
    Rect sheet;       // граница листа (линия обреза)
    Rect frame;       // рамка: поля 20 мм слева, 5 мм сверху, справа и снизу
    Rect titleBlock;  // основная надпись, в правом нижнем углу рамки
    std::map<int, Rect> graphs;  // ячейки граф по номерам ГОСТ 2.104
};

// Добавляет в чертёж рамку, линии основной надписи и тексты граф. Слои создаются, если их нет.
// Ошибки: формат меньше основной надписи; форма 1 на листе А4 альбомной ориентации
// (ГОСТ 2.104: на А4 основная надпись только вдоль короткой стороны); текст не
// помещается в графу (при заданном шрифте).
Result<TitleBlockLayout> addFrameAndTitleBlock(Drawing& drawing, const TitleBlockData& data,
                                               const FrameOptions& options = {});

}  // namespace bendcore::drawing_v2
