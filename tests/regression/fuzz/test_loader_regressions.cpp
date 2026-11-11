// Регрессия дефектов, найденных fuzz-проверкой загрузчиков (docs/qa/QA.md, раздел 7).
//
// 1. AssemblyDocument::fromJson выпускал наружу nlohmann::json::out_of_range (406, переполнение
//    числа при разборе: «1e999») — ловился только parse_error. Найдено: assembly_json, мутации
//    #231 и #282 (зерно 20261111). Теперь — ParseError. Те же входы проверяются у загрузчиков
//    детали и чертежа, где дефекта не было.
// 2. UBSan (build-fuzz): PartDocument::fromJson приводил вещественные значения целых полей эскиза
//    (start, projection, a, …: −1e308, −1,8e19) к int через json::value/get<int> — неопределённое
//    поведение (part_json #144, #238). Теперь целое поле — только целое число в диапазоне.
// 3. UBSan: DrawingDocument::fromJson — переполнение int в сравнении масштаба (scale [2^31, 1],
//    drawing_json #219) и приведение revision = 1e308 к uint32 (#245). Теперь — ParseError.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include "fuzz_targets.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>

using bendcore::kernel::ErrorCode;

namespace {

// Число вне диапазона double в разных местах документа: в корне, во вложенном поле, в массиве.
const char* const kOverflowDocs[] = {
    R"({"format":"bendcore-assembly","version":2,"name":"A","definitions":[],"x":1e999})",
    R"({"format":"bendcore-assembly","version":2,"properties":{"density":-1e999}})",
    R"([1e+3081])",
    R"({"format":"bendcore-part","version":3,"features":[{"parameters":{"length":"10"}, "v": 1e400}]})",
    R"({"format":"bendcore-drawing","version":2,"sheets":[{"scale":[1, 1e999]}]})",
};

}  // namespace

TEST(LoaderRegressions, AssemblyJsonNumberOverflowIsParseError) {
    for (const char* doc : kOverflowDocs) {
        auto r = bendcore::assembly_v2::AssemblyDocument::fromJson(doc, {});
        ASSERT_FALSE(r.ok()) << doc;
        EXPECT_EQ(r.error().code, ErrorCode::ParseError) << doc << ": " << r.error().message;
    }
}

TEST(LoaderRegressions, PartAndDrawingJsonNumberOverflowIsParseError) {
    for (const char* doc : kOverflowDocs) {
        auto p = bendcore::modeling::PartDocument::fromJson(doc, bendcore::modeling::partRegistry());
        ASSERT_FALSE(p.ok()) << doc;
        EXPECT_EQ(p.error().code, ErrorCode::ParseError) << doc << ": " << p.error().message;
        auto d = bendcore::drawing_v2::document::DrawingDocument::fromJson(doc);
        ASSERT_FALSE(d.ok()) << doc;
        EXPECT_EQ(d.error().code, ErrorCode::ParseError) << doc << ": " << d.error().message;
    }
}

namespace {

nlohmann::json seedJson(const char* target) {
    return nlohmann::json::parse(bendcore::qa::fuzz::seeds(target, BENDCORE_SOURCE_DIR).front());
}

}  // namespace

TEST(LoaderRegressions, PartSketchIntegerFieldsRejectOutOfRangeNumbers) {
    const nlohmann::json base = seedJson("part_json");
    ASSERT_TRUE(bendcore::modeling::PartDocument::fromJson(base.dump(), bendcore::modeling::partRegistry()).ok());
    // Первая операция кронштейна — эскиз; его поля-индексы.
    ASSERT_EQ(base["features"][0]["type"], "Sketch");
    const nlohmann::json bad[] = {-1e308, 1e308, -18446744073709551616.0, 2147483648LL, 3.5, "1"};
    for (const char* field : {"start", "projection", "degree"}) {
        for (const auto& v : bad) {
            nlohmann::json doc = base;
            doc["features"][0]["sketch"]["geometry"][0][field] = v;
            auto r = bendcore::modeling::PartDocument::fromJson(doc.dump(), bendcore::modeling::partRegistry());
            ASSERT_FALSE(r.ok()) << field << " = " << v.dump();
            EXPECT_EQ(r.error().code, ErrorCode::ParseError) << field << " = " << v.dump() << ": " << r.error().message;
        }
    }
    nlohmann::json doc = base;
    doc["features"][0]["sketch"]["regions"] = {0, -1e308};
    EXPECT_EQ(bendcore::modeling::PartDocument::fromJson(doc.dump(), bendcore::modeling::partRegistry()).error().code,
              ErrorCode::ParseError);
    doc = base;
    doc["version"] = 1e308;
    EXPECT_EQ(bendcore::modeling::PartDocument::fromJson(doc.dump(), bendcore::modeling::partRegistry()).error().code,
              ErrorCode::ParseError);
}

TEST(LoaderRegressions, DrawingScaleAndRevisionRejectOutOfRangeNumbers) {
    using bendcore::drawing_v2::document::DrawingDocument;
    const nlohmann::json base = seedJson("drawing_json");
    ASSERT_TRUE(DrawingDocument::fromJson(base.dump()).ok());
    for (const nlohmann::json& scale : {nlohmann::json{2147483648LL, 1}, nlohmann::json{-2147483648LL, 2},
                                        nlohmann::json{1e308, 1}, nlohmann::json{1, 0}}) {
        nlohmann::json doc = base;
        doc["sheets"][0]["scale"] = scale;
        auto r = DrawingDocument::fromJson(doc.dump());
        ASSERT_FALSE(r.ok()) << scale.dump();
        EXPECT_EQ(r.error().code, ErrorCode::ParseError) << scale.dump() << ": " << r.error().message;
    }
    for (const nlohmann::json& rev : {nlohmann::json(1e308), nlohmann::json(-1), nlohmann::json(4294967296LL)}) {
        nlohmann::json doc = base;
        doc["model"]["revision"] = rev;
        auto r = DrawingDocument::fromJson(doc.dump());
        ASSERT_FALSE(r.ok()) << rev.dump();
        EXPECT_EQ(r.error().code, ErrorCode::ParseError) << rev.dump() << ": " << r.error().message;
    }
    // Допустимые значения по-прежнему читаются: масштаб 2:1 ГОСТ 2.302, ревизия 7.
    nlohmann::json ok = base;
    ok["sheets"][0]["scale"] = {2, 1};
    ok["model"]["revision"] = 7;
    auto r = DrawingDocument::fromJson(ok.dump());
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().model().revision, 7u);
}
