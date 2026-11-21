#pragma once

// Рамка листа (ГОСТ 2.301, ГОСТ 2.104) и основная надпись ГОСТ 2.104-2006:
// форма 1 (185×55, первый лист чертежа), форма 2 (185×40, первый лист текстового
// документа — спецификации ГОСТ 2.106) и форма 2а (185×15, последующие листы).
// Вариант ISO: рамка ISO 5457 и основная надпись ISO 7200 (180 мм) со знаком метода
// проекций ISO 5456-2 (addIsoFrameAndTitleBlock).

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/text_font.hpp>
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
    // Метрики шрифта для проверки, что текст помещается в графу: gostStrokeFont() (шрифт типа Б
    // ГОСТ 2.304 — тексты основной надписи помещаются без сжатия) или TrueTypeFont. Если текст
    // не помещается при данной высоте, сначала он сжимается (коэффициент ширины до 0,6), затем
    // высота уменьшается по ряду ГОСТ 2.304 до 2,5; не помещается и при 2,5 — ошибка
    // InvalidInput. Если не задан, ширина не проверяется.
    const TextMetrics* font = nullptr;
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
    Rect frame;       // рамка: ЕСКД — поля 20 мм слева, 5 мм остальные; ISO 5457 — 20 и 10
    Rect titleBlock;  // основная надпись, в правом нижнем углу рамки
    std::map<int, Rect> graphs;  // ячейки граф по номерам ГОСТ 2.104 (ISO — int(Iso7200Field))
    Rect projectionSymbol;       // ISO: габарит знака метода проекций (пусто — нет знака)
};

// Добавляет в чертёж рамку, линии основной надписи и тексты граф. Слои создаются, если их нет.
// Ошибки: формат меньше основной надписи; форма 1 на листе А4 альбомной ориентации
// (ГОСТ 2.104: на А4 основная надпись только вдоль короткой стороны); текст не
// помещается в графу (при заданном шрифте).
Result<TitleBlockLayout> addFrameAndTitleBlock(Drawing& drawing, const TitleBlockData& data,
                                               const FrameOptions& options = {});

// ---------------------------------------------------------------- ISO 7200 / ISO 5457

// Метод проекций для знака ISO 5456-2 (ISO 128-30: метод E — первый угол, метод A — третий).
enum class ProjectionSymbol { None, FirstAngle, ThirdAngle };

// Поля данных основной надписи ISO 7200:2004 (разд. 4): обязательные — владелец, обозначение
// (identification number), наименование (title), дата выпуска, лист, ревизия, тип документа,
// автор, утвердивший; необязательные — статус, язык, ответственный отдел.
struct Iso7200Data {
    std::string legalOwner;      // Legal owner
    std::string identification;  // Identification number
    std::string title;           // Title
    std::string documentType;    // Document type
    std::string documentStatus;  // Document status
    std::string revision;        // Revision index
    std::string issueDate;       // Date of issue (ISO 8601: ГГГГ-ММ-ДД)
    int sheet = 1;               // Sheet number: «лист/листов»
    int sheets = 1;
    std::string language = "ru";  // Language code (ISO 639); «ru…» — русские наименования полей
    std::string creator;         // Created by
    std::string approver;        // Approved by
    std::string department;      // Responsible department
    std::string scale = "1:1";   // масштаб (ISO 5455) — в ячейке рядом со знаком метода проекций
    ProjectionSymbol projection = ProjectionSymbol::FirstAngle;
};

// Ячейки полей (ключи TitleBlockLayout::graphs).
enum class Iso7200Field {
    Department = 101,
    Creator,
    Approver,
    Projection,
    Scale,
    LegalOwner,
    DocumentType,
    Title,
    DocumentStatus,
    Identification,
    Revision,
    IssueDate,
    Language,
    Sheet,
};

// Ширина и высота основной надписи ISO 7200 (мм) и поля рамки ISO 5457.
inline constexpr double kIso7200Width = 180.0;
inline constexpr double kIso7200Height = 36.0;
inline constexpr double kIso5457LeftMargin = 20.0;
inline constexpr double kIso5457Margin = 10.0;

struct IsoFrameOptions {
    // Метрики шрифта (как FrameOptions::font): подбор высоты значений, проверка, что текст
    // помещается в поле; nullptr — ширина не проверяется.
    const TextMetrics* font = nullptr;
    bool centringMarks = true;  // метки центрирования ISO 5457 на осях симметрии листа
    std::string frameLayer = "FRAME";
    std::string textLayer = "TITLE";
};

// Наименование поля в основной надписи ISO на языке language (ISO 639; «ru…» — по-русски,
// иначе по-английски, как в ISO 7200).
std::string iso7200FieldLabel(Iso7200Field field, const std::string& language);

// Знак метода проекций ISO 5456-2 (рис. 4 и 5): усечённый конус (трапеция, узкий торец слева,
// диаметры h и 2h, длина 2h) и его вид с торца (окружности h и 2h), зазор h, осевые линии.
// Первый угол — окружности справа от трапеции, третий — слева. center — центр габарита знака,
// h — высота шрифта (знак 5h × 2h).
struct ProjectionSymbolGeometry {
    std::vector<LineSeg> outline;  // трапеция (4 отрезка)
    Circle innerCircle, outerCircle;
    std::vector<LineSeg> axes;     // осевые: горизонтальная через оба вида и вертикальная у окружностей
    Rect bounds;
};
Result<ProjectionSymbolGeometry> projectionSymbolGeometry(ProjectionSymbol method, Point2 center, double h);
// Добавляет знак на слой layer: контур — сплошная тонкая, оси — штрихпунктирная тонкая.
Status addProjectionSymbol(Drawing& drawing, const ProjectionSymbolGeometry& symbol, const std::string& layer);

// Рамка ISO 5457 (поля 20 мм слева и 10 мм остальные; линия рамки — сплошная толстая; метки
// центрирования — сплошные толстые от края листа до 5 мм за рамку) и основная надпись ISO 7200
// 180×36 мм в правом нижнем углу рамки: каждое поле — наименование (h = 2,5 в левом верхнем углу)
// и значение (h до 3,5; наименование и обозначение — до 7). Раскладка полей —
// docs/drawing/DRAWING_MODEL.md, «Основная надпись ISO 7200». Ошибки: лист меньше надписи, номер
// листа вне 1…листов, текст не помещается в поле (при заданном шрифте).
Result<TitleBlockLayout> addIsoFrameAndTitleBlock(Drawing& drawing, const Iso7200Data& data,
                                                  const IsoFrameOptions& options = {});

}  // namespace bendcore::drawing_v2
