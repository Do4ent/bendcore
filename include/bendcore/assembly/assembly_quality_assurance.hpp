#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Dimension verification result
 */
struct DimensionCheck {
    EntityId dimension_id;
    std::string dimension_name;
    double nominal_value = 0.0;
    double lower_tolerance = 0.0;
    double upper_tolerance = 0.0;
    double measured_value = 0.0;
    bool is_within_tolerance = false;
    std::string status;  // OK, FAIL, WARN
};

/**
 * @brief Functional test case
 */
struct FunctionalTest {
    EntityId test_id;
    std::string test_name;
    std::string description;
    std::string test_procedure;
    std::vector<std::string> acceptance_criteria;
    bool test_passed = false;
    std::string notes;
};

/**
 * @brief Inspection procedure step
 */
struct InspectionStep {
    uint32_t step_number = 0;
    std::string description;
    std::vector<DimensionCheck> dimensions_to_check;
    std::vector<std::string> visual_checks;
    std::string inspector_name;
    std::string inspection_date;
    bool step_approved = false;
};

/**
 * @brief Quality inspection report
 */
struct InspectionReport {
    EntityId assembly_id;
    std::string assembly_name;
    std::string inspection_date;
    std::string inspector_name;
    uint32_t revision = 0;
    std::vector<InspectionStep> inspection_steps;
    std::vector<DimensionCheck> dimension_results;
    std::vector<FunctionalTest> functional_test_results;
    bool overall_approved = false;
    std::string sign_off_name;
    std::string sign_off_date;
    std::string sign_off_title;
};

/**
 * @brief Assembly Quality Assurance - Verification and inspection
 *
 * Provides:
 * - Assembly verification
 * - Dimension checking
 * - Functional testing
 * - Inspection procedures
 * - Sign-off requirements
 */
class AssemblyQualityAssurance {
public:
    /**
     * @brief Constructor
     * @param assembly_id Assembly ID
     */
    explicit AssemblyQualityAssurance(const EntityId& assembly_id) noexcept
        : assembly_id_(assembly_id) {}

    /**
     * @brief Add a dimension to check
     * @param dimension Dimension check specification
     * @return Dimension ID
     */
    EntityId addDimensionCheck(const DimensionCheck& dimension);

    /**
     * @brief Update dimension measurement
     * @param dimension_id Dimension ID
     * @param measured_value Measured value
     * @return true if updated, false if not found
     */
    bool updateDimensionMeasurement(const EntityId& dimension_id, double measured_value) noexcept;

    /**
     * @brief Get dimension check
     * @param dimension_id Dimension ID
     * @return Dimension check, or empty optional if not found
     */
    [[nodiscard]] std::optional<DimensionCheck> getDimensionCheck(
        const EntityId& dimension_id) const noexcept;

    /**
     * @brief Get all dimension checks
     */
    [[nodiscard]] const std::vector<DimensionCheck>& getAllDimensionChecks() const noexcept {
        return dimension_checks_;
    }

    /**
     * @brief Verify all dimensions
     * @return true if all dimensions within tolerance, false otherwise
     */
    [[nodiscard]] bool verifyAllDimensions() const noexcept;

    /**
     * @brief Get dimension check results
     * @return Vector of failed dimension checks (empty if all pass)
     */
    [[nodiscard]] std::vector<DimensionCheck> getDimensionCheckResults() const;

    /**
     * @brief Add functional test
     * @param test Functional test specification
     * @return Test ID
     */
    EntityId addFunctionalTest(const FunctionalTest& test);

    /**
     * @brief Update functional test result
     * @param test_id Test ID
     * @param passed true if test passed
     * @return true if updated, false if not found
     */
    bool updateFunctionalTestResult(const EntityId& test_id, bool passed) noexcept;

    /**
     * @brief Get functional test
     * @param test_id Test ID
     * @return Functional test, or empty optional if not found
     */
    [[nodiscard]] std::optional<FunctionalTest> getFunctionalTest(
        const EntityId& test_id) const noexcept;

    /**
     * @brief Get all functional tests
     */
    [[nodiscard]] const std::vector<FunctionalTest>& getAllFunctionalTests() const noexcept {
        return functional_tests_;
    }

    /**
     * @brief Check if all functional tests passed
     */
    [[nodiscard]] bool allFunctionalTestsPassed() const noexcept;

    /**
     * @brief Add inspection step
     * @param step Inspection step
     * @return Step number
     */
    uint32_t addInspectionStep(const InspectionStep& step);

    /**
     * @brief Approve inspection step
     * @param step_number Step number to approve
     * @param approved true to approve, false to reject
     * @return true if approved, false if step not found
     */
    bool approveInspectionStep(uint32_t step_number, bool approved = true) noexcept;

    /**
     * @brief Get inspection step
     * @param step_number Step number
     * @return Inspection step, or empty optional if not found
     */
    [[nodiscard]] std::optional<InspectionStep> getInspectionStep(
        uint32_t step_number) const noexcept;

    /**
     * @brief Get all inspection steps
     */
    [[nodiscard]] const std::vector<InspectionStep>& getAllInspectionSteps() const noexcept {
        return inspection_steps_;
    }

    /**
     * @brief Check if assembly inspection is complete
     */
    [[nodiscard]] bool isInspectionComplete() const noexcept;

    /**
     * @brief Generate inspection report
     * @param inspector_name Inspector name
     * @param signed_by Sign-off name (if approved)
     * @return Inspection report
     */
    [[nodiscard]] InspectionReport generateInspectionReport(
        const std::string& inspector_name,
        const std::string& signed_by = "");

    /**
     * @brief Approve assembly for production
     * @param sign_off_name Name of person signing off
     * @param title Title of person signing off
     * @return true if approved, false if any checks failed
     */
    bool approveAssembly(const std::string& sign_off_name, const std::string& title = "Inspector");

    /**
     * @brief Check if assembly is approved
     */
    [[nodiscard]] bool isAssemblyApproved() const noexcept { return assembly_approved_; }

    /**
     * @brief Get approval sign-off name
     */
    [[nodiscard]] const std::string& getSignOffName() const noexcept { return sign_off_name_; }

    /**
     * @brief Get approval sign-off date
     */
    [[nodiscard]] const std::string& getSignOffDate() const noexcept { return sign_off_date_; }

    /**
     * @brief Export quality report as text
     */
    [[nodiscard]] std::string exportQualityReportAsText() const;

    /**
     * @brief Get quality score (0.0-100.0)
     * @return Quality score
     */
    [[nodiscard]] double getQualityScore() const;

private:
    EntityId assembly_id_;
    std::vector<DimensionCheck> dimension_checks_;
    std::vector<FunctionalTest> functional_tests_;
    std::vector<InspectionStep> inspection_steps_;
    bool assembly_approved_ = false;
    std::string sign_off_name_;
    std::string sign_off_date_;

    // Helper functions
    std::string getCurrentDate() const;
};

} // namespace BendCore::CadCore
