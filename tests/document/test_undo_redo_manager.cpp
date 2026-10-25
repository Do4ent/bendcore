#include "bendcore/document/undo_redo_manager.hpp"
#include "bendcore/document/cad_document.hpp"
#include <cassert>
#include <iostream>

using namespace BendCore::CadCore;

void test_undo_redo_creation() {
    std::cout << "Test: UndoRedo Manager Creation\n";
    UndoRedoManager manager(100, 100000000);

    assert(!manager.canUndo());
    assert(!manager.canRedo());
    assert(manager.getUndoCount() == 0);
    assert(manager.getRedoCount() == 0);

    std::cout << "✓ Manager creation passed\n";
}

void test_undo_redo_save_snapshot() {
    std::cout << "Test: Save Snapshot\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    bool result = manager.saveSnapshot(doc, "Added part");
    assert(result);
    assert(manager.canUndo());
    assert(manager.getUndoCount() == 1);

    std::cout << "✓ Save snapshot passed\n";
}

void test_undo_redo_multiple_snapshots() {
    std::cout << "Test: Multiple Snapshots\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    for (int i = 0; i < 5; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
        manager.saveSnapshot(doc, "Added part " + std::to_string(i));
    }

    assert(manager.getUndoCount() == 5);
    assert(!manager.canRedo());

    std::cout << "✓ Multiple snapshots passed\n";
}

void test_undo_redo_undo_operation() {
    std::cout << "Test: Undo Operation\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity1(EntityType::PART, "Part1");
    doc.addEntity(entity1);
    manager.saveSnapshot(doc, "Added Part1");

    Entity entity2(EntityType::PART, "Part2");
    doc.addEntity(entity2);
    manager.saveSnapshot(doc, "Added Part2");

    assert(manager.getUndoCount() == 2);
    assert(!manager.canRedo());

    bool undo_result = manager.undo(doc);
    assert(undo_result);
    assert(manager.getUndoCount() == 1);
    assert(manager.canRedo());

    std::cout << "✓ Undo operation passed\n";
}

void test_undo_redo_redo_operation() {
    std::cout << "Test: Redo Operation\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);
    manager.saveSnapshot(doc, "Added part");

    manager.undo(doc);
    assert(manager.canRedo());

    bool redo_result = manager.redo(doc);
    assert(redo_result);
    assert(!manager.canRedo());
    assert(manager.getRedoCount() == 0);

    std::cout << "✓ Redo operation passed\n";
}

void test_undo_redo_clear_redo_on_new_snapshot() {
    std::cout << "Test: Clear Redo on New Snapshot\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity1(EntityType::PART, "Part1");
    doc.addEntity(entity1);
    manager.saveSnapshot(doc, "Added Part1");

    manager.undo(doc);
    assert(manager.canRedo());

    // Save new snapshot - should clear redo stack
    Entity entity2(EntityType::PART, "Part2");
    doc.addEntity(entity2);
    manager.saveSnapshot(doc, "Added Part2");

    assert(!manager.canRedo());

    std::cout << "✓ Redo clear on new snapshot passed\n";
}

void test_undo_redo_branch_point() {
    std::cout << "Test: Branch Point\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity1(EntityType::PART, "Part1");
    doc.addEntity(entity1);
    manager.saveSnapshot(doc, "Added Part1");

    manager.createBranchPoint();

    Entity entity2(EntityType::PART, "Part2");
    doc.addEntity(entity2);
    manager.saveSnapshot(doc, "Added Part2");

    Entity entity3(EntityType::PART, "Part3");
    doc.addEntity(entity3);
    manager.saveSnapshot(doc, "Added Part3");

    bool restore_result = manager.restoreToBranchPoint(doc);
    assert(restore_result);

    std::cout << "✓ Branch point passed\n";
}

void test_undo_redo_history() {
    std::cout << "Test: Undo/Redo History\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    for (int i = 0; i < 5; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
        manager.saveSnapshot(doc, "Added part " + std::to_string(i));
    }

    auto undo_history = manager.getUndoHistory(3);
    assert(undo_history.size() == 3);
    assert(undo_history[0] == "Added part 4");
    assert(undo_history[1] == "Added part 3");
    assert(undo_history[2] == "Added part 2");

    std::cout << "✓ History passed\n";
}

void test_undo_redo_memory_limit() {
    std::cout << "Test: Memory Limit\n";
    CadDocument doc;
    UndoRedoManager manager(3, 50000);  // Only 3 snapshots or 50KB

    for (int i = 0; i < 10; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
        manager.saveSnapshot(doc, "Added part");
    }

    // Should have trimmed to max_snapshots (3)
    assert(manager.getUndoCount() <= 3);

    std::cout << "✓ Memory limit passed\n";
}

void test_undo_redo_clear() {
    std::cout << "Test: Clear\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);
    manager.saveSnapshot(doc, "Added part");

    assert(manager.canUndo());

    manager.clear();

    assert(!manager.canUndo());
    assert(manager.getUndoCount() == 0);
    assert(manager.getRedoCount() == 0);

    std::cout << "✓ Clear passed\n";
}

void test_undo_redo_snapshot_count() {
    std::cout << "Test: Snapshot Count\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    Entity entity1(EntityType::PART, "Part1");
    doc.addEntity(entity1);
    manager.saveSnapshot(doc, "Added Part1");

    manager.undo(doc);
    manager.redo(doc);

    Entity entity2(EntityType::PART, "Part2");
    doc.addEntity(entity2);
    manager.saveSnapshot(doc, "Added Part2");

    size_t total = manager.getSnapshotCount();
    assert(total >= 1);

    std::cout << "✓ Snapshot count passed\n";
}

void test_undo_redo_sequential_operations() {
    std::cout << "Test: Sequential Undo/Redo\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    // Create 3 snapshots
    for (int i = 0; i < 3; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
        manager.saveSnapshot(doc, "Added part " + std::to_string(i));
    }

    assert(manager.getUndoCount() == 3);

    // Undo twice
    manager.undo(doc);
    manager.undo(doc);
    assert(manager.getUndoCount() == 1);
    assert(manager.getRedoCount() == 2);

    // Redo once
    manager.redo(doc);
    assert(manager.getUndoCount() == 2);
    assert(manager.getRedoCount() == 1);

    std::cout << "✓ Sequential operations passed\n";
}

void test_undo_redo_no_undo_when_empty() {
    std::cout << "Test: No Undo When Empty\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    assert(!manager.canUndo());
    bool result = manager.undo(doc);
    assert(!result);

    std::cout << "✓ No undo when empty passed\n";
}

void test_undo_redo_no_redo_when_empty() {
    std::cout << "Test: No Redo When Empty\n";
    CadDocument doc;
    UndoRedoManager manager(100, 100000000);

    assert(!manager.canRedo());
    bool result = manager.redo(doc);
    assert(!result);

    std::cout << "✓ No redo when empty passed\n";
}

int main() {
    std::cout << "Running UndoRedo Manager Tests\n\n";

    try {
        test_undo_redo_creation();
        test_undo_redo_save_snapshot();
        test_undo_redo_multiple_snapshots();
        test_undo_redo_undo_operation();
        test_undo_redo_redo_operation();
        test_undo_redo_clear_redo_on_new_snapshot();
        test_undo_redo_branch_point();
        test_undo_redo_history();
        test_undo_redo_memory_limit();
        test_undo_redo_clear();
        test_undo_redo_snapshot_count();
        test_undo_redo_sequential_operations();
        test_undo_redo_no_undo_when_empty();
        test_undo_redo_no_redo_when_empty();

        std::cout << "\n✓✓✓ All UndoRedo tests passed! ✓✓✓\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
