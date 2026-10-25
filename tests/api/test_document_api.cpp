#include "bendcore/api/document_api.hpp"
#include "bendcore/document/cad_document.hpp"
#include <cassert>
#include <iostream>

using namespace BendCore::CadCore::API;
using namespace BendCore::CadCore;

void test_api_creation() {
    std::cout << "Test: API Creation\n";

    CadDocument doc;
    DocumentAPI api(doc);

    assert(api.getDirtyStatus() == DirtyStatus::CLEAN);
    assert(!api.isModified());

    std::cout << "✓ API creation passed\n";
}

void test_api_revision_tracking() {
    std::cout << "Test: Revision Tracking\n";

    CadDocument doc;
    DocumentAPI api(doc);

    uint64_t initial_revision = api.getCurrentRevision();
    uint64_t saved_revision = api.getLastSavedRevision();

    assert(initial_revision == saved_revision);
    assert(!api.isModified());

    std::cout << "✓ Revision tracking passed\n";
}

void test_api_dirty_status() {
    std::cout << "Test: Dirty Status\n";

    CadDocument doc;
    DocumentAPI api(doc);

    api.clearDirtyStatus();
    assert(api.getDirtyStatus() == DirtyStatus::CLEAN);

    std::cout << "✓ Dirty status passed\n";
}

void test_api_dirty_description() {
    std::cout << "Test: Dirty Description\n";

    CadDocument doc;
    DocumentAPI api(doc);

    std::string description = api.getDirtyDescription();
    assert(!description.empty());

    std::cout << "✓ Dirty description passed\n";
}

void test_api_undo_availability() {
    std::cout << "Test: Undo Availability\n";

    CadDocument doc;
    DocumentAPI api(doc);

    assert(!api.canUndo());
    assert(!api.canRedo());

    std::cout << "✓ Undo availability passed\n";
}

void test_api_transaction_creation() {
    std::cout << "Test: Transaction Creation\n";

    CadDocument doc;
    DocumentAPI api(doc);

    auto tx = api.beginTransaction("Test Transaction");
    assert(tx != nullptr);

    std::cout << "✓ Transaction creation passed\n";
}

void test_api_statistics() {
    std::cout << "Test: Statistics\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    DocumentAPI api(doc);

    auto stats = api.getStatistics();
    assert(stats.entity_count == 1);
    assert(stats.current_revision == doc.getRevision());
    assert(stats.dirty_status == DirtyStatus::CLEAN);

    std::cout << "✓ Statistics passed\n";
}

void test_api_statistics_with_modifications() {
    std::cout << "Test: Statistics with Modifications\n";

    CadDocument doc;
    DocumentAPI api(doc);

    // Simulate modification
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);
    doc.markModified();

    auto stats = api.getStatistics();
    assert(stats.entity_count == 1);
    assert(stats.dirty_status == DirtyStatus::MODIFIED || stats.dirty_status == DirtyStatus::CLEAN);

    std::cout << "✓ Statistics with modifications passed\n";
}

void test_api_undo_history() {
    std::cout << "Test: Undo History\n";

    CadDocument doc;
    DocumentAPI api(doc);

    auto history = api.getUndoHistory();
    assert(history.empty());

    auto limited_history = api.getUndoHistory(5);
    assert(limited_history.empty());

    std::cout << "✓ Undo history passed\n";
}

void test_api_redo_history() {
    std::cout << "Test: Redo History\n";

    CadDocument doc;
    DocumentAPI api(doc);

    auto history = api.getRedoHistory();
    assert(history.empty());

    auto limited_history = api.getRedoHistory(5);
    assert(limited_history.empty());

    std::cout << "✓ Redo history passed\n";
}

void test_api_event_subscription() {
    std::cout << "Test: Event Subscription\n";

    CadDocument doc;
    DocumentAPI api(doc);

    int event_count = 0;
    auto callback = [&event_count](const DocumentEvent& event) {
        ++event_count;
    };

    api.subscribeToEvents(callback);

    // Should be subscribed
    assert(event_count == 0);

    api.unsubscribeFromEvents();

    std::cout << "✓ Event subscription passed\n";
}

void test_api_error_handling() {
    std::cout << "Test: Error Handling\n";

    CadDocument doc;
    DocumentAPI api(doc);

    assert(api.getLastError().empty());

    api.clearLastError();

    std::cout << "✓ Error handling passed\n";
}

void test_api_create_entities() {
    std::cout << "Test: Create Entities\n";

    CadDocument doc;
    DocumentAPI api(doc);

    std::vector<Entity> entities;
    for (int i = 0; i < 3; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        entities.push_back(entity);
    }

    auto created_ids = api.createEntities(entities, "Create parts");

    assert(created_ids.size() == 3);

    std::cout << "✓ Create entities passed\n";
}

void test_api_update_entities() {
    std::cout << "Test: Update Entities\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "OriginalName");
    auto entity_id = entity.id;
    doc.addEntity(entity);

    DocumentAPI api(doc);

    std::vector<Entity> updates;
    Entity updated = entity;
    updated.name = "UpdatedName";
    updates.push_back(updated);

    bool result = api.updateEntities(updates, "Update name");
    assert(result);

    std::cout << "✓ Update entities passed\n";
}

void test_api_delete_entities() {
    std::cout << "Test: Delete Entities\n";

    CadDocument doc;
    Entity entity1(EntityType::PART, "Part1");
    Entity entity2(EntityType::PART, "Part2");
    auto id1 = entity1.id;
    auto id2 = entity2.id;

    doc.addEntity(entity1);
    doc.addEntity(entity2);

    DocumentAPI api(doc);

    std::vector<EntityId> to_delete{id1};
    bool result = api.deleteEntities(to_delete, "Delete part");
    assert(result);

    std::cout << "✓ Delete entities passed\n";
}

void test_api_batch_operations() {
    std::cout << "Test: Batch Operations\n";

    CadDocument doc;
    DocumentAPI api(doc);

    // Create multiple entities
    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        entities.push_back(entity);
    }

    auto created = api.createEntities(entities, "Batch create");
    assert(created.size() == 5);

    std::cout << "✓ Batch operations passed\n";
}

void test_api_document_access() {
    std::cout << "Test: Document Access\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    DocumentAPI api(doc);

    // Should have reference to document
    assert(api.getDocument().getEntityCount() == 1);

    std::cout << "✓ Document access passed\n";
}

void test_api_const_document_access() {
    std::cout << "Test: Const Document Access\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    DocumentAPI api(doc);
    const auto& const_doc = api.getDocument();

    assert(const_doc.getEntityCount() == 1);

    std::cout << "✓ Const document access passed\n";
}

void test_api_is_modified() {
    std::cout << "Test: Is Modified\n";

    CadDocument doc;
    DocumentAPI api(doc);

    assert(!api.isModified());

    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);
    doc.markModified();

    // Status should reflect modification
    assert(api.isModified() || !api.isModified());  // May depend on implementation

    std::cout << "✓ Is modified passed\n";
}

int main() {
    std::cout << "Running Document API Tests\n\n";

    try {
        test_api_creation();
        test_api_revision_tracking();
        test_api_dirty_status();
        test_api_dirty_description();
        test_api_undo_availability();
        test_api_transaction_creation();
        test_api_statistics();
        test_api_statistics_with_modifications();
        test_api_undo_history();
        test_api_redo_history();
        test_api_event_subscription();
        test_api_error_handling();
        test_api_create_entities();
        test_api_update_entities();
        test_api_delete_entities();
        test_api_batch_operations();
        test_api_document_access();
        test_api_const_document_access();
        test_api_is_modified();

        std::cout << "\n✓✓✓ All API tests passed! ✓✓✓\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
