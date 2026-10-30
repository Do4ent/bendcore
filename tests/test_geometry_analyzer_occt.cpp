// GeometryAnalyzer на реальном B-rep Open CASCADE: тело трубы строится по пути, анализ должен
// вернуть те же ∅, стенку, длину по оси, углы и радиусы гибов.
#include "bendcore/geometry_analyzer.hpp"
#include "bendcore/occt/geometry_backend.hpp"
#include "bendcore/tube_body.hpp"
#include "bendcore/tubegeom.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace bendcore;

static bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }

static void test_straight_tube() {
    const Path path{Straight{250}};
    const auto p = geometry::GeometryAnalyzer().analyze(geom::body_brep(path, 25.4, 1.6));
    assert(near(p.outer_diameter, 25.4, 1e-6));
    assert(near(p.wall_thickness, 1.6, 1e-6));
    assert(near(p.total_length, 250.0, 1e-4));
    assert(p.bend_angles.empty() && p.shape_type == "TUBE" && p.is_valid());
    std::printf("PASS straight tube: OD %.3f wall %.3f L %.3f\n", p.outer_diameter, p.wall_thickness, p.total_length);
}

static void test_bent_tube() {
    const Path path{Straight{100}, Bend{50, 90}, Straight{80}, Bend{60, 45}, Straight{40}};
    const double L = axis_length(path);
    const auto p = geometry::GeometryAnalyzer().analyze(geom::body_brep(path, 20.0, 2.0));
    assert(near(p.outer_diameter, 20.0, 1e-6));
    assert(near(p.wall_thickness, 2.0, 1e-6));
    assert(p.bend_angles.size() == 2 && p.shape_type == "COMPLEX");
    std::vector<std::pair<double, double>> bends;
    for (size_t i = 0; i < p.bend_angles.size(); ++i) bends.push_back({p.bend_angles[i], p.bend_radii[i]});
    std::sort(bends.begin(), bends.end());
    assert(near(bends[0].first, 45.0, 1e-4) && near(bends[0].second, 60.0, 1e-6));
    assert(near(bends[1].first, 90.0, 1e-4) && near(bends[1].second, 50.0, 1e-6));
    assert(near(p.total_length, L, 1e-3 * L));
    std::printf("PASS bent tube: 2 bends, L %.3f (path %.3f)\n", p.total_length, L);
}

static void test_solid_bar_and_garbage() {
    const auto bar = geometry::GeometryAnalyzer().analyze(geom::body_brep(Path{Straight{100}}, 10.0, 0.0));
    assert(near(bar.outer_diameter, 10.0, 1e-6) && bar.inner_diameter == 0.0);

    const auto junk = geometry::GeometryAnalyzer().analyze("this is not a brep");
    assert(junk.confidence == 0.0 && !junk.analysis_notes.empty());
    std::printf("PASS solid bar and invalid B-rep\n");
}

int main() {
    occt::install_geometry_backend();
    assert(geometry::has_geometry_backend());
    test_straight_tube();
    test_bent_tube();
    test_solid_bar_and_garbage();
    std::printf("All OCCT geometry analyzer tests passed\n");
    return 0;
}
