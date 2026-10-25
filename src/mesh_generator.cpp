#include <bendcore/geometry/mesh_generator.h>
#include <cmath>
#include <algorithm>

namespace bendcore {

void Mesh::computeNormals() {
    // Initialize normals to zero
    std::vector<std::array<float, 3>> faceNormals(triangles.size());
    for (auto& v : vertices) {
        v.nx = v.ny = v.nz = 0.0f;
    }

    // Compute face normals and accumulate to vertices
    for (size_t i = 0; i < triangles.size(); ++i) {
        const auto& tri = triangles[i];
        const auto& v1 = vertices[tri.v1];
        const auto& v2 = vertices[tri.v2];
        const auto& v3 = vertices[tri.v3];

        // Edge vectors
        float e1x = v2.x - v1.x, e1y = v2.y - v1.y, e1z = v2.z - v1.z;
        float e2x = v3.x - v1.x, e2y = v3.y - v1.y, e2z = v3.z - v1.z;

        // Cross product
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;

        // Normalize
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0) {
            nx /= len;
            ny /= len;
            nz /= len;
        }

        // Accumulate to vertices
        vertices[tri.v1].nx += nx;
        vertices[tri.v1].ny += ny;
        vertices[tri.v1].nz += nz;

        vertices[tri.v2].nx += nx;
        vertices[tri.v2].ny += ny;
        vertices[tri.v2].nz += nz;

        vertices[tri.v3].nx += nx;
        vertices[tri.v3].ny += ny;
        vertices[tri.v3].nz += nz;
    }

    // Normalize vertex normals
    for (auto& v : vertices) {
        float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        if (len > 0) {
            v.nx /= len;
            v.ny /= len;
            v.nz /= len;
        }
    }
}

void Mesh::optimize() {
    // Simple optimization: remove duplicate vertices
    std::vector<uint32_t> remap(vertices.size());
    std::vector<Vertex> optimizedVertices;

    for (size_t i = 0; i < vertices.size(); ++i) {
        bool found = false;
        for (size_t j = 0; j < optimizedVertices.size(); ++j) {
            const auto& v1 = vertices[i];
            const auto& v2 = optimizedVertices[j];

            float dx = v1.x - v2.x;
            float dy = v1.y - v2.y;
            float dz = v1.z - v2.z;

            if (dx * dx + dy * dy + dz * dz < 1e-6f) {
                remap[i] = j;
                found = true;
                break;
            }
        }

        if (!found) {
            remap[i] = optimizedVertices.size();
            optimizedVertices.push_back(vertices[i]);
        }
    }

    vertices = optimizedVertices;

    // Remap triangle indices
    for (auto& tri : triangles) {
        tri.v1 = remap[tri.v1];
        tri.v2 = remap[tri.v2];
        tri.v3 = remap[tri.v3];
    }
}

std::shared_ptr<Mesh> MeshGenerator::generateFromBox(
    float width, float height, float depth,
    [[maybe_unused]] Quality quality
) {
    auto mesh = std::make_shared<Mesh>();

    float w = width / 2.0f;
    float h = height / 2.0f;
    float d = depth / 2.0f;

    // Define 8 vertices of the box
    std::array<Vertex, 8> vertices = {{
        {-w, -h, -d, 0, 0, -1, 0, 0},
        {w, -h, -d, 0, 0, -1, 1, 0},
        {w, h, -d, 0, 0, -1, 1, 1},
        {-w, h, -d, 0, 0, -1, 0, 1},
        {-w, -h, d, 0, 0, 1, 0, 0},
        {w, -h, d, 0, 0, 1, 1, 0},
        {w, h, d, 0, 0, 1, 1, 1},
        {-w, h, d, 0, 0, 1, 0, 1},
    }};

    // Add vertices
    for (const auto& v : vertices) {
        mesh->addVertex(v);
    }

    // Define 12 triangles (2 per face)
    std::array<Triangle, 12> triangles = {{
        // Front face
        {0, 1, 2}, {0, 2, 3},
        // Back face
        {4, 6, 5}, {4, 7, 6},
        // Left face
        {4, 0, 3}, {4, 3, 7},
        // Right face
        {1, 5, 6}, {1, 6, 2},
        // Top face
        {3, 2, 6}, {3, 6, 7},
        // Bottom face
        {4, 5, 1}, {4, 1, 0},
    }};

    for (const auto& tri : triangles) {
        mesh->addTriangle(tri);
    }

    mesh->computeNormals();
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::generateFromSphere(
    float radius,
    [[maybe_unused]] Quality quality
) {
    auto mesh = std::make_shared<Mesh>();
    int subdivisions = getSubdivisions(quality);

    const float PI = 3.14159265359f;

    // Generate vertices using spherical coordinates
    for (int i = 0; i <= subdivisions; ++i) {
        float theta = PI * i / subdivisions;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int j = 0; j <= subdivisions * 2; ++j) {
            float phi = 2 * PI * j / (subdivisions * 2);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            Vertex v;
            v.x = radius * sinTheta * cosPhi;
            v.y = radius * cosTheta;
            v.z = radius * sinTheta * sinPhi;

            v.nx = sinTheta * cosPhi;
            v.ny = cosTheta;
            v.nz = sinTheta * sinPhi;

            v.u = j / (float)(subdivisions * 2);
            v.v = i / (float)subdivisions;

            mesh->addVertex(v);
        }
    }

    // Generate triangles
    int vertPerRow = subdivisions * 2 + 1;
    for (int i = 0; i < subdivisions; ++i) {
        for (int j = 0; j < subdivisions * 2; ++j) {
            int v1 = i * vertPerRow + j;
            int v2 = v1 + 1;
            int v3 = v1 + vertPerRow;
            int v4 = v3 + 1;

            mesh->addTriangle({(uint32_t)v1, (uint32_t)v3, (uint32_t)v2});
            mesh->addTriangle({(uint32_t)v2, (uint32_t)v3, (uint32_t)v4});
        }
    }

    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::generateFromCylinder(
    float radius, float height,
    [[maybe_unused]] Quality quality
) {
    auto mesh = std::make_shared<Mesh>();
    int subdivisions = getSubdivisions(quality);

    const float PI = 3.14159265359f;
    float h = height / 2.0f;

    // Top and bottom centers
    Vertex topCenter = {0, h, 0, 0, 1, 0, 0.5f, 0.5f};
    Vertex bottomCenter = {0, -h, 0, 0, -1, 0, 0.5f, 0.5f};

    mesh->addVertex(topCenter);
    mesh->addVertex(bottomCenter);

    // Side vertices
    for (int i = 0; i <= subdivisions; ++i) {
        float angle = 2 * PI * i / subdivisions;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);

        Vertex v_top = {x, h, z, x / radius, 0, z / radius, i / (float)subdivisions, 1.0f};
        Vertex v_bottom = {x, -h, z, x / radius, 0, z / radius, i / (float)subdivisions, 0.0f};

        mesh->addVertex(v_top);
        mesh->addVertex(v_bottom);
    }

    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::generateFromCone(
    [[maybe_unused]] float baseRadius, [[maybe_unused]] float height,
    [[maybe_unused]] Quality quality
) {
    auto mesh = std::make_shared<Mesh>();
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::generateFromTorus(
    [[maybe_unused]] float majorRadius, [[maybe_unused]] float minorRadius,
    [[maybe_unused]] Quality quality
) {
    auto mesh = std::make_shared<Mesh>();
    return mesh;
}

int MeshGenerator::getSubdivisions(Quality quality) {
    switch (quality) {
        case Quality::Low:
            return 8;
        case Quality::Medium:
            return 16;
        case Quality::High:
            return 32;
        case Quality::VeryHigh:
            return 64;
        default:
            return 16;
    }
}

} // namespace bendcore
