// Рамка и основная надпись против размеров ГОСТ 2.104-2006 (форма 1 и 2а) и ГОСТ 2.301.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace dv2test;

namespace {

bool hasLine(const Drawing& d, Point2 a, Point2 b, LineType type) {
    for (const auto& e : d.entities()) {
        const auto* l = std::get_if<LineSeg>(&e.geometry);
        if (!l || e.lineType != type) continue;
        auto same = [](Point2 p, Point2 q) { return std::fabs(p.x - q.x) < 1e-9 && std::fabs(p.y - q.y) < 1e-9; };
        if ((same(l->a, a) && same(l->b, b)) || (same(l->a, b) && same(l->b, a))) return true;
    }
    return false;
}

const Text* findText(const Drawing& d, const std::string& s) {
    for (const auto& e : d.entities()) {
        if (const auto* t = std::get_if<Text>(&e.geometry); t && t->utf8 == s) return t;
    }
    return nullptr;
}

void expectRect(const Rect& r, double x0, double y0, double w, double h) {
    EXPECT_NEAR(r.x0, x0, 1e-9);
    EXPECT_NEAR(r.y0, y0, 1e-9);
    EXPECT_NEAR(r.width(), w, 1e-9);
    EXPECT_NEAR(r.height(), h, 1e-9);
}

}  // namespace

TEST(DrawingV2TitleBlock, Form1OnA3LandscapeMatchesGost2104) {
    auto s = makeSample(GostFormat::A3, Orientation::Landscape, systemFont());
    const auto& L = s.layout;
    const Drawing& d = s.drawing;
    // Лист 420×297, поля: 20 слева, 5 остальные.
    expectRect(L.sheet, 0, 0, 420, 297);
    expectRect(L.frame, 20, 5, 395, 287);
    // Основная надпись 185×55 в правом нижнем углу рамки.
    expectRect(L.titleBlock, 230, 5, 185, 55);
    const double X = 230, Y = 5;
    // Графы (ширина × высота) по форме 1.
    expectRect(L.graphs.at(1), X + 65, Y + 15, 70, 25);  // наименование
    expectRect(L.graphs.at(2), X + 65, Y + 40, 120, 15); // обозначение
    expectRect(L.graphs.at(3), X + 65, Y, 70, 15);       // материал
    expectRect(L.graphs.at(4), X + 135, Y + 20, 15, 15); // литера (3 × 5)
    expectRect(L.graphs.at(5), X + 150, Y + 20, 17, 15); // масса
    expectRect(L.graphs.at(6), X + 167, Y + 20, 18, 15); // масштаб
    expectRect(L.graphs.at(7), X + 135, Y + 15, 20, 5);  // лист
    expectRect(L.graphs.at(8), X + 155, Y + 15, 30, 5);  // листов
    expectRect(L.graphs.at(9), X + 135, Y, 50, 15);      // организация
    // Колонки левой части: 7, 10, 23, 15, 10.
    const double widths[5] = {7, 10, 23, 15, 10};
    for (int k = 0; k < 5; ++k) EXPECT_NEAR(L.graphs.at(14 + k).width(), widths[k], 1e-9);
    EXPECT_NEAR(L.graphs.at(10).width() + L.graphs.at(11).width() + L.graphs.at(12).width() +
                    L.graphs.at(13).width(), 65.0, 1e-9);
    // Рамка — сплошная толстая основная; линия обреза — тонкая.
    EXPECT_TRUE(hasLine(d, {20, 5}, {415, 5}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {20, 292}, {20, 5}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {0, 0}, {420, 0}, LineType::SolidThin));
    // Вертикали левой части через всю высоту и линия x = 7 только в зоне изменений.
    for (double x : {17.0, 40.0, 55.0, 65.0}) EXPECT_TRUE(hasLine(d, {X + x, Y}, {X + x, Y + 55}, LineType::SolidThick)) << x;
    EXPECT_TRUE(hasLine(d, {X + 7, Y + 30}, {X + 7, Y + 55}, LineType::SolidThick));
    // 11 строк по 5 мм: заголовок — толстые линии, остальные — тонкие.
    EXPECT_TRUE(hasLine(d, {X, Y + 30}, {X + 65, Y + 30}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {X, Y + 35}, {X + 65, Y + 35}, LineType::SolidThick));
    for (double y : {5.0, 10.0, 15.0, 20.0, 25.0, 40.0, 45.0, 50.0}) {
        EXPECT_TRUE(hasLine(d, {X, Y + y}, {X + 65, Y + y}, LineType::SolidThin)) << y;
    }
    EXPECT_TRUE(hasLine(d, {X + 65, Y + 40}, {X + 185, Y + 40}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {X + 135, Y}, {X + 135, Y + 40}, LineType::SolidThick));
    // Тексты заполнены и стоят внутри своих граф.
    const Text* des = findText(d, "АБВГ.123456.001");
    ASSERT_NE(des, nullptr);
    EXPECT_NEAR(des->position.x, X + 65 + 60, 1e-9);
    EXPECT_NEAR(des->position.y, Y + 47.5, 1e-9);
    EXPECT_EQ(des->hAlign, HAlign::Center);
    EXPECT_EQ(des->vAlign, VAlign::Middle);
    for (const char* s2 : {"Вал ступенчатый", "Сталь 45 ГОСТ 1050-2013", "1:2", "1,25", "Лист 1", "Листов 2",
                           "Иванов", "Петров", "Разраб.", "Пров.", "Т.контр.", "Н.контр.", "Утв.", "Изм.",
                           "№ докум.", "Масштаб", "О", "ООО «Бенд»", "Инв. № подл."}) {
        EXPECT_NE(findText(d, s2), nullptr) << s2;
    }
    const Text* dev = findText(d, "Иванов");
    EXPECT_NEAR(dev->position.y, Y + 27.5, 1e-9);  // строка «Разраб.» — 25…30 мм
    // Каждый текст с подобранной шириной помещается в графу (по метрикам шрифта).
    if (const TrueTypeFont* f = systemFont()) {
        const Text* mat = findText(d, "Сталь 45 ГОСТ 1050-2013");
        const double w = f->textWidthMm(decodeUtf8(mat->utf8).value(), mat->height) * mat->widthFactor;
        EXPECT_LE(w, 70.0 - 1.0 + 1e-9);
        EXPECT_GE(mat->widthFactor, 0.6);
    }
}

TEST(DrawingV2TitleBlock, StorageStripGraphs19To23) {
    auto s = makeSample(GostFormat::A4, Orientation::Portrait, systemFont());
    const auto& L = s.layout;
    expectRect(L.titleBlock, 20, 5, 185, 55);  // на А4 надпись занимает всю ширину рамки
    const double lengths[5] = {25, 35, 25, 25, 35};
    double y = 5, total = 0;
    for (int i = 0; i < 5; ++i) {
        expectRect(L.graphs.at(19 + i), 8, y, 12, lengths[i]);
        y += lengths[i];
        total += lengths[i];
    }
    EXPECT_DOUBLE_EQ(total, 145.0);
    const Text* t = findText(s.drawing, "Взам. инв. №");
    ASSERT_NE(t, nullptr);
    EXPECT_DOUBLE_EQ(t->rotationDeg, 90.0);
}

TEST(DrawingV2TitleBlock, Form2aAndErrors) {
    auto d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    FrameOptions fo;
    fo.form = TitleBlockForm::Form2a;
    fo.font = systemFont();
    auto data = sampleTitleData();
    data.sheet = 2;
    auto L = addFrameAndTitleBlock(d, data, fo);
    ASSERT_TRUE(L) << L.error().message;
    expectRect(L.value().titleBlock, 230, 5, 185, 15);
    expectRect(L.value().graphs.at(2), 295, 5, 110, 15);
    expectRect(L.value().graphs.at(7), 405, 5, 10, 15);
    EXPECT_TRUE(hasLine(d, {405, 13}, {415, 13}, LineType::SolidThick));
    EXPECT_NE(findText(d, "2"), nullptr);

    auto a4l = Drawing::create(gostSheet(GostFormat::A4, 1, Orientation::Landscape).value()).value();
    EXPECT_FALSE(addFrameAndTitleBlock(a4l, sampleTitleData()));
    auto a5 = Drawing::create(iso216Sheet(5).value()).value();
    EXPECT_FALSE(addFrameAndTitleBlock(a5, sampleTitleData()));
    if (systemFont()) {
        auto a3 = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
        auto longName = sampleTitleData();
        longName.scale = "1:1000000000000";  // 18 мм графы не хватает и при h = 2,5, k = 0,6
        FrameOptions f2;
        f2.font = systemFont();
        auto r = addFrameAndTitleBlock(a3, longName, f2);
        ASSERT_FALSE(r);
        EXPECT_NE(r.error().message.find("scale"), std::string::npos);
    }
    auto bad = sampleTitleData();
    bad.sheet = 3;
    auto a3b = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    EXPECT_FALSE(addFrameAndTitleBlock(a3b, bad));
}
