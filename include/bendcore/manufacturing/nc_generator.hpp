#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief NC Code (Numerical Control) generator for CNC machines
 *
 * Generates G-code for tube bending, cutting, drilling, and machining operations
 */
class NCGenerator {
public:
    /**
     * @brief NC operation type
     */
    enum class OperationType {
        RAPID_MOVE,      // G00 - Rapid positioning
        LINEAR_MOVE,     // G01 - Linear interpolation
        CW_ARC,          // G02 - Circular interpolation (CW)
        CCW_ARC,         // G03 - Circular interpolation (CCW)
        DRILL,           // Drilling operation
        TAP,             // Tapping operation
        BORE,            // Boring operation
        THREAD,          // Threading operation
        BEND,            // Tube bending operation
        CUT,             // Cutting operation
    };

    /**
     * @brief NC block (line of G-code)
     */
    struct NCBlock {
        size_t line_number = 0;
        OperationType operation = OperationType::LINEAR_MOVE;
        Vector3D position;
        double feed_rate = 0.0;  // mm/min
        double spindle_speed = 0.0;  // RPM
        std::string comment;
    };

    /**
     * @brief Constructor
     */
    NCGenerator(const EntityId& geometry_id) : geometry_id_(geometry_id) {}

    /**
     * @brief Add NC operation
     */
    void addOperation(const NCBlock& block) noexcept {
        blocks_.push_back(block);
        blocks_.back().line_number = blocks_.size() * 10;
    }

    /**
     * @brief Get all NC blocks
     */
    [[nodiscard]] const std::vector<NCBlock>& getBlocks() const noexcept {
        return blocks_;
    }

    /**
     * @brief Set machine tool offset (tool length, tool diameter)
     */
    void setToolOffset(double length, double diameter) noexcept {
        tool_offset_length_ = length;
        tool_diameter_ = diameter;
    }

    /**
     * @brief Generate G-code string
     */
    [[nodiscard]] std::string generateGCode() const noexcept;

    /**
     * @brief Export to file
     */
    [[nodiscard]] bool exportToFile(const std::string& filename) const noexcept;

    /**
     * @brief Get estimated machining time (minutes)
     */
    [[nodiscard]] double estimateTime() const noexcept;

private:
    EntityId geometry_id_;
    std::vector<NCBlock> blocks_;
    double tool_offset_length_ = 0.0;
    double tool_diameter_ = 0.0;
};

/**
 * @brief Bill of Materials (BOM) generator
 */
class BOMMaker {
public:
    /**
     * @brief BOM line item
     */
    struct BOMItem {
        size_t item_number = 0;
        std::string part_name;
        std::string part_number;
        size_t quantity = 1;
        std::string material;
        double weight = 0.0;  // kg
        std::string unit_cost_currency;
        double unit_cost = 0.0;
        std::string supplier;
        std::string notes;
    };

    /**
     * @brief Constructor
     */
    BOMMaker(const EntityId& assembly_id) : assembly_id_(assembly_id) {}

    /**
     * @brief Add BOM item
     */
    void addItem(const BOMItem& item) noexcept {
        items_.push_back(item);
        items_.back().item_number = items_.size();
    }

    /**
     * @brief Get all BOM items
     */
    [[nodiscard]] const std::vector<BOMItem>& getItems() const noexcept {
        return items_;
    }

    /**
     * @brief Calculate total weight
     */
    [[nodiscard]] double getTotalWeight() const noexcept;

    /**
     * @brief Calculate total cost
     */
    [[nodiscard]] double getTotalCost() const noexcept;

    /**
     * @brief Generate BOM as CSV
     */
    [[nodiscard]] std::string generateCSV() const noexcept;

    /**
     * @brief Export BOM to file
     */
    [[nodiscard]] bool exportToFile(const std::string& filename) const noexcept;

private:
    EntityId assembly_id_;
    std::vector<BOMItem> items_;
};

/**
 * @brief Technical drawing generator (2D projection views)
 */
class DrawingGenerator {
public:
    /**
     * @brief View type
     */
    enum class ViewType {
        FRONT,           // Front view
        TOP,             // Top view
        RIGHT_SIDE,      // Right side view
        LEFT_SIDE,       // Left side view
        BOTTOM,          // Bottom view
        ISOMETRIC,       // Isometric 3D view
    };

    /**
     * @brief Drawing view
     */
    struct DrawingView {
        ViewType type = ViewType::FRONT;
        std::string title;
        Vector3D projection_origin;
        Vector3D view_direction;
        double scale = 1.0;
        std::vector<std::string> dimensions;
        std::vector<std::string> notes;
    };

    /**
     * @brief Constructor
     */
    DrawingGenerator(const EntityId& geometry_id) : geometry_id_(geometry_id) {}

    /**
     * @brief Add drawing view
     */
    void addView(const DrawingView& view) noexcept {
        views_.push_back(view);
    }

    /**
     * @brief Get all views
     */
    [[nodiscard]] const std::vector<DrawingView>& getViews() const noexcept {
        return views_;
    }

    /**
     * @brief Add dimension annotation
     */
    void addDimension(const std::string& dimension_text, const Vector3D& position) noexcept {
        dimensions_.push_back({dimension_text, position});
    }

    /**
     * @brief Generate drawing as PDF
     */
    [[nodiscard]] bool generatePDF(const std::string& filename) const noexcept;

    /**
     * @brief Generate drawing as SVG
     */
    [[nodiscard]] std::string generateSVG() const noexcept;

    /**
     * @brief Export to file
     */
    [[nodiscard]] bool exportToFile(const std::string& filename,
                                    const std::string& format) const noexcept;

private:
    EntityId geometry_id_;
    std::vector<DrawingView> views_;
    std::vector<std::pair<std::string, Vector3D>> dimensions_;
};

/**
 * @brief 3D PDF exporter (with embedded geometry)
 */
class PDF3DExporter {
public:
    /**
     * @brief Constructor
     */
    PDF3DExporter(const EntityId& geometry_id) : geometry_id_(geometry_id) {}

    /**
     * @brief Set PDF document properties
     */
    void setDocumentProperties(const std::string& title,
                              const std::string& author,
                              const std::string& subject) noexcept {
        title_ = title;
        author_ = author;
        subject_ = subject;
    }

    /**
     * @brief Enable annotations in PDF
     */
    void enableAnnotations(bool enable) noexcept { with_annotations_ = enable; }

    /**
     * @brief Enable measurements in PDF
     */
    void enableMeasurements(bool enable) noexcept { with_measurements_ = enable; }

    /**
     * @brief Enable assembly tree in PDF
     */
    void enableAssemblyTree(bool enable) noexcept { with_assembly_tree_ = enable; }

    /**
     * @brief Export 3D geometry to PDF
     */
    [[nodiscard]] bool exportGeometry(const std::string& filename) const noexcept;

    /**
     * @brief Generate embedded 3D model (U3D format)
     */
    [[nodiscard]] std::string generateU3D() const noexcept;

private:
    EntityId geometry_id_;
    std::string title_;
    std::string author_;
    std::string subject_;
    bool with_annotations_ = false;
    bool with_measurements_ = false;
    bool with_assembly_tree_ = false;
};

/**
 * @brief Manufacturing report generator
 */
class ManufacturingReport {
public:
    /**
     * @brief Constructor
     */
    ManufacturingReport(const EntityId& assembly_id) : assembly_id_(assembly_id) {}

    /**
     * @brief Set report metadata
     */
    void setMetadata(const std::string& project_name,
                    const std::string& revision,
                    const std::string& date) noexcept {
        project_name_ = project_name;
        revision_ = revision;
        date_ = date;
    }

    /**
     * @brief Add BOM section
     */
    void addBOM(const std::shared_ptr<BOMMaker>& bom) noexcept {
        bom_ = bom;
    }

    /**
     * @brief Add NC code section
     */
    void addNCCode(const std::shared_ptr<NCGenerator>& nc) noexcept {
        nc_generator_ = nc;
    }

    /**
     * @brief Add drawings section
     */
    void addDrawings(const std::shared_ptr<DrawingGenerator>& drawings) noexcept {
        drawings_ = drawings;
    }

    /**
     * @brief Generate HTML report
     */
    [[nodiscard]] std::string generateHTML() const noexcept;

    /**
     * @brief Export report to file
     */
    [[nodiscard]] bool exportToFile(const std::string& filename) const noexcept;

private:
    EntityId assembly_id_;
    std::string project_name_;
    std::string revision_;
    std::string date_;
    std::shared_ptr<BOMMaker> bom_;
    std::shared_ptr<NCGenerator> nc_generator_;
    std::shared_ptr<DrawingGenerator> drawings_;
};

} // namespace BendCore::CadCore
