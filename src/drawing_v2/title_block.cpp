// Рамка и основная надпись ГОСТ 2.104-2006. Размеры граф — см. docs/drawing/DRAWING_MODEL.md.

#include <bendcore/drawing_v2/title_block.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace bendcore::drawing_v2 {

namespace {

constexpr double kLeftMargin = 20.0;   // ГОСТ 2.104: поле подшивки
constexpr double kOtherMargin = 5.0;
constexpr double kTitleWidth = 185.0;
constexpr double kMinWidthFactor = 0.6;  // ширина прописных типа Б 6/10 h (ГОСТ 2.304) против ≈1,0 h у DejaVu Sans
constexpr double kCellMargin = 0.5;

class Builder {
public:
    Builder(Drawing& d, const FrameOptions& o) : d_(d), o_(o) {}

    Status line(double x0, double y0, double x1, double y1, bool thick) {
        return d_.add(Entity{LineSeg{{x0, y0}, {x1, y1}}, o_.frameLayer,
                             thick ? LineType::SolidThick : LineType::SolidThin});
    }
    Status rect(const Rect& r, bool thick) {
        if (auto s = line(r.x0, r.y0, r.x1, r.y0, thick); !s) return s;
        if (auto s = line(r.x1, r.y0, r.x1, r.y1, thick); !s) return s;
        if (auto s = line(r.x1, r.y1, r.x0, r.y1, thick); !s) return s;
        return line(r.x0, r.y1, r.x0, r.y0, thick);
    }

    // Текст в ячейке: подбор высоты по ряду ГОСТ 2.304 (от preferred вниз до 2,5) и
    // коэффициента ширины (не меньше 0,6) по шрифту, если он задан.
    Status text(const Rect& cell, const std::string& value, double preferred, HAlign h,
                bool vertical = false, const char* field = "") {
        if (value.empty()) return okStatus();
        auto cps = decodeUtf8(value);
        if (!cps) return cps.error();
        const double along = vertical ? cell.height() : cell.width();
        const double across = vertical ? cell.width() : cell.height();
        const double avail = along - 2.0 * kCellMargin;
        double height = 0.0, wf = 1.0;
        for (auto it = kGostTextHeights.rbegin(); it != kGostTextHeights.rend(); ++it) {
            const double hgt = *it;
            if (hgt > preferred + 1e-9 || hgt > across - 1.0 + 1e-9) continue;
            if (!o_.font) {
                height = hgt;
                break;
            }
            const double w = o_.font->textWidthMm(cps.value(), hgt);
            const double need = w <= avail ? 1.0 : avail / w;
            if (need >= kMinWidthFactor) {
                height = hgt;
                wf = need;
                break;
            }
        }
        if (height == 0.0) {
            return Error{ErrorCode::InvalidInput, std::string("title block: text of field '") + field +
                                                      "' does not fit its cell: " + value};
        }
        Text t;
        t.utf8 = value;
        t.height = height;
        t.widthFactor = wf;
        t.hAlign = h;
        t.vAlign = VAlign::Middle;
        if (!vertical) {
            const double cy = (cell.y0 + cell.y1) / 2.0;
            const double x = h == HAlign::Left ? cell.x0 + kCellMargin
                             : h == HAlign::Right ? cell.x1 - kCellMargin
                                                  : (cell.x0 + cell.x1) / 2.0;
            t.position = {x, cy};
        } else {
            t.rotationDeg = 90.0;
            t.hAlign = HAlign::Center;
            t.position = {(cell.x0 + cell.x1) / 2.0, (cell.y0 + cell.y1) / 2.0};
        }
        return d_.add(Entity{t, o_.textLayer, LineType::SolidThin});
    }

private:
    Drawing& d_;
    const FrameOptions& o_;
};

#define BC_TRY(expr)                    \
    do {                                \
        if (auto _s = (expr); !_s) {    \
            return _s.error();          \
        }                               \
    } while (0)

}  // namespace

Result<TitleBlockLayout> addFrameAndTitleBlock(Drawing& drawing, const TitleBlockData& data,
                                               const FrameOptions& options) {
    const Sheet& sh = drawing.sheet();
    const double W = sh.widthMm, H = sh.heightMm;
    const double tbH = options.form == TitleBlockForm::Form1   ? 55.0
                       : options.form == TitleBlockForm::Form2 ? 40.0
                                                               : 15.0;
    if (W < kLeftMargin + kOtherMargin + kTitleWidth - 1e-9 || H < 2 * kOtherMargin + tbH + 1e-9) {
        return Error{ErrorCode::InvalidInput, "sheet " + sh.name + " is too small for the title block"};
    }
    if (sh.standard == SheetStandard::Gost2301 && sh.gostBase == GostFormat::A4 && sh.multiplier == 1 &&
        sh.orientation == Orientation::Landscape) {
        return Error{ErrorCode::InvalidInput,
                     "GOST 2.104: on A4 the title block is placed only along the short side (use Portrait)"};
    }
    if (options.storageStrip && H < kOtherMargin + 145.0 + kOtherMargin) {
        return Error{ErrorCode::InvalidInput, "sheet too low for the storage strip (graphs 19-23, 145 mm)"};
    }
    if (data.sheet < 1 || data.sheets < 1 || data.sheet > data.sheets) {
        return Error{ErrorCode::InvalidInput, "title block: sheet number must be 1..sheets"};
    }
    if (data.litera.size() > 0) {
        auto l = decodeUtf8(data.litera);
        if (!l) return l.error();
        if (l.value().size() > 3) return Error{ErrorCode::InvalidInput, "title block: litera has at most 3 cells"};
    }
    for (const std::string& name : {options.frameLayer, options.textLayer}) {
        if (!drawing.findLayer(name)) BC_TRY(drawing.addLayer(Layer{name, 7}));
    }

    TitleBlockLayout L;
    L.sheet = {0, 0, W, H};
    L.frame = {kLeftMargin, kOtherMargin, W - kOtherMargin, H - kOtherMargin};
    const double X = L.frame.x1 - kTitleWidth, Y = L.frame.y0;
    L.titleBlock = {X, Y, L.frame.x1, Y + tbH};
    auto R = [&](double x0, double y0, double x1, double y1) { return Rect{X + x0, Y + y0, X + x1, Y + y1}; };

    Builder b(drawing, options);
    BC_TRY(b.rect(L.sheet, false));  // линия обреза листа
    BC_TRY(b.rect(L.frame, true));   // рамка — сплошная толстая основная
    // Нижняя и правая стороны основной надписи совпадают с рамкой.
    BC_TRY(b.line(L.titleBlock.x0, L.titleBlock.y0, L.titleBlock.x0, L.titleBlock.y1, true));
    BC_TRY(b.line(L.titleBlock.x0, L.titleBlock.y1, L.titleBlock.x1, L.titleBlock.y1, true));
    auto hl = [&](double x0, double x1, double y, bool thick) { return b.line(X + x0, Y + y, X + x1, Y + y, thick); };
    auto vl = [&](double x, double y0, double y1, bool thick) { return b.line(X + x, Y + y0, X + x, Y + y1, thick); };

    // Левая часть (65 мм): колонки 7, 10, 23, 15, 10.
    const char* headers[5] = {"Изм.", "Лист", "№ докум.", "Подп.", "Дата"};
    const double colX[6] = {0, 7, 17, 40, 55, 65};

    if (options.form == TitleBlockForm::Form1) {
        // Вертикали левой части; линия x = 7 — только в строках изменений и заголовке.
        BC_TRY(vl(7, 30, 55, true));
        for (double x : {17.0, 40.0, 55.0, 65.0}) BC_TRY(vl(x, 0, 55, true));
        // Горизонтали левой части: 11 строк по 5 мм; границы заголовка (30, 35) — толстые.
        for (int i = 1; i < 11; ++i) {
            const double y = 5.0 * i;
            BC_TRY(hl(0, 65, y, y == 30.0 || y == 35.0));
        }
        // Средняя и правая части.
        BC_TRY(hl(65, 185, 40, true));   // низ графы 2
        BC_TRY(hl(65, 185, 15, true));   // верх граф 3 и 9
        BC_TRY(vl(135, 0, 40, true));    // граница граф 1/3 и правой части
        BC_TRY(hl(135, 185, 35, true));  // низ заголовков «Лит.», «Масса», «Масштаб»
        BC_TRY(hl(135, 185, 20, true));  // верх строки «Лист / Листов»
        BC_TRY(vl(140, 20, 35, false));  // клетки литеры
        BC_TRY(vl(145, 20, 35, false));
        BC_TRY(vl(150, 20, 40, true));
        BC_TRY(vl(167, 20, 40, true));
        BC_TRY(vl(155, 15, 20, true));

        L.graphs[1] = R(65, 15, 135, 40);
        L.graphs[2] = R(65, 40, 185, 55);
        L.graphs[3] = R(65, 0, 135, 15);
        L.graphs[4] = R(135, 20, 150, 35);
        L.graphs[5] = R(150, 20, 167, 35);
        L.graphs[6] = R(167, 20, 185, 35);
        L.graphs[7] = R(135, 15, 155, 20);
        L.graphs[8] = R(155, 15, 185, 20);
        L.graphs[9] = R(135, 0, 185, 15);
        L.graphs[10] = R(0, 0, 17, 30);
        L.graphs[11] = R(17, 0, 40, 30);
        L.graphs[12] = R(40, 0, 55, 30);
        L.graphs[13] = R(55, 0, 65, 30);
        for (int k = 0; k < 5; ++k) L.graphs[14 + k] = R(colX[k], 35, colX[k + 1], 55);

        for (int k = 0; k < 5; ++k) BC_TRY(b.text(R(colX[k], 30, colX[k + 1], 35), headers[k], 2.5, HAlign::Center, false, headers[k]));
        BC_TRY(b.text(R(135, 35, 150, 40), "Лит.", 2.5, HAlign::Center));
        BC_TRY(b.text(R(150, 35, 167, 40), "Масса", 2.5, HAlign::Center));
        BC_TRY(b.text(R(167, 35, 185, 40), "Масштаб", 2.5, HAlign::Center));

        struct Row {
            const char* label;
            const SignatureRow* sig;
        };
        const Row rows[6] = {{"Разраб.", &data.developer}, {"Пров.", &data.checker},
                             {"Т.контр.", &data.techControl}, {"", nullptr},
                             {"Н.контр.", &data.normControl}, {"Утв.", &data.approver}};
        for (int i = 0; i < 6; ++i) {
            const double y1 = 30.0 - 5.0 * i, y0 = y1 - 5.0;
            BC_TRY(b.text(R(0, y0, 17, y1), rows[i].label, 2.5, HAlign::Left, false, rows[i].label));
            if (rows[i].sig) {
                BC_TRY(b.text(R(17, y0, 40, y1), rows[i].sig->surname, 2.5, HAlign::Left, false, "surname"));
                BC_TRY(b.text(R(55, y0, 65, y1), rows[i].sig->date, 2.5, HAlign::Center, false, "date"));
            }
        }

        BC_TRY(b.text(L.graphs[2], data.designation, 7.0, HAlign::Center, false, "designation"));
        BC_TRY(b.text(L.graphs[1], data.name, 7.0, HAlign::Center, false, "name"));
        BC_TRY(b.text(L.graphs[3], data.material, 5.0, HAlign::Center, false, "material"));
        if (!data.litera.empty()) {
            const auto lit = decodeUtf8(data.litera).value();
            for (std::size_t i = 0; i < lit.size(); ++i) {
                BC_TRY(b.text(R(135 + 5.0 * double(i), 20, 140 + 5.0 * double(i), 35),
                              encodeUtf8(std::u32string(1, lit[i])), 5.0, HAlign::Center, false, "litera"));
            }
        }
        BC_TRY(b.text(L.graphs[5], data.mass, 5.0, HAlign::Center, false, "mass"));
        BC_TRY(b.text(L.graphs[6], data.scale, 5.0, HAlign::Center, false, "scale"));
        BC_TRY(b.text(L.graphs[7], "Лист " + std::to_string(data.sheet), 2.5, HAlign::Left, false, "sheet"));
        BC_TRY(b.text(L.graphs[8], "Листов " + std::to_string(data.sheets), 2.5, HAlign::Left, false, "sheets"));
        BC_TRY(b.text(L.graphs[9], data.organization, 5.0, HAlign::Center, false, "organization"));
    } else if (options.form == TitleBlockForm::Form2) {
        // Форма 2 (185×40, первый лист текстовых документов): левая часть — 8 строк по 5 мм:
        // две строки изменений (30…40), заголовок (25…30), подписи (0…25: Разраб., Пров.,
        // резерв, Н.контр., Утв.). Правая часть: графа 2 — 65…185 × 25…40; графа 1 —
        // 65…135 × 0…25; «Лит.» (3 клетки по 5), «Лист» 15, «Листов» 20 — заголовки 20…25,
        // значения 15…20; графа 9 — 135…185 × 0…15.
        BC_TRY(vl(7, 25, 40, true));
        for (double x : {17.0, 40.0, 55.0, 65.0}) BC_TRY(vl(x, 0, 40, true));
        for (int i = 1; i < 8; ++i) {
            const double y = 5.0 * i;
            BC_TRY(hl(0, 65, y, y == 25.0 || y == 30.0));
        }
        BC_TRY(hl(65, 185, 25, true));
        BC_TRY(hl(135, 185, 20, true));
        BC_TRY(hl(135, 185, 15, true));
        BC_TRY(vl(135, 0, 25, true));
        BC_TRY(vl(150, 15, 25, true));
        BC_TRY(vl(165, 15, 25, true));
        BC_TRY(vl(140, 15, 20, false));
        BC_TRY(vl(145, 15, 20, false));

        L.graphs[1] = R(65, 0, 135, 25);
        L.graphs[2] = R(65, 25, 185, 40);
        L.graphs[4] = R(135, 15, 150, 20);
        L.graphs[7] = R(150, 15, 165, 20);
        L.graphs[8] = R(165, 15, 185, 20);
        L.graphs[9] = R(135, 0, 185, 15);
        L.graphs[10] = R(0, 0, 17, 25);
        L.graphs[11] = R(17, 0, 40, 25);
        L.graphs[12] = R(40, 0, 55, 25);
        L.graphs[13] = R(55, 0, 65, 25);
        for (int k = 0; k < 5; ++k) L.graphs[14 + k] = R(colX[k], 30, colX[k + 1], 40);

        for (int k = 0; k < 5; ++k) BC_TRY(b.text(R(colX[k], 25, colX[k + 1], 30), headers[k], 2.5, HAlign::Center, false, headers[k]));
        BC_TRY(b.text(R(135, 20, 150, 25), "Лит.", 2.5, HAlign::Center));
        BC_TRY(b.text(R(150, 20, 165, 25), "Лист", 2.5, HAlign::Center));
        BC_TRY(b.text(R(165, 20, 185, 25), "Листов", 2.5, HAlign::Center));

        struct Row {
            const char* label;
            const SignatureRow* sig;
        };
        const Row rows[5] = {{"Разраб.", &data.developer}, {"Пров.", &data.checker}, {"", nullptr},
                             {"Н.контр.", &data.normControl}, {"Утв.", &data.approver}};
        for (int i = 0; i < 5; ++i) {
            const double y1 = 25.0 - 5.0 * i, y0 = y1 - 5.0;
            BC_TRY(b.text(R(0, y0, 17, y1), rows[i].label, 2.5, HAlign::Left, false, rows[i].label));
            if (rows[i].sig) {
                BC_TRY(b.text(R(17, y0, 40, y1), rows[i].sig->surname, 2.5, HAlign::Left, false, "surname"));
                BC_TRY(b.text(R(55, y0, 65, y1), rows[i].sig->date, 2.5, HAlign::Center, false, "date"));
            }
        }
        BC_TRY(b.text(L.graphs[2], data.designation, 7.0, HAlign::Center, false, "designation"));
        BC_TRY(b.text(L.graphs[1], data.name, 7.0, HAlign::Center, false, "name"));
        if (!data.litera.empty()) {
            const auto lit = decodeUtf8(data.litera).value();
            for (std::size_t i = 0; i < lit.size(); ++i) {
                BC_TRY(b.text(R(135 + 5.0 * double(i), 15, 140 + 5.0 * double(i), 20),
                              encodeUtf8(std::u32string(1, lit[i])), 2.5, HAlign::Center, false, "litera"));
            }
        }
        BC_TRY(b.text(L.graphs[7], std::to_string(data.sheet), 2.5, HAlign::Center, false, "sheet"));
        BC_TRY(b.text(L.graphs[8], std::to_string(data.sheets), 2.5, HAlign::Center, false, "sheets"));
        BC_TRY(b.text(L.graphs[9], data.organization, 5.0, HAlign::Center, false, "organization"));
    } else {
        // Форма 2а: 185×15; левая часть — две строки изменений и заголовок снизу.
        for (double x : {7.0, 17.0, 40.0, 55.0, 65.0, 175.0}) BC_TRY(vl(x, 0, 15, true));
        BC_TRY(hl(0, 65, 5, true));
        BC_TRY(hl(0, 65, 10, false));
        BC_TRY(hl(175, 185, 8, true));
        L.graphs[2] = R(65, 0, 175, 15);
        L.graphs[7] = R(175, 0, 185, 15);
        for (int k = 0; k < 5; ++k) L.graphs[14 + k] = R(colX[k], 5, colX[k + 1], 15);
        for (int k = 0; k < 5; ++k) BC_TRY(b.text(R(colX[k], 0, colX[k + 1], 5), headers[k], 2.5, HAlign::Center, false, headers[k]));
        BC_TRY(b.text(L.graphs[2], data.designation, 7.0, HAlign::Center, false, "designation"));
        BC_TRY(b.text(R(175, 8, 185, 15), "Лист", 2.5, HAlign::Center, false, "sheet label"));
        BC_TRY(b.text(R(175, 0, 185, 8), std::to_string(data.sheet), 5.0, HAlign::Center, false, "sheet"));
    }

    if (options.storageStrip) {
        // Графы 19–23 вдоль левой границы рамки: 12 мм (5 — наименование, 7 — значение).
        const double sx0 = kLeftMargin - 12.0, sx1 = kLeftMargin, sxm = sx0 + 5.0;
        const double lengths[5] = {25, 35, 25, 25, 35};
        const char* labels[5] = {"Инв. № подл.", "Подп. и дата", "Взам. инв. №", "Инв. № дубл.", "Подп. и дата"};
        double y = kOtherMargin;
        BC_TRY(b.line(sxm, y, sxm, y + 145.0, true));
        for (int i = 0; i < 5; ++i) {
            const Rect cell{sx0, y, sx1, y + lengths[i]};
            L.graphs[19 + i] = cell;
            BC_TRY(b.rect(cell, true));
            BC_TRY(b.text(Rect{sx0, y, sxm, y + lengths[i]}, labels[i], 2.5, HAlign::Center, true, labels[i]));
            y += lengths[i];
        }
    }
    return L;
}

}  // namespace bendcore::drawing_v2
