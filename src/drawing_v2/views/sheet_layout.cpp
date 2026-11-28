// Масштабы ГОСТ 2.302-68 и компоновка видов с проекционной связью (ГОСТ 2.305-2008, разд. 5).

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <cmath>
#include <cstdio>

namespace bendcore::drawing_v2::views {

namespace {

std::string ratioNumber(int a, int b) {
    // a/b — целое или с одним знаком после запятой (2,5).
    if (a % b == 0) return std::to_string(a / b);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", double(a) / b);
    std::string s = buf;
    for (char& c : s)
        if (c == '.') c = ',';
    return s;
}

}  // namespace

std::string ScaleRatio::label() const {
    if (num >= den) return ratioNumber(num, den) + ":1";
    return "1:" + ratioNumber(den, num);
}

const std::vector<ScaleRatio>& gostScales() {
    // ГОСТ 2.302-68, п. 2: масштабы увеличения, натуральная величина, масштабы уменьшения.
    static const std::vector<ScaleRatio> scales = {
        {100, 1}, {50, 1}, {40, 1}, {20, 1}, {10, 1}, {5, 1}, {4, 1}, {5, 2}, {2, 1},
        {1, 1},
        {1, 2}, {2, 5}, {1, 4}, {1, 5}, {1, 10}, {1, 15}, {1, 20}, {1, 25}, {1, 40},
        {1, 50}, {1, 75}, {1, 100}, {1, 200}, {1, 400}, {1, 500}, {1, 800}, {1, 1000}};
    return scales;
}

Result<ScaleRatio> chooseGostScale(double w, double h, double availW, double availH) {
    if (!(w >= 0) || !(h >= 0) || !(availW > 0) || !(availH > 0) || (w == 0 && h == 0))
        return Result<ScaleRatio>::failure(ErrorCode::InvalidInput, "scale: invalid extents");
    for (const ScaleRatio& s : gostScales())
        if (w * s.value() <= availW && h * s.value() <= availH) return s;
    return Result<ScaleRatio>::failure(ErrorCode::InvalidInput,
                                       "scale: the view does not fit even at 1:1000");
}

Box2 SheetView::sheetBounds() const {
    Box2 b;
    if (view.bounds.empty) return b;
    b.add(toSheet({view.bounds.minX, view.bounds.minY}));
    b.add(toSheet({view.bounds.maxX, view.bounds.maxY}));
    return b;
}

Point2 alignedAnchor(const SheetView& parent, const ProjectedView& child, double scale,
                     SheetSide side, double gap) {
    const Box2 pb = parent.sheetBounds();
    const Box2& cb = child.bounds;
    switch (side) {
        case SheetSide::Below:
            return {parent.anchor.x, pb.minY - gap - scale * cb.maxY};
        case SheetSide::Above:
            return {parent.anchor.x, pb.maxY + gap - scale * cb.minY};
        case SheetSide::RightOf:
            return {pb.maxX + gap - scale * cb.minX, parent.anchor.y};
        case SheetSide::LeftOf:
            return {pb.minX - gap - scale * cb.maxX, parent.anchor.y};
    }
    return parent.anchor;
}

Result<Layout> layoutViews(const std::vector<LayoutEntry>& entries, const Box2& area, double gap) {
    using R = Result<Layout>;
    if (entries.empty() || area.empty || !(gap >= 0))
        return R::failure(ErrorCode::InvalidInput, "layout: no views or empty area");
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].view || entries[i].view->bounds.empty)
            return R::failure(ErrorCode::InvalidInput, "layout: view without geometry");
        if (i == 0 ? entries[i].parent != -1
                   : (entries[i].parent < 0 || entries[i].parent >= int(i)))
            return R::failure(ErrorCode::InvalidInput,
                              "layout: the first entry is the main view, parents precede children");
    }
    for (const ScaleRatio& s : gostScales()) {
        std::vector<SheetView> placed;
        placed.reserve(entries.size());
        Box2 all;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            SheetView sv{*entries[i].view, s.value(), {0, 0}};
            if (i > 0)
                sv.anchor = alignedAnchor(placed[entries[i].parent], *entries[i].view, s.value(),
                                          entries[i].side, gap);
            const Box2 b = sv.sheetBounds();
            all.add({b.minX, b.minY});
            all.add({b.maxX, b.maxY});
            placed.push_back(std::move(sv));
        }
        if (all.width() > area.width() || all.height() > area.height()) continue;
        const double dx = 0.5 * (area.minX + area.maxX) - 0.5 * (all.minX + all.maxX);
        const double dy = 0.5 * (area.minY + area.maxY) - 0.5 * (all.minY + all.maxY);
        Layout out;
        out.scale = s;
        for (const SheetView& sv : placed) out.anchors.push_back({sv.anchor.x + dx, sv.anchor.y + dy});
        out.bounds.add({all.minX + dx, all.minY + dy});
        out.bounds.add({all.maxX + dx, all.maxY + dy});
        return out;
    }
    return R::failure(ErrorCode::InvalidInput, "layout: the views do not fit the area at 1:1000");
}

std::string designationLetter(int index) {
    // ГОСТ 2.316: не применяются Ё, З, Й, О, Х, Ъ, Ы, Ь.
    static const char* letters[] = {"А", "Б", "В", "Г", "Д", "Е", "Ж", "И", "К", "Л", "М",
                                    "Н", "П", "Р", "С", "Т", "У", "Ф", "Ц", "Ч", "Ш", "Щ",
                                    "Э", "Ю", "Я"};
    constexpr int n = int(sizeof letters / sizeof letters[0]);
    if (index < 0) index = 0;
    // После исчерпания алфавита — буква с цифровым индексом: «А1», «Б1», …
    const std::string base = letters[index % n];
    return index < n ? base : base + std::to_string(index / n);
}

}  // namespace bendcore::drawing_v2::views
