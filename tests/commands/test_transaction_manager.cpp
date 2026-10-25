#include <cassert>
#include <iostream>

#include "bendcore/commands/transaction_manager.hpp"

using namespace bendcore::commands;
using namespace bendcore::document;

void test_transaction_success() {
    std::cout << "Testing successful transaction...\n";

    CadDocument doc;
    TransactionManager tm(doc);

    tm.begin_transaction();
    assert(tm.in_transaction());

    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult result = tm.execute_command(create_part);
    assert(result.success);
    assert(tm.transaction_size() == 1);

    bool commit_success = tm.commit();
    assert(commit_success);
    assert(!tm.in_transaction());

    auto parts = doc.all_parts();
    assert(parts.size() == 1);

    std::cout << "✓ Transaction success passed\n";
}

void test_transaction_multiple_commands() {
    std::cout << "Testing transaction with multiple commands...\n";

    CadDocument doc;
    TransactionManager tm(doc);

    tm.begin_transaction();

    // Create Part
    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult part_result = tm.execute_command(create_part);
    assert(part_result.success);

    // Create Instance
    std::string part_id = part_result.result_data["part_id"];
    Command create_inst("CreateInstance");
    create_inst.args["part_id"] = part_id;
    CommandResult inst_result = tm.execute_command(create_inst);
    assert(inst_result.success);

    // Commit both
    bool commit_success = tm.commit();
    assert(commit_success);
    assert(tm.transaction_size() == 0);  // Cleared after commit

    auto parts = doc.all_parts();
    auto instances = doc.all_instances();
    assert(parts.size() == 1);
    assert(instances.size() == 1);

    std::cout << "✓ Transaction multiple commands passed\n";
}

void test_transaction_failure() {
    std::cout << "Testing transaction with command failure...\n";

    CadDocument doc;
    TransactionManager tm(doc);

    tm.begin_transaction();

    // Valid command
    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    CommandResult part_result = tm.execute_command(create_part);
    assert(part_result.success);

    // Invalid command (missing required field)
    Command invalid_cmd("CreatePart");
    // No designation field
    CommandResult invalid_result = tm.execute_command(invalid_cmd);
    assert(!invalid_result.success);

    // Commit should fail
    bool commit_success = tm.commit();
    assert(!commit_success);
    assert(!tm.in_transaction());

    std::cout << "✓ Transaction failure passed\n";
}

void test_transaction_no_overlap() {
    std::cout << "Testing transactions don't overlap...\n";

    CadDocument doc;
    TransactionManager tm(doc);

    tm.begin_transaction();
    Command create_part1("CreatePart");
    create_part1.args["designation"] = "PART-001";
    tm.execute_command(create_part1);
    tm.commit();

    // Should be able to start new transaction
    tm.begin_transaction();
    assert(tm.in_transaction());
    Command create_part2("CreatePart");
    create_part2.args["designation"] = "PART-002";
    tm.execute_command(create_part2);
    tm.commit();

    auto parts = doc.all_parts();
    assert(parts.size() == 2);

    std::cout << "✓ Transaction no overlap passed\n";
}

void test_rollback() {
    std::cout << "Testing rollback...\n";

    CadDocument doc;
    TransactionManager tm(doc);

    tm.begin_transaction();
    Command create_part("CreatePart");
    create_part.args["designation"] = "PART-001";
    tm.execute_command(create_part);

    // Rollback
    tm.rollback();
    assert(!tm.in_transaction());
    assert(tm.transaction_size() == 0);

    // Part should still exist (transaction manager doesn't actually undo yet)
    // This is expected for Phase B MVP - full undo support comes with HistoryStack

    std::cout << "✓ Rollback passed\n";
}

int main() {
    std::cout << "=== CAD-CORE-006: TransactionManager Tests ===\n\n";

    test_transaction_success();
    test_transaction_multiple_commands();
    test_transaction_failure();
    test_transaction_no_overlap();
    test_rollback();

    std::cout << "\n✓ All CAD-CORE-006 TransactionManager tests passed!\n";
    return 0;
}
