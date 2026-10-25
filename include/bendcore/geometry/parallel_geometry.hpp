// CAD-CORE-029: ParallelGeometry - OpenMP Parallel Computations
// Phase H: Advanced Optimization for 10K+ Elements
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <tuple>

namespace bendcore::geometry {

/**
 * ParallelGeometry
 *
 * Provides OpenMP-accelerated geometry computations for large assemblies.
 * Handles:
 * - Parallel offset calculations (10K+ curves)
 * - Batch surface operations
 * - Concurrent mesh generation
 * - Stress analysis in parallel
 *
 * Requirements:
 * - OpenMP 4.5+
 * - C++20 std::chrono for timing
 * - Thread-safe data structures
 */

struct CurveData {
    std::vector<double> controlPoints;
    std::vector<double> knots;
    int degree;
    double tolerance;
    std::string curveId;
};

struct SurfaceData {
    std::vector<double> vertices;
    std::vector<int> faceIndices;
    std::vector<double> normals;
    std::string surfaceId;
};

struct OffsetResult {
    std::string curveId;
    std::vector<double> offsetPoints;
    double computeTimeMs;
    bool success;
    std::string errorMsg;
};

struct MeshGenerationResult {
    std::string geometryId;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int triangleCount;
    double computeTimeMs;
    bool success;
};

class ParallelGeometry {
public:
    ParallelGeometry();
    ~ParallelGeometry();

    // Set number of threads for OpenMP
    void setThreadCount(int numThreads);
    int getThreadCount() const;

    // Parallel offset computation for multiple curves
    std::vector<OffsetResult> computeParallelOffsets(
        const std::vector<CurveData>& curves,
        const std::vector<double>& offsets
    );

    // Single curve offset with timing
    OffsetResult computeOffset(
        const CurveData& curve,
        double offset
    );

    // Parallel surface normal computation
    void computeSurfaceNormalsParallel(
        std::vector<SurfaceData>& surfaces
    );

    // Parallel mesh generation from multiple geometries
    std::vector<MeshGenerationResult> generateMeshesParallel(
        const std::vector<std::string>& geometryIds,
        const std::vector<std::vector<double>>& vertexArrays,
        const std::vector<std::vector<int>>& indexArrays
    );

    // Parallel stress analysis computation
    struct StressResult {
        std::string componentId;
        double maxStress;
        double averageStress;
        std::vector<double> stressField;
        double computeTimeMs;
    };

    std::vector<StressResult> computeStressParallel(
        const std::vector<std::string>& componentIds,
        const std::vector<std::vector<double>>& geometries,
        const std::vector<double>& forces
    );

    // Batch bounding box calculation
    struct BoundingBox {
        double minX, minY, minZ;
        double maxX, maxY, maxZ;
        std::string componentId;
    };

    std::vector<BoundingBox> computeBoundingBoxesParallel(
        const std::vector<std::string>& componentIds,
        const std::vector<std::vector<double>>& geometries
    );

    // Parallel intersection testing (for 10K+ components)
    struct IntersectionTest {
        std::string id1;
        std::string id2;
        bool intersects;
        std::vector<double> intersectionPoints;
    };

    std::vector<IntersectionTest> testIntersectionsParallel(
        const std::vector<std::string>& geometryIds,
        const std::vector<std::vector<double>>& geometries
    );

    // Performance statistics
    struct Statistics {
        int64_t totalOperations;
        double totalComputeTimeMs;
        double averageTimePerOperation;
        double speedupFactor;  // vs single-threaded
        int activeThreads;
    };

    Statistics getStatistics() const;
    void resetStatistics();

private:
    int numThreads_;
    int64_t totalOperations_;
    double totalComputeTime_;

    // Helper methods
    OffsetResult computeOffsetInternal(const CurveData& curve, double offset);
    std::vector<double> computeNormalsForSurface(const SurfaceData& surface);
    MeshGenerationResult generateMeshInternal(
        const std::string& geometryId,
        const std::vector<double>& vertices,
        const std::vector<int>& indices
    );
    StressResult computeStressInternal(
        const std::string& componentId,
        const std::vector<double>& geometry,
        double force
    );
};

}  // namespace bendcore::geometry
