#pragma once

#include <memory>
#include <vector>

#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/curve_operations.hpp"
#include "bendcore/surfaces/nurbs_surface.hpp"

namespace bendcore::surfaces {

/**
 * M5 Phase - Surface Modeling (GAP-021)
 * Specialized Surface Types
 */

/**
 * BlendSurface — Smooth transition between two surfaces
 * Creates a smooth blending surface between two boundary curves
 */
class BlendSurface : public NURBSSurface {
public:
    BlendSurface();

    /**
     * Create blend surface between two curves
     * @param boundary_curve1 First boundary curve
     * @param boundary_curve2 Second boundary curve
     * @param tangent_curve1 Optional tangent direction at curve 1
     * @param tangent_curve2 Optional tangent direction at curve 2
     */
    static std::unique_ptr<NURBSSurface> create_blend(
        const geometry::NurbsCurve& boundary_curve1,
        const geometry::NurbsCurve& boundary_curve2,
        const geometry::Vector3D& tangent_curve1,
        const geometry::Vector3D& tangent_curve2
    );

    [[nodiscard]] bool is_blend_surface() const { return true; }
};

/**
 * OffsetSurface — Parallel surface at specified distance
 * Creates a surface parallel to the original at a given distance
 */
class OffsetSurface {
public:
    OffsetSurface(const std::shared_ptr<NURBSSurface>& base_surface, double offset_distance);

    [[nodiscard]] geometry::Point3D evaluate(double u, double v) const;
    [[nodiscard]] geometry::Vector3D normal(double u, double v) const;

    [[nodiscard]] const std::shared_ptr<NURBSSurface>& base_surface() const { return base_surface_; }
    [[nodiscard]] double offset_distance() const { return offset_distance_; }

    /**
     * Convert to explicit NURBS representation (for export/operations)
     */
    [[nodiscard]] std::unique_ptr<NURBSSurface> to_nurbs(int samples_u = 10, int samples_v = 10) const;

private:
    std::shared_ptr<NURBSSurface> base_surface_;
    double offset_distance_;
};

/**
 * ExtrudedSurface — Surface created by extruding a curve
 * Generates a surface by translating a curve along an extrusion direction
 */
class ExtrudedSurface : public NURBSSurface {
public:
    ExtrudedSurface();

    /**
     * Create extruded surface from a profile curve
     * @param profile The curve to extrude
     * @param direction The extrusion direction vector
     * @param distance The extrusion distance (can be negative)
     */
    static std::unique_ptr<NURBSSurface> create_extrusion(
        const geometry::NurbsCurve& profile,
        const geometry::Vector3D& direction,
        double distance
    );

    [[nodiscard]] const geometry::NurbsCurve* profile() const { return profile_.get(); }
    [[nodiscard]] geometry::Vector3D extrusion_direction() const { return extrusion_direction_; }
    [[nodiscard]] double extrusion_distance() const { return extrusion_distance_; }

private:
    std::unique_ptr<geometry::NurbsCurve> profile_;
    geometry::Vector3D extrusion_direction_;
    double extrusion_distance_ = 0.0;
};

/**
 * RuledSurface — Surface defined by ruling lines between two curves
 * Creates a surface where straight lines connect corresponding points on two curves
 */
class RuledSurface : public NURBSSurface {
public:
    RuledSurface();

    /**
     * Create ruled surface between two curves
     * @param curve1 First boundary curve
     * @param curve2 Second boundary curve
     * @param straightness_factor 0=linear interpolation, >0=curved ruling
     */
    static std::unique_ptr<NURBSSurface> create_ruled(
        const geometry::NurbsCurve& curve1,
        const geometry::NurbsCurve& curve2,
        double straightness_factor = 0.0
    );

    [[nodiscard]] const geometry::NurbsCurve* curve1() const { return curve1_.get(); }
    [[nodiscard]] const geometry::NurbsCurve* curve2() const { return curve2_.get(); }

private:
    std::unique_ptr<geometry::NurbsCurve> curve1_;
    std::unique_ptr<geometry::NurbsCurve> curve2_;
};

/**
 * FilletSurface — Surface fillet between two surfaces
 * Creates a smooth fillet surface between two adjacent surfaces
 */
class FilletSurface {
public:
    FilletSurface(
        const std::shared_ptr<NURBSSurface>& surface1,
        const std::shared_ptr<NURBSSurface>& surface2,
        double radius,
        bool is_convex = true
    );

    [[nodiscard]] geometry::Point3D evaluate(double u, double v) const;
    [[nodiscard]] geometry::Vector3D normal(double u, double v) const;
    [[nodiscard]] double radius() const { return radius_; }
    [[nodiscard]] bool is_convex() const { return is_convex_; }

    /**
     * Convert to explicit NURBS representation
     */
    [[nodiscard]] std::unique_ptr<NURBSSurface> to_nurbs(int samples_u = 10, int samples_v = 10) const;

private:
    std::shared_ptr<NURBSSurface> surface1_;
    std::shared_ptr<NURBSSurface> surface2_;
    double radius_;
    bool is_convex_;
};

/**
 * SewedSurface — Multi-surface body with continuity constraints
 * Represents multiple surfaces joined together with defined continuity
 */
class SewedSurface {
public:
    SewedSurface();

    /**
     * Add a surface to the sewed body
     * @param surface Surface to add
     * @param continuity Required continuity with adjacent surfaces
     */
    void add_surface(
        const std::shared_ptr<NURBSSurface>& surface,
        ContinuityLevel continuity = ContinuityLevel::G0
    );

    /**
     * Get all surfaces in the sewed body
     */
    [[nodiscard]] const std::vector<std::shared_ptr<NURBSSurface>>& surfaces() const {
        return surfaces_;
    }

    /**
     * Evaluate surface at parameter (automatically selects correct surface)
     */
    [[nodiscard]] geometry::Point3D evaluate(double u, double v) const;

    /**
     * Check continuity between adjacent surfaces
     */
    [[nodiscard]] ContinuityAnalysis check_continuity(int surface_index1, int surface_index2) const;

    /**
     * Get all continuity issues
     */
    [[nodiscard]] std::vector<ContinuityAnalysis> analyze_all_continuities() const;

    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] int surface_count() const { return surfaces_.size(); }

private:
    std::vector<std::shared_ptr<NURBSSurface>> surfaces_;
    std::vector<ContinuityLevel> continuities_;
};

} // namespace bendcore::surfaces
