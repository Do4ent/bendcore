// Тесты GeometryAnalyzer без OCCT: поведение без бэкенда, перевод признаков B-rep в параметры
// трубы, численные методы (окружность, PCA). Разбор реального B-rep — в test_geometry_analyzer_occt.
#include <bendcore/geometry_analyzer.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace bendcore::geometry;

static bool near(double a, double b, double tol = 1e-6) { return std::abs(a - b) <= tol; }

static void test_no_backend_is_explicit() {
    set_geometry_backend(nullptr);
    GeometryAnalyzer analyzer;
    auto p = analyzer.analyze("any brep text");
    assert(!has_geometry_backend());
    assert(p.confidence == 0.0);
    assert(!p.is_valid());
    assert(p.outer_diameter == 0.0);  // никаких выдуманных значений
    assert(p.analysis_notes.find("backend not available") != std::string::npos);
    assert(analyzer.detect_bends("any").empty());

    auto empty = analyzer.analyze("");
    assert(empty.confidence == 0.0 && empty.analysis_notes == "No geometry data provided");
    std::cout << "PASS no backend -> explicit failure\n";
}

static void test_features_bent_tube() {
    // Труба 25.4 x 1.6: два прямых участка по 100 мм и гиб 90° с радиусом по оси 50 мм.
    BRepTubeFeatures f;
    f.ok = true;
    f.outer_radius = 12.7;
    f.inner_radius = 11.1;
    f.straight_lengths = {100.0, 100.0};
    f.bends = {{90.0, 50.0}};
    auto p = GeometryAnalyzer::params_from_features(f);
    assert(near(p.outer_diameter, 25.4));
    assert(near(p.inner_diameter, 22.2));
    assert(near(p.wall_thickness, 1.6));
    assert(p.bend_angles.size() == 1 && near(p.bend_angles[0], 90.0));
    assert(near(p.bend_radii[0], 50.0));
    assert(near(p.bend_arc_length, 50.0 * M_PI / 2.0));
    assert(near(p.total_length, 200.0 + 50.0 * M_PI / 2.0));
    assert(p.shape_type == "BEND");
    assert(p.confidence >= 0.5 && p.is_valid());
    std::cout << "PASS bent tube features -> OD/wall/length\n";
}

static void test_features_solid_bar_and_error() {
    BRepTubeFeatures bar;
    bar.ok = true;
    bar.outer_radius = 5.0;
    bar.straight_lengths = {300.0};
    auto p = GeometryAnalyzer::params_from_features(bar);
    assert(near(p.outer_diameter, 10.0) && p.inner_diameter == 0.0 && near(p.wall_thickness, 5.0));
    assert(p.shape_type == "TUBE" && near(p.total_length, 300.0));

    BRepTubeFeatures bad;
    bad.error = "no cylindrical faces";
    auto e = GeometryAnalyzer::params_from_features(bad);
    assert(e.confidence == 0.0 && e.analysis_notes.find("no cylindrical faces") != std::string::npos);
    std::cout << "PASS solid bar and backend error\n";
}

static void test_backend_injection() {
    int calls = 0;
    set_geometry_backend([&](const std::string& text) {
        ++calls;
        BRepTubeFeatures f;
        f.ok = text == "tube";
        f.error = "unknown shape";
        f.outer_radius = 8.0;
        f.inner_radius = 7.0;
        f.straight_lengths = {50.0, 60.0, 70.0};
        f.bends = {{45.0, 32.0}, {30.0, 32.0}};
        return f;
    });
    GeometryAnalyzer analyzer;
    auto p = analyzer.analyze("tube");
    assert(calls == 1 && p.shape_type == "COMPLEX" && near(p.outer_diameter, 16.0));
    assert(analyzer.detect_bends("tube").size() == 2);
    assert(analyzer.analyze("other").confidence == 0.0);

    set_geometry_backend([](const std::string&) -> BRepTubeFeatures { throw std::runtime_error("boom"); });
    auto t = analyzer.analyze("tube");
    assert(t.confidence == 0.0 && t.analysis_notes.find("boom") != std::string::npos);
    set_geometry_backend(nullptr);
    std::cout << "PASS backend injection and exceptions\n";
}

static void test_fit_cylinder() {
    GeometryAnalyzer analyzer;
    std::vector<std::pair<double, double>> pts;
    for (int i = 0; i < 36; ++i) {
        double a = i * M_PI / 18.0;
        pts.push_back({3.0 + 12.7 * std::cos(a), -2.0 + 12.7 * std::sin(a)});
    }
    double d = analyzer.fit_cylinder(pts);
    assert(std::abs(d - 25.4) < 1e-6);  // диаметр окружности
    assert(analyzer.fit_cylinder({{0, 0}, {1, 1}}) < 0);
    std::cout << "PASS fit_cylinder (" << d << ")\n";
}

static void test_pca_axis() {
    GeometryAnalyzer analyzer;
    std::vector<std::vector<double>> pts;
    for (int i = 0; i < 50; ++i) pts.push_back({0.1 * (i % 3), 0.1 * (i % 2), double(i)});
    auto r = analyzer.find_tube_axis_pca(pts);
    assert(r.axis_direction.size() == 3);
    assert(std::abs(std::abs(r.axis_direction[2]) - 1.0) < 0.01);
    std::cout << "PASS PCA axis along Z\n";
}

int main() {
    test_no_backend_is_explicit();
    test_features_bent_tube();
    test_features_solid_bar_and_error();
    test_backend_injection();
    test_fit_cylinder();
    test_pca_axis();
    std::cout << "All geometry analyzer tests passed\n";
    return 0;
}
