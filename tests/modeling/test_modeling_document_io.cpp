// JSON save/load: round trip, BRep cache, migration from format v1, errors.

#include "modeling_document_test_util.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

// Box + pocket on top + hole in the front face + fillet on the top-front edge.
PartDocument makeModel() {
    PartDocument doc(partRegistry());
    setVar(doc, "L", 60);
    setVar(doc, "W", 40);
    setVar(doc, "H", 25);
    add(doc, {"Box", "Body", "Base block", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
    doc.rebuild();
    NamedShape s = doc.finalShape();
    addSketchCut(doc, "Pocket", refTo(s, planarFace(s.shape(), gp::DZ(), 25)), "L/2", "W/2 + 5", "H",
                 centredRectangle("20", "10"), "8");
    addSketchCut(doc, "Hole", refTo(s, planarFace(s.shape(), gp::DY().Reversed(), 0)), "L/2", "0", "H/2 - 3",
                 centredCircle("4"), "12");
    doc.rebuild();
    s = doc.finalShape();
    add(doc, {"Fillet", "Round", "", {{"radius", "2"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, 25))}}}, {}, false});
    doc.rebuild();
    return doc;
}

double expectedVolume(double l, double w, double h) {
    return l * w * h - 20 * 10 * 8 - kPi * 16 * 12 - filletLoss(2, l);
}

void expectSameDocument(const PartDocument& a, const PartDocument& b) {
    ASSERT_EQ(a.featureIds(), b.featureIds());
    for (const auto& id : a.featureIds()) {
        FeatureInfo fa = info(a, id), fb = info(b, id);
        EXPECT_EQ(fa.state, fb.state) << id;
        EXPECT_EQ(fa.parameters, fb.parameters) << id;
        EXPECT_EQ(fa.options, fb.options) << id;
        ASSERT_EQ(fa.references.size(), fb.references.size());
        for (const auto& [slot, refs] : fa.references) {
            ASSERT_EQ(refs.size(), fb.references.at(slot).size());
            for (std::size_t k = 0; k < refs.size(); ++k) {
                EXPECT_EQ(refs[k].name, fb.references.at(slot)[k].name);
                EXPECT_EQ(refs[k].signature.geometry, fb.references.at(slot)[k].signature.geometry);
            }
        }
        ASSERT_EQ(static_cast<bool>(fa.output), static_cast<bool>(fb.output)) << id;
        if (fa.output) {
            EXPECT_NEAR(volume(fa.output->shape()), volume(fb.output->shape()), 1e-6) << id;
            for (ElementKind k : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
                ASSERT_EQ(fa.output->count(k), fb.output->count(k)) << id;
                for (std::size_t i = 0; i < fa.output->count(k); ++i) {
                    EXPECT_EQ(fa.output->record(k, i).name, fb.output->record(k, i).name) << id;
                }
            }
        }
    }
}

}  // namespace

TEST(ModelingDocumentIo, RoundTripRecomputesIdenticalResults) {
    PartDocument doc = makeModel();
    for (const auto& id : doc.featureIds()) {
        EXPECT_EQ(info(doc, id).state, FeatureState::Ok) << id << ": " << messages(info(doc, id));
    }
    EXPECT_NEAR(volume(doc.finalShape().shape()), expectedVolume(60, 40, 25), 1e-6);
    EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), 4, 1, 30, 9.5)) << "hole axis along Y at x = 30, z = 9.5";

    auto text = doc.toJson();
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto j = nlohmann::json::parse(text.value());
    EXPECT_EQ(j.at("format"), "bendcore-part");
    EXPECT_EQ(j.at("version"), PartDocument::kFormatVersion);
    EXPECT_FALSE(j.contains("cache"));
    EXPECT_EQ(j.at("features").at(5).at("references").at("edges").at(0).at("name"), "E(Body:Front|Body:Top)");

    RebuildReport rep;
    auto loaded = PartDocument::fromJson(text.value(), partRegistry(), &rep);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(rep.recomputed.size(), 6u) << "results are recomputed at load";
    expectSameDocument(doc, loaded.value());
    EXPECT_EQ(info(loaded.value(), "Body").label, "Base block");

    // The loaded document stays parametric.
    setVar(loaded.value(), "L", 80);
    setVar(loaded.value(), "H", 30);
    loaded.value().rebuild();
    EXPECT_EQ(info(loaded.value(), "Round").state, FeatureState::Ok);
    EXPECT_NEAR(volume(loaded.value().finalShape().shape()), expectedVolume(80, 40, 30), 1e-6);
}

TEST(ModelingDocumentIo, BrepCacheIsUsedWhenInputsMatch) {
    PartDocument doc = makeModel();
    auto text = doc.toJson({.includeCache = true});
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto j = nlohmann::json::parse(text.value());
    ASSERT_TRUE(j.contains("cache"));
    EXPECT_EQ(j.at("cache").size(), 4u);

    RebuildReport rep;
    auto loaded = PartDocument::fromJson(text.value(), partRegistry(), &rep);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(rep.fromCache.size(), 4u);
    EXPECT_EQ(rep.recomputed, (std::vector<std::string>{"PocketSketch", "HoleSketch"})) << "sketches are not cached";
    expectSameDocument(doc, loaded.value());

    // A changed variable invalidates every cached result downstream of it.
    j["variables"][2]["expression"] = "27";  // H
    RebuildReport rep2;
    auto changed = PartDocument::fromJson(j.dump(), partRegistry(), &rep2);
    ASSERT_TRUE(changed.ok()) << changed.error().message;
    EXPECT_TRUE(rep2.fromCache.empty());
    EXPECT_EQ(rep2.recomputed.size(), 6u);
    EXPECT_NEAR(volume(changed.value().finalShape().shape()), expectedVolume(60, 40, 27), 1e-6);

    // A damaged cache entry is ignored, the feature is recomputed.
    j = nlohmann::json::parse(text.value());
    j["cache"]["Round"]["brep"] = "AAAA";
    RebuildReport rep3;
    auto damaged = PartDocument::fromJson(j.dump(), partRegistry(), &rep3);
    ASSERT_TRUE(damaged.ok()) << damaged.error().message;
    EXPECT_EQ(rep3.fromCache.size(), 3u);
    EXPECT_EQ(rep3.recomputed, (std::vector<std::string>{"PocketSketch", "HoleSketch", "Round"}));
    EXPECT_NEAR(volume(damaged.value().finalShape().shape()), expectedVolume(60, 40, 25), 1e-6);
}

TEST(ModelingDocumentIo, MigratesFormatVersion1) {
    const char* v1 = R"json({
      "format": "bendcore-part", "version": 1,
      "parameters": {"H": 20},
      "operations": [
        {"id": "B", "type": "Box", "params": {"length": 40, "width": 30, "height": "H"}},
        {"id": "F", "type": "Fillet", "params": {"radius": 3}, "refs": {"edges": ["E(B:Front|B:Top)"]}},
        {"id": "C", "type": "Chamfer", "params": {"distance": 1.5}, "refs": {"edges": ["E(B:Back|B:Right)"]},
         "suppressed": true}
      ]})json";
    RebuildReport rep;
    auto doc = PartDocument::fromJson(v1, partRegistry(), &rep);
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    EXPECT_EQ(doc.value().variables(), (std::vector<std::pair<std::string, std::string>>{{"H", "20"}}));
    EXPECT_EQ(info(doc.value(), "F").state, FeatureState::Ok) << messages(info(doc.value(), "F"));
    EXPECT_EQ(info(doc.value(), "C").state, FeatureState::Suppressed);
    EXPECT_NEAR(volume(doc.value().finalShape().shape()), 40 * 30 * 20 - filletLoss(3, 40), 1e-6);
    // After the first resolution the reference carries a signature, saved as the current version.
    EXPECT_TRUE(info(doc.value(), "F").references.at("edges").at(0).signature.valid);
    auto saved = nlohmann::json::parse(doc.value().toJson().value());
    EXPECT_EQ(saved.at("version"), PartDocument::kFormatVersion);
    EXPECT_EQ(saved.at("features").at(1).at("parameters").at("radius"), "3");

    ASSERT_TRUE(doc.value().setSuppressed("C", false).ok());
    doc.value().rebuild();
    EXPECT_NEAR(volume(doc.value().finalShape().shape()),
                40 * 30 * 20 - filletLoss(3, 40) - chamferLoss(1.5, 20), 1e-6);
}

TEST(ModelingDocumentIo, RejectsBadInput) {
    auto reg = partRegistry();
    EXPECT_EQ(PartDocument::fromJson("{not json", reg).error().code, ErrorCode::ParseError);
    EXPECT_EQ(PartDocument::fromJson(R"json({"format": "other", "version": 2})json", reg).error().code, ErrorCode::ParseError);
    auto newer = PartDocument::fromJson(R"json({"format": "bendcore-part", "version": 99, "variables": [], "features": []})json", reg);
    EXPECT_EQ(newer.error().code, ErrorCode::Unsupported);
    EXPECT_NE(newer.error().message.find("newer"), std::string::npos);
    auto unknown = PartDocument::fromJson(
        R"json({"format": "bendcore-part", "version": 2, "variables": [], "features": [{"id": "S", "type": "Sweep"}]})json", reg);
    EXPECT_EQ(unknown.error().code, ErrorCode::Unsupported);
    auto missing = PartDocument::fromJson(R"json({"format": "bendcore-part", "version": 2})json", reg);
    EXPECT_EQ(missing.error().code, ErrorCode::ParseError);
    auto badKind = PartDocument::fromJson(
        R"json({"format": "bendcore-part", "version": 2, "variables": [], "features": [
            {"id": "F", "type": "Fillet", "references": {"edges": [{"name": "x", "kind": "solid"}]}}]})json", reg);
    EXPECT_EQ(badKind.error().code, ErrorCode::ParseError);
}
