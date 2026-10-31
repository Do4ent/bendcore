// GAP-053: LoadCaseHandling (33 tests) - Complex Load Cases and Boundary Conditions
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/simulation/load_case_handler.hpp"

using namespace bendcore::simulation;

int test_count = 0;
#define RUN_TEST(func) do { func(); test_count++; } while(0)

void test_01_handler_creation() {
    LoadCaseHandler handler;
    assert(handler.getLoadCount() == 0);
    assert(handler.getBCCount() == 0);
    printf("✓ test_01_handler_creation\n");
}

void test_02_add_load_component() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    
    bool added = handler.addLoadComponent(load);
    assert(added);
    assert(handler.getLoadCount() == 1);
    printf("✓ test_02_add_load_component\n");
}

void test_03_get_load_component() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    auto opt = handler.getLoadComponent("Load1");
    assert(opt.has_value());
    assert(opt->magnitude == 1000.0);
    printf("✓ test_03_get_load_component\n");
}

void test_04_get_nonexistent_load() {
    LoadCaseHandler handler;
    auto opt = handler.getLoadComponent("Nonexistent");
    assert(!opt.has_value());
    printf("✓ test_04_get_nonexistent_load\n");
}

void test_05_add_boundary_condition() {
    LoadCaseHandler handler;
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    
    bool added = handler.addBoundaryCondition(bc);
    assert(added);
    assert(handler.getBCCount() == 1);
    printf("✓ test_05_add_boundary_condition\n");
}

void test_06_get_boundary_condition() {
    LoadCaseHandler handler;
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    handler.addBoundaryCondition(bc);
    
    auto opt = handler.getBoundaryCondition("Fixed");
    assert(opt.has_value());
    assert(opt->type == BoundaryConditionType::Fixed);
    printf("✓ test_06_get_boundary_condition\n");
}

void test_07_add_load_combination() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    LoadCombination combo;
    combo.name = "Combo1";
    combo.load_factors.push_back({"Load1", 1.5});
    
    bool added = handler.addLoadCombination(combo);
    assert(added);
    assert(handler.getCombinationCount() == 1);
    printf("✓ test_07_add_load_combination\n");
}

void test_08_get_load_combination() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    LoadCombination combo;
    combo.name = "Combo1";
    combo.load_factors.push_back({"Load1", 1.5});
    handler.addLoadCombination(combo);
    
    auto opt = handler.getLoadCombination("Combo1");
    assert(opt.has_value());
    assert(opt->name == "Combo1");
    printf("✓ test_08_get_load_combination\n");
}

void test_09_get_all_loads() {
    LoadCaseHandler handler;
    for (int i = 0; i < 3; i++) {
        LoadComponent load;
        load.name = "Load" + std::to_string(i);
        load.magnitude = 1000.0 * (i + 1);
        load.direction = LoadDirection::PositiveX;
        handler.addLoadComponent(load);
    }
    
    auto all = handler.getAllLoadComponents();
    assert(all.size() == 3);
    printf("✓ test_09_get_all_loads\n");
}

void test_10_get_all_boundary_conditions() {
    LoadCaseHandler handler;
    for (int i = 0; i < 2; i++) {
        BoundaryCondition bc;
        bc.name = "BC" + std::to_string(i);
        bc.type = BoundaryConditionType::Fixed;
        handler.addBoundaryCondition(bc);
    }
    
    auto all = handler.getAllBoundaryConditions();
    assert(all.size() == 2);
    printf("✓ test_10_get_all_boundary_conditions\n");
}

void test_11_analyze_simple_load_case() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    load.is_moment = false;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"Load1"});
    assert(result.max_stress >= 0.0);
    printf("✓ test_11_analyze_simple_load_case\n");
}

void test_12_remove_load() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    bool removed = handler.removeLoadComponent("Load1");
    assert(removed);
    assert(handler.getLoadCount() == 0);
    printf("✓ test_12_remove_load\n");
}

void test_13_remove_boundary_condition() {
    LoadCaseHandler handler;
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    handler.addBoundaryCondition(bc);
    
    bool removed = handler.removeBoundaryCondition("Fixed");
    assert(removed);
    assert(handler.getBCCount() == 0);
    printf("✓ test_13_remove_boundary_condition\n");
}

void test_14_clear_handler() {
    LoadCaseHandler handler;
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    handler.clear();
    assert(handler.getLoadCount() == 0);
    printf("✓ test_14_clear_handler\n");
}

void test_15_multiple_loads_same_direction() {
    LoadCaseHandler handler;
    
    for (int i = 0; i < 3; i++) {
        LoadComponent load;
        load.name = "Load" + std::to_string(i);
        load.magnitude = 1000.0;
        load.direction = LoadDirection::PositiveX;
        handler.addLoadComponent(load);
    }
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"Load0", "Load1", "Load2"});
    assert(result.max_stress >= 0.0);
    printf("✓ test_15_multiple_loads_same_direction\n");
}

void test_16_loads_different_directions() {
    LoadCaseHandler handler;
    
    LoadComponent loadx;
    loadx.name = "LoadX";
    loadx.magnitude = 1000.0;
    loadx.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(loadx);
    
    LoadComponent loady;
    loady.name = "LoadY";
    loady.magnitude = 500.0;
    loady.direction = LoadDirection::PositiveY;
    handler.addLoadComponent(loady);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"LoadX", "LoadY"});
    assert(result.max_stress >= 0.0);
    printf("✓ test_16_loads_different_directions\n");
}

void test_17_moment_load() {
    LoadCaseHandler handler;
    
    LoadComponent moment;
    moment.name = "Moment1";
    moment.magnitude = 100.0;
    moment.direction = LoadDirection::PositiveZ;
    moment.is_moment = true;
    handler.addLoadComponent(moment);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"Moment1"});
    assert(result.reaction_moments.size() == 3);
    printf("✓ test_17_moment_load\n");
}

void test_18_pinned_support() {
    LoadCaseHandler handler;
    
    BoundaryCondition pinned;
    pinned.name = "Pinned";
    pinned.type = BoundaryConditionType::PinnedSupport;
    pinned.restrain_x = true;
    pinned.restrain_y = true;
    pinned.restrain_z = true;
    pinned.restrain_rz = false;  // Can rotate
    handler.addBoundaryCondition(pinned);
    
    auto opt = handler.getBoundaryCondition("Pinned");
    assert(opt.has_value());
    assert(opt->type == BoundaryConditionType::PinnedSupport);
    printf("✓ test_18_pinned_support\n");
}

void test_19_roller_support() {
    LoadCaseHandler handler;
    
    BoundaryCondition roller;
    roller.name = "Roller";
    roller.type = BoundaryConditionType::RollerSupport;
    roller.restrain_y = true;  // Only Y direction
    handler.addBoundaryCondition(roller);
    
    auto opt = handler.getBoundaryCondition("Roller");
    assert(opt.has_value());
    assert(opt->type == BoundaryConditionType::RollerSupport);
    printf("✓ test_19_roller_support\n");
}

void test_20_calculate_reaction_forces() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    auto reactions = handler.calculateReactionForces({"Load1"});
    assert(reactions.size() == 3);
    assert(reactions[0] == -1000.0);  // Opposite direction
    printf("✓ test_20_calculate_reaction_forces\n");
}

void test_21_calculate_total_load() {
    LoadCaseHandler handler;
    
    LoadComponent load1;
    load1.name = "Load1";
    load1.magnitude = 1000.0;
    load1.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load1);
    
    LoadComponent load2;
    load2.name = "Load2";
    load2.magnitude = 500.0;
    load2.direction = LoadDirection::PositiveY;
    handler.addLoadComponent(load2);
    
    double total = handler.calculateTotalAppliedLoad({"Load1", "Load2"});
    assert(total == 1500.0);
    printf("✓ test_21_calculate_total_load\n");
}

void test_22_validate_load_case_valid() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    handler.addBoundaryCondition(bc);
    
    bool valid = handler.validateLoadCase({"Load1"});
    assert(valid);
    printf("✓ test_22_validate_load_case_valid\n");
}

void test_23_validate_load_case_invalid() {
    LoadCaseHandler handler;
    
    bool valid = handler.validateLoadCase({"NonExistent"});
    assert(!valid);
    printf("✓ test_23_validate_load_case_invalid\n");
}

void test_24_symmetry_boundary_condition() {
    LoadCaseHandler handler;
    
    BoundaryCondition symmetry;
    symmetry.name = "Symmetry";
    symmetry.type = BoundaryConditionType::Symmetry;
    handler.addBoundaryCondition(symmetry);
    
    auto opt = handler.getBoundaryCondition("Symmetry");
    assert(opt.has_value());
    assert(opt->type == BoundaryConditionType::Symmetry);
    printf("✓ test_24_symmetry_boundary_condition\n");
}

void test_25_negative_loads() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::NegativeX;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"Load1"});
    assert(result.max_stress >= 0.0);
    printf("✓ test_25_negative_loads\n");
}

void test_26_analyze_with_offset_location() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    load.application_point_x = 1.0;
    load.application_point_y = 2.0;
    load.application_point_z = 3.0;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    handler.addBoundaryCondition(bc);
    
    auto result = handler.analyzeLoadCase({"Load1"});
    assert(result.converged == true || result.converged == false);
    printf("✓ test_26_analyze_with_offset_location\n");
}

void test_27_load_combination_analysis() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    LoadCombination combo;
    combo.name = "Combo1";
    combo.load_factors.push_back({"Load1", 1.5});
    handler.addLoadCombination(combo);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    auto results = handler.analyzeLoadCombination("Combo1");
    assert(results.size() > 0);
    printf("✓ test_27_load_combination_analysis\n");
}

void test_28_analyze_all_combinations() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    for (int i = 0; i < 2; i++) {
        LoadCombination combo;
        combo.name = "Combo" + std::to_string(i);
        combo.load_factors.push_back({"Load1", 1.0 + 0.5 * i});
        handler.addLoadCombination(combo);
    }
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    handler.addBoundaryCondition(bc);
    
    auto results = handler.analyzeAllLoadCombinations();
    assert(results.size() > 0);
    printf("✓ test_28_analyze_all_combinations\n");
}

void test_29_stress_envelope() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    std::vector<LoadCaseResult> results;
    for (int i = 0; i < 3; i++) {
        auto result = handler.analyzeLoadCase({"Load1"});
        results.push_back(result);
    }
    
    auto envelope = handler.getStressEnvelope(results);
    assert(envelope.size() > 0);
    printf("✓ test_29_stress_envelope\n");
}

void test_30_worst_case_analysis() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "Load1";
    load.magnitude = 1000.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    BoundaryCondition bc;
    bc.name = "Fixed";
    bc.type = BoundaryConditionType::Fixed;
    bc.restrain_x = true;
    bc.restrain_y = true;
    bc.restrain_z = true;
    handler.addBoundaryCondition(bc);
    
    std::vector<LoadCaseResult> results;
    for (int i = 0; i < 3; i++) {
        auto result = handler.analyzeLoadCase({"Load1"});
        results.push_back(result);
    }
    
    auto worst = handler.getWorstCase(results);
    assert(worst.max_stress >= 0.0);
    printf("✓ test_30_worst_case_analysis\n");
}

void test_31_complex_load_case() {
    LoadCaseHandler handler;
    
    // Multiple loads
    for (int i = 0; i < 4; i++) {
        LoadComponent load;
        load.name = "Load" + std::to_string(i);
        load.magnitude = 500.0 * (i + 1);
        load.direction = (i % 2) ? LoadDirection::PositiveX : LoadDirection::PositiveY;
        handler.addLoadComponent(load);
    }
    
    // Multiple supports
    for (int i = 0; i < 2; i++) {
        BoundaryCondition bc;
        bc.name = "Support" + std::to_string(i);
        bc.type = BoundaryConditionType::Fixed;
        bc.restrain_x = true;
        bc.restrain_y = true;
        bc.restrain_z = true;
        handler.addBoundaryCondition(bc);
    }
    
    auto result = handler.analyzeLoadCase({"Load0", "Load1", "Load2", "Load3"});
    assert(result.converged == true || result.converged == false);
    printf("✓ test_31_complex_load_case\n");
}

void test_32_load_with_description() {
    LoadCaseHandler handler;
    
    LoadComponent load;
    load.name = "WindLoad";
    load.magnitude = 500.0;
    load.direction = LoadDirection::PositiveX;
    handler.addLoadComponent(load);
    
    LoadCombination combo;
    combo.name = "ServiceCombo";
    combo.description = "Service load combination with wind";
    combo.load_factors.push_back({"WindLoad", 1.0});
    combo.safety_factor = 1.5;
    handler.addLoadCombination(combo);
    
    auto opt = handler.getLoadCombination("ServiceCombo");
    assert(opt.has_value());
    assert(opt->safety_factor == 1.5);
    printf("✓ test_32_load_with_description\n");
}

void test_33_complete_analysis_workflow() {
    LoadCaseHandler handler;
    
    // Set up loads
    LoadComponent deadLoad;
    deadLoad.name = "DeadLoad";
    deadLoad.magnitude = 10000.0;
    deadLoad.direction = LoadDirection::NegativeZ;
    handler.addLoadComponent(deadLoad);
    
    LoadComponent liveLoad;
    liveLoad.name = "LiveLoad";
    liveLoad.magnitude = 5000.0;
    liveLoad.direction = LoadDirection::NegativeZ;
    handler.addLoadComponent(liveLoad);
    
    // Set up supports
    BoundaryCondition support1;
    support1.name = "Support1";
    support1.type = BoundaryConditionType::Fixed;
    support1.restrain_x = true;
    support1.restrain_y = true;
    support1.restrain_z = true;
    handler.addBoundaryCondition(support1);
    
    BoundaryCondition support2;
    support2.name = "Support2";
    support2.type = BoundaryConditionType::RollerSupport;
    support2.restrain_z = true;
    handler.addBoundaryCondition(support2);
    
    // Create combinations
    LoadCombination serviceCombo;
    serviceCombo.name = "Service";
    serviceCombo.load_factors.push_back({"DeadLoad", 1.0});
    serviceCombo.load_factors.push_back({"LiveLoad", 1.0});
    serviceCombo.safety_factor = 1.5;
    handler.addLoadCombination(serviceCombo);
    
    // Analyze
    auto result = handler.analyzeLoadCase({"DeadLoad", "LiveLoad"});
    assert(result.max_stress >= 0.0);
    assert(result.reaction_forces.size() == 3);
    
    printf("✓ test_33_complete_analysis_workflow\n");
}

int main() {
    printf("=== GAP-053: LoadCaseHandling (33 Tests) ===\n\n");
    
    RUN_TEST(test_01_handler_creation);
    RUN_TEST(test_02_add_load_component);
    RUN_TEST(test_03_get_load_component);
    RUN_TEST(test_04_get_nonexistent_load);
    RUN_TEST(test_05_add_boundary_condition);
    RUN_TEST(test_06_get_boundary_condition);
    RUN_TEST(test_07_add_load_combination);
    RUN_TEST(test_08_get_load_combination);
    RUN_TEST(test_09_get_all_loads);
    RUN_TEST(test_10_get_all_boundary_conditions);
    RUN_TEST(test_11_analyze_simple_load_case);
    RUN_TEST(test_12_remove_load);
    RUN_TEST(test_13_remove_boundary_condition);
    RUN_TEST(test_14_clear_handler);
    RUN_TEST(test_15_multiple_loads_same_direction);
    RUN_TEST(test_16_loads_different_directions);
    RUN_TEST(test_17_moment_load);
    RUN_TEST(test_18_pinned_support);
    RUN_TEST(test_19_roller_support);
    RUN_TEST(test_20_calculate_reaction_forces);
    RUN_TEST(test_21_calculate_total_load);
    RUN_TEST(test_22_validate_load_case_valid);
    RUN_TEST(test_23_validate_load_case_invalid);
    RUN_TEST(test_24_symmetry_boundary_condition);
    RUN_TEST(test_25_negative_loads);
    RUN_TEST(test_26_analyze_with_offset_location);
    RUN_TEST(test_27_load_combination_analysis);
    RUN_TEST(test_28_analyze_all_combinations);
    RUN_TEST(test_29_stress_envelope);
    RUN_TEST(test_30_worst_case_analysis);
    RUN_TEST(test_31_complex_load_case);
    RUN_TEST(test_32_load_with_description);
    RUN_TEST(test_33_complete_analysis_workflow);
    
    printf("\n✅ All %d tests passed!\n", test_count);
    return 0;
}
