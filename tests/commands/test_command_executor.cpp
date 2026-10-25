#include <cassert>
#include <iostream>

#include "bendcore/commands/command_executor.hpp"

using namespace bendcore::commands;
using namespace bendcore::document;

void test_create_part_success() {
    std::cout << "Testing CreatePart command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    Command cmd("CreatePart");
    cmd.args["designation"] = "PART-001";

    CommandResult result = executor.execute(cmd);
    assert(result.success);
    assert(result.result_data.contains("part_id"));

    auto parts = doc.all_parts();
    assert(parts.size() == 1);

    std::cout << "✓ CreatePart success passed\n";
}

void test_create_instance_success() {
    std::cout << "Testing CreateInstance command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    // First create a Part
    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult part_result = executor.execute(create_part);
    assert(part_result.success);

    std::string part_id_str = part_result.result_data["part_id"];

    // Then create an Instance
    Command create_inst("CreateInstance");
    create_inst.args["part_id"] = part_id_str;

    CommandResult inst_result = executor.execute(create_inst);
    assert(inst_result.success);
    assert(inst_result.result_data.contains("instance_id"));

    auto instances = doc.all_instances();
    assert(instances.size() == 1);

    std::cout << "✓ CreateInstance success passed\n";
}

void test_set_parent_success() {
    std::cout << "Testing SetParent command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    // Create Part and Instance
    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult part_result = executor.execute(create_part);
    std::string part_id = part_result.result_data["part_id"];

    Command create_inst("CreateInstance");
    create_inst.args["part_id"] = part_id;
    CommandResult inst_result = executor.execute(create_inst);
    std::string inst_id = inst_result.result_data["instance_id"];

    // Set parent
    Command set_parent("SetParent");
    set_parent.args["child_id"] = inst_id;
    set_parent.args["parent_id"] = part_id;

    CommandResult result = executor.execute(set_parent);
    assert(result.success);

    auto parent = doc.parent_of(EntityId::from_string(inst_id).value());
    assert(parent == EntityId::from_string(part_id).value());

    std::cout << "✓ SetParent success passed\n";
}

void test_set_parent_cycle_prevention() {
    std::cout << "Testing SetParent cycle prevention...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    Command create_part1("CreatePart");
    create_part1.args["designation"] = "PART-001";
    CommandResult part1_result = executor.execute(create_part1);
    std::string part1_id = part1_result.result_data["part_id"];

    Command create_part2("CreatePart");
    create_part2.args["designation"] = "PART-002";
    CommandResult part2_result = executor.execute(create_part2);
    std::string part2_id = part2_result.result_data["part_id"];

    // Set Part2 as child of Part1
    Command set_parent1("SetParent");
    set_parent1.args["child_id"] = part2_id;
    set_parent1.args["parent_id"] = part1_id;
    CommandResult result1 = executor.execute(set_parent1);
    assert(result1.success);

    // Try to create a cycle (Part1 as child of Part2)
    Command set_parent2("SetParent");
    set_parent2.args["child_id"] = part1_id;
    set_parent2.args["parent_id"] = part2_id;
    CommandResult result2 = executor.execute(set_parent2);
    assert(!result2.success);  // Should fail due to cycle

    std::cout << "✓ SetParent cycle prevention passed\n";
}

void test_delete_entity() {
    std::cout << "Testing DeleteEntity command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult part_result = executor.execute(create_part);
    std::string part_id = part_result.result_data["part_id"];

    // Delete the part
    Command delete_cmd("DeleteEntity");
    delete_cmd.args["entity_id"] = part_id;

    CommandResult result = executor.execute(delete_cmd);
    assert(result.success);
    assert(result.result_data["deleted_count"] == 1);

    std::cout << "✓ DeleteEntity passed\n";
}

void test_edit_part() {
    std::cout << "Testing EditPart command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    Command create_part("CreatePart");
    create_part.args["designation"] = "OLD-PART";
    CommandResult part_result = executor.execute(create_part);
    std::string part_id = part_result.result_data["part_id"];

    // Edit the part
    Command edit_cmd("EditPart");
    edit_cmd.args["part_id"] = part_id;
    edit_cmd.args["designation"] = "NEW-PART";

    CommandResult result = executor.execute(edit_cmd);
    assert(result.success);
    assert(result.result_data["old_designation"] == "OLD-PART");
    assert(result.result_data["new_designation"] == "NEW-PART");

    auto* part = doc.get_entity<Part>(EntityId::from_string(part_id).value());
    assert(part->designation == "NEW-PART");

    std::cout << "✓ EditPart passed\n";
}

void test_batch_move() {
    std::cout << "Testing BatchMove command...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    // Create 2 Parts with Instances
    Command create_part1("CreatePart");
    create_part1.args["designation"] = "PART-001";
    CommandResult part1_result = executor.execute(create_part1);
    std::string part1_id = part1_result.result_data["part_id"];

    Command create_part2("CreatePart");
    create_part2.args["designation"] = "PART-002";
    CommandResult part2_result = executor.execute(create_part2);
    std::string part2_id = part2_result.result_data["part_id"];

    Command create_inst("CreateInstance");
    create_inst.args["part_id"] = part1_id;
    CommandResult inst_result = executor.execute(create_inst);
    std::string inst_id = inst_result.result_data["instance_id"];

    // Batch move instance to part2
    Command batch_move("BatchMove");
    batch_move.args["moves"] = json::array();
    batch_move.args["moves"].push_back(json{{"child_id", inst_id}, {"parent_id", part2_id}});

    CommandResult result = executor.execute(batch_move);
    assert(result.success);
    assert(result.result_data["success_count"] == 1);

    std::cout << "✓ BatchMove passed\n";
}

void test_invalid_create_part() {
    std::cout << "Testing invalid CreatePart...\n";

    CadDocument doc;
    CommandExecutor executor(doc);

    Command cmd("CreatePart");
    // Missing designation field
    CommandResult result = executor.execute(cmd);
    assert(!result.success);

    std::cout << "✓ Invalid CreatePart passed\n";
}

int main() {
    std::cout << "=== CAD-CORE-005: CommandExecutor Tests ===\n\n";

    test_create_part_success();
    test_create_instance_success();
    test_set_parent_success();
    test_set_parent_cycle_prevention();
    test_delete_entity();
    test_edit_part();
    test_batch_move();
    test_invalid_create_part();

    std::cout << "\n✓ All CAD-CORE-005 CommandExecutor tests passed!\n";
    return 0;
}
