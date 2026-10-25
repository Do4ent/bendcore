#include "bendcore/sketch/edit_tools.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore::sketch {

// ============= TrimTool =============
TrimTool::TrimTool(std::shared_ptr<Line> geometry, const Point& trim_point)
    : line_(geometry), arc_(nullptr), trim_point_(trim_point), is_line_(true) {}

TrimTool::TrimTool(std::shared_ptr<Arc> geometry, const Point& trim_point)
    : line_(nullptr), arc_(geometry), trim_point_(trim_point), is_line_(false) {}

bool TrimTool::execute() {
    if (is_line_ && line_) {
        saved_start_ = line_->getStart();
        saved_end_ = line_->getEnd();

        // Find intersection point
        Point intersection = findIntersection(trim_point_);

        // Trim the line at the intersection
        double dist_to_start = saved_start_.distance(intersection);
        double dist_to_end = saved_end_.distance(intersection);

        if (dist_to_start < dist_to_end) {
            line_->setStart(intersection);
        } else {
            line_->setEnd(intersection);
        }
        return true;
    } else if (!is_line_ && arc_) {
        // Similar logic for arcs
        return true;
    }
    return false;
}

bool TrimTool::undo() {
    if (is_line_ && line_) {
        line_->setStart(saved_start_);
        line_->setEnd(saved_end_);
        return true;
    }
    return false;
}

Point TrimTool::findIntersection(const Point& trim_point, double tolerance) {
    if (is_line_ && line_) {
        const Point& start = line_->getStart();
        const Point& end = line_->getEnd();

        // Project trim_point onto the line
        double dx = end.getX() - start.getX();
        double dy = end.getY() - start.getY();
        double len_sq = dx * dx + dy * dy;

        if (len_sq < tolerance * tolerance) {
            return start;
        }

        double t = ((trim_point.getX() - start.getX()) * dx +
                   (trim_point.getY() - start.getY()) * dy) / len_sq;
        t = std::max(0.0, std::min(1.0, t));

        return Point(start.getX() + t * dx, start.getY() + t * dy);
    }
    return trim_point;
}

// ============= ExtendTool =============
ExtendTool::ExtendTool(std::shared_ptr<Line> geometry, const Line& target)
    : geometry_(geometry), target_(target), saved_end_(Point()) {}

bool ExtendTool::execute() {
    if (!geometry_) return false;

    saved_end_ = geometry_->getEnd();
    Point extension = calculateExtension(*geometry_, target_);
    geometry_->setEnd(extension);

    return true;
}

bool ExtendTool::undo() {
    if (geometry_) {
        geometry_->setEnd(saved_end_);
        return true;
    }
    return false;
}

Point ExtendTool::calculateExtension(const Line& geometry, const Line& target) {
    // Find intersection point of two lines
    double x1 = geometry.getStart().getX(), y1 = geometry.getStart().getY();
    double x2 = geometry.getEnd().getX(), y2 = geometry.getEnd().getY();
    double x3 = target.getStart().getX(), y3 = target.getStart().getY();
    double x4 = target.getEnd().getX(), y4 = target.getEnd().getY();

    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

    if (std::abs(denom) < 1e-10) {
        return geometry.getEnd();  // Lines are parallel
    }

    double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;

    return Point(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
}

// ============= OffsetTool =============
OffsetTool::OffsetTool(std::shared_ptr<Line> geometry, double distance)
    : line_(geometry), circle_(nullptr), arc_(nullptr), offset_distance_(distance),
      is_line_(true), is_circle_(false), is_arc_(false) {}

OffsetTool::OffsetTool(std::shared_ptr<Circle> geometry, double distance)
    : line_(nullptr), circle_(geometry), arc_(nullptr), offset_distance_(distance),
      is_line_(false), is_circle_(true), is_arc_(false) {}

OffsetTool::OffsetTool(std::shared_ptr<Arc> geometry, double distance)
    : line_(nullptr), circle_(nullptr), arc_(geometry), offset_distance_(distance),
      is_line_(false), is_circle_(false), is_arc_(true) {}

bool OffsetTool::execute() {
    if (is_line_ && line_) {
        saved_start_ = line_->getStart();
        saved_end_ = line_->getEnd();

        // Offset line perpendicular to its direction
        double dx = saved_end_.getX() - saved_start_.getX();
        double dy = saved_end_.getY() - saved_start_.getY();
        double len = std::sqrt(dx * dx + dy * dy);

        if (len > 1e-10) {
            double nx = -dy / len;
            double ny = dx / len;

            line_->setStart(Point(saved_start_.getX() + nx * offset_distance_,
                                saved_start_.getY() + ny * offset_distance_));
            line_->setEnd(Point(saved_end_.getX() + nx * offset_distance_,
                              saved_end_.getY() + ny * offset_distance_));
        }
        return true;
    } else if (is_circle_ && circle_) {
        saved_radius_ = circle_->getRadius();
        circle_->setRadius(saved_radius_ + offset_distance_);
        return true;
    } else if (is_arc_ && arc_) {
        saved_radius_ = arc_->getRadius();
        arc_->setRadius(saved_radius_ + offset_distance_);
        return true;
    }
    return false;
}

bool OffsetTool::undo() {
    if (is_line_ && line_) {
        line_->setStart(saved_start_);
        line_->setEnd(saved_end_);
        return true;
    } else if (is_circle_ && circle_) {
        circle_->setRadius(saved_radius_);
        return true;
    } else if (is_arc_ && arc_) {
        arc_->setRadius(saved_radius_);
        return true;
    }
    return false;
}

// ============= MirrorTool =============
MirrorTool::MirrorTool(std::vector<std::shared_ptr<void>>& geometries, const Line& axis)
    : geometries_(geometries), mirror_axis_(axis) {}

bool MirrorTool::execute() {
    saved_state_ = geometries_;
    mirrored_copies_ = createMirrorCopies();
    geometries_.insert(geometries_.end(), mirrored_copies_.begin(), mirrored_copies_.end());
    return true;
}

bool MirrorTool::undo() {
    geometries_ = saved_state_;
    return true;
}

Point MirrorTool::mirrorPoint(const Point& p, const Line& axis) {
    const Point& start = axis.getStart();
    const Point& end = axis.getEnd();

    double dx = end.getX() - start.getX();
    double dy = end.getY() - start.getY();
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-10) {
        return p;
    }

    double t = ((p.getX() - start.getX()) * dx + (p.getY() - start.getY()) * dy) / len_sq;

    double closest_x = start.getX() + t * dx;
    double closest_y = start.getY() + t * dy;

    return Point(2.0 * closest_x - p.getX(), 2.0 * closest_y - p.getY());
}

std::vector<std::shared_ptr<void>> MirrorTool::createMirrorCopies() {
    std::vector<std::shared_ptr<void>> copies;

    for (const auto& geometry : geometries_) {
        // Clone and mirror geometry
        // This is a simplified version - real implementation would clone properly
        copies.push_back(geometry);
    }

    return copies;
}

// ============= FilletTool =============
FilletTool::FilletTool(std::shared_ptr<Line> line1, std::shared_ptr<Line> line2, double radius)
    : line1_(line1), line2_(line2), radius_(radius) {}

bool FilletTool::execute() {
    if (!line1_ || !line2_) return false;

    Arc fillet = calculateFillet(*line1_, *line2_, radius_);
    saved_arc_ = fillet;
    saved_trim1_ = line1_->getEnd();
    saved_trim2_ = line2_->getStart();

    // Trim lines at fillet points
    line1_->setEnd(fillet.getStartPoint());
    line2_->setStart(fillet.getEndPoint());

    return true;
}

bool FilletTool::undo() {
    if (line1_) line1_->setEnd(saved_trim1_);
    if (line2_) line2_->setStart(saved_trim2_);
    return true;
}

Arc FilletTool::calculateFillet(const Line& line1, const Line& line2, double radius) {
    // Simplified fillet calculation
    Point intersection = Point(0, 0);

    // Find intersection point
    double x1 = line1.getStart().getX(), y1 = line1.getStart().getY();
    double x2 = line1.getEnd().getX(), y2 = line1.getEnd().getY();
    double x3 = line2.getStart().getX(), y3 = line2.getStart().getY();
    double x4 = line2.getEnd().getX(), y4 = line2.getEnd().getY();

    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

    if (std::abs(denom) > 1e-10) {
        double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        intersection = Point(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
    }

    return Arc(intersection, radius, 0.0, 90.0);
}

// ============= ChamferTool =============
ChamferTool::ChamferTool(std::shared_ptr<Line> line1, std::shared_ptr<Line> line2, double size)
    : line1_(line1), line2_(line2), size_(size) {}

bool ChamferTool::execute() {
    if (!line1_ || !line2_) return false;

    Line chamfer = calculateChamfer(*line1_, *line2_, size_);
    saved_line_ = chamfer;
    saved_trim1_ = line1_->getEnd();
    saved_trim2_ = line2_->getStart();

    // Trim lines at chamfer points
    double dist1 = std::sqrt((saved_trim1_.getX() - chamfer.getStart().getX()) * (saved_trim1_.getX() - chamfer.getStart().getX()) +
                            (saved_trim1_.getY() - chamfer.getStart().getY()) * (saved_trim1_.getY() - chamfer.getStart().getY()));

    if (dist1 < size_) {
        line1_->setEnd(chamfer.getStart());
    }

    return true;
}

bool ChamferTool::undo() {
    if (line1_) line1_->setEnd(saved_trim1_);
    if (line2_) line2_->setStart(saved_trim2_);
    return true;
}

Line ChamferTool::calculateChamfer(const Line& line1, const Line& line2, double size) {
    // Simplified chamfer calculation
    return Line(line1.getEnd(), line2.getStart());
}

// ============= MoveTool =============
bool MoveTool::execute() {
    // Template execution - would need actual type information
    return true;
}

bool MoveTool::undo() {
    // Template undo
    return true;
}

// ============= RotateTool =============
RotateTool::RotateTool(std::vector<std::shared_ptr<void>>& geometries, const Point& center, double angle_deg)
    : geometries_(geometries), rotation_center_(center), angle_degrees_(angle_deg) {}

bool RotateTool::execute() {
    saved_state_ = geometries_;

    // Rotate each geometry
    for (auto& geometry : geometries_) {
        // Implementation would rotate actual geometry
    }

    return true;
}

bool RotateTool::undo() {
    geometries_ = saved_state_;
    return true;
}

Point RotateTool::rotatePoint(const Point& p, const Point& center, double angle_deg) {
    double rad = angle_deg * M_PI / 180.0;
    double cos_a = std::cos(rad);
    double sin_a = std::sin(rad);

    double dx = p.getX() - center.getX();
    double dy = p.getY() - center.getY();

    return Point(center.getX() + dx * cos_a - dy * sin_a,
                center.getY() + dx * sin_a + dy * cos_a);
}

// ============= PatternTool =============
PatternTool::PatternTool(std::vector<std::shared_ptr<void>>& geometries,
                        const Point& direction,
                        int count,
                        double spacing,
                        PatternType type)
    : geometries_(geometries), direction_(direction), count_(count),
      spacing_(spacing), type_(type) {}

PatternTool::PatternTool(std::vector<std::shared_ptr<void>>& geometries,
                        const Point& center,
                        int count,
                        PatternType type)
    : geometries_(geometries), center_(center), count_(count), type_(type) {}

bool PatternTool::execute() {
    if (type_ == PatternType::LINEAR) {
        pattern_copies_ = createLinearCopies();
    } else if (type_ == PatternType::CIRCULAR) {
        pattern_copies_ = createCircularCopies();
    }

    geometries_.insert(geometries_.end(), pattern_copies_.begin(), pattern_copies_.end());
    return true;
}

bool PatternTool::undo() {
    for (size_t i = 0; i < pattern_copies_.size(); i++) {
        if (!geometries_.empty()) {
            geometries_.pop_back();
        }
    }
    return true;
}

std::vector<std::shared_ptr<void>> PatternTool::createLinearCopies() {
    std::vector<std::shared_ptr<void>> copies;

    for (int i = 1; i < count_; i++) {
        double dx = direction_.getX() * spacing_;
        double dy = direction_.getY() * spacing_;

        for (const auto& geometry : geometries_) {
            // Clone geometry with offset
            copies.push_back(geometry);
        }
    }

    return copies;
}

std::vector<std::shared_ptr<void>> PatternTool::createCircularCopies() {
    std::vector<std::shared_ptr<void>> copies;

    double angle_step = 360.0 / count_;

    for (int i = 1; i < count_; i++) {
        double angle = i * angle_step;

        for (const auto& geometry : geometries_) {
            // Clone geometry rotated around center
            copies.push_back(geometry);
        }
    }

    return copies;
}

// ============= EditToolsCoordinator =============
bool EditToolsCoordinator::executeOperation(std::shared_ptr<EditOperation> operation) {
    if (!operation) return false;

    if (operation->execute()) {
        undo_stack_.push_back(operation);

        // Limit history size
        if (undo_stack_.size() > MAX_HISTORY) {
            undo_stack_.erase(undo_stack_.begin());
        }

        redo_stack_.clear();
        return true;
    }

    return false;
}

bool EditToolsCoordinator::undo() {
    if (undo_stack_.empty()) return false;

    auto operation = undo_stack_.back();
    undo_stack_.pop_back();

    if (operation->undo()) {
        redo_stack_.push_back(operation);
        return true;
    }

    return false;
}

bool EditToolsCoordinator::redo() {
    if (redo_stack_.empty()) return false;

    auto operation = redo_stack_.back();
    redo_stack_.pop_back();

    if (operation->execute()) {
        undo_stack_.push_back(operation);
        return true;
    }

    return false;
}

std::vector<std::string> EditToolsCoordinator::getOperationHistory() const {
    std::vector<std::string> history;

    for (const auto& op : undo_stack_) {
        history.push_back(op->getDescription());
    }

    return history;
}

void EditToolsCoordinator::clearHistory() {
    undo_stack_.clear();
    redo_stack_.clear();
}

} // namespace bendcore::sketch
