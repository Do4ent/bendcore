// Групповые документы ГОСТ 2.113-75: таблица исполнений группового чертежа и групповая
// спецификация. Правила и допущения — docs/drawing/ANNOTATIONS.md («Групповой чертёж»),
// docs/drawing/SPECIFICATION.md («Групповая спецификация ГОСТ 2.113»).

#include <bendcore/drawing_v2/group_documents.hpp>

#include "spec_text.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace bendcore::drawing_v2::annotations {

namespace {

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

#define BC_TRY(expr)                 \
    do {                             \
        if (auto _s = (expr); !_s) { \
            return _s.error();       \
        }                            \
    } while (0)

constexpr SpecSection kOrder[] = {SpecSection::Documentation, SpecSection::Complexes,
                                  SpecSection::AssemblyUnits, SpecSection::Parts,
                                  SpecSection::StandardProducts, SpecSection::OtherProducts,
                                  SpecSection::Materials, SpecSection::Kits};

bool sortedByDesignation(SpecSection s) {
    return s == SpecSection::Complexes || s == SpecSection::AssemblyUnits || s == SpecSection::Parts ||
           s == SpecSection::Kits;
}

// Порядок записи внутри раздела — как buildSpecification (ГОСТ 2.106).
bool itemLess(SpecSection s, const SpecItem& a, const SpecItem& b) {
    if (sortedByDesignation(s)) {
        if (a.designation.empty() != b.designation.empty()) return b.designation.empty();
        if (a.designation != b.designation) return naturalLess(a.designation, b.designation);
        return naturalLess(a.name, b.name);
    }
    if (a.name != b.name) return naturalLess(a.name, b.name);
    return naturalLess(a.designation, b.designation);
}

// Заголовок графы количества исполнения: суффикс без «-» («-01» → «01»); без суффикса — «–».
std::string defaultColumnHeader(const std::string& designation) {
    if (!designation.empty() && designation.front() == '-') return designation.substr(1);
    return "–";
}

}  // namespace

// ---------------------------------------------------------------- таблица исполнений

std::string groupDimensionLetter(int index) {
    static const char kLetters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";  // без I и O
    constexpr int n = int(sizeof kLetters) - 1;
    if (index < 0) index = 0;
    std::string out;
    // Как столбцы электронной таблицы: A … Z, AA, AB, …
    int i = index;
    do {
        out.insert(out.begin(), kLetters[i % n]);
        i = i / n - 1;
    } while (i >= 0);
    return out;
}

Result<std::vector<double>> groupTableWidths(const GroupTable& t, const GroupTableOptions& o) {
    if (t.headers.empty()) return invalid("group table: no columns");
    std::vector<double> w(t.headers.size(), o.minColumnWidth);
    auto widen = [&](std::size_t c, const std::string& s) {
        const double tw = estimateTextWidth(s, o.textHeight, o.font) + 2.0 * o.padding;
        w[c] = std::max(w[c], std::ceil(tw - 1e-9));
    };
    for (std::size_t c = 0; c < t.headers.size(); ++c) widen(c, t.headers[c]);
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        if (t.rows[r].size() != t.headers.size())
            return invalid("group table: row " + std::to_string(r + 1) + " has " + std::to_string(t.rows[r].size()) +
                           " cells, the table has " + std::to_string(t.headers.size()) + " columns");
        for (std::size_t c = 0; c < t.rows[r].size(); ++c) widen(c, t.rows[r][c]);
    }
    return w;
}

double groupTableHeight(const GroupTable& t, const GroupTableOptions& o) {
    return o.headerHeight + o.rowHeight * double(t.rows.size());
}

Result<GroupTableLayout> addGroupTable(Drawing& d, Point2 br, const Rect& frame, const GroupTable& t,
                                       const GroupTableOptions& o) {
    if (t.rows.empty()) return invalid("group table: no configurations");
    auto widths = groupTableWidths(t, o);
    if (!widths) return widths.error();
    double total = 0.0;
    for (double w : widths.value()) total += w;
    const double x0 = br.x - total, y0 = br.y, y1 = y0 + groupTableHeight(t, o);
    if (x0 < frame.x0 - 1e-9 || y1 > frame.y1 + 1e-9) return Error{ErrorCode::InvalidInput, "group table does not fit the frame"};
    BC_TRY(ensureLayer(d, o.layer, 7));
    auto line = [&](double ax, double ay, double bx, double by, bool thick) {
        return d.add(Entity{LineSeg{{ax, ay}, {bx, by}}, o.layer, thick ? LineType::SolidThick : LineType::SolidThin});
    };
    GroupTableLayout lay;
    lay.box = {x0, y0, br.x, y1};
    lay.columnX.push_back(x0);
    for (double w : widths.value()) lay.columnX.push_back(lay.columnX.back() + w);
    // Контур и вертикали — основные, линия под заголовками — основная, строки — тонкие.
    BC_TRY(line(x0, y1, br.x, y1, true));
    BC_TRY(line(x0, y0, br.x, y0, true));
    for (double x : lay.columnX) BC_TRY(line(x, y0, x, y1, true));
    const double headBottom = y1 - o.headerHeight;
    BC_TRY(line(x0, headBottom, br.x, headBottom, true));
    lay.rowTops.push_back(y1);
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const double top = headBottom - o.rowHeight * double(r);
        lay.rowTops.push_back(top);
        if (r > 0) BC_TRY(line(x0, top, br.x, top, false));
    }
    auto put = [&](std::size_t c, double ya, double yb, const std::string& s) -> Status {
        if (s.empty()) return okStatus();
        auto txt = detail::fitCellText({lay.columnX[c], ya, lay.columnX[c + 1], yb}, s, o.textHeight, HAlign::Center, false,
                                       o.font, t.headers[c].c_str());
        if (!txt) return txt.error();
        return d.add(Entity{txt.value(), o.layer, LineType::SolidThin});
    };
    for (std::size_t c = 0; c < t.headers.size(); ++c) BC_TRY(put(c, headBottom, y1, t.headers[c]));
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        for (std::size_t c = 0; c < t.headers.size(); ++c)
            BC_TRY(put(c, lay.rowTops[r + 1] - o.rowHeight, lay.rowTops[r + 1], t.rows[r][c]));
    return lay;
}

// ---------------------------------------------------------------- групповая спецификация

const char* toString(GroupSpecLayout layout) noexcept {
    return layout == GroupSpecLayout::VariableData ? "variableData" : "columns";
}

bool GroupSpecItem::constant() const {
    return std::all_of(quantities.begin(), quantities.end(), [&](int q) { return q == quantities.front(); });
}

std::optional<int> GroupSpecification::positionOf(std::uint32_t definition) const {
    for (const auto& s : sections)
        for (const auto& it : s.items)
            if (it.item.definition && *it.item.definition == definition && it.item.position > 0) return it.item.position;
    return std::nullopt;
}

Specification GroupSpecification::forConfiguration(std::size_t c) const {
    Specification out;
    out.designation = c < configurations.size() && !configurations[c].empty() ? configurations[c] : designation;
    out.name = name;
    for (const auto& s : sections) {
        SpecSectionData d;
        d.section = s.section;
        for (const auto& it : s.items) {
            SpecItem item = it.item;
            if (s.section != SpecSection::Documentation) {
                const int q = c < it.quantities.size() ? it.quantities[c] : 0;
                if (q <= 0) continue;
                item.quantity = q;
            }
            d.items.push_back(std::move(item));
        }
        if (!d.items.empty()) out.sections.push_back(std::move(d));
    }
    std::stable_sort(out.sections.begin(), out.sections.end(),
                     [](const SpecSectionData& a, const SpecSectionData& b) { return int(a.section) < int(b.section); });
    return out;
}

Result<GroupSpecification> buildGroupSpecification(const assembly_v2::ConfigurationBom& bom, std::string designation,
                                                   std::string name, const GroupSpecOptions& opt) {
    const SpecificationOptions& o = opt.base;
    const std::size_t n = bom.configurations.size();
    if (n == 0) return invalid("group specification: no configurations");
    GroupSpecification g;
    g.layout = opt.layout;
    g.designation = designation.empty() ? bom.configurations.front().fullDesignation : std::move(designation);
    g.name = std::move(name);
    for (std::size_t i = 0; i < n; ++i) {
        g.configurations.push_back(bom.configurations[i].fullDesignation);
        std::string h = i < opt.columnHeaders.size() ? opt.columnHeaders[i] : std::string();
        g.columnHeaders.push_back(h.empty() ? defaultColumnHeader(bom.configurations[i].designation) : h);
    }

    std::map<SpecSection, std::vector<GroupSpecItem>> buckets;
    auto document = [&](SpecItem d) {
        GroupSpecItem gi;
        gi.item = std::move(d);
        gi.item.quantity.reset();
        gi.quantities.assign(n, 0);
        buckets[SpecSection::Documentation].push_back(std::move(gi));
    };
    if (o.addAssemblyDrawing) {
        if (g.designation.empty()) return invalid("group specification: the assembly has no designation (assembly drawing entry)");
        SpecItem d;
        d.format = o.assemblyDrawingFormat;
        d.designation = g.designation + " СБ";
        d.name = "Сборочный чертёж";
        document(d);
    }
    for (const auto& d : o.documents) {
        if (d.position != 0) return invalid("group specification: documents have no position numbers");
        document(d);
    }
    for (const auto& row : bom.rows) {
        if (o.dissolve.count(row.definition))
            return Error{ErrorCode::InvalidInput, "group specification: dissolved subassemblies are not supported ('" + row.name + "')"};
        if (row.quantities.size() != n)
            return invalid("group specification: row '" + row.name + "' has " + std::to_string(row.quantities.size()) +
                           " quantities, expected " + std::to_string(n));
        if (std::all_of(row.quantities.begin(), row.quantities.end(), [](int q) { return q <= 0; })) continue;
        SpecSection sec = row.kind == assembly_v2::DefinitionKind::Assembly ? SpecSection::AssemblyUnits
                          : row.designation.empty()                         ? SpecSection::StandardProducts
                                                                            : SpecSection::Parts;
        if (auto it = o.sectionOverride.find(row.definition); it != o.sectionOverride.end()) sec = it->second;
        if (sec == SpecSection::Documentation) return invalid("group specification: a component cannot be a document");
        if (row.name.empty())
            return invalid("group specification: component without a name (definition " + std::to_string(row.definition) + ")");
        GroupSpecItem gi;
        gi.item.designation = row.designation;
        gi.item.name = row.name;
        gi.item.material = row.material;
        gi.item.definition = row.definition;
        if (auto it = o.formats.find(row.definition); it != o.formats.end()) gi.item.format = it->second;
        if (auto it = o.notes.find(row.definition); it != o.notes.end()) gi.item.note = it->second;
        gi.quantities = row.quantities;
        buckets[sec].push_back(std::move(gi));
    }

    for (SpecSection s : kOrder) {
        auto it = buckets.find(s);
        if (it == buckets.end() || it->second.empty()) continue;
        auto& items = it->second;
        if (s != SpecSection::Documentation)
            std::stable_sort(items.begin(), items.end(),
                             [&](const GroupSpecItem& a, const GroupSpecItem& b) { return itemLess(s, a.item, b.item); });
        g.sections.push_back({s, std::move(items)});
    }
    // Позиции: Columns — в порядке записи; VariableData — постоянная часть, затем переменная.
    int pos = 1;
    const int reserve = std::max(0, o.reservePositions);
    for (int pass = 0; pass < (g.layout == GroupSpecLayout::Columns ? 1 : 2); ++pass) {
        for (auto& sec : g.sections) {
            if (sec.section == SpecSection::Documentation) continue;
            bool any = false;
            for (auto& it : sec.items) {
                if (g.layout == GroupSpecLayout::VariableData && it.constant() != (pass == 0)) continue;
                it.item.position = pos++;
                any = true;
            }
            if (any) pos += reserve;
        }
    }
    return g;
}

Result<std::vector<double>> groupSpecColumnWidths(std::size_t n) {
    if (n == 0) return invalid("group specification: no configurations");
    if (n > 7)
        return Error{ErrorCode::InvalidInput,
                     "group specification: " + std::to_string(n) + " configurations do not fit the quantity columns (at most 7)"};
    const double q = n <= 4 ? 10.0 : 8.0;
    const double note = n <= 4 ? 22.0 : 12.0;
    const double name = 185.0 - (6 + 6 + 8 + 70) - note - q * double(n);
    std::vector<double> w = {6, 6, 8, 70, name};
    for (std::size_t i = 0; i < n; ++i) w.push_back(q);
    w.push_back(note);
    return w;
}

Result<SpecDocument> renderGroupSpecification(const GroupSpecification& spec, const SpecSheetOptions& o) {
    if (spec.sections.empty()) return invalid("group specification is empty");
    const bool columns = spec.layout == GroupSpecLayout::Columns;
    const std::size_t n = spec.configurations.size();
    if (spec.columnHeaders.size() != n) return invalid("group specification: column headers do not match the configurations");
    // Графы: Формат, Зона, Поз., Обозначение, Наименование, [количество по исполнениям | Кол.], Примечание.
    std::vector<double> W;
    if (columns) {
        auto w = groupSpecColumnWidths(n);
        if (!w) return w.error();
        W = std::move(w).value();
    } else {
        const auto& f = specColumnWidths();
        W.assign(f.begin(), f.end());
    }
    const std::size_t qn = columns ? n : 1;  // граф количества
    const std::size_t cols = 5 + qn + 1;
    std::vector<std::string> names = {"Формат", "Зона", "Поз.", "Обозначение", "Наименование"};
    for (std::size_t i = 0; i < qn; ++i) names.push_back(columns ? spec.columnHeaders[i] : "Кол.");
    names.push_back("Примечание");
    const double h = o.textHeight;
    AnnotationStyle st;
    st.font = o.font;
    const double nameWidth = W[4] - 2.0;

    struct Block {
        std::vector<SpecRow> rows;
        bool header = false;
    };
    std::vector<Block> blocks;
    auto headerBlock = [&](const std::string& title) {
        Block hb;
        hb.header = true;
        hb.rows.push_back({});
        SpecRow hr;
        hr.kind = SpecRowKind::Header;
        hr.cells[4] = title;
        hb.rows.push_back(hr);
        hb.rows.push_back({});
        blocks.push_back(hb);
    };
    auto itemBlock = [&](const GroupSpecItem& gi, std::optional<std::size_t> config, bool documentation) -> Status {
        const SpecItem& item = gi.item;
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
                r.cells[6] = item.note;
                if (!documentation) {
                    if (columns) {
                        for (int q : gi.quantities) r.quantities.push_back(q > 0 ? std::to_string(q) : "–");
                    } else {
                        const int q = config ? gi.quantities[*config] : gi.quantities.front();
                        r.cells[5] = std::to_string(q);
                    }
                }
            }
            ib.rows.push_back(r);
        }
        blocks.push_back(ib);
        return okStatus();
    };
    for (const auto& sec : spec.sections) {
        const bool doc = sec.section == SpecSection::Documentation;
        std::vector<const GroupSpecItem*> items;
        for (const auto& it : sec.items)
            if (columns || doc || it.constant()) items.push_back(&it);
        if (items.empty()) continue;
        headerBlock(sectionTitle(sec.section));
        for (const GroupSpecItem* it : items) BC_TRY(itemBlock(*it, std::nullopt, doc));
    }
    if (!columns) {
        bool anyVariable = false;
        for (const auto& sec : spec.sections)
            if (sec.section != SpecSection::Documentation)
                for (const auto& it : sec.items) anyVariable = anyVariable || !it.constant();
        if (anyVariable) {
            headerBlock("Переменные данные для исполнений");
            for (std::size_t c = 0; c < n; ++c) {
                headerBlock(spec.configurations[c]);
                for (const auto& sec : spec.sections) {
                    if (sec.section == SpecSection::Documentation) continue;
                    std::vector<const GroupSpecItem*> items;
                    for (const auto& it : sec.items)
                        if (!it.constant() && it.quantities[c] > 0) items.push_back(&it);
                    if (items.empty()) continue;
                    headerBlock(sectionTitle(sec.section));
                    for (const GroupSpecItem* it : items) BC_TRY(itemBlock(*it, c, false));
                }
            }
        }
    }

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
        if (blocks[b].header && b + 1 < blocks.size()) need += 1;
        if (need > capacity(0) && need > capacity(1)) return invalid("group specification: entry taller than a sheet");
        if (pages.back().size() + need > capacity(pages.size() - 1)) pages.emplace_back();
        auto& page = pages.back();
        std::size_t start = (blocks[b].header && page.empty()) ? 1 : 0;
        for (std::size_t i = start; i < blocks[b].rows.size(); ++i) page.push_back(blocks[b].rows[i]);
    }

    for (std::size_t p = 0; p < pages.size(); ++p) {
        auto dr = Drawing::create(gs.value(), 0.5);
        if (!dr) return dr.error();
        Drawing d = std::move(dr).value();
        d.setTextFont(o.textFont);
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
        std::vector<double> xs{fr.x0};
        for (double w : W) xs.push_back(xs.back() + w);
        const double qTop = headBottom + 8.0;  // Columns: подзаголовки исполнений — нижние 8 мм строки заголовков
        for (std::size_t c = 1; c < cols; ++c) {
            // Вертикали между графами количества не пересекают общий заголовок «Кол. на исполн.».
            const bool inner = columns && c > 5 && c < 5 + qn;
            BC_TRY(line(xs[c], inner ? qTop : top, xs[c], bottom, true));
        }
        BC_TRY(line(fr.x0, headBottom, fr.x1, headBottom, true));
        if (columns) BC_TRY(line(xs[5], qTop, xs[5 + qn], qTop, true));
        const int cap = int(capacity(p));
        for (int r = 1; r < cap; ++r) {
            const double y = headBottom - r * o.rowHeight;
            BC_TRY(line(fr.x0, y, fr.x1, y, false));
        }
        for (std::size_t c = 0; c < cols; ++c) {
            if (columns && c >= 5 && c < 5 + qn) continue;
            const Rect cell{xs[c], headBottom, xs[c + 1], top};
            auto t = detail::fitCellText(cell, names[c], 5.0, HAlign::Center, c < 3, o.font, names[c].c_str());
            if (!t) return t.error();
            BC_TRY(d.add(Entity{t.value(), layer, LineType::SolidThin}));
        }
        if (columns) {
            // Общий заголовок граф количества: «Кол. на исполн.», если не помещается — «Кол.».
            const Rect cell{xs[5], qTop, xs[5 + qn], top};
            auto t = detail::fitCellText(cell, "Кол. на исполн.", 3.5, HAlign::Center, false, o.font, "Кол.");
            if (!t) t = detail::fitCellText(cell, "Кол.", 3.5, HAlign::Center, false, o.font, "Кол.");
            if (!t) return t.error();
            BC_TRY(d.add(Entity{t.value(), layer, LineType::SolidThin}));
            for (std::size_t i = 0; i < qn; ++i) {
                const Rect sub{xs[5 + i], headBottom, xs[6 + i], qTop};
                auto s = detail::fitCellText(sub, names[5 + i], 3.5, HAlign::Center, false, o.font, names[5 + i].c_str());
                if (!s) return s.error();
                BC_TRY(d.add(Entity{s.value(), layer, LineType::SolidThin}));
            }
        }
        for (std::size_t r = 0; r < pages[p].size(); ++r) {
            const SpecRow& row = pages[p][r];
            const double y1 = headBottom - double(r) * o.rowHeight, y0 = y1 - o.rowHeight;
            auto put = [&](std::size_t c, const std::string& value, HAlign al, bool underline) -> Status {
                if (value.empty()) return okStatus();
                const Rect cell{xs[c], y0, xs[c + 1], y1};
                auto t = detail::fitCellText(cell, value, h, al, false, o.font, names[c].c_str());
                if (!t) return t.error();
                if (auto s = d.add(Entity{t.value(), layer, LineType::SolidThin}); !s) return s;
                if (underline) {
                    const double w = estimateTextWidth(value, t.value().height, o.font, t.value().widthFactor);
                    const double cx = (cell.x0 + cell.x1) / 2.0, yl = t.value().position.y - t.value().height / 2.0 - 0.8;
                    return line(cx - w / 2.0, yl, cx + w / 2.0, yl, false);
                }
                return okStatus();
            };
            const bool header = row.kind == SpecRowKind::Header;
            for (std::size_t c = 0; c < 5; ++c) {
                const HAlign al = header || c <= 2 ? HAlign::Center : HAlign::Left;
                BC_TRY(put(c, row.cells[c], al, header));
            }
            if (columns) {
                for (std::size_t i = 0; i < row.quantities.size() && i < qn; ++i)
                    BC_TRY(put(5 + i, row.quantities[i], HAlign::Center, false));
            } else {
                BC_TRY(put(5, row.cells[5], HAlign::Center, false));
            }
            BC_TRY(put(cols - 1, row.cells[6], HAlign::Left, false));
        }
        doc.sheets.push_back(std::move(d));
    }
    doc.rows = std::move(pages);
    return doc;
}

}  // namespace bendcore::drawing_v2::annotations
