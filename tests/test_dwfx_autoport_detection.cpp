// Tests for automatic port detection (spec 9.2.2)
#include "bendcore/dwfx/autoport_detection.hpp"

#include <cstdio>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void test_coaxial_features() {
    const Vec3d tube_end = {0.0, 0.0, 0.0};
    const Vec3d tube_axis = {0.0, 0.0, 1.0};
    const double tube_od = 20.0;
    
    std::vector<CylindricalFeature> features;
    features.push_back({
        {0.0, 0.0, 10.0}, {0.0, 0.0, 1.0},
        20.0, false, "coaxial"
    });
    
    AutoportSettings settings;
    settings.linear_tolerance_mm = 0.5;
    settings.od_tolerance_min_mm = 0.5;
    
    const auto candidates = find_coaxial_features(tube_end, tube_axis, tube_od, 
                                                   features, settings);
    
    check(candidates.size() >= 1, "found at least one candidate");
}

static void test_no_features() {
    const Vec3d end_a = {0.0, 0.0, 0.0};
    const Vec3d end_b = {0.0, 0.0, 100.0};
    const Vec3d axis = {0.0, 0.0, 1.0};
    
    std::vector<CylindricalFeature> env;
    
    AutoportSettings settings;
    const auto ports = detect_ports(end_a, end_b, axis, 20.0, env, settings);
    
    check(ports.empty(), "no ports when no environment");
}

int main() {
    std::printf("Tests for autoport detection\n");
    
    test_coaxial_features();
    test_no_features();
    
    if (failures == 0) {
        std::printf("OK autoport_detection\n");
        return 0;
    } else {
        std::printf("%d failures\n", failures);
        return 1;
    }
}
