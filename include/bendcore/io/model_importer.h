#pragma once

#include <string>
#include <vector>
#include <memory>

namespace bendcore::io {

/**
 * 3D Model structure
 */
struct Mesh {
    struct Vertex {
        float x, y, z;
        Vertex() : x(0), y(0), z(0) {}
        Vertex(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    };

    struct Normal {
        float x, y, z;
        Normal() : x(0), y(0), z(0) {}
    };

    struct Face {
        unsigned int v1, v2, v3;  // vertex indices
        Face() : v1(0), v2(0), v3(0) {}
        Face(unsigned int v1_, unsigned int v2_, unsigned int v3_)
            : v1(v1_), v2(v2_), v3(v3_) {}
    };

    std::string name;
    std::vector<Vertex> vertices;
    std::vector<Normal> normals;
    std::vector<Face> faces;

    size_t vertex_count() const { return vertices.size(); }
    size_t face_count() const { return faces.size(); }
};

struct Model3D {
    std::string filename;
    std::string format;
    std::vector<Mesh> meshes;

    size_t mesh_count() const { return meshes.size(); }
    size_t total_vertices() const;
    size_t total_faces() const;
};

/**
 * 3D Model Import wrapper around Assimp library
 * Supports FBX, DAE, X3D, OBJ, GLTF, STL and many other formats
 */
class ModelImporter {
public:
    /**
     * Load model from file
     * Supports: FBX, DAE, X3D, OBJ, GLTF, GLTF2, GLB, STL, 3DS, IFC, Collada, STEP, IGES
     */
    static std::unique_ptr<Model3D> load_model(const std::string& filepath);

    /**
     * Load model with specific post-processing flags
     */
    static std::unique_ptr<Model3D> load_model_with_options(
        const std::string& filepath,
        bool triangulate = true,
        bool remove_duplicates = true,
        bool generate_normals = true);

    /**
     * Get list of supported import formats
     */
    static std::vector<std::string> supported_formats();

    /**
     * Check if format is supported
     */
    static bool is_format_supported(const std::string& format);

    /**
     * Validate model file
     */
    static bool validate_model_file(const std::string& filepath);

    /**
     * Get model info without full loading
     */
    static std::string get_model_info(const std::string& filepath);

    /**
     * Convert model between formats (FBX -> STL, etc.)
     * Note: Assimp supports import of many formats but export is limited
     */
    static bool convert_model(
        const std::string& input_file,
        const std::string& output_file);

    /**
     * Scale model to specific size
     */
    static void scale_model(Model3D& model, float scale_factor);

    /**
     * Transform model by translation
     */
    static void translate_model(Model3D& model, float dx, float dy, float dz);

    /**
     * Generate bounding box for model
     */
    static void get_bounds(
        const Model3D& model,
        float& min_x, float& max_x,
        float& min_y, float& max_y,
        float& min_z, float& max_z);

    /**
     * Extract vertex data in flat array format
     */
    static std::vector<float> get_vertex_data(const Model3D& model);

    /**
     * Extract face indices in flat array format
     */
    static std::vector<unsigned int> get_face_indices(const Model3D& model);

    /**
     * Calculate mesh statistics
     */
    static void calculate_statistics(const Model3D& model,
        float& avg_face_area,
        float& min_face_area,
        float& max_face_area);

    /**
     * Inspect model structure
     */
    static void print_model_structure(const Model3D& model);
};

} // namespace bendcore::io
