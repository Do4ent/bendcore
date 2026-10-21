// Integration tests for import coordinator (Points 2, 3, 4)
#include "bendcore/dwfx/import_coordinator.hpp"

#include <cstdio>
#include <cmath>
#include <vector>
#include <array>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

// Create a simple cylindrical mesh (tube along Z axis, diameter 20mm, length 80mm)
static std::vector<Vec3d> create_tube_vertices() {
    std::vector<Vec3d> vertices;
    const double radius = 10.0;  // 20mm diameter
    const int circumference_points = 12;
    const int length_points = 5;  // 5 rings along Z

    for (int z_idx = 0; z_idx < length_points; ++z_idx) {
        const double z = z_idx * 20.0;

        for (int angle_idx = 0; angle_idx < circumference_points; ++angle_idx) {
            const double angle = 2.0 * M_PI * angle_idx / circumference_points;
            const double x = radius * std::cos(angle);
            const double y = radius * std::sin(angle);
            vertices.push_back({x, y, z});
        }
    }

    return vertices;
}

static void test_basic_import() {
    const auto vertices = create_tube_vertices();
    std::vector<std::array<std::int32_t, 3>> faces;  // Empty faces
    std::vector<Vec3d> env_features;                 // No environment features for this test

    ImportCoordinatorSettings settings;

    const auto result = import_tube_from_mesh(
        "test_tube",
        vertices,
        faces,
        env_features,
        20.0,  // OD
        1.0,   // Wall thickness
        settings
    );

    check(result.success, "import succeeded");
    check(result.tube_model.has_value(), "TubeModel created");
    check(result.mesh_sampling_status == "OK", "mesh sampling OK");
    check(result.geometry_rounding_status == "OK", "geometry rounding OK");
}

static void test_tube_model_properties() {
    const auto vertices = create_tube_vertices();
    std::vector<std::array<std::int32_t, 3>> faces;
    std::vector<Vec3d> env_features;

    ImportCoordinatorSettings settings;

    const auto result = import_tube_from_mesh(
        "my_tube",
        vertices,
        faces,
        env_features,
        20.0,
        1.0,
        settings
    );

    if (result.success && result.tube_model) {
        const auto& model = result.tube_model.value();
        check(model.id == "my_tube", "tube ID preserved");
        check(model.od == 20.0, "OD preserved");
        check(model.wall == 1.0, "wall thickness preserved");
        check(model.source == "dwfx", "source is dwfx");
        check(model.path.has_value(), "path created from samples");
    }
}

static void test_empty_mesh() {
    std::vector<Vec3d> empty_vertices;
    std::vector<std::array<std::int32_t, 3>> faces;
    std::vector<Vec3d> env_features;

    ImportCoordinatorSettings settings;

    const auto result = import_tube_from_mesh(
        "empty",
        empty_vertices,
        faces,
        env_features,
        20.0,
        1.0,
        settings
    );

    check(!result.success, "import fails for empty mesh");
    check(!result.tube_model.has_value(), "no TubeModel for empty mesh");
}

static void test_rounding_tolerance() {
    const auto vertices = create_tube_vertices();
    std::vector<std::array<std::int32_t, 3>> faces;
    std::vector<Vec3d> env_features;

    ImportCoordinatorSettings settings;
    settings.max_deviation_mm = 0.1;  // Very strict tolerance

    const auto result = import_tube_from_mesh(
        "strict",
        vertices,
        faces,
        env_features,
        20.0,
        1.0,
        settings
    );

    // Should still succeed, even if tolerance is exceeded (warning, not error)
    check(result.success, "import succeeds even with strict tolerance");
}

int main() {
    std::printf("Integration tests for DWFx import coordinator\n");
    std::printf("Points 2 (Geometry Rounding), 3 (Autoport Detection), 4 (Mesh Sampling)\n\n");

    test_basic_import();
    test_tube_model_properties();
    test_empty_mesh();
    test_rounding_tolerance();

    if (failures == 0) {
        std::printf("OK import_coordinator\n");
        return 0;
    } else {
        std::printf("%d failures\n", failures);
        return 1;
    }
}
