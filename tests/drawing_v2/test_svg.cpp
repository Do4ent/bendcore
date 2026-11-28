// SVG: разбор элементов и атрибутов, проверка размеров листа в мм, координат (ось Y вниз),
// толщин и stroke-dasharray по ГОСТ 2.303; проверка корректности XML через xmllint.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <map>
#include <regex>

using namespace dv2test;

namespace {

struct Element {
    std::string name;
    std::map<std::string, std::string> attrs;
    std::string text;
};

std::vector<Element> parseElements(const std::string& svg) {
    std::vector<Element> out;
    const std::regex tag(R"re(<([a-zA-Z]+)((?:\s+[a-zA-Z0-9:-]+="[^"]*")*)\s*(/?)>)re");
    const std::regex attr(R"re(([a-zA-Z0-9:-]+)="([^"]*)")re");
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), tag); it != std::sregex_iterator(); ++it) {
        Element e;
        e.name = (*it)[1];
        const std::string a = (*it)[2];
        for (auto jt = std::sregex_iterator(a.begin(), a.end(), attr); jt != std::sregex_iterator(); ++jt) {
            e.attrs[(*jt)[1]] = (*jt)[2];
        }
        if ((*it)[3] != "/") {
            const std::size_t start = std::size_t(it->position() + it->length());
            const std::size_t end = svg.find("</" + e.name + ">", start);
            if (end != std::string::npos && svg.find('<', start) == end) e.text = svg.substr(start, end - start);
        }
        out.push_back(std::move(e));
    }
    return out;
}

const Element* findWith(const std::vector<Element>& els, const std::string& name,
                        const std::map<std::string, std::string>& want) {
    for (const auto& e : els) {
        if (e.name != name) continue;
        bool ok = true;
        for (const auto& [k, v] : want) {
            auto it = e.attrs.find(k);
            ok = ok && it != e.attrs.end() && it->second == v;
        }
        if (ok) return &e;
    }
    return nullptr;
}

}  // namespace

TEST(DrawingV2Svg, SizeCoordinatesAndDashes) {
    auto s = makeSample(GostFormat::A3, Orientation::Landscape, systemFont());
    SvgOptions o;
    o.font = systemFont();
    o.title = "Вал ступенчатый";
    auto svg = writeSvg(s.drawing, o);
    ASSERT_TRUE(svg) << svg.error().message;
    saveToFile(outPath("sample_a3.svg"), svg.value()).value();
    const auto els = parseElements(readFile(outPath("sample_a3.svg")));

    const Element* root = findWith(els, "svg", {});
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->attrs.at("width"), "420mm");
    EXPECT_EQ(root->attrs.at("height"), "297mm");
    EXPECT_EQ(root->attrs.at("viewBox"), "0 0 420 297");

    // Отрезок (50,100)–(150,100), толстая s = 0,5: в SVG y = 297 − 100 = 197.
    EXPECT_NE(findWith(els, "line", {{"x1", "50"}, {"y1", "197"}, {"x2", "150"}, {"y2", "197"},
                                     {"stroke-width", "0.5"}}),
              nullptr);
    // Штриховая s/2 = 0,25, шаблон 4 / 1,5.
    const Element* dashed = findWith(els, "line", {{"y1", "177"}, {"data-linetype", "GOST_DASHED"}});
    ASSERT_NE(dashed, nullptr);
    EXPECT_EQ(dashed->attrs.at("stroke-width"), "0.25");
    EXPECT_EQ(dashed->attrs.at("stroke-dasharray"), "4,1.5");
    // Штрихпунктирная тонкая: 15, точка = толщина 0,25, промежутки (3 − 0,25)/2.
    const Element* axis = findWith(els, "line", {{"y1", "157"}, {"data-linetype", "GOST_DASHDOT_THIN"}});
    ASSERT_NE(axis, nullptr);
    EXPECT_EQ(axis->attrs.at("stroke-dasharray"), "15,1.375,0.25,1.375");
    // Рамка: x = 20 от y = 5 до 292 (SVG: 292 … 5).
    EXPECT_NE(findWith(els, "line", {{"x1", "20"}, {"y1", "5"}, {"x2", "20"}, {"y2", "292"}, {"stroke-width", "0.5"}}),
              nullptr);
    // Окружность и дуга 0…90° радиуса 30 вокруг (100,180): из (130,117) в (100,87), sweep 0.
    EXPECT_NE(findWith(els, "circle", {{"cx", "100"}, {"cy", "117"}, {"r", "20"}}), nullptr);
    EXPECT_NE(findWith(els, "path", {{"d", "M 130 117 A 30 30 0 0 0 100 87"}}), nullptr);
    EXPECT_NE(findWith(els, "polygon", {{"points", "60,97 80,97 80,77 60,77"}}), nullptr);

    // Текст: кегль = h / (capHeight/unitsPerEm); для DejaVu Sans 5 / (1493/2048).
    const Element* text = nullptr;
    for (const auto& e : els) {
        if (e.name == "text" && e.text == "Технические требования: Ø20") text = &e;
    }
    ASSERT_NE(text, nullptr);
    if (systemFont()) {
        EXPECT_NEAR(std::stod(text->attrs.at("font-size")), 5.0 / systemFont()->capHeightRatio(), 1e-6);
    }
    EXPECT_EQ(text->attrs.at("transform"), "translate(50 207) skewX(-15)");
    // Тексты основной надписи с выравниванием по центру графы.
    bool designation = false;
    for (const auto& e : els) {
        if (e.name == "text" && e.text == "АБВГ.123456.001") {
            designation = true;
            EXPECT_EQ(e.attrs.at("text-anchor"), "middle");
            EXPECT_EQ(e.attrs.at("transform").rfind("translate(355 244.5) translate(0 3.5) skewX(-15)", 0), 0u);
        }
    }
    EXPECT_TRUE(designation);
    EXPECT_NE(findWith(els, "title", {}), nullptr);

#ifdef BENDCORE_XMLLINT
    int code = 0;
    const std::string out = runCommand(std::string(BENDCORE_XMLLINT) + " --noout " + outPath("sample_a3.svg") + " 2>&1", &code);
    EXPECT_EQ(code, 0) << out;
#endif
}
