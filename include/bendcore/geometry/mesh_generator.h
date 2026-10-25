#pragma once

#include <vector>
#include <array>
#include <memory>

namespace bendcore {

struct Vertex {
    float x, y, z;
    float nx, ny, nz; // Normal
    float u, v;        // Texture coordinates
};

struct Triangle {
    uint32_t v1, v2, v3;
};

class Mesh {
public:
    Mesh() = default;

    void addVertex(const Vertex& v) { vertices.push_back(v); }
    void addTriangle(const Triangle& t) { triangles.push_back(t); }

    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<Triangle>& getTriangles() const { return triangles; }

    size_t getVertexCount() const { return vertices.size(); }
    size_t getTriangleCount() const { return triangles.size(); }

    void computeNormals();
    void optimize();

private:
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

class MeshGenerator {
public:
    enum class Quality {
        Low,
        Medium,
        High,
        VeryHigh
    };

    static std::shared_ptr<Mesh> generateFromBox(
        float width, float height, float depth,
        Quality quality = Quality::Medium
    );

    static std::shared_ptr<Mesh> generateFromSphere(
        float radius,
        Quality quality = Quality::Medium
    );

    static std::shared_ptr<Mesh> generateFromCylinder(
        float radius, float height,
        Quality quality = Quality::Medium
    );

    static std::shared_ptr<Mesh> generateFromCone(
        float baseRadius, float height,
        Quality quality = Quality::Medium
    );

    static std::shared_ptr<Mesh> generateFromTorus(
        float majorRadius, float minorRadius,
        Quality quality = Quality::Medium
    );

private:
    static int getSubdivisions(Quality quality);
};

} // namespace bendcore
