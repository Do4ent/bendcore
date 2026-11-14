// Позиции подсборок на сборочном чертеже (ГОСТ 2.109, ГОСТ 2.106): узел «вал + подсборка
// (втулка + штифт)». Подсборка — сборочная единица: одна позиция по спецификации, выноска — на
// линии одной из её деталей, детали подсборки своих позиций не получают. «Разборная» подсборка
// (SpecificationSettings::dissolved): в спецификацию и позиции входят её детали.

#include "test_drawing_document_support.hpp"

#include <gtest/gtest.h>

#include <set>

using namespace ddtest;
using namespace bendcore::assembly_v2;

namespace {

struct Nested {
    std::shared_ptr<AssemblyDocument> doc = std::make_shared<AssemblyDocument>("Узел");
    DefinitionId shaft = 0, sub = 0, bushing = 0, pin = 0;
};

Nested buildNested() {
    Nested n;
    auto rod = makeRod(15, 120);
    auto bush = makeBushing();
    auto pin = makeRod(3, 20);
    EXPECT_TRUE(n.doc->setProperties(n.doc->root(), {"ВТ.03.000", "Узел", "", std::nullopt}).ok());
    n.shaft = n.doc->addPart("Вал", partDataFromDocument(*rod).value(), {"ВТ.03.001", "Вал", "Сталь 45", 7850}).value();
    n.bushing = n.doc->addPart("Втулка", partDataFromDocument(*bush).value(), {"ВТ.03.101", "Втулка", "Сталь 20", 7850}).value();
    n.pin = n.doc->addPart("Штифт", partDataFromDocument(*pin).value(), {"ВТ.03.102", "Штифт", "Сталь 45", 7850}).value();
    n.sub = n.doc->addSubassembly("Втулка в сборе", {"ВТ.03.100", "Втулка в сборе", "", std::nullopt}).value();
    EXPECT_TRUE(n.doc->addOccurrence(n.bushing, Transform::identity(), "Втулка", n.sub).ok());
    EXPECT_TRUE(n.doc->addOccurrence(n.pin, Transform::fromTranslation(bendcore::assembly_v2::Vec3(70, 0, 0)), "Штифт", n.sub).ok());
    auto so = n.doc->addOccurrence(n.sub, Transform::identity(), "Втулка в сборе");
    EXPECT_TRUE(so.ok());
    EXPECT_TRUE(n.doc->setFixed(so.value(), true).ok());
    EXPECT_TRUE(n.doc->addOccurrence(n.shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -40)), "Вал").ok());
    return n;
}

DrawingDocument nestedDrawing() {
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    d.model().path = "nested.json";
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f;
    f.id = "F";
    f.hidden = false;
    f.autoBalloons = true;
    s.views.push_back(f);
    d.sheets().push_back(std::move(s));
    return d;
}

SpecificationDocument specFor(const ModelSnapshot& snap, std::set<std::string> dissolved) {
    SpecificationDocument spec;
    spec.model().kind = ModelKind::Assembly;
    spec.model().path = "nested.json";
    spec.settings().dissolved = std::move(dissolved);
    EXPECT_TRUE(spec.update(snap).ok());
    return spec;
}

// Точка выноски лежит на видимой линии детали owner.
bool onPart(const ComputedView& v, const ModelSnapshot& m, const annotations::Balloon& b, DefinitionId owner) {
    for (const views::ViewEdge& e : v.sheetView.view.edges) {
        if (e.visibility != views::Visibility::Visible || !e.edgeIndex || m.edgeOwner[*e.edgeIndex] != owner) continue;
        if (const auto* l = std::get_if<LineSeg>(&e.curve)) {
            const Point2 a = v.sheetView.toSheet(l->a), c = v.sheetView.toSheet(l->b);
            const double cross = (c.x - a.x) * (b.target.y - a.y) - (c.y - a.y) * (b.target.x - a.x);
            const double len = std::hypot(c.x - a.x, c.y - a.y);
            const double t = ((b.target.x - a.x) * (c.x - a.x) + (b.target.y - a.y) * (c.y - a.y)) / (len * len);
            if (len > 0 && std::abs(cross) / len < 1e-6 && t > -1e-9 && t < 1 + 1e-9) return true;
        }
        if (const auto* ar = std::get_if<Arc>(&e.curve)) {
            const Point2 c = v.sheetView.toSheet(ar->center);
            if (std::abs(std::hypot(b.target.x - c.x, b.target.y - c.y) - ar->radius * v.sheetView.scale) < 1e-6) return true;
        }
    }
    return false;
}

}  // namespace

TEST(DrawingDocumentSubassembly, SubassemblyIsOneAssemblyUnitPosition) {
    Nested n = buildNested();
    auto snap = snapshotAssembly(n.doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    // Цепочки вхождений: деталь подсборки → [подсборка, деталь].
    ASSERT_EQ(snap.value().childChain.size(), 3u);
    std::set<std::vector<DefinitionId>> chains(snap.value().childChain.begin(), snap.value().childChain.end());
    EXPECT_TRUE(chains.count({n.sub, n.bushing}));
    EXPECT_TRUE(chains.count({n.sub, n.pin}));
    EXPECT_TRUE(chains.count({n.shaft}));

    SpecificationDocument spec = specFor(snap.value(), {});
    const annotations::Specification& sp = *spec.specification();
    // ГОСТ 2.106: «Документация», «Сборочные единицы» (подсборка — поз. 1), «Детали» (вал — поз. 2);
    // втулки и штифта в спецификации узла нет — они в спецификации подсборки.
    ASSERT_EQ(sp.sections.size(), 3u);
    EXPECT_EQ(sp.sections[1].section, annotations::SpecSection::AssemblyUnits);
    EXPECT_EQ(sp.positionOf(n.sub), 1);
    EXPECT_EQ(sp.positionOf(n.shaft), 2);
    EXPECT_FALSE(sp.positionOf(n.bushing));
    EXPECT_FALSE(sp.positionOf(n.pin));
    // Владелец позиции рёбер деталей подсборки — подсборка.
    for (std::uint32_t e = 0; e < snap.value().edgeOwner.size(); ++e) {
        const DefinitionId part = snap.value().edgeOwner[e];
        auto owner = positionOwner(snap.value(), e, sp);
        ASSERT_TRUE(owner) << e;
        EXPECT_EQ(*owner, part == n.shaft ? n.shaft : n.sub) << e;
    }

    DrawingDocument d = nestedDrawing();
    views::ViewCache cache(16, 2);
    auto rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    ASSERT_EQ(rep.value().balloons.size(), 1u);
    EXPECT_TRUE(rep.value().balloons[0].withoutBalloon.empty());
    const ComputedSheet& cs = d.computedSheets()[0];
    ASSERT_EQ(cs.balloons.size(), 2u);
    std::map<DefinitionId, std::string> numbers;
    for (const auto& b : cs.balloons) {
        numbers[b.definition] = b.text;
        // Выноска подсборки — на линии втулки или штифта.
        if (b.definition == n.sub)
            EXPECT_TRUE(onPart(cs.views[0], snap.value(), b, n.bushing) || onPart(cs.views[0], snap.value(), b, n.pin));
        else
            EXPECT_TRUE(onPart(cs.views[0], snap.value(), b, n.shaft));
    }
    EXPECT_EQ(numbers[n.sub], "1");
    EXPECT_EQ(numbers[n.shaft], "2");

    // Ручная позиция на ребре штифта — номер подсборки; автоматическая для подсборки не ставится.
    std::optional<std::uint32_t> pinEdge;
    for (const views::ViewEdge& e : cs.views[0].sheetView.view.edges)
        if (e.visibility == views::Visibility::Visible && e.edgeIndex && snap.value().edgeOwner[*e.edgeIndex] == n.pin &&
            std::holds_alternative<LineSeg>(e.curve))
            pinEdge = *e.edgeIndex;
    ASSERT_TRUE(pinEdge);
    BalloonItem bi;
    bi.id = "P1";
    bi.view = "F";
    bi.edge = elementReference(snap.value(), bendcore::modeling::ElementKind::Edge, *pinEdge).value();
    d.sheets()[0].balloons.push_back(bi);
    rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs2 = d.computedSheets()[0];
    ASSERT_EQ(cs2.manualBalloons.size(), 1u);
    ASSERT_TRUE(cs2.manualBalloons[0]);
    EXPECT_EQ(cs2.manualBalloons[0]->text, "1");
    EXPECT_EQ(cs2.manualBalloons[0]->definition, n.sub);
    ASSERT_EQ(cs2.balloons.size(), 1u);
    EXPECT_EQ(cs2.balloons[0].definition, n.shaft);
}

TEST(DrawingDocumentSubassembly, DissolvedSubassemblyNumbersItsParts) {
    Nested n = buildNested();
    auto snap = snapshotAssembly(n.doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    SpecificationDocument spec = specFor(snap.value(), {"ВТ.03.100"});
    const annotations::Specification& sp = *spec.specification();
    // Подсборки в спецификации нет; детали — по возрастанию обозначений: вал, втулка, штифт.
    EXPECT_FALSE(sp.positionOf(n.sub));
    EXPECT_EQ(sp.positionOf(n.shaft), 1);
    EXPECT_EQ(sp.positionOf(n.bushing), 2);
    EXPECT_EQ(sp.positionOf(n.pin), 3);
    for (const auto& sec : sp.sections) EXPECT_NE(sec.section, annotations::SpecSection::AssemblyUnits);

    DrawingDocument d = nestedDrawing();
    views::ViewCache cache(16, 2);
    auto rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs = d.computedSheets()[0];
    ASSERT_EQ(cs.balloons.size(), 3u);
    for (const auto& b : cs.balloons) {
        EXPECT_EQ(std::stoi(b.text), *sp.positionOf(b.definition));
        EXPECT_TRUE(onPart(cs.views[0], snap.value(), b, b.definition)) << b.text;
    }
    // Настройка сохраняется в файле спецификации.
    auto json = spec.toJson();
    ASSERT_TRUE(json.ok());
    auto back = SpecificationDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value().settings().dissolved, (std::set<std::string>{"ВТ.03.100"}));
    EXPECT_EQ(back.value().settings().textFont, TextFont::Gost2304);
}
