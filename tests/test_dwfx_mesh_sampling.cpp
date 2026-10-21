// Tests for mesh-derived tube samples (spec 9.10)
#include "bendcore/dwfx/mesh_sampling.hpp"

#include <cstdio>
#include <cmath>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

// Create a simple cylindrical mesh (tube along Z axis, diameter 20mm)
static std::vector<Vec3d> create_tube_vertices() {
    std::vector<Vec3d> vertices;
    const double radius = 10.0;  // 20mm diameter
    const int circumference_points = 12;
    const int length_points = 5;  // 5 rings along Z

    // Create rings of vertices
    for (int z_idx = 0; z_idx < length_points; ++z_idx) {
        const double z = z_idx * 20.0;  // 0, 20, 40, 60, 80 mm

        for (int angle_idx = 0; angle_idx < circumference_points; ++angle_idx) {
            const double angle = 2.0 * M_PI * angle_idx / circumference_points;
            const double x = radius * std::cos(angle);
            const double y = radius * std::sin(angle);
            vertices.push_back({x, y, z});
        }
    }

    return vertices;
}

static void test_tube_sampling() {
    const auto vertices = create_tube_vertices();

    MeshSamplingSettings settings;
    settings.min_samples = 3;
    settings.radius_tolerance_mm = 1.0;

    const auto result = sample_tube_from_mesh(vertices, {}, settings);

    check(!result.samples.empty(), "found samples");
    check(result.start_point.has_value(), "found start point");
    check(result.end_point.has_value(), "found end point");
    check(near(result.avg_diameter_mm, 20.0, 0.5), "average diameter is ~20mm");
    check(result.validation_status == "OK", "validation status OK");
}

static void test_tube_endpoints() {
    const auto vertices = create_tube_vertices();

    MeshSamplingSettings settings;
    const auto result = sample_tube_from_mesh(vertices, {}, settings);

    if (result.start_point && result.end_point) {
        const double z_start = result.start_point->z;
        const double z_end = result.end_point->z;
        const double z_length = std::abs(z_end - z_start);

        check(near(z_length, 80.0, 1.0), "tube length ~80mm");
    }
}

static void test_empty_mesh() {
    std::vector<Vec3d> empty;
    MeshSamplingSettings settings;

    const auto result = sample_tube_from_mesh(empty, {}, settings);

    check(result.samples.empty(), "no samples for empty mesh");
    check(result.validation_status == "insufficient_data", "insufficient data status");
}

static void test_sample_count() {
    const auto vertices = create_tube_vertices();

    MeshSamplingSettings settings;
    settings.min_samples = 5;

    const auto result = sample_tube_from_mesh(vertices, {}, settings);

    check(result.samples.size() >= 3, "at least 3 samples extracted");
}

int main() {
    std::printf("Tests for mesh-derived tube samples (spec 9.10)\n");

    test_tube_sampling();
    test_tube_endpoints();
    test_empty_mesh();
    test_sample_count();

    if (failures == 0) {
        std::printf("OK mesh_sampling\n");
        return 0;
    } else {
        std::printf("%d failures\n", failures);
        return 1;
    }
}
