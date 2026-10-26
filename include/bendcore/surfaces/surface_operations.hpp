#pragma once

#include <memory>
#include <vector>
#include <optional>

#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/curve_operations.hpp"
#include "bendcore/surfaces/nurbs_surface.hpp"

namespace bendcore::surfaces {

/**
 * M5 Phase - Surface Modeling (GAP-021)
 * Surface Operations and Analysis
 */

/**
 * Surface Intersection Result
 */
struct SurfaceIntersection {
    std::vector<std::vector<geometry::Point3D>> intersection_curves;  // Multiple intersection curves possible
    std::vector<std::pair<double, double>> curve_parameters;          // (u, v) parameters along curves
    bool has_intersections = false;
    double max_intersection_error = 0.0;
};

/**
 * Surface Operations - Core geometric operations
 */
class SurfaceOperations {
public:
    /**
     * Compute surface-surface intersection
     */
    static std::unique_ptr<SurfaceIntersection> intersect_surfaces(
        const NURBSSurface& surface1,
        const NURBSSurface& surface2,
        double tolerance = 1e-6,
        int max_iterations = 100
    );

    /**
     * Compute surface-curve intersection
     */
    static std::vector<geometry::Point3D> intersect_surface_curve(
        const NURBSSurface& surface,
        const geometry::NurbsCurve& curve,
        double tolerance = 1e-6,
        int samples = 50
    );

    /**
     * Trim surface to a closed 2D region in parameter space
     * @param surface Surface to trim
     * @param trim_boundary Closed boundary curve in parameter space
     */
    static std::unique_ptr<NURBSSurface> trim_surface(
        const NURBSSurface& surface,
        const std::vector<geometry::Point3D>& trim_boundary  // (u, v) points
    );

    /**
     * Extend surface beyond current parameter range
     * @param surface Surface to extend
     * @param extension_length How far to extend
     * @param direction Which edge to extend from (u_min=0, u_max=1, v_min=2, v_max=3)
     */
    static std::unique_ptr<NURBSSurface> extend_surface(
        const NURBSSurface& surface,
        double extension_length,
        int direction
    );

    /**
     * Merge (sew) two surfaces together with continuity constraints
     */
    static std::unique_ptr<NURBSSurface> merge_surfaces(
        const NURBSSurface& surface1,
        const NURBSSurface& surface2,
        ContinuityLevel continuity = ContinuityLevel::G1,
        double tolerance = 1e-6
    );

    /**
     * Extract boundary curves from surface
     */
    static std::vector<geometry::NurbsCurve> extract_boundaries(const NURBSSurface& surface);

    /**
     * Create a closed surface from boundary curves
     */
    static std::unique_ptr<NURBSSurface> create_from_boundary_curves(
        const std::vector<geometry::NurbsCurve>& boundary_curves
    );
};

/**
 * Surface Quality Analysis - Evaluate surface quality metrics
 */
class SurfaceQualityAnalyzer {
public:
    /**
     * Check surface continuity with neighboring surface
     */
    static ContinuityAnalysis check_continuity(
        const NURBSSurface& surface1,
        const NURBSSurface& surface2,
        const std::vector<geometry::Point3D>& shared_boundary,
        double tolerance = 1e-6
    );

    /**
     * Analyze curvature distribution
     */
    struct CurvatureAnalysis {
        double max_gaussian = 0.0;
        double min_gaussian = 0.0;
        double mean_gaussian = 0.0;
        double max_mean = 0.0;
        double min_mean = 0.0;
        double mean_mean = 0.0;
        std::vector<double> gaussian_samples;
        std::vector<double> mean_samples;
    };

    static CurvatureAnalysis analyze_curvature(
        const NURBSSurface& surface,
        int samples_u = 10,
        int samples_v = 10
    );

    /**
     * Analyze surface smoothness
     */
    struct SmoothessAnalysis {
        double smoothness_score = 0.0;  // 0.0-1.0, 1.0 = perfect smoothness
        std::vector<double> curvature_variations;
        double max_variation = 0.0;
        std::string issues;
    };

    static SmoothessAnalysis analyze_smoothness(
        const NURBSSurface& surface,
        int samples_u = 10,
        int samples_v = 10
    );

    /**
     * Detect gaps between surfaces
     */
    struct GapAnalysis {
        std::vector<geometry::Point3D> gap_locations;
        std::vector<double> gap_distances;
        double max_gap = 0.0;
        bool has_gaps = false;
    };

    static GapAnalysis detect_gaps(
        const NURBSSurface& surface1,
        const NURBSSurface& surface2,
        const std::vector<geometry::Point3D>& test_points
    );

    /**
     * Detect self-intersections
     */
    struct SelfIntersectionAnalysis {
        std::vector<geometry::Point3D> intersection_points;
        std::vector<std::pair<std::pair<double, double>, std::pair<double, double>>> parameters;  // ((u1,v1), (u2,v2))
        bool has_self_intersections = false;
        int intersection_count = 0;
    };

    static SelfIntersectionAnalysis detect_self_intersections(
        const NURBSSurface& surface,
        int samples_u = 20,
        int samples_v = 20
    );

    /**
     * Validate surface integrity
     */
    struct SurfaceValidation {
        bool is_valid = true;
        std::vector<std::string> issues;
        std::vector<std::string> warnings;
        bool is_closed = false;
        bool is_manifold = true;
    };

    static SurfaceValidation validate_surface(const NURBSSurface& surface);
};

/**
 * Surface Editing Operations
 */
class SurfaceEditor {
public:
    /**
     * Modify control points
     */
    static void update_control_points(
        NURBSSurface& surface,
        const std::vector<std::vector<geometry::Point3D>>& new_points
    );

    /**
     * Adjust knot vectors
     */
    static void adjust_knot_vector_u(
        NURBSSurface& surface,
        const std::vector<double>& new_knots
    );

    static void adjust_knot_vector_v(
        NURBSSurface& surface,
        const std::vector<double>& new_knots
    );

    /**
     * Change surface degree (elevation)
     */
    static std::unique_ptr<NURBSSurface> elevate_degree(
        const NURBSSurface& surface,
        int new_u_degree,
        int new_v_degree
    );

    /**
     * Rebuild surface with new parameters
     */
    static std::unique_ptr<NURBSSurface> rebuild(
        const NURBSSurface& surface,
        int new_u_degree,
        int new_v_degree,
        int u_control_points,
        int v_control_points
    );

    /**
     * Fix discontinuities
     */
    static std::unique_ptr<NURBSSurface> fix_discontinuities(
        const NURBSSurface& surface,
        double tolerance = 1e-6
    );

    /**
     * Smooth surface (remove oscillations)
     */
    static std::unique_ptr<NURBSSurface> smooth(
        const NURBSSurface& surface,
        int iterations = 3,
        double smoothing_factor = 0.3
    );

    /**
     * Create offset surface (parallel at distance)
     */
    static std::unique_ptr<NURBSSurface> create_offset(
        const NURBSSurface& surface,
        double distance,
        bool invert_normal = false
    );
};

} // namespace bendcore::surfaces
