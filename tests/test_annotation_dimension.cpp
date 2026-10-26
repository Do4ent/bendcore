// M4 Phase - Annotation & Dimensioning System Tests (GAP-018)
#include "bendcore/drawing/annotation_dimension.hpp"

#include <cmath>
#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>

using namespace BendCore::Drawing;

// ============================================================================
// Test Infrastructure
// ============================================================================

static int test_count = 0;
static int failures = 0;

void test(bool condition, const std::string& description) {
    test_count++;
    if (!condition) {
        std::cerr << "FAIL: " << description << std::endl;
        failures++;
    } else {
        std::cout << "PASS: " << description << std::endl;
    }
}

bool near(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

// ============================================================================
// ToleranceSpec Tests
// ============================================================================

void test_tolerance_spec_none() {
    ToleranceSpec tol;
    tol.type = ToleranceType::None;
    test(tol.isValid(), "ToleranceSpec::isValid() for None tolerance");
    test(tol.toString().empty(), "ToleranceSpec::toString() for None tolerance is empty");
}

void test_tolerance_spec_bilateral() {
    ToleranceSpec tol;
    tol.type = ToleranceType::Bilateral;
    tol.upper_limit = 0.5;
    tol.lower_limit = 0.5;
    test(tol.isValid(), "ToleranceSpec::isValid() for bilateral tolerance");
    std::string str = tol.toString();
    test(str.find("+/-") != std::string::npos, "ToleranceSpec::toString() contains '+/-'");
}

void test_tolerance_spec_unilateral() {
    ToleranceSpec tol;
    tol.type = ToleranceType::Unilateral;
    tol.upper_limit = 0.3;
    tol.lower_limit = 0.1;
    test(tol.isValid(), "ToleranceSpec::isValid() for unilateral tolerance");
}

void test_tolerance_spec_limit() {
    ToleranceSpec tol;
    tol.type = ToleranceType::Limit;
    tol.upper_limit = 50.5;
    tol.lower_limit = 49.5;
    test(tol.isValid(), "ToleranceSpec::isValid() for limit tolerance");
    test(tol.upper_limit >= tol.lower_limit, "ToleranceSpec limit upper >= lower");
}

void test_tolerance_spec_invalid_limit() {
    ToleranceSpec tol;
    tol.type = ToleranceType::Limit;
    tol.upper_limit = 49.5;
    tol.lower_limit = 50.5;
    test(!tol.isValid(), "ToleranceSpec::isValid() detects invalid limit");
}

void test_tolerance_spec_with_note() {
    ToleranceSpec tol;
    tol.type = ToleranceType::Bilateral;
    tol.upper_limit = 0.5;
    tol.lower_limit = 0.5;
    tol.note = "Critical dimension";
    test(tol.toString().find("Critical dimension") != std::string::npos,
         "ToleranceSpec::toString() includes note");
}

// ============================================================================
// DimensionProperties Tests
// ============================================================================

void test_dimension_properties_default() {
    DimensionProperties props;
    test(near(props.value, 0.0), "DimensionProperties default value is 0.0");
    test(props.unit == "mm", "DimensionProperties default unit is 'mm'");
    test(props.precision == 1, "DimensionProperties default precision is 1");
    test(props.show_dimension, "DimensionProperties default show_dimension is true");
    test(props.text_height > 3.0, "DimensionProperties text_height is set");
}

void test_dimension_properties_formatting() {
    DimensionProperties props;
    props.value = 50.0;
    props.precision = 2;
    props.text_prefix = "Ø";
    props.text_suffix = "MAX";
    std::string str = props.toString();
    test(str.find("Ø") != std::string::npos, "DimensionProperties includes prefix");
    test(str.find("MAX") != std::string::npos, "DimensionProperties includes suffix");
    test(str.find("50.00") != std::string::npos, "DimensionProperties formats value with precision");
}

void test_dimension_properties_with_tolerance() {
    DimensionProperties props;
    props.value = 25.0;
    props.tolerance.type = ToleranceType::Bilateral;
    props.tolerance.upper_limit = 0.2;
    props.tolerance.lower_limit = 0.2;
    std::string str = props.toString();
    test(str.find("+/-") != std::string::npos, "DimensionProperties includes tolerance");
}

// ============================================================================
// LinearDimension Tests
// ============================================================================

void test_linear_dimension_horizontal_creation() {
    auto dim = std::make_shared<LinearDimension>("lin_h_1", LinearDimensionSubType::Horizontal);
    test(dim->getId() == "lin_h_1", "LinearDimension ID is set");
    test(dim->getType() == DimensionType::Linear, "LinearDimension type is Linear");
    test(dim->getSubType() == LinearDimensionSubType::Horizontal, "LinearDimension subtype is Horizontal");
}

void test_linear_dimension_vertical_creation() {
    auto dim = std::make_shared<LinearDimension>("lin_v_1", LinearDimensionSubType::Vertical);
    test(dim->getSubType() == LinearDimensionSubType::Vertical, "LinearDimension subtype is Vertical");
}

void test_linear_dimension_aligned_creation() {
    auto dim = std::make_shared<LinearDimension>("lin_a_1", LinearDimensionSubType::Aligned);
    test(dim->getSubType() == LinearDimensionSubType::Aligned, "LinearDimension subtype is Aligned");
}

void test_linear_dimension_value_calculation_horizontal() {
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal);
    dim->setReferencePoints({0.0, 0.0}, {50.0, 0.0});
    dim->updateValue();
    test(near(dim->getValue(), 50.0), "LinearDimension horizontal value = 50.0");
}

void test_linear_dimension_value_calculation_vertical() {
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Vertical);
    dim->setReferencePoints({0.0, 0.0}, {0.0, 30.0});
    dim->updateValue();
    test(near(dim->getValue(), 30.0), "LinearDimension vertical value = 30.0");
}

void test_linear_dimension_value_calculation_aligned() {
    auto dim = createLinearDimension("lin_1", LinearDimensionSubType::Aligned, {0.0, 0.0}, {3.0, 4.0});
    test(near(dim->getValue(), 5.0), "LinearDimension aligned value = 5.0 (3-4-5 triangle)");
}

void test_linear_dimension_offset() {
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal);
    dim->setDimensionLineOffset(15.0);
    test(near(dim->getDimensionLineOffset(), 15.0), "LinearDimension offset is 15.0");
}

// ============================================================================
// RadialDimension Tests
// ============================================================================

void test_radial_dimension_radius() {
    auto dim = std::make_shared<RadialDimension>("rad_1", false);
    test(dim->getType() == DimensionType::Radial, "RadialDimension type is Radial");
    test(!dim->isDiameter(), "RadialDimension is radius");
}

void test_radial_dimension_diameter() {
    auto dim = std::make_shared<RadialDimension>("diam_1", true);
    test(dim->getType() == DimensionType::Diameter, "RadialDimension type is Diameter");
    test(dim->isDiameter(), "RadialDimension is diameter");
}

void test_radial_dimension_value_radius() {
    auto dim = createRadialDimension("rad_1", {0.0, 0.0}, {10.0, 0.0}, false);
    test(near(dim->getValue(), 10.0), "RadialDimension radius value = 10.0");
    test(dim->getProperties().text_prefix == "R", "RadialDimension radius has 'R' prefix");
}

void test_radial_dimension_value_diameter() {
    auto dim = createRadialDimension("diam_1", {0.0, 0.0}, {0.0, 5.0}, true);
    test(near(dim->getValue(), 10.0), "RadialDimension diameter value = 10.0");
    test(dim->getProperties().text_prefix == "Ø", "RadialDimension diameter has 'Ø' prefix");
}

// ============================================================================
// AngularDimension Tests
// ============================================================================

void test_angular_dimension_creation() {
    auto dim = std::make_shared<AngularDimension>("ang_1");
    test(dim->getType() == DimensionType::Angular, "AngularDimension type is Angular");
}

void test_angular_dimension_90_degrees() {
    auto dim = createAngularDimension("ang_1",
        {0.0, 0.0}, {1.0, 0.0},  // Horizontal line
        {0.0, 0.0}, {0.0, 1.0}   // Vertical line
    );
    test(near(dim->getValue(), 90.0), "AngularDimension 90 degrees");
    test(dim->getProperties().unit == "°", "AngularDimension unit is '°'");
}

void test_angular_dimension_45_degrees() {
    auto dim = createAngularDimension("ang_1",
        {0.0, 0.0}, {1.0, 0.0},
        {0.0, 0.0}, {1.0, 1.0}
    );
    test(near(dim->getValue(), 45.0, 0.1), "AngularDimension 45 degrees");
}

// ============================================================================
// OrdinateDimension Tests
// ============================================================================

void test_ordinate_dimension_horizontal() {
    auto dim = std::make_shared<OrdinateDimension>("ord_1", true);
    test(dim->getType() == DimensionType::Ordinate, "OrdinateDimension type is Ordinate");
    test(dim->isHorizontal(), "OrdinateDimension is horizontal");
}

void test_ordinate_dimension_vertical() {
    auto dim = std::make_shared<OrdinateDimension>("ord_1", false);
    test(!dim->isHorizontal(), "OrdinateDimension is vertical");
}

void test_ordinate_dimension_value() {
    auto dim = std::make_shared<OrdinateDimension>("ord_1", true);
    dim->setDatum({0.0, 0.0});
    dim->setReferencePoint({30.0, 0.0});
    dim->updateValue();
    test(near(dim->getValue(), 30.0), "OrdinateDimension horizontal value = 30.0");
}

// ============================================================================
// LeaderDimension Tests
// ============================================================================

void test_leader_dimension_creation() {
    auto dim = std::make_shared<LeaderDimension>("lead_1");
    test(dim->getType() == DimensionType::Leader, "LeaderDimension type is Leader");
}

void test_leader_dimension_text() {
    auto dim = std::make_shared<LeaderDimension>("lead_1");
    dim->setText("Important note");
    test(dim->getText() == "Important note", "LeaderDimension text is set");
}

void test_leader_dimension_points() {
    auto dim = std::make_shared<LeaderDimension>("lead_1");
    std::vector<Vec2> points = {{0.0, 0.0}, {10.0, 10.0}, {20.0, 10.0}};
    dim->setLeaderPoints(points);
    test(dim->getLeaderPoints().size() == 3, "LeaderDimension has 3 points");
}

// ============================================================================
// TextAnnotation Tests
// ============================================================================

void test_text_annotation_creation() {
    auto ann = std::make_shared<TextAnnotation>("txt_1");
    test(ann->getId() == "txt_1", "TextAnnotation ID is set");
    test(ann->getType() == AnnotationType::TextNote, "TextAnnotation type is TextNote");
}

void test_text_annotation_properties() {
    auto ann = std::make_shared<TextAnnotation>("txt_1");
    ann->setText("This is a note");
    ann->setTextHeight(5.0);
    ann->setAnchorPoint("start");
    test(ann->getText() == "This is a note", "TextAnnotation text is set");
    test(near(ann->getTextHeight(), 5.0), "TextAnnotation text height is 5.0");
    test(ann->getAnchorPoint() == "start", "TextAnnotation anchor point is 'start'");
}

void test_text_annotation_rotation() {
    auto ann = std::make_shared<TextAnnotation>("txt_1");
    ann->setRotation(45.0);
    test(near(ann->getRotation(), 45.0), "TextAnnotation rotation is 45 degrees");
}

// ============================================================================
// RevisionCloudAnnotation Tests
// ============================================================================

void test_revision_cloud_creation() {
    auto ann = std::make_shared<RevisionCloudAnnotation>("cloud_1");
    test(ann->getType() == AnnotationType::RevisionCloud, "RevisionCloudAnnotation type");
}

void test_revision_cloud_properties() {
    auto ann = std::make_shared<RevisionCloudAnnotation>("cloud_1");
    ann->setRevisionNumber(3);
    test(ann->getRevisionNumber() == 3, "RevisionCloudAnnotation revision number is 3");

    std::vector<Vec2> points = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};
    ann->setContourPoints(points);
    test(ann->getContourPoints().size() == 4, "RevisionCloudAnnotation has 4 contour points");
}

// ============================================================================
// WeldSymbolAnnotation Tests
// ============================================================================

void test_weld_symbol_creation() {
    auto ann = std::make_shared<WeldSymbolAnnotation>("weld_1");
    test(ann->getType() == AnnotationType::WeldSymbol, "WeldSymbolAnnotation type");
}

void test_weld_symbol_properties() {
    auto ann = std::make_shared<WeldSymbolAnnotation>("weld_1");
    ann->setWeldType(WeldSymbolType::Fillet);
    ann->setSize(5.0);
    ann->setLength(50.0);
    ann->setArrowSide(true);
    ann->setOtherSide(false);

    test(ann->getWeldType() == WeldSymbolType::Fillet, "WeldSymbolAnnotation type is Fillet");
    test(near(ann->getSize(), 5.0), "WeldSymbolAnnotation size is 5.0");
    test(near(ann->getLength(), 50.0), "WeldSymbolAnnotation length is 50.0");
    test(ann->getArrowSide(), "WeldSymbolAnnotation arrow side is true");
    test(!ann->hasOtherSide(), "WeldSymbolAnnotation other side is false");
}

// ============================================================================
// SurfaceFinishAnnotation Tests
// ============================================================================

void test_surface_finish_annotation() {
    auto ann = std::make_shared<SurfaceFinishAnnotation>("finish_1");
    test(ann->getType() == AnnotationType::SurfaceFinish, "SurfaceFinishAnnotation type");
}

void test_surface_finish_properties() {
    auto ann = std::make_shared<SurfaceFinishAnnotation>("finish_1");
    ann->setFinishType(SurfaceFinishType::Machined);
    ann->setRoughnessValue(1.6);
    ann->setNote("All surfaces");

    test(ann->getFinishType() == SurfaceFinishType::Machined, "SurfaceFinishAnnotation type is Machined");
    test(near(ann->getRoughnessValue(), 1.6), "SurfaceFinishAnnotation roughness is 1.6");
    test(ann->getNote() == "All surfaces", "SurfaceFinishAnnotation note");
}

// ============================================================================
// GDandTAnnotation Tests
// ============================================================================

void test_gd_and_t_annotation() {
    auto ann = std::make_shared<GDandTAnnotation>("gdt_1");
    test(ann->getType() == AnnotationType::GDandT, "GDandTAnnotation type");
}

void test_gd_and_t_properties() {
    auto ann = std::make_shared<GDandTAnnotation>("gdt_1");
    ann->setToleranceType(GDandTType::Position);
    ann->setToleranceValue(0.1);
    ann->setDatumReference("A");

    test(ann->getToleranceType() == GDandTType::Position, "GDandTAnnotation type is Position");
    test(near(ann->getToleranceValue(), 0.1), "GDandTAnnotation tolerance is 0.1");
    test(ann->getDatumReference() == "A", "GDandTAnnotation datum reference is 'A'");
}

// ============================================================================
// ReferenceTagAnnotation Tests
// ============================================================================

void test_reference_tag_annotation() {
    auto ann = std::make_shared<ReferenceTagAnnotation>("ref_1");
    test(ann->getType() == AnnotationType::ReferenceTag, "ReferenceTagAnnotation type");
}

void test_reference_tag_properties() {
    auto ann = std::make_shared<ReferenceTagAnnotation>("ref_1");
    ann->setLabel("DETAIL A");
    ann->setReferencedEntity("entity_123");

    test(ann->getLabel() == "DETAIL A", "ReferenceTagAnnotation label");
    test(ann->getReferencedEntity() == "entity_123", "ReferenceTagAnnotation referenced entity");
}

// ============================================================================
// BalloonAnnotation Tests
// ============================================================================

void test_balloon_annotation() {
    auto ann = std::make_shared<BalloonAnnotation>("balloon_1");
    test(ann->getType() == AnnotationType::Balloon, "BalloonAnnotation type");
}

void test_balloon_annotation_properties() {
    auto ann = std::make_shared<BalloonAnnotation>("balloon_1");
    ann->setNumber(5);
    ann->setRadius(7.0);
    ann->setLeaderPoint({50.0, 50.0});

    test(ann->getNumber() == 5, "BalloonAnnotation number is 5");
    test(near(ann->getRadius(), 7.0), "BalloonAnnotation radius is 7.0");
    test(near(ann->getLeaderPoint()[0], 50.0), "BalloonAnnotation leader point X");
}

// ============================================================================
// DimensionManager Tests
// ============================================================================

void test_dimension_manager_creation() {
    DimensionManager mgr("view_1");
    test(mgr.getTotalElementCount() == 0, "DimensionManager starts empty");
}

void test_dimension_manager_add_dimension() {
    DimensionManager mgr("view_1");
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal);
    auto result = mgr.addDimension(dim);
    test(result == dim, "DimensionManager adds dimension successfully");
    test(mgr.getDimension("lin_1") != nullptr, "DimensionManager retrieves dimension");
    test(mgr.getDimensions().size() == 1, "DimensionManager dimension count is 1");
}

void test_dimension_manager_remove_dimension() {
    DimensionManager mgr("view_1");
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal);
    mgr.addDimension(dim);
    mgr.removeDimension("lin_1");
    test(mgr.getDimension("lin_1") == nullptr, "DimensionManager removes dimension");
    test(mgr.getDimensions().size() == 0, "DimensionManager dimension count is 0 after removal");
}

void test_dimension_manager_add_annotation() {
    DimensionManager mgr("view_1");
    auto ann = std::make_shared<TextAnnotation>("txt_1");
    auto result = mgr.addAnnotation(ann);
    test(result == ann, "DimensionManager adds annotation");
    test(mgr.getAnnotation("txt_1") != nullptr, "DimensionManager retrieves annotation");
    test(mgr.getAnnotations().size() == 1, "DimensionManager annotation count is 1");
}

void test_dimension_manager_remove_annotation() {
    DimensionManager mgr("view_1");
    auto ann = std::make_shared<TextAnnotation>("txt_1");
    mgr.addAnnotation(ann);
    mgr.removeAnnotation("txt_1");
    test(mgr.getAnnotation("txt_1") == nullptr, "DimensionManager removes annotation");
    test(mgr.getAnnotations().size() == 0, "DimensionManager annotation count is 0");
}

void test_dimension_manager_get_by_type() {
    DimensionManager mgr("view_1");
    mgr.addDimension(std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal));
    mgr.addDimension(std::make_shared<LinearDimension>("lin_2", LinearDimensionSubType::Vertical));
    mgr.addDimension(std::make_shared<AngularDimension>("ang_1"));

    auto linear_dims = mgr.getDimensionsByType(DimensionType::Linear);
    test(linear_dims.size() == 2, "DimensionManager returns 2 linear dimensions");

    auto angular_dims = mgr.getDimensionsByType(DimensionType::Angular);
    test(angular_dims.size() == 1, "DimensionManager returns 1 angular dimension");
}

void test_dimension_manager_entity_linking() {
    DimensionManager mgr("view_1");
    auto dim = std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal);
    mgr.addDimension(dim);

    mgr.linkDimensionToEntity("lin_1", "edge_1");
    auto dims = mgr.getDimensionsForEntity("edge_1");
    test(dims.size() == 1, "DimensionManager links dimension to entity");
    test(dims[0]->getId() == "lin_1", "DimensionManager linked dimension is correct");
}

void test_dimension_manager_clear() {
    DimensionManager mgr("view_1");
    mgr.addDimension(std::make_shared<LinearDimension>("lin_1", LinearDimensionSubType::Horizontal));
    mgr.addAnnotation(std::make_shared<TextAnnotation>("txt_1"));

    test(mgr.getTotalElementCount() == 2, "DimensionManager has 2 elements");
    mgr.clear();
    test(mgr.getTotalElementCount() == 0, "DimensionManager clear removes all elements");
}

// ============================================================================
// Utility Functions Tests
// ============================================================================

void test_calculate_distance() {
    double dist1 = calculateDistance({0.0, 0.0}, {3.0, 4.0});
    test(near(dist1, 5.0), "calculateDistance for 3-4-5 triangle");

    double dist2 = calculateDistance({0.0, 0.0}, {1.0, 1.0});
    test(near(dist2, std::sqrt(2.0)), "calculateDistance for 1-1-sqrt(2) triangle");
}

void test_format_dimension_value() {
    std::string str1 = formatDimensionValue(50.0, 1, "mm");
    test(str1 == "50.0 mm", "formatDimensionValue with precision 1");

    std::string str2 = formatDimensionValue(25.123, 2, "mm");
    test(str2 == "25.12 mm", "formatDimensionValue with precision 2");
}

void test_dimension_type_to_string() {
    test(dimensionTypeToString(DimensionType::Linear) == "Linear", "dimensionTypeToString Linear");
    test(dimensionTypeToString(DimensionType::Radial) == "Radial", "dimensionTypeToString Radial");
    test(dimensionTypeToString(DimensionType::Angular) == "Angular", "dimensionTypeToString Angular");
}

void test_annotation_type_to_string() {
    test(annotationTypeToString(AnnotationType::TextNote) == "TextNote", "annotationTypeToString TextNote");
    test(annotationTypeToString(AnnotationType::WeldSymbol) == "WeldSymbol", "annotationTypeToString WeldSymbol");
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_dimension_with_properties() {
    auto dim = createLinearDimension("lin_1", LinearDimensionSubType::Horizontal, {0.0, 0.0}, {100.0, 0.0});

    auto& props = dim->getProperties();
    props.tolerance.type = ToleranceType::Bilateral;
    props.tolerance.upper_limit = 0.5;
    props.tolerance.lower_limit = 0.5;
    props.text_prefix = "DIM: ";

    test(near(dim->getValue(), 100.0), "Dimension value is 100.0");
    test(props.tolerance.type == ToleranceType::Bilateral, "Dimension tolerance type is Bilateral");

    std::string str = props.toString();
    test(str.find("DIM: ") != std::string::npos, "Dimension formatted string includes prefix");
    test(str.find("100.0") != std::string::npos, "Dimension formatted string includes value");
    test(str.find("+/-") != std::string::npos, "Dimension formatted string includes tolerance");
}

void test_complex_manager_scenario() {
    DimensionManager mgr("drawing_view_1");

    // Add multiple dimensions
    auto lin_dim = createLinearDimension("lin_1", LinearDimensionSubType::Horizontal, {0.0, 0.0}, {50.0, 0.0});
    auto rad_dim = createRadialDimension("rad_1", {0.0, 0.0}, {10.0, 0.0}, false);
    auto ang_dim = createAngularDimension("ang_1", {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0});

    mgr.addDimension(lin_dim);
    mgr.addDimension(rad_dim);
    mgr.addDimension(ang_dim);

    // Add annotations
    auto txt_ann = std::make_shared<TextAnnotation>("txt_1");
    txt_ann->setText("Detail view");
    mgr.addAnnotation(txt_ann);

    auto weld_ann = std::make_shared<WeldSymbolAnnotation>("weld_1");
    weld_ann->setWeldType(WeldSymbolType::Fillet);
    mgr.addAnnotation(weld_ann);

    test(mgr.getDimensions().size() == 3, "Manager has 3 dimensions");
    test(mgr.getAnnotations().size() == 2, "Manager has 2 annotations");
    test(mgr.getTotalElementCount() == 5, "Manager has 5 total elements");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "Running M4 Phase - Annotation & Dimensioning System Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // ToleranceSpec tests
    test_tolerance_spec_none();
    test_tolerance_spec_bilateral();
    test_tolerance_spec_unilateral();
    test_tolerance_spec_limit();
    test_tolerance_spec_invalid_limit();
    test_tolerance_spec_with_note();

    // DimensionProperties tests
    test_dimension_properties_default();
    test_dimension_properties_formatting();
    test_dimension_properties_with_tolerance();

    // LinearDimension tests
    test_linear_dimension_horizontal_creation();
    test_linear_dimension_vertical_creation();
    test_linear_dimension_aligned_creation();
    test_linear_dimension_value_calculation_horizontal();
    test_linear_dimension_value_calculation_vertical();
    test_linear_dimension_value_calculation_aligned();
    test_linear_dimension_offset();

    // RadialDimension tests
    test_radial_dimension_radius();
    test_radial_dimension_diameter();
    test_radial_dimension_value_radius();
    test_radial_dimension_value_diameter();

    // AngularDimension tests
    test_angular_dimension_creation();
    test_angular_dimension_90_degrees();
    test_angular_dimension_45_degrees();

    // OrdinateDimension tests
    test_ordinate_dimension_horizontal();
    test_ordinate_dimension_vertical();
    test_ordinate_dimension_value();

    // LeaderDimension tests
    test_leader_dimension_creation();
    test_leader_dimension_text();
    test_leader_dimension_points();

    // TextAnnotation tests
    test_text_annotation_creation();
    test_text_annotation_properties();
    test_text_annotation_rotation();

    // RevisionCloudAnnotation tests
    test_revision_cloud_creation();
    test_revision_cloud_properties();

    // WeldSymbolAnnotation tests
    test_weld_symbol_creation();
    test_weld_symbol_properties();

    // SurfaceFinishAnnotation tests
    test_surface_finish_annotation();
    test_surface_finish_properties();

    // GDandTAnnotation tests
    test_gd_and_t_annotation();
    test_gd_and_t_properties();

    // ReferenceTagAnnotation tests
    test_reference_tag_annotation();
    test_reference_tag_properties();

    // BalloonAnnotation tests
    test_balloon_annotation();
    test_balloon_annotation_properties();

    // DimensionManager tests
    test_dimension_manager_creation();
    test_dimension_manager_add_dimension();
    test_dimension_manager_remove_dimension();
    test_dimension_manager_add_annotation();
    test_dimension_manager_remove_annotation();
    test_dimension_manager_get_by_type();
    test_dimension_manager_entity_linking();
    test_dimension_manager_clear();

    // Utility function tests
    test_calculate_distance();
    test_format_dimension_value();
    test_dimension_type_to_string();
    test_annotation_type_to_string();

    // Integration tests
    test_dimension_with_properties();
    test_complex_manager_scenario();

    // Summary
    std::cout << std::endl;
    std::cout << "==========================================================" << std::endl;
    std::cout << "Tests: " << test_count << ", Failures: " << failures << std::endl;

    if (failures == 0) {
        std::cout << "SUCCESS: All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE: " << failures << " test(s) failed" << std::endl;
        return 1;
    }
}
