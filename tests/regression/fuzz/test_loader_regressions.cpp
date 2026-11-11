// Регрессия дефектов, найденных fuzz-проверкой загрузчиков (docs/qa/QA.md, раздел 7).
//
// 1. AssemblyDocument::fromJson выпускал наружу nlohmann::json::out_of_range (406, переполнение
//    числа при разборе: «1e999») — ловился только parse_error. Найдено: assembly_json, мутации
//    #231 и #282 (зерно 20261111). Теперь — ParseError. Те же входы проверяются у загрузчиков
//    детали и чертежа, где дефекта не было.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <gtest/gtest.h>

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
