// Модели крепежа: объёмы и габариты против формул по табличным размерам, валидность BRep,
// обозначения, карта граней, атрибуты резьбы, ошибки (docs/assembly/FASTENERS.md, раздел 7).

#include <bendcore/assembly_v2/fasteners.hpp>

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Bnd_Box.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;
using bendcore::kernel::ErrorCode;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kTan30 = 0.57735026918962576;

// Площадь пересечения правильного шестиугольника (размер под ключ s) и круга радиуса r
// с общим центром: круг минус шесть сегментов за сторонами.
double hexDiskArea(double s, double r) {
    const double a = s / 2, R = s / std::sqrt(3.0);
    if (r >= R) return std::sqrt(3.0) / 2 * s * s;
    if (r <= a) return kPi * r * r;
    return kPi * r * r - 6 * (r * r * std::acos(a / r) - a * std::sqrt(r * r - a * a));
}

// Объём шестигранника высотой h с конусными фасками 30°, начинающимися на окружности 0,95·s
// (условная фаска модели, docs/assembly/FASTENERS.md), интегрированием площади сечения (Симпсон).
double chamferedHexVolume(double s, double h, bool bottom, bool top) {
    const double rt = 0.95 * s / 2;
    const int n = 200000;
    auto radius = [&](double z) {
        double r = 1e9;
        if (bottom) r = std::min(r, rt + z / kTan30);
        if (top) r = std::min(r, rt + (h - z) / kTan30);
        return r;
    };
    const double dz = h / n;
    double sum = 0;
    for (int i = 0; i <= n; ++i) {
        const double w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
        sum += w * hexDiskArea(s, radius(i * dz));
    }
    return sum * dz / 3;
}

double frustum(double r1, double r2, double h) { return kPi * h / 3 * (r1 * r1 + r1 * r2 + r2 * r2); }

// Стержень длиной l, радиуса r, с фаской 45° до радиуса r3 на конце.
double shankVolume(double r, double r3, double l) {
    const double c = r - r3;
    return kPi * r * r * (l - c) + frustum(r3, r, c);
}

double volume(const Fastener& f) { return f.properties.volume; }

Fastener make(FastenerStandard st, int d, double l = 0) {
    auto r = makeFastener({st, d, l});
    EXPECT_TRUE(r.ok()) << toString(st) << " " << d << ": " << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : Fastener{};
}

Bnd_Box bounds(const Fastener& f) {
    Bnd_Box b;
    BRepBndLib::AddOptimal(f.shape, b, false, false);
    return b;
}

void expectBox(const Fastener& f, double xmin, double ymin, double zmin, double xmax, double ymax, double zmax) {
    double a[6];
    bounds(f).Get(a[0], a[1], a[2], a[3], a[4], a[5]);
    const double e[6] = {xmin, ymin, zmin, xmax, ymax, zmax};
    for (int i = 0; i < 6; ++i) EXPECT_NEAR(a[i], e[i], 1e-5) << f.properties.designation << " box " << i;
}

void expectRelNear(double actual, double expected, double rel, const std::string& what) {
    EXPECT_NEAR(actual, expected, rel * std::abs(expected)) << what;
}

Entity faceEntity(const Fastener& f, const std::string& role, EntityUse use = EntityUse::Auto) {
    auto idx = f.elementMap->find(ElementKind::Face, faceName(role));
    EXPECT_TRUE(idx.has_value()) << role;
    auto e = entityFromElement(f.elementMap->element(ElementKind::Face, idx.value_or(0)), use);
    EXPECT_TRUE(e.ok()) << role;
    return e.ok() ? e.value() : Entity{};
}

}  // namespace

TEST(FastenerModels, HexBoltVolumeBoxAndFaces) {
    const Fastener f = make(FastenerStandard::Gost7798, 10, 40);
    // ГОСТ 7798-70 М10: s = 17, k = 7, P = 1,5.
    const double s = 17, k = 7, d = 10, l = 40, d3 = 10 - 1.226869 * 1.5;
    expectRelNear(volume(f), chamferedHexVolume(s, k, false, true) + shankVolume(d / 2, d3 / 2, l), 1e-6, "bolt V");
    const double e = 2 * s / std::sqrt(3.0);
    expectBox(f, -s / 2, -e / 2, -l, s / 2, e / 2, k);
    EXPECT_TRUE(BRepCheck_Analyzer(f.shape).IsValid());

    const Entity bearing = faceEntity(f, roles::kBearing);
    EXPECT_EQ(bearing.type, EntityType::Plane);
    EXPECT_NEAR(bearing.point.z(), 0, 1e-12);
    EXPECT_NEAR(bearing.direction.z(), -1, 1e-12);
    const Entity top = faceEntity(f, roles::kTop);
    EXPECT_NEAR(top.point.z(), k, 1e-12);
    EXPECT_NEAR(top.direction.z(), 1, 1e-12);
    const Entity shank = faceEntity(f, roles::kShank);
    EXPECT_EQ(shank.type, EntityType::Cylinder);
    EXPECT_NEAR(shank.radius, 5, 1e-12);
    EXPECT_NEAR(std::abs(shank.direction.z()), 1, 1e-12);
    EXPECT_NEAR(faceEntity(f, roles::kTip).point.z(), -l, 1e-12);
    for (int i = 1; i <= 6; ++i) {
        const Entity flat = faceEntity(f, std::string(roles::kFlat) + std::to_string(i));
        EXPECT_NEAR(flat.direction.dot(Vec3(flat.point.x(), flat.point.y(), 0).normalized()), 1, 1e-9);
        EXPECT_NEAR(Vec3(flat.point.x(), flat.point.y(), 0).dot(flat.direction), s / 2, 1e-9);
    }

    EXPECT_EQ(f.properties.designation, "Болт М10×40 ГОСТ 7798-70");
    EXPECT_EQ(f.properties.equivalentDesignation, "");  // ISO 4014 M10: s = 16, k = 6,4
    EXPECT_EQ(f.properties.section, "Стандартные изделия");
    EXPECT_NEAR(f.properties.mass, volume(f) * 7850e-9, 1e-15);
    EXPECT_TRUE(f.thread.present);
    EXPECT_FALSE(f.thread.internal);
    EXPECT_EQ(f.thread.designation, "M10");
    EXPECT_DOUBLE_EQ(f.thread.pitch, 1.5);
    EXPECT_NEAR(f.thread.minorDiameter, d3, 1e-12);
    EXPECT_DOUBLE_EQ(f.thread.length, 26);  // b = 2d + 6
    EXPECT_DOUBLE_EQ(f.dimension("s").value(), 17);
    EXPECT_DOUBLE_EQ(f.dimension("k").value(), 7);

    // Короткий болт ГОСТ — резьба на всю длину.
    EXPECT_DOUBLE_EQ(make(FastenerStandard::Gost7798, 10, 20).thread.length, 20);
}

TEST(FastenerModels, IsoBoltsAndThreadLength) {
    const Fastener f = make(FastenerStandard::Iso4014, 12, 80);
    EXPECT_EQ(f.properties.designation, "Болт ISO 4014 - M12 x 80");
    EXPECT_DOUBLE_EQ(f.thread.length, 30);  // 2·12 + 6
    expectRelNear(volume(f),
                  chamferedHexVolume(18, 7.5, false, true) + shankVolume(6, (12 - 1.226869 * 1.75) / 2, 80), 1e-6,
                  "ISO 4014 M12x80 V");
    EXPECT_DOUBLE_EQ(make(FastenerStandard::Iso4014, 12, 150).thread.length, 36);  // 2d + 12
    EXPECT_DOUBLE_EQ(make(FastenerStandard::Iso4014, 12, 220).thread.length, 49);  // 2d + 25
    EXPECT_DOUBLE_EQ(make(FastenerStandard::Iso4017, 12, 30).thread.length, 30);
    EXPECT_DOUBLE_EQ(f.dimension("e_min").value(), 20.03);

    auto shortIso = makeFastener({FastenerStandard::Iso4014, 10, 20});
    ASSERT_FALSE(shortIso.ok());
    EXPECT_EQ(shortIso.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(shortIso.error().message.find("ISO 4017"), std::string::npos);

    // Совпадение размеров с ГОСТ: М16 (s = 24, k = 10) в обеих таблицах.
    EXPECT_EQ(make(FastenerStandard::Iso4014, 16, 100).properties.equivalentDesignation, "Болт М16×100 ГОСТ 7798-70");
    EXPECT_EQ(make(FastenerStandard::Gost7798, 16, 30).properties.equivalentDesignation,
              "Болт ISO 4017 - M16 x 30");
}

TEST(FastenerModels, SocketHeadScrew) {
    const Fastener f = make(FastenerStandard::Iso4762, 8, 30);
    // ISO 4762 M8: dk = 13, k = 8, s = 6, t = 4; фаска кромки 0,1·d = 0,8.
    const double r = 6.5, k = 8, c = 0.8, s = 6, t = 4;
    const double head = kPi * r * r * (k - c) + frustum(r - c, r, c);
    const double socket = std::sqrt(3.0) / 2 * s * s * t;
    expectRelNear(volume(f), head - socket + shankVolume(4, (8 - 1.226869 * 1.25) / 2, 30), 1e-6, "ISO 4762 V");
    expectBox(f, -r, -r, -30, r, r, k);
    EXPECT_TRUE(BRepCheck_Analyzer(f.shape).IsValid());
    EXPECT_NEAR(faceEntity(f, roles::kSocketBottom).point.z(), k - t, 1e-12);
    EXPECT_NEAR(faceEntity(f, roles::kOuter).radius, r, 1e-12);
    EXPECT_EQ(f.properties.designation, "Винт ISO 4762 - M8 x 30");
    EXPECT_EQ(f.properties.equivalentDesignation, "Винт М8×30 ГОСТ 11738-84");
    EXPECT_DOUBLE_EQ(f.thread.length, 28);  // 2d + 12
    EXPECT_DOUBLE_EQ(make(FastenerStandard::Gost11738, 8, 20).thread.length, 20);
}

TEST(FastenerModels, HexNut) {
    const Fastener f = make(FastenerStandard::Gost5915, 10);
    const double s = 17, m = 8;
    expectRelNear(volume(f), chamferedHexVolume(s, m, true, true) - kPi * 25 * m, 1e-6, "nut V");
    const double e = 2 * s / std::sqrt(3.0);
    expectBox(f, -s / 2, -e / 2, 0, s / 2, e / 2, m);
    EXPECT_TRUE(BRepCheck_Analyzer(f.shape).IsValid());
    EXPECT_EQ(f.properties.designation, "Гайка М10 ГОСТ 5915-70");
    EXPECT_EQ(f.properties.equivalentDesignation, "");
    EXPECT_TRUE(f.thread.internal);
    EXPECT_NEAR(f.thread.minorDiameter, 10 - 1.082532 * 1.5, 1e-12);
    EXPECT_DOUBLE_EQ(f.thread.length, m);
    const Entity bore = faceEntity(f, roles::kBore);
    EXPECT_NEAR(bore.radius, 5, 1e-12);
    const Entity bearing = faceEntity(f, roles::kBearing);
    EXPECT_NEAR(bearing.direction.z(), -1, 1e-12);

    const Fastener iso = make(FastenerStandard::Iso4032, 3);
    EXPECT_EQ(iso.properties.designation, "Гайка ISO 4032 - M3");
    EXPECT_EQ(iso.properties.equivalentDesignation, "Гайка М3 ГОСТ 5915-70");  // s = 5,5, m = 2,4 в обеих
    expectRelNear(volume(iso), chamferedHexVolume(5.5, 2.4, true, true) - kPi * 1.5 * 1.5 * 2.4, 1e-6, "M3 nut V");
}

TEST(FastenerModels, Washers) {
    const Fastener w = make(FastenerStandard::Gost11371, 10);
    expectRelNear(volume(w), kPi / 4 * (21 * 21 - 10.5 * 10.5) * 2, 1e-9, "washer V");
    expectBox(w, -10.5, -10.5, 0, 10.5, 10.5, 2);
    EXPECT_EQ(w.properties.designation, "Шайба 10 ГОСТ 11371-78");
    EXPECT_EQ(w.properties.equivalentDesignation, "");  // ISO 7089: d2 = 20
    EXPECT_FALSE(w.thread.present);
    EXPECT_EQ(make(FastenerStandard::Iso7089, 12).properties.equivalentDesignation, "Шайба 12 ГОСТ 11371-78");
    EXPECT_NEAR(faceEntity(w, roles::kTop).point.z(), 2, 1e-12);

    const Fastener sw = make(FastenerStandard::Gost6402, 10);
    // ГОСТ 6402 М10 Н: d = 10,2, s = b = 2,5; разрез шириной s.
    const double r1 = 5.1, r2 = 7.6, s = 2.5, g = 2.5;
    auto F = [](double y, double R) { return 0.5 * (y * std::sqrt(R * R - y * y) + R * R * std::asin(y / R)); };
    const double slot = 2 * (F(g / 2, r2) - F(g / 2, r1));
    expectRelNear(volume(sw), (kPi * (r2 * r2 - r1 * r1) - slot) * s, 1e-9, "spring washer V");
    EXPECT_TRUE(BRepCheck_Analyzer(sw.shape).IsValid());
    EXPECT_EQ(sw.properties.designation, "Шайба 10 65Г ГОСТ 6402-70");
    EXPECT_EQ(sw.properties.name, "Шайба пружинная");
    EXPECT_EQ(sw.properties.equivalentDesignation, "");
    EXPECT_NEAR(faceEntity(sw, "Slot1").point.y(), -g / 2, 1e-12);
    EXPECT_NEAR(faceEntity(sw, "Slot2").point.y(), g / 2, 1e-12);

    auto missing = makeFastener({FastenerStandard::Gost6402, 20, 0});
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, ErrorCode::Unsupported);
}

TEST(FastenerModels, ParallelPin) {
    const Fastener p = make(FastenerStandard::Gost3128, 8, 40);
    const double r = 4, c = 1.6, l = 40;
    expectRelNear(volume(p), kPi * r * r * (l - 2 * c) + 2 * frustum(r - c, r, c), 1e-9, "pin V");
    expectBox(p, -r, -r, 0, r, r, l);
    EXPECT_EQ(p.properties.designation, "Штифт 8 m6×40 ГОСТ 3128-70");
    EXPECT_EQ(p.properties.equivalentDesignation, "Штифт ISO 2338 - 8 m6 x 40");
    EXPECT_NEAR(faceEntity(p, "End2").point.z(), l, 1e-12);
    auto shortPin = makeFastener({FastenerStandard::Iso2338, 8, 3});
    ASSERT_FALSE(shortPin.ok());
    EXPECT_EQ(shortPin.error().code, ErrorCode::InvalidInput);
}

// Все типоразмеры всех таблиц строятся, тела валидны, объём положителен, имена граней есть.
TEST(FastenerModels, AllTableSizesBuildValidSolids) {
    for (FastenerStandard st : allStandards()) {
        const TableInfo info = tableInfo(st);
        EXPECT_FALSE(info.verified);
        EXPECT_FALSE(info.source.empty());
        EXPECT_NE(info.verification.find("требует сверки"), std::string::npos);
        ASSERT_FALSE(info.sizes.empty());
        for (int d : info.sizes) {
            double l = 0;
            const FastenerKind kind = kindOf(st);
            if (kind == FastenerKind::HexBolt || kind == FastenerKind::SocketHeadScrew) l = 3.0 * d + 20;
            if (kind == FastenerKind::ParallelPin) l = 4.0 * d;
            auto f = makeFastener({st, d, l});
            ASSERT_TRUE(f.ok()) << toString(st) << " " << d << ": " << f.error().message;
            EXPECT_TRUE(BRepCheck_Analyzer(f.value().shape).IsValid()) << toString(st) << " " << d;
            EXPECT_GT(f.value().properties.volume, 0);
            ASSERT_TRUE(f.value().elementMap);
            EXPECT_TRUE(f.value().elementMap->shape().IsSame(f.value().shape));
            const char* axisRole = (kind == FastenerKind::HexNut || kind == FastenerKind::PlainWasher ||
                                    kind == FastenerKind::SpringWasher)
                                       ? roles::kBore
                                       : roles::kShank;
            EXPECT_TRUE(f.value().elementMap->find(ElementKind::Face, faceName(axisRole)).has_value());
            if (kind != FastenerKind::ParallelPin) {
                EXPECT_TRUE(f.value().elementMap->find(ElementKind::Face, faceName(roles::kBearing)).has_value());
            }
        }
    }
}

TEST(FastenerModels, TableLookupErrors) {
    auto wrong = hexNutRow(FastenerStandard::Iso4014, 10);
    ASSERT_FALSE(wrong.ok());
    EXPECT_EQ(wrong.error().code, ErrorCode::InvalidInput);
    auto noSize = hexBoltRow(FastenerStandard::Gost7798, 4);  // ГОСТ 7798 начинается с М6
    ASSERT_FALSE(noSize.ok());
    EXPECT_EQ(noSize.error().code, ErrorCode::Unsupported);
    EXPECT_NE(noSize.error().message.find("6 8 10"), std::string::npos);
    EXPECT_FALSE(coarsePitch(7).has_value());
    EXPECT_DOUBLE_EQ(coarsePitch(24).value(), 3.0);
    EXPECT_FALSE(counterpart(FastenerStandard::Gost6402).has_value());
    EXPECT_EQ(counterpart(FastenerStandard::Iso4032), FastenerStandard::Gost5915);
}
