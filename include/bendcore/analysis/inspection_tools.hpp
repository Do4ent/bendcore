#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Distance measurement between geometric entities
 */
class DistanceMeasurement {
public:
    /**
     * @brief Measurement type
     */
    enum class MeasurementType {
        POINT_TO_POINT,
        POINT_TO_LINE,
        POINT_TO_PLANE,
        LINE_TO_LINE,
        PLANE_TO_PLANE,
        SURFACE_TO_SURFACE,
    };

    /**
     * @brief Constructor
     */
    DistanceMeasurement(const EntityId& entity1_id, const EntityId& entity2_id)
        : entity1_id_(entity1_id), entity2_id_(entity2_id), distance_(0.0) {}

    /**
     * @brief Set measurement positions
     */
    void setPositions(const Vector3D& pos1, const Vector3D& pos2) noexcept {
        pos1_ = pos1;
        pos2_ = pos2;

        // Calculate Euclidean distance
        double dx = pos2.x - pos1.x;
        double dy = pos2.y - pos1.y;
        double dz = pos2.z - pos1.z;
        distance_ = std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    /**
     * @brief Get measurement type
     */
    [[nodiscard]] MeasurementType getMeasurementType() const noexcept { return type_; }

    /**
     * @brief Get measured distance
     */
    [[nodiscard]] double getDistance() const noexcept { return distance_; }

    /**
     * @brief Get closest points
     */
    [[nodiscard]] std::pair<Vector3D, Vector3D> getClosestPoints() const noexcept {
        return {pos1_, pos2_};
    }

    /**
     * @brief Check if distance within tolerance
     */
    [[nodiscard]] bool isWithinTolerance(double tolerance) const noexcept {
        return distance_ <= tolerance;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    MeasurementType type_ = MeasurementType::POINT_TO_POINT;
    Vector3D pos1_;
    Vector3D pos2_;
    double distance_;
};

/**
 * @brief Angle measurement between geometric entities
 */
class AngleMeasurement {
public:
    /**
     * @brief Constructor
     */
    AngleMeasurement(const EntityId& entity1_id, const EntityId& entity2_id)
        : entity1_id_(entity1_id), entity2_id_(entity2_id), angle_degrees_(0.0) {}

    /**
     * @brief Set vectors for angle calculation
     */
    void setVectors(const Vector3D& vec1, const Vector3D& vec2) noexcept {
        // Normalize vectors
        double mag1 = std::sqrt(vec1.x*vec1.x + vec1.y*vec1.y + vec1.z*vec1.z);
        double mag2 = std::sqrt(vec2.x*vec2.x + vec2.y*vec2.y + vec2.z*vec2.z);

        if (mag1 < 1e-9 || mag2 < 1e-9) {
            angle_degrees_ = 0.0;
            return;
        }

        Vector3D v1(vec1.x/mag1, vec1.y/mag1, vec1.z/mag1);
        Vector3D v2(vec2.x/mag2, vec2.y/mag2, vec2.z/mag2);

        // Dot product
        double dot = v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
        dot = std::max(-1.0, std::min(1.0, dot));  // Clamp to [-1, 1]

        double angle_rad = std::acos(dot);
        angle_degrees_ = angle_rad * 180.0 / M_PI;
    }

    /**
     * @brief Get angle in degrees
     */
    [[nodiscard]] double getAngleDegrees() const noexcept { return angle_degrees_; }

    /**
     * @brief Get angle in radians
     */
    [[nodiscard]] double getAngleRadians() const noexcept {
        return angle_degrees_ * M_PI / 180.0;
    }

    /**
     * @brief Check if angle within tolerance
     */
    [[nodiscard]] bool isWithinTolerance(double tolerance_degrees) const noexcept {
        return std::abs(angle_degrees_) <= tolerance_degrees;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    double angle_degrees_;
};

/**
 * @brief Curvature analysis of curves and surfaces
 */
class CurvatureAnalysis {
public:
    /**
     * @brief Curvature type
     */
    enum class CurvatureType {
        GAUSSIAN,     // Gaussian curvature (surfaces)
        MEAN,         // Mean curvature (surfaces)
        PRINCIPAL_1,  // First principal curvature
        PRINCIPAL_2,  // Second principal curvature
    };

    /**
     * @brief Constructor
     */
    CurvatureAnalysis(const EntityId& geometry_id) : geometry_id_(geometry_id) {}

    /**
     * @brief Add curvature sample
     */
    void addSample(double parameter, double curvature_value) noexcept {
        samples_.push_back({parameter, curvature_value});
    }

    /**
     * @brief Get minimum curvature
     */
    [[nodiscard]] double getMinCurvature() const noexcept;

    /**
     * @brief Get maximum curvature
     */
    [[nodiscard]] double getMaxCurvature() const noexcept;

    /**
     * @brief Get average curvature
     */
    [[nodiscard]] double getAverageCurvature() const noexcept;

    /**
     * @brief Get curvature continuity (C0, C1, C2)
     */
    [[nodiscard]] std::string analyzeContinuity() const noexcept;

    /**
     * @brief Get all samples
     */
    [[nodiscard]] const std::vector<std::pair<double, double>>& getSamples() const noexcept {
        return samples_;
    }

private:
    EntityId geometry_id_;
    std::vector<std::pair<double, double>> samples_;  // parameter, curvature
};

/**
 * @brief Surface quality assessment
 */
class SurfaceQuality {
public:
    /**
     * @brief Quality metric
     */
    enum class QualityMetric {
        SMOOTHNESS,      // Surface smoothness
        FLATNESS,        // Deviation from plane
        CYLINDRICITY,    // Deviation from cylinder
        ROUGHNESS,       // Surface texture
        CONTINUITY,      // G0, G1, G2 continuity
    };

    /**
     * @brief Constructor
     */
    SurfaceQuality(const EntityId& surface_id) : surface_id_(surface_id) {}

    /**
     * @brief Evaluate surface quality
     */
    [[nodiscard]] bool evaluateQuality() noexcept;

    /**
     * @brief Get quality score (0-100)
     */
    [[nodiscard]] int getQualityScore() const noexcept { return quality_score_; }

    /**
     * @brief Get quality assessment
     */
    [[nodiscard]] std::string getAssessment() const noexcept;

    /**
     * @brief Get defect areas
     */
    [[nodiscard]] const std::vector<Vector3D>& getDefectLocations() const noexcept {
        return defect_locations_;
    }

    /**
     * @brief Add defect location
     */
    void addDefect(const Vector3D& location) noexcept {
        defect_locations_.push_back(location);
    }

private:
    EntityId surface_id_;
    int quality_score_ = 0;  // 0-100
    std::vector<Vector3D> defect_locations_;
};

/**
 * @brief Tolerance verification
 */
class ToleranceChecker {
public:
    /**
     * @brief Tolerance type
     */
    enum class ToleranceType {
        DIMENSIONAL,   // Linear dimension tolerance
        GEOMETRIC,     // Geometric tolerance (GD&T)
        POSITIONAL,    // Position tolerance
        RUNOUT,        // Runout tolerance
    };

    /**
     * @brief Tolerance specification
     */
    struct ToleranceSpec {
        ToleranceType type;
        double nominal_value;
        double upper_limit;
        double lower_limit;
        std::string description;
    };

    /**
     * @brief Constructor
     */
    ToleranceChecker(const EntityId& feature_id) : feature_id_(feature_id) {}

    /**
     * @brief Add tolerance specification
     */
    void addTolerance(const ToleranceSpec& spec) noexcept {
        tolerances_.push_back(spec);
    }

    /**
     * @brief Check if measurement passes tolerance
     */
    [[nodiscard]] bool checkTolerance(double measured_value) noexcept;

    /**
     * @brief Get tolerance violations
     */
    [[nodiscard]] const std::vector<std::string>& getViolations() const noexcept {
        return violations_;
    }

    /**
     * @brief Get compliance status
     */
    [[nodiscard]] bool isCompliant() const noexcept { return violations_.empty(); }

private:
    EntityId feature_id_;
    std::vector<ToleranceSpec> tolerances_;
    std::vector<std::string> violations_;
};

/**
 * @brief Geometry validation and repair
 */
class GeometryValidator {
public:
    /**
     * @brief Validation issue
     */
    struct ValidationIssue {
        std::string description;
        EntityId entity_id;
        int severity;  // 1-5, where 5 is critical
        std::string suggestion;
    };

    /**
     * @brief Constructor
     */
    GeometryValidator(const EntityId& assembly_id) : assembly_id_(assembly_id) {}

    /**
     * @brief Validate geometry
     */
    [[nodiscard]] bool validate() noexcept;

    /**
     * @brief Get validation issues
     */
    [[nodiscard]] const std::vector<ValidationIssue>& getIssues() const noexcept {
        return issues_;
    }

    /**
     * @brief Check for self-intersections
     */
    [[nodiscard]] bool hasSelfIntersections() const noexcept {
        return self_intersecting_;
    }

    /**
     * @brief Check for gaps/overlaps
     */
    [[nodiscard]] bool hasGapsOrOverlaps() const noexcept {
        return has_gaps_overlaps_;
    }

    /**
     * @brief Attempt automatic repair
     */
    [[nodiscard]] bool autoRepair() noexcept;

    /**
     * @brief Get repair log
     */
    [[nodiscard]] const std::vector<std::string>& getRepairLog() const noexcept {
        return repair_log_;
    }

private:
    EntityId assembly_id_;
    std::vector<ValidationIssue> issues_;
    bool self_intersecting_ = false;
    bool has_gaps_overlaps_ = false;
    std::vector<std::string> repair_log_;
};

/**
 * @brief Inspection tools coordinator
 */
class InspectionTools {
public:
    /**
     * @brief Constructor
     */
    InspectionTools(const EntityId& assembly_id)
        : assembly_id_(assembly_id), validator_(assembly_id) {}

    /**
     * @brief Create distance measurement
     */
    [[nodiscard]] std::shared_ptr<DistanceMeasurement> measureDistance(
        const EntityId& entity1, const EntityId& entity2) noexcept;

    /**
     * @brief Create angle measurement
     */
    [[nodiscard]] std::shared_ptr<AngleMeasurement> measureAngle(
        const EntityId& entity1, const EntityId& entity2) noexcept;

    /**
     * @brief Analyze curvature
     */
    [[nodiscard]] std::shared_ptr<CurvatureAnalysis> analyzeCurvature(
        const EntityId& geometry_id) noexcept;

    /**
     * @brief Assess surface quality
     */
    [[nodiscard]] std::shared_ptr<SurfaceQuality> assessSurfaceQuality(
        const EntityId& surface_id) noexcept;

    /**
     * @brief Create tolerance checker
     */
    [[nodiscard]] std::shared_ptr<ToleranceChecker> createToleranceChecker(
        const EntityId& feature_id) noexcept;

    /**
     * @brief Get geometry validator
     */
    [[nodiscard]] GeometryValidator& getValidator() noexcept {
        return validator_;
    }

    /**
     * @brief Get measurement count
     */
    [[nodiscard]] size_t getMeasurementCount() const noexcept {
        return distances_.size() + angles_.size();
    }

    /**
     * @brief Get analysis count
     */
    [[nodiscard]] size_t getAnalysisCount() const noexcept {
        return curvatures_.size() + surfaces_.size();
    }

private:
    EntityId assembly_id_;
    std::vector<std::shared_ptr<DistanceMeasurement>> distances_;
    std::vector<std::shared_ptr<AngleMeasurement>> angles_;
    std::vector<std::shared_ptr<CurvatureAnalysis>> curvatures_;
    std::vector<std::shared_ptr<SurfaceQuality>> surfaces_;
    std::vector<std::shared_ptr<ToleranceChecker>> tolerances_;
    GeometryValidator validator_;
};

} // namespace BendCore::CadCore
