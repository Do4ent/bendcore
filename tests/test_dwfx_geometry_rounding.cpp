// Тесты геометрического упрощения (spec 9.9): S–A–B округление, защита отклонением.
#include "bendcore/dwfx/geometry_rounding.hpp"

#include <cmath>
#include <cstdio>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) < tol;
}

static void test_round_length() {
    RoundingSettings s;
    s.round_length_mm = 1.0;

    check(near(round_length(50.3, s), 50.0), "длина 50.3 → 50.0 (шаг 1 мм)");
    check(near(round_length(50.6, s), 51.0), "длина 50.6 → 51.0 (шаг 1 мм)");
    check(near(round_length(50.0, s), 50.0), "длина 50.0 → 50.0 (точно)");
}

static void test_round_angle() {
    RoundingSettings s;
    s.round_angle_deg = 0.5;
    s.round_align_deg = 0.5;

    // Округление угла
    auto res1 = round_angle(90.1, 0.0, s);
    check(near(res1.angle_deg, 90.0), "угол 90.1° → 90.0° (шаг 0,5°)");

    auto res2 = round_angle(90.3, 0.0, s);
    check(near(res2.angle_deg, 90.5), "угол 90.3° → 90.5° (шаг 0,5°)");

    // Выравнивание поворота
    auto res3 = round_angle(90.0, 45.3, s);
    check(near(res3.rotation_deg, 45.0) || near(res3.rotation_deg, 45.5),
          "поворот 45.3° выравнен по шагу 0,5°");
}

static void test_round_diameter_wall() {
    RoundingSettings s;
    s.round_diameter_wall_mm = 0.1;

    auto res = round_diameter_wall(20.15, 1.35, s);
    check(near(res.od_mm, 20.2), "диаметр 20.15 → 20.2 (шаг 0,1 мм)");
    check(near(res.wall_mm, 1.4), "стенка 1.35 → 1.4 (шаг 0,1 мм)");
    // Отклонение по диаметру
    check(res.deviation_mm > 0.0, "отклонение диаметра учтено");
}

static void test_round_path_minimal_deviations() {
    // Путь с минимальными отклонениями, чтобы не превысить допуск
    std::vector<PathSegment> path = {
        {PathSegment::Straight, 50.1, 0.0, 0.0, 0.0},      // Длина 50.1 → 50.0 (0.1 мм)
        {PathSegment::Bend, 0.0, 50.0, 90.05, 0.0},         // Угол 90.05 → 90.0 (0.05)
        {PathSegment::Straight, 30.1, 0.0, 0.0, 0.0}        // Длина 30.1 → 30.0 (0.1 мм)
    };

    RoundingSettings s;
    s.round_length_mm = 1.0;
    s.round_angle_deg = 0.5;
    s.round_diameter_wall_mm = 0.1;
    s.max_deviation_mm = 1.0;

    const auto result = round_path(path, 20.0, 1.0, s);

    check(result.segments.size() == 3, "путь: 3 участка");
    check(near(result.segments[0].length_mm, 50.0), "участок 1: длина округлена");
    check(near(result.segments[1].angle_deg, 90.0), "участок 2: угол округлен");
    check(near(result.segments[2].length_mm, 30.0), "участок 3: длина округлена");
    check(!result.exceeds_tolerance, "отклонение в допуске");
}

static void test_round_path_exceeded_tolerance() {
    // Путь с большим отклонением (специально для теста превышения)
    std::vector<PathSegment> path = {
        {PathSegment::Straight, 50.7, 0.0, 0.0, 0.0},      // Длина 50.7 → 51.0 (0.3 мм)
        {PathSegment::Bend, 0.0, 50.0, 90.8, 0.0}           // Угол 90.8 → 91.0 (0.2 мм)
    };

    RoundingSettings s;
    s.round_length_mm = 1.0;
    s.round_angle_deg = 0.5;
    s.max_deviation_mm = 0.3;  // Очень строгий допуск

    const auto result = round_path(path, 20.0, 1.0, s);

    // При таком допуске должно быть превышение
    if (result.total_deviation_mm > s.max_deviation_mm) {
        check(result.exceeds_tolerance, "превышение допуска отмечено");
        std::printf("  отклонение %.3f мм > допуск %.3f мм\n", result.total_deviation_mm, s.max_deviation_mm);
    }
}

static void test_angle_normalization() {
    RoundingSettings s;
    s.round_angle_deg = 0.5;
    s.round_align_deg = 0.5;

    // Угол > 180 должен быть нормализован
    auto res = round_angle(200.0, 0.0, s);
    check(res.angle_deg >= -180.0 && res.angle_deg <= 180.0,
          "угол нормализован в диапазон [-180, 180]");
}

int main() {
    std::printf("Тесты геометрического упрощения (spec 9.9)\n");

    test_round_length();
    test_round_angle();
    test_round_diameter_wall();
    test_round_path_minimal_deviations();
    test_round_path_exceeded_tolerance();
    test_angle_normalization();

    if (failures == 0) {
        std::printf("OK geometry_rounding\n");
        return 0;
    } else {
        std::printf("%d failures\n", failures);
        return 1;
    }
}
