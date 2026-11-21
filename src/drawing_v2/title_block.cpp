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
constexpr double kCellMargin = 0.5;

class Builder {
public:
    Builder(Drawing& d, const TextMetrics* font, const std::string& frameLayer, const std::string& textLayer)
        : d_(d), font_(font), frameLayer_(frameLayer), textLayer_(textLayer) {}

    Status line(double x0, double y0, double x1, double y1, bool thick) {
        return d_.add(Entity{LineSeg{{x0, y0}, {x1, y1}}, frameLayer_,
                             thick ? LineType::SolidThick : LineType::SolidThin});
    }
    Status rect(const Rect& r, bool thick) {
        if (auto s = line(r.x0, r.y0, r.x1, r.y0, thick); !s) return s;
        if (auto s = line(r.x1, r.y0, r.x1, r.y1, thick); !s) return s;
        if (auto s = line(r.x1, r.y1, r.x0, r.y1, thick); !s) return s;
        return line(r.x0, r.y1, r.x0, r.y0, thick);
    }

    // Текст в ячейке: подбор высоты по ряду ГОСТ 2.304 (от preferred вниз до 2,5) и
    // коэффициента ширины (не меньше TextMetrics::minWidthFactor: TrueType — 0,6, шрифт
    // ГОСТ 2.304 — без сжатия) по метрикам шрифта, если они заданы.
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
            if (!font_) {
                height = hgt;
                break;
            }
            const double w = font_->textWidthMm(cps.value(), hgt);
            const double need = w <= avail ? 1.0 : avail / w;
            if (need >= font_->minWidthFactor() - 1e-12) {
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
        return d_.add(Entity{t, textLayer_, LineType::SolidThin});
    }

private:
    Drawing& d_;
    const TextMetrics* font_;
    const std::string& frameLayer_;
    const std::string& textLayer_;
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

    Builder b(drawing, options.font, options.frameLayer, options.textLayer);
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

// ---------------------------------------------------------------- ISO 7200 / ISO 5457

std::string iso7200FieldLabel(Iso7200Field field, const std::string& language) {
    const bool ru = language.size() >= 2 && (language.compare(0, 2, "ru") == 0 || language.compare(0, 2, "RU") == 0);
    switch (field) {
        case Iso7200Field::Department: return ru ? "Отдел" : "Responsible dept.";
        case Iso7200Field::Creator: return ru ? "Разработал" : "Created by";
        case Iso7200Field::Approver: return ru ? "Утвердил" : "Approved by";
        case Iso7200Field::Projection: return "";
        case Iso7200Field::Scale: return ru ? "Масштаб" : "Scale";
        case Iso7200Field::LegalOwner: return ru ? "Владелец" : "Legal owner";
        case Iso7200Field::DocumentType: return ru ? "Тип документа" : "Document type";
        case Iso7200Field::Title: return ru ? "Наименование" : "Title";
        case Iso7200Field::DocumentStatus: return ru ? "Статус документа" : "Document status";
        case Iso7200Field::Identification: return ru ? "Обозначение" : "Identification number";
        case Iso7200Field::Revision: return ru ? "Изм." : "Rev.";
        case Iso7200Field::IssueDate: return ru ? "Дата выпуска" : "Date of issue";
        case Iso7200Field::Language: return ru ? "Язык" : "Lang.";
        case Iso7200Field::Sheet: return ru ? "Лист" : "Sheet";
    }
    return "";
}

Result<ProjectionSymbolGeometry> projectionSymbolGeometry(ProjectionSymbol method, Point2 center, double h) {
    using R = Result<ProjectionSymbolGeometry>;
    if (method == ProjectionSymbol::None) return R::failure(ErrorCode::InvalidInput, "projection symbol: no projection method");
    if (!(h > 0.0) || !std::isfinite(h) || !std::isfinite(center.x) || !std::isfinite(center.y))
        return R::failure(ErrorCode::InvalidInput, "projection symbol: height must be positive and finite");
    // Габарит 5h × 2h: трапеция длиной 2h, зазор h, окружности диаметром 2h.
    const double H = 2.0 * h, total = 5.0 * h;
    const double left = center.x - total / 2.0;
    const bool first = method == ProjectionSymbol::FirstAngle;
    const double tx0 = first ? left : left + total - H;  // трапеция
    const double cx = first ? left + total - h : left + h;  // центр окружностей
    const double cy = center.y;
    ProjectionSymbolGeometry g;
    // Узкий торец (диаметр h) — слева, широкий (диаметр 2h) — справа.
    const Point2 a{tx0, cy - h / 2.0}, b{tx0 + H, cy - h}, c{tx0 + H, cy + h}, d{tx0, cy + h / 2.0};
    g.outline = {LineSeg{a, b}, LineSeg{b, c}, LineSeg{c, d}, LineSeg{d, a}};
    g.innerCircle = Circle{{cx, cy}, h / 2.0};
    g.outerCircle = Circle{{cx, cy}, h};
    const double over = h / 4.0;  // выход осевых за контур
    g.axes = {LineSeg{{left - over, cy}, {left + total + over, cy}}, LineSeg{{cx, cy - h - over}, {cx, cy + h + over}}};
    g.bounds = {left, cy - h, left + total, cy + h};
    return g;
}

Status addProjectionSymbol(Drawing& drawing, const ProjectionSymbolGeometry& g, const std::string& layer) {
    for (const LineSeg& l : g.outline)
        if (auto s = drawing.add(Entity{l, layer, LineType::SolidThin}); !s) return s;
    for (const Circle& c : {g.innerCircle, g.outerCircle})
        if (auto s = drawing.add(Entity{c, layer, LineType::SolidThin}); !s) return s;
    for (const LineSeg& l : g.axes)
        if (auto s = drawing.add(Entity{l, layer, LineType::DashDotThin}); !s) return s;
    return okStatus();
}

Result<TitleBlockLayout> addIsoFrameAndTitleBlock(Drawing& drawing, const Iso7200Data& data, const IsoFrameOptions& options) {
    const Sheet& sh = drawing.sheet();
    const double W = sh.widthMm, H = sh.heightMm;
    if (W < kIso5457LeftMargin + kIso5457Margin + kIso7200Width - 1e-9 || H < 2 * kIso5457Margin + kIso7200Height + 1e-9)
        return Error{ErrorCode::InvalidInput, "sheet " + sh.name + " is too small for the ISO 7200 title block"};
    if (data.sheet < 1 || data.sheets < 1 || data.sheet > data.sheets)
        return Error{ErrorCode::InvalidInput, "title block: sheet number must be 1..sheets"};
    for (const std::string& name : {options.frameLayer, options.textLayer}) {
        if (!drawing.findLayer(name)) BC_TRY(drawing.addLayer(Layer{name, 7}));
    }

    TitleBlockLayout L;
    L.sheet = {0, 0, W, H};
    L.frame = {kIso5457LeftMargin, kIso5457Margin, W - kIso5457Margin, H - kIso5457Margin};
    const double X = L.frame.x1 - kIso7200Width, Y = L.frame.y0;
    L.titleBlock = {X, Y, L.frame.x1, Y + kIso7200Height};
    auto R = [&](double x0, double y0, double x1, double y1) { return Rect{X + x0, Y + y0, X + x1, Y + y1}; };

    Builder b(drawing, options.font, options.frameLayer, options.textLayer);
    BC_TRY(b.rect(L.sheet, false));
    BC_TRY(b.rect(L.frame, true));
    if (options.centringMarks) {
        // ISO 5457, п. 3.3: метки центрирования — от края листа до 5 мм за рамку внутрь.
        const double mx = W / 2.0, my = H / 2.0;
        BC_TRY(b.line(mx, 0.0, mx, L.frame.y0 + 5.0, true));
        BC_TRY(b.line(mx, H, mx, L.frame.y1 - 5.0, true));
        BC_TRY(b.line(0.0, my, L.frame.x0 + 5.0, my, true));
        BC_TRY(b.line(W, my, L.frame.x1 - 5.0, my, true));
    }

    using F = Iso7200Field;
    struct Cell {
        F field;
        Rect r;
    };
    // Раскладка 180×36: строки по 9 мм; левая часть 60 мм, середина 60, правая 60.
    const Cell cells[] = {
        {F::Department, R(0, 27, 30, 36)},      {F::Creator, R(30, 27, 60, 36)},
        {F::Projection, R(0, 18, 15, 27)},      {F::Scale, R(15, 18, 30, 27)},
        {F::Approver, R(30, 18, 60, 27)},       {F::LegalOwner, R(0, 0, 60, 18)},
        {F::DocumentType, R(60, 27, 120, 36)},  {F::Title, R(60, 0, 120, 27)},
        {F::DocumentStatus, R(120, 27, 180, 36)}, {F::Identification, R(120, 9, 180, 27)},
        {F::Revision, R(120, 0, 130, 9)},       {F::IssueDate, R(130, 0, 156, 9)},
        {F::Language, R(156, 0, 166, 9)},       {F::Sheet, R(166, 0, 180, 9)},
    };
    // Контур надписи — сплошная толстая (нижняя и правая стороны совпадают с рамкой), границы
    // полей — сплошная тонкая (ISO 7200, п. 3.2: линии как у рамки и тонкие внутри).
    BC_TRY(b.line(L.titleBlock.x0, L.titleBlock.y0, L.titleBlock.x0, L.titleBlock.y1, true));
    BC_TRY(b.line(L.titleBlock.x0, L.titleBlock.y1, L.titleBlock.x1, L.titleBlock.y1, true));
    auto hl = [&](double x0, double x1, double y) { return b.line(X + x0, Y + y, X + x1, Y + y, false); };
    auto vl = [&](double x, double y0, double y1) { return b.line(X + x, Y + y0, X + x, Y + y1, false); };
    BC_TRY(hl(0, 180, 27));
    BC_TRY(hl(0, 60, 18));
    BC_TRY(hl(120, 180, 9));
    BC_TRY(vl(60, 0, 36));
    BC_TRY(vl(120, 0, 36));
    BC_TRY(vl(30, 18, 36));
    BC_TRY(vl(15, 18, 27));
    for (double x : {130.0, 156.0, 166.0}) BC_TRY(vl(x, 0, 9));

    auto value = [&](F f) -> std::string {
        switch (f) {
            case F::Department: return data.department;
            case F::Creator: return data.creator;
            case F::Approver: return data.approver;
            case F::Projection: return "";
            case F::Scale: return data.scale;
            case F::LegalOwner: return data.legalOwner;
            case F::DocumentType: return data.documentType;
            case F::Title: return data.title;
            case F::DocumentStatus: return data.documentStatus;
            case F::Identification: return data.identification;
            case F::Revision: return data.revision;
            case F::IssueDate: return data.issueDate;
            case F::Language: return data.language;
            case F::Sheet: return std::to_string(data.sheet) + "/" + std::to_string(data.sheets);
        }
        return "";
    };
    constexpr double kLabelStrip = 3.5;  // полоса наименования поля: h = 2,5 и 0,5 мм сверху и снизу
    for (const Cell& c : cells) {
        L.graphs[int(c.field)] = c.r;
        if (c.field == F::Projection) continue;
        const std::string label = iso7200FieldLabel(c.field, data.language);
        BC_TRY(b.text(Rect{c.r.x0, c.r.y1 - kLabelStrip, c.r.x1, c.r.y1}, label, 2.5, HAlign::Left, false, label.c_str()));
        const bool big = c.field == F::Title || c.field == F::Identification;
        const bool centered = c.field == F::Revision || c.field == F::Language || c.field == F::Sheet;
        const std::string v = value(c.field);
        const std::string name = label.empty() ? "value" : label;
        BC_TRY(b.text(Rect{c.r.x0, c.r.y0, c.r.x1, c.r.y1 - kLabelStrip}, v, big ? 7.0 : 3.5,
                      big || centered ? HAlign::Center : HAlign::Left, false, name.c_str()));
    }
    if (data.projection != ProjectionSymbol::None) {
        const Rect& pc = L.graphs.at(int(F::Projection));
        auto g = projectionSymbolGeometry(data.projection, {(pc.x0 + pc.x1) / 2.0, (pc.y0 + pc.y1) / 2.0}, 2.5);
        if (!g) return g.error();
        BC_TRY(addProjectionSymbol(drawing, g.value(), options.frameLayer));
        L.projectionSymbol = g.value().bounds;
    }
    return L;
}

}  // namespace bendcore::drawing_v2
