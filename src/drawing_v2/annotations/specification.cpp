// Спецификация ГОСТ 2.106-96 из состава сборки и перечень составных частей ISO 7573.
// Правила и допущения — docs/drawing/SPECIFICATION.md.

#include <bendcore/drawing_v2/specification.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace bendcore::drawing_v2::annotations {

namespace {

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

constexpr SpecSection kOrder[] = {SpecSection::Documentation, SpecSection::Complexes,
                                  SpecSection::AssemblyUnits, SpecSection::Parts,
                                  SpecSection::StandardProducts, SpecSection::OtherProducts,
                                  SpecSection::Materials, SpecSection::Kits};

bool sortedByDesignation(SpecSection s) {
    return s == SpecSection::Complexes || s == SpecSection::AssemblyUnits || s == SpecSection::Parts ||
           s == SpecSection::Kits;
}

// Текст в ячейке: высота по ряду ГОСТ 2.304 от preferred вниз до 2,5 и сжатие до 0,6.
Result<Text> fitText(const Rect& cell, const std::string& value, double preferred, HAlign h, bool vertical,
                     const TrueTypeFont* font, const char* what) {
    constexpr double kMargin = 0.5, kMinWf = 0.6;
    const double along = vertical ? cell.height() : cell.width();
    const double across = vertical ? cell.width() : cell.height();
    const double avail = along - 2.0 * kMargin;
    for (auto it = kGostTextHeights.rbegin(); it != kGostTextHeights.rend(); ++it) {
        const double hgt = *it;
        if (hgt > preferred + 1e-9 || hgt > across - 1.0 + 1e-9) continue;
        const double w = estimateTextWidth(value, hgt, font);
        const double wf = w <= avail ? 1.0 : avail / w;
        if (wf < kMinWf) continue;
        Text t;
        t.utf8 = value;
        t.height = hgt;
        t.widthFactor = wf;
        t.vAlign = VAlign::Middle;
        if (vertical) {
            t.rotationDeg = 90.0;
            t.hAlign = HAlign::Center;
            t.position = {(cell.x0 + cell.x1) / 2.0, (cell.y0 + cell.y1) / 2.0};
        } else {
            t.hAlign = h;
            const double x = h == HAlign::Left ? cell.x0 + kMargin + 0.5
                             : h == HAlign::Right ? cell.x1 - kMargin
                                                  : (cell.x0 + cell.x1) / 2.0;
            t.position = {x, (cell.y0 + cell.y1) / 2.0};
        }
        return t;
    }
    return invalid(std::string("specification: text of column '") + what + "' does not fit: " + value);
}

#define BC_TRY(expr)                 \
    do {                             \
        if (auto _s = (expr); !_s) { \
            return _s.error();       \
        }                            \
    } while (0)

constexpr const char* kColumnNames[7] = {"Формат", "Зона", "Поз.", "Обозначение", "Наименование", "Кол.",
                                         "Примечание"};

}  // namespace

const char* sectionTitle(SpecSection s) {
    switch (s) {
        case SpecSection::Documentation: return "Документация";
        case SpecSection::Complexes: return "Комплексы";
        case SpecSection::AssemblyUnits: return "Сборочные единицы";
        case SpecSection::Parts: return "Детали";
        case SpecSection::StandardProducts: return "Стандартные изделия";
        case SpecSection::OtherProducts: return "Прочие изделия";
        case SpecSection::Materials: return "Материалы";
        case SpecSection::Kits: return "Комплекты";
    }
    return "";
}

bool naturalLess(const std::string& a, const std::string& b) {
    std::size_t i = 0, j = 0;
    auto digit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    while (i < a.size() && j < b.size()) {
        if (digit(a[i]) && digit(b[j])) {
            std::size_t i2 = i, j2 = j;
            while (i2 < a.size() && digit(a[i2])) ++i2;
            while (j2 < b.size() && digit(b[j2])) ++j2;
            std::string na = a.substr(i, i2 - i), nb = b.substr(j, j2 - j);
            na.erase(0, std::min(na.find_first_not_of('0'), na.size()));
            nb.erase(0, std::min(nb.find_first_not_of('0'), nb.size()));
            if (na.size() != nb.size()) return na.size() < nb.size();
            if (na != nb) return na < nb;
            i = i2;
            j = j2;
            continue;
        }
        // Байты UTF-8 сравниваются как беззнаковые: порядок кодовых точек (кириллица А…я).
        const auto ca = static_cast<unsigned char>(a[i]), cb = static_cast<unsigned char>(b[j]);
        if (ca != cb) return ca < cb;
        ++i;
        ++j;
    }
    return a.size() - i < b.size() - j;
}

std::optional<int> Specification::positionOf(std::uint32_t definition) const {
    for (const auto& s : sections) {
        for (const auto& it : s.items) {
            if (it.definition && *it.definition == definition && it.position > 0) return it.position;
        }
    }
    return std::nullopt;
}

const SpecSectionData* Specification::section(SpecSection s) const {
    for (const auto& d : sections) {
        if (d.section == s) return &d;
    }
    return nullptr;
}

Result<Specification> buildSpecification(const assembly_v2::AssemblyDocument& doc, const SpecificationOptions& o) {
    auto bom = assembly_v2::structuredBom(doc);
    if (!bom) return bom.error();
    const auto& root = doc.definition(doc.root());
    Specification spec;
    spec.designation = root.properties.designation;
    spec.name = root.properties.name.empty() ? root.name : root.properties.name;

    std::map<SpecSection, std::vector<SpecItem>> buckets;
    if (o.addAssemblyDrawing) {
        if (spec.designation.empty()) return invalid("specification: the assembly has no designation (assembly drawing entry)");
        SpecItem d;
        d.format = o.assemblyDrawingFormat;
        d.designation = spec.designation + " СБ";
        d.name = "Сборочный чертёж";
        buckets[SpecSection::Documentation].push_back(d);
    }
    for (const auto& d : o.documents) {
        if (d.position != 0) return invalid("specification: documents have no position numbers");
        buckets[SpecSection::Documentation].push_back(d);
    }
    for (const auto& row : bom.value().rows) {
        if (row.level != 0) continue;
        SpecSection sec = row.kind == assembly_v2::DefinitionKind::Assembly ? SpecSection::AssemblyUnits
                          : row.designation.empty()                         ? SpecSection::StandardProducts
                                                                            : SpecSection::Parts;
        if (auto it = o.sectionOverride.find(row.definition); it != o.sectionOverride.end()) sec = it->second;
        if (sec == SpecSection::Documentation) return invalid("specification: a component cannot be a document");
        SpecItem item;
        item.designation = row.designation;
        item.name = row.name;
        item.material = row.material;
        item.quantity = row.quantity;
        item.definition = row.definition;
        if (auto it = o.formats.find(row.definition); it != o.formats.end()) item.format = it->second;
        if (auto it = o.notes.find(row.definition); it != o.notes.end()) item.note = it->second;
        if (item.name.empty()) return invalid("specification: component without a name (definition " + std::to_string(row.definition) + ")");
        buckets[sec].push_back(std::move(item));
    }

    int pos = 1;
    for (SpecSection s : kOrder) {
        auto it = buckets.find(s);
        if (it == buckets.end() || it->second.empty()) continue;
        auto& items = it->second;
        if (s != SpecSection::Documentation) {
            std::stable_sort(items.begin(), items.end(), [&](const SpecItem& a, const SpecItem& b) {
                if (sortedByDesignation(s)) {
                    if (a.designation.empty() != b.designation.empty()) return b.designation.empty();
                    if (a.designation != b.designation) return naturalLess(a.designation, b.designation);
                    return naturalLess(a.name, b.name);
                }
                if (a.name != b.name) return naturalLess(a.name, b.name);
                return naturalLess(a.designation, b.designation);
            });
            for (auto& item : items) item.position = pos++;
            pos += std::max(0, o.reservePositions);
        }
        spec.sections.push_back({s, std::move(items)});
    }
    return spec;
}

const std::array<double, 7>& specColumnWidths() {
    static const std::array<double, 7> w = {6, 6, 8, 70, 63, 10, 22};
    return w;
}

Result<SpecDocument> renderSpecification(const Specification& spec, const SpecSheetOptions& o) {
    if (spec.sections.empty()) return invalid("specification is empty");
    const auto& W = specColumnWidths();
    const double h = o.textHeight;
    AnnotationStyle st;
    st.font = o.font;
    const double nameWidth = W[4] - 2.0;

    // 1. Строки.
    struct Block {
        std::vector<SpecRow> rows;
        bool header = false;  // заголовок раздела с пустыми строками — не разрывать с первой позицией
    };
    std::vector<Block> blocks;
    for (const auto& sec : spec.sections) {
        Block hb;
        hb.header = true;
        hb.rows.push_back({});
        SpecRow hr;
        hr.kind = SpecRowKind::Header;
        hr.cells[4] = sectionTitle(sec.section);
        hb.rows.push_back(hr);
        hb.rows.push_back({});
        blocks.push_back(hb);
        for (const auto& item : sec.items) {
            auto lines = wrapText(item.name, nameWidth, h, st);
            if (!lines) return lines.error();
            Block ib;
            for (std::size_t i = 0; i < lines.value().size(); ++i) {
                SpecRow r;
                r.kind = SpecRowKind::Item;
                r.continuation = i > 0;
                r.cells[4] = lines.value()[i];
                if (i == 0) {
                    r.cells[0] = item.format;
                    r.cells[1] = item.zone;
                    r.cells[2] = item.position > 0 ? std::to_string(item.position) : "";
                    r.cells[3] = item.designation;
                    r.cells[5] = item.quantity ? std::to_string(*item.quantity) : "";
                    r.cells[6] = item.note;
                }
                ib.rows.push_back(r);
            }
            blocks.push_back(ib);
        }
    }

    // 2. Листы: ёмкость по высоте рамки минус основная надпись и строка заголовков граф.
    auto gs = gostSheet(GostFormat::A4, 1, Orientation::Portrait);
    if (!gs) return gs.error();
    const double frameH = gs.value().heightMm - 10.0;
    SpecDocument doc;
    doc.firstSheetRows = int(std::floor((frameH - 40.0 - o.headerHeight) / o.rowHeight + 1e-9));
    doc.otherSheetRows = int(std::floor((frameH - 15.0 - o.headerHeight) / o.rowHeight + 1e-9));
    std::vector<std::vector<SpecRow>> pages(1);
    auto capacity = [&](std::size_t page) { return std::size_t(page == 0 ? doc.firstSheetRows : doc.otherSheetRows); };
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        std::size_t need = blocks[b].rows.size();
        // Заголовок раздела переносится вместе с первой строкой первой позиции.
        if (blocks[b].header && b + 1 < blocks.size()) need += 1;
        if (need > capacity(0) && need > capacity(1)) return invalid("specification: entry taller than a sheet");
        if (pages.back().size() + need > capacity(pages.size() - 1)) pages.emplace_back();
        auto& page = pages.back();
        // Пустая строка перед заголовком в начале листа не нужна.
        std::size_t start = (blocks[b].header && page.empty()) ? 1 : 0;
        for (std::size_t i = start; i < blocks[b].rows.size(); ++i) page.push_back(blocks[b].rows[i]);
    }

    // 3. Чертежи листов.
    for (std::size_t p = 0; p < pages.size(); ++p) {
        auto dr = Drawing::create(gs.value(), 0.5);
        if (!dr) return dr.error();
        Drawing d = std::move(dr).value();
        TitleBlockData td = o.title;
        td.designation = spec.designation;
        td.name = spec.name;
        td.sheet = int(p) + 1;
        td.sheets = int(pages.size());
        FrameOptions fo;
        fo.form = p == 0 ? TitleBlockForm::Form2 : TitleBlockForm::Form2a;
        fo.storageStrip = o.storageStrip;
        fo.font = o.font;
        auto lay = addFrameAndTitleBlock(d, td, fo);
        if (!lay) return lay.error();
        const Rect fr = lay.value().frame;
        const double top = fr.y1, bottom = lay.value().titleBlock.y1;
        const double headBottom = top - o.headerHeight;
        const std::string layer = "TABLE";
        BC_TRY(ensureLayer(d, layer, 7));
        auto line = [&](double x0, double y0, double x1, double y1, bool thick) {
            return d.add(Entity{LineSeg{{x0, y0}, {x1, y1}}, layer, thick ? LineType::SolidThick : LineType::SolidThin});
        };
        // Вертикали граф и линия под заголовками — основные; строки — тонкие.
        double x = fr.x0;
        std::array<double, 8> xs{};
        xs[0] = x;
        for (int c = 0; c < 7; ++c) {
            x += W[c];
            xs[c + 1] = x;
        }
        for (int c = 1; c < 7; ++c) BC_TRY(line(xs[c], top, xs[c], bottom, true));
        BC_TRY(line(fr.x0, headBottom, fr.x1, headBottom, true));
        const int cap = int(capacity(p));
        for (int r = 1; r < cap; ++r) {
            const double y = headBottom - r * o.rowHeight;
            BC_TRY(line(fr.x0, y, fr.x1, y, false));
        }
        for (int c = 0; c < 7; ++c) {
            const Rect cell{xs[c], headBottom, xs[c + 1], top};
            auto t = fitText(cell, kColumnNames[c], 5.0, HAlign::Center, c < 3, o.font, kColumnNames[c]);
            if (!t) return t.error();
            BC_TRY(d.add(Entity{t.value(), layer, LineType::SolidThin}));
        }
        for (std::size_t r = 0; r < pages[p].size(); ++r) {
            const SpecRow& row = pages[p][r];
            const double y1 = headBottom - double(r) * o.rowHeight, y0 = y1 - o.rowHeight;
            for (int c = 0; c < 7; ++c) {
                if (row.cells[c].empty()) continue;
                const Rect cell{xs[c], y0, xs[c + 1], y1};
                const bool header = row.kind == SpecRowKind::Header;
                const HAlign al = header || c == 0 || c == 1 || c == 2 || c == 5 ? HAlign::Center : HAlign::Left;
                auto t = fitText(cell, row.cells[c], h, al, false, o.font, kColumnNames[c]);
                if (!t) return t.error();
                BC_TRY(d.add(Entity{t.value(), layer, LineType::SolidThin}));
                if (header) {
                    // Заголовок раздела подчёркивают тонкой линией.
                    const double w = estimateTextWidth(row.cells[c], t.value().height, o.font, t.value().widthFactor);
                    const double cx = (cell.x0 + cell.x1) / 2.0, yl = t.value().position.y - t.value().height / 2.0 - 0.8;
                    BC_TRY(line(cx - w / 2.0, yl, cx + w / 2.0, yl, false));
                }
            }
        }
        doc.sheets.push_back(std::move(d));
    }
    doc.rows = std::move(pages);
    return doc;
}

Result<Rect> addPartsList(Drawing& d, const TitleBlockLayout& layout, const Specification& spec,
                          const PartsListOptions& o) {
    std::vector<const SpecItem*> items;
    for (const auto& s : spec.sections) {
        for (const auto& it : s.items) {
            if (it.position > 0) items.push_back(&it);
        }
    }
    if (items.empty()) return invalid("parts list: no positioned items");
    std::sort(items.begin(), items.end(), [](const SpecItem* a, const SpecItem* b) { return a->position < b->position; });
    double total = 0.0;
    for (double w : o.widths) {
        if (!(w > 0)) return invalid("parts list: column widths must be positive");
        total += w;
    }
    const double x0 = layout.titleBlock.x1 - total, y0 = layout.titleBlock.y1;
    const double y1 = y0 + o.rowHeight * double(items.size() + 1);
    if (x0 < layout.frame.x0 - 1e-9 || y1 > layout.frame.y1 + 1e-9) return invalid("parts list does not fit the frame");
    BC_TRY(ensureLayer(d, o.layer, 7));
    auto line = [&](double ax, double ay, double bx, double by, bool thick) {
        return d.add(Entity{LineSeg{{ax, ay}, {bx, by}}, o.layer, thick ? LineType::SolidThick : LineType::SolidThin});
    };
    std::array<double, 6> xs{};
    xs[0] = x0;
    for (int c = 0; c < 5; ++c) xs[c + 1] = xs[c] + o.widths[c];
    // Контур и вертикали — основные линии, строки — тонкие; заголовок отделён основной линией.
    BC_TRY(line(x0, y1, xs[5], y1, true));
    for (int c = 0; c <= 5; ++c) {
        if (c == 5 && std::abs(xs[5] - layout.frame.x1) < 1e-9) continue;  // совпадает с рамкой
        BC_TRY(line(xs[c], y0, xs[c], y1, true));
    }
    BC_TRY(line(x0, y0 + o.rowHeight, xs[5], y0 + o.rowHeight, true));
    for (std::size_t r = 2; r <= items.size(); ++r) {
        const double y = y0 + o.rowHeight * double(r);
        BC_TRY(line(x0, y, xs[5], y, false));
    }
    auto put = [&](int c, double ry0, const std::string& s, HAlign al) -> Status {
        if (s.empty()) return okStatus();
        auto t = fitText({xs[c], ry0, xs[c + 1], ry0 + o.rowHeight}, s, o.textHeight, al, false, o.font,
                         o.headers[c].c_str());
        if (!t) return t.error();
        return d.add(Entity{t.value(), o.layer, LineType::SolidThin});
    };
    for (int c = 0; c < 5; ++c) BC_TRY(put(c, y0, o.headers[c], HAlign::Center));
    for (std::size_t r = 0; r < items.size(); ++r) {
        const double ry = y0 + o.rowHeight * double(r + 1);
        const SpecItem& it = *items[r];
        BC_TRY(put(0, ry, std::to_string(it.position), HAlign::Center));
        BC_TRY(put(1, ry, it.quantity ? std::to_string(*it.quantity) : "", HAlign::Center));
        BC_TRY(put(2, ry, it.name, HAlign::Left));
        BC_TRY(put(3, ry, it.designation, HAlign::Left));
        BC_TRY(put(4, ry, it.material, HAlign::Left));
    }
    return Rect{x0, y0, xs[5], y1};
}

Result<BalloonSyncReport> synchronizeBalloons(std::vector<Balloon>& balloons, const Specification& spec) {
    BalloonSyncReport rep;
    std::map<std::uint32_t, int> count;
    for (auto& b : balloons) {
        const auto pos = spec.positionOf(b.definition);
        if (!pos) {
            return invalid("balloon refers to definition " + std::to_string(b.definition) +
                           " which has no position in the specification");
        }
        b.text = std::to_string(*pos);
        ++count[b.definition];
    }
    for (const auto& s : spec.sections) {
        for (const auto& it : s.items) {
            if (it.position <= 0 || !it.definition) continue;
            const auto c = count.find(*it.definition);
            if (c == count.end()) rep.withoutBalloon.push_back(*it.definition);
            else if (c->second > 1) rep.duplicated.push_back(*it.definition);
        }
    }
    return rep;
}

}  // namespace bendcore::drawing_v2::annotations
