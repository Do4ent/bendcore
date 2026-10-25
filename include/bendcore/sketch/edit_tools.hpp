#pragma once

#include "primitives.hpp"
#include <vector>
#include <memory>
#include <optional>

namespace bendcore::sketch {

/**
 * @brief Edit operation base class
 */
class EditOperation {
public:
    virtual ~EditOperation() = default;
    virtual bool execute() = 0;
    virtual bool undo() = 0;
    virtual std::string getDescription() const = 0;
};

/**
 * @brief Trim tool - trim line/arc at intersection
 */
class TrimTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param geometry Geometry to trim
     * @param trim_point Point where to trim
     */
    TrimTool(std::shared_ptr<Line> geometry, const Point& trim_point);
    TrimTool(std::shared_ptr<Arc> geometry, const Point& trim_point);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Trim edge"; }

    /**
     * @brief Find closest intersection point
     */
    Point findIntersection(const Point& trim_point, double tolerance = 1e-6);

private:
    std::shared_ptr<Line> line_;
    std::shared_ptr<Arc> arc_;
    Point trim_point_;
    Point saved_start_, saved_end_;
    bool is_line_;
};

/**
 * @brief Extend tool - extend line to target geometry
 */
class ExtendTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param geometry Geometry to extend
     * @param target Target geometry to extend to
     */
    ExtendTool(std::shared_ptr<Line> geometry, const Line& target);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Extend edge"; }

    /**
     * @brief Calculate extension point
     */
    Point calculateExtension(const Line& geometry, const Line& target);

private:
    std::shared_ptr<Line> geometry_;
    Line target_;
    Point saved_end_;
};

/**
 * @brief Offset tool - offset line/circle/arc by distance
 */
class OffsetTool : public EditOperation {
public:
    /**
     * @brief Offset a line
     * @param geometry Line to offset
     * @param distance Offset distance
     */
    OffsetTool(std::shared_ptr<Line> geometry, double distance);

    /**
     * @brief Offset a circle
     * @param geometry Circle to offset
     * @param distance Offset distance (radius change)
     */
    OffsetTool(std::shared_ptr<Circle> geometry, double distance);

    /**
     * @brief Offset an arc
     * @param geometry Arc to offset
     * @param distance Offset distance (radius change)
     */
    OffsetTool(std::shared_ptr<Arc> geometry, double distance);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Offset"; }

private:
    std::shared_ptr<Line> line_;
    std::shared_ptr<Circle> circle_;
    std::shared_ptr<Arc> arc_;
    double offset_distance_;
    Point saved_start_, saved_end_;
    double saved_radius_;
    bool is_line_, is_circle_, is_arc_;
};

/**
 * @brief Mirror tool - mirror geometry about axis
 */
class MirrorTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param geometry Geometry to mirror
     * @param axis Line representing mirror axis
     */
    MirrorTool(std::vector<std::shared_ptr<void>>& geometries, const Line& axis);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Mirror"; }

    /**
     * @brief Mirror a point about an axis
     */
    static Point mirrorPoint(const Point& p, const Line& axis);

    /**
     * @brief Create mirrored copies
     */
    std::vector<std::shared_ptr<void>> createMirrorCopies();

private:
    std::vector<std::shared_ptr<void>> geometries_;
    Line mirror_axis_;
    std::vector<std::shared_ptr<void>> mirrored_copies_;
    std::vector<std::shared_ptr<void>> saved_state_;
};

/**
 * @brief Fillet tool - round corners with circular arc
 */
class FilletTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param line1 First line
     * @param line2 Second line
     * @param radius Fillet radius
     */
    FilletTool(std::shared_ptr<Line> line1, std::shared_ptr<Line> line2, double radius);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Fillet"; }

    /**
     * @brief Calculate fillet geometry
     */
    Arc calculateFillet(const Line& line1, const Line& line2, double radius);

private:
    std::shared_ptr<Line> line1_, line2_;
    double radius_;
    Arc saved_arc_;
    Point saved_trim1_, saved_trim2_;
};

/**
 * @brief Chamfer tool - bevel corners with straight line
 */
class ChamferTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param line1 First line
     * @param line2 Second line
     * @param size Chamfer size (distance along each edge)
     */
    ChamferTool(std::shared_ptr<Line> line1, std::shared_ptr<Line> line2, double size);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Chamfer"; }

    /**
     * @brief Calculate chamfer line
     */
    Line calculateChamfer(const Line& line1, const Line& line2, double size);

private:
    std::shared_ptr<Line> line1_, line2_;
    double size_;
    Line saved_line_;
    Point saved_trim1_, saved_trim2_;
};

/**
 * @brief Move tool - translate geometry
 */
class MoveTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param geometry Geometry to move
     * @param offset Movement vector (dx, dy)
     */
    template<typename T>
    MoveTool(std::shared_ptr<T> geometry, double dx, double dy)
        : offset_x_(dx), offset_y_(dy), geometry_ptr_(geometry) {}

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Move"; }

private:
    double offset_x_, offset_y_;
    std::shared_ptr<void> geometry_ptr_;
    std::vector<Point> saved_points_;
};

/**
 * @brief Rotate tool - rotate geometry about point
 */
class RotateTool : public EditOperation {
public:
    /**
     * @brief Constructor
     * @param geometries Geometries to rotate
     * @param center Rotation center
     * @param angle Rotation angle in degrees
     */
    RotateTool(std::vector<std::shared_ptr<void>>& geometries, const Point& center, double angle_deg);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Rotate"; }

    /**
     * @brief Rotate a point about center
     */
    static Point rotatePoint(const Point& p, const Point& center, double angle_deg);

private:
    std::vector<std::shared_ptr<void>> geometries_;
    Point rotation_center_;
    double angle_degrees_;
    std::vector<std::shared_ptr<void>> saved_state_;
};

/**
 * @brief Pattern tool - create array of geometry
 */
class PatternTool : public EditOperation {
public:
    enum class PatternType {
        LINEAR,      // Linear array
        CIRCULAR,    // Circular array
        RECTANGULAR  // Grid array
    };

    /**
     * @brief Linear pattern
     * @param geometry Geometry to pattern
     * @param direction Direction vector (dx, dy)
     * @param count Number of copies
     * @param spacing Space between copies
     */
    PatternTool(std::vector<std::shared_ptr<void>>& geometries,
                const Point& direction,
                int count,
                double spacing,
                PatternType type = PatternType::LINEAR);

    /**
     * @brief Circular pattern
     * @param geometry Geometry to pattern
     * @param center Center of rotation
     * @param count Number of copies
     */
    PatternTool(std::vector<std::shared_ptr<void>>& geometries,
                const Point& center,
                int count,
                PatternType type = PatternType::CIRCULAR);

    bool execute() override;
    bool undo() override;
    std::string getDescription() const override { return "Pattern"; }

    /**
     * @brief Get created pattern copies
     */
    const std::vector<std::shared_ptr<void>>& getPatternCopies() const {
        return pattern_copies_;
    }

private:
    std::vector<std::shared_ptr<void>> geometries_;
    Point direction_;
    Point center_;
    int count_;
    double spacing_;
    PatternType type_;
    std::vector<std::shared_ptr<void>> pattern_copies_;

    std::vector<std::shared_ptr<void>> createLinearCopies();
    std::vector<std::shared_ptr<void>> createCircularCopies();
};

/**
 * @brief Edit tools coordinator
 */
class EditToolsCoordinator {
public:
    /**
     * @brief Execute an edit operation
     */
    bool executeOperation(std::shared_ptr<EditOperation> operation);

    /**
     * @brief Undo last operation
     */
    bool undo();

    /**
     * @brief Redo last undone operation
     */
    bool redo();

    /**
     * @brief Get undo/redo history
     */
    std::vector<std::string> getOperationHistory() const;

    /**
     * @brief Clear history
     */
    void clearHistory();

    /**
     * @brief Can undo?
     */
    bool canUndo() const { return !undo_stack_.empty(); }

    /**
     * @brief Can redo?
     */
    bool canRedo() const { return !redo_stack_.empty(); }

private:
    std::vector<std::shared_ptr<EditOperation>> undo_stack_;
    std::vector<std::shared_ptr<EditOperation>> redo_stack_;
    static const size_t MAX_HISTORY = 100;
};

} // namespace bendcore::sketch
