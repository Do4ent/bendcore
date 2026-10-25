#include "bendcore/document/transaction.hpp"
#include "bendcore/document/cad_document.hpp"
#include <cassert>
#include <iostream>

using namespace BendCore::CadCore;

void test_transaction_creation() {
    std::cout << "Test: Transaction Creation\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Test Transaction");

    assert(tx.isActive());
    assert(tx.getDescription() == "Test Transaction");
    assert(tx.getId() > 0);

    std::cout << "✓ Transaction creation passed\n";
}

void test_transaction_commit() {
    std::cout << "Test: Transaction Commit\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Add Entity");

    Entity entity(EntityType::PART, "TestPart");
    auto entity_id = entity.id;
    doc.addEntity(entity);
    tx.recordEntityChange(entity_id);

    assert(tx.isActive());
    bool commit_result = tx.commit();
    assert(commit_result);
    assert(!tx.isActive());

    std::cout << "✓ Transaction commit passed\n";
}

void test_transaction_rollback() {
    std::cout << "Test: Transaction Rollback\n";
    CadDocument doc;

    // Add initial entity
    Entity initial_entity(EntityType::PART, "InitialPart");
    auto initial_id = initial_entity.id;
    doc.addEntity(initial_entity);
    size_t initial_count = doc.getEntityCount();

    DocumentTransaction tx(doc, "Add Another Entity");
    Entity new_entity(EntityType::BODY, "NewBody");
    auto new_id = new_entity.id;
    doc.addEntity(new_entity);
    tx.recordEntityChange(new_id);

    assert(doc.getEntityCount() == initial_count + 1);

    bool rollback_result = tx.rollback();
    assert(rollback_result);
    assert(!tx.isActive());
    assert(doc.getEntityCount() == initial_count);
    assert(!doc.hasEntity(new_id));

    std::cout << "✓ Transaction rollback passed\n";
}

void test_transaction_affected_entities() {
    std::cout << "Test: Transaction Affected Entities\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Modify Entities");

    Entity entity1(EntityType::PART, "Part1");
    Entity entity2(EntityType::PART, "Part2");
    auto id1 = entity1.id;
    auto id2 = entity2.id;

    doc.addEntity(entity1);
    doc.addEntity(entity2);

    tx.recordEntityChange(id1);
    tx.recordEntityChange(id2);

    auto affected = tx.getAffectedEntities();
    assert(affected.size() == 2);
    assert(std::find(affected.begin(), affected.end(), id1) != affected.end());
    assert(std::find(affected.begin(), affected.end(), id2) != affected.end());

    std::cout << "✓ Transaction affected entities passed\n";
}

void test_transaction_validation() {
    std::cout << "Test: Transaction Validation\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Valid Transaction");

    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);
    tx.recordEntityChange(entity.id);

    std::string validation_error = tx.validate();
    assert(validation_error.empty());

    std::cout << "✓ Transaction validation passed\n";
}

void test_transaction_guard() {
    std::cout << "Test: Transaction Guard\n";
    CadDocument doc;
    {
        DocumentTransaction tx(doc, "Guarded Transaction");
        Entity entity(EntityType::PART, "TestPart");
        doc.addEntity(entity);

        TransactionGuard guard(tx);
        guard.commit();
    }

    // Transaction should have been committed
    assert(!doc.getEntityIds().empty());

    std::cout << "✓ Transaction guard passed\n";
}

void test_transaction_guard_rollback() {
    std::cout << "Test: Transaction Guard Rollback\n";
    CadDocument doc;

    Entity initial_entity(EntityType::PART, "Initial");
    doc.addEntity(initial_entity);
    size_t initial_count = doc.getEntityCount();

    {
        DocumentTransaction tx(doc, "Guarded Transaction");
        Entity entity(EntityType::BODY, "NewBody");
        doc.addEntity(entity);

        TransactionGuard guard(tx);
        // Don't call commit, so it should rollback
    }

    // Entity should have been rolled back
    assert(doc.getEntityCount() == initial_count);

    std::cout << "✓ Transaction guard rollback passed\n";
}

void test_transaction_empty() {
    std::cout << "Test: Empty Transaction\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Empty");

    bool commit_result = tx.commit();
    assert(commit_result);
    assert(!tx.isActive());

    std::cout << "✓ Empty transaction passed\n";
}

void test_transaction_multiple_entities() {
    std::cout << "Test: Multiple Entities Transaction\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Add Multiple");

    std::vector<EntityId> added_ids;
    for (int i = 0; i < 10; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        auto id = entity.id;
        doc.addEntity(entity);
        tx.recordEntityChange(id);
        added_ids.push_back(id);
    }

    auto affected = tx.getAffectedEntities();
    assert(affected.size() == 10);

    tx.commit();

    for (const auto& id : added_ids) {
        assert(doc.hasEntity(id));
    }

    std::cout << "✓ Multiple entities transaction passed\n";
}

void test_transaction_size() {
    std::cout << "Test: Transaction Size\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Size Test");

    for (int i = 0; i < 5; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
        tx.recordEntityChange(entity.id);
    }

    size_t size = tx.getSize();
    assert(size > 0);
    assert(size >= 5 * 512);  // At least 5 entities * 512 bytes

    std::cout << "✓ Transaction size passed\n";
}

void test_transaction_duplicate_recording() {
    std::cout << "Test: Duplicate Entity Recording\n";
    CadDocument doc;
    DocumentTransaction tx(doc, "Duplicate Test");

    Entity entity(EntityType::PART, "TestPart");
    auto id = entity.id;
    doc.addEntity(entity);

    // Record same entity multiple times
    tx.recordEntityChange(id);
    tx.recordEntityChange(id);
    tx.recordEntityChange(id);

    auto affected = tx.getAffectedEntities();
    assert(affected.size() == 1);  // Should only appear once

    std::cout << "✓ Duplicate recording passed\n";
}

int main() {
    std::cout << "Running Transaction Tests\n\n";

    try {
        test_transaction_creation();
        test_transaction_commit();
        test_transaction_rollback();
        test_transaction_affected_entities();
        test_transaction_validation();
        test_transaction_guard();
        test_transaction_guard_rollback();
        test_transaction_empty();
        test_transaction_multiple_entities();
        test_transaction_size();
        test_transaction_duplicate_recording();

        std::cout << "\n✓✓✓ All transaction tests passed! ✓✓✓\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
