#include <cassert>
#include <iostream>

#include "bendcore/commands/document_event_bus.hpp"

using namespace bendcore::commands;
using namespace bendcore::document;

void test_subscribe_publish() {
    std::cout << "Testing subscribe and publish...\n";

    DocumentEventBus bus;
    int event_count = 0;

    auto sub_id = bus.subscribe(
        DocumentEventType::EntityCreated,
        [&event_count](const DocumentEvent& event) {
            event_count++;
            assert(event.type == DocumentEventType::EntityCreated);
        }
    );

    assert(bus.subscriber_count(DocumentEventType::EntityCreated) == 1);

    DocumentEvent event(DocumentEventType::EntityCreated, EntityId(EntityId::Type::Part), "Created");
    bus.publish(event);

    assert(event_count == 1);
    bus.unsubscribe(sub_id);
    assert(bus.subscriber_count(DocumentEventType::EntityCreated) == 0);

    std::cout << "✓ Subscribe and publish passed\n";
}

void test_multiple_subscribers() {
    std::cout << "Testing multiple subscribers...\n";

    DocumentEventBus bus;
    int count1 = 0, count2 = 0, count3 = 0;

    auto sub1 = bus.subscribe(
        DocumentEventType::EntityCreated,
        [&count1](const DocumentEvent&) { count1++; }
    );

    auto sub2 = bus.subscribe(
        DocumentEventType::EntityCreated,
        [&count2](const DocumentEvent&) { count2++; }
    );

    auto sub3 = bus.subscribe(
        DocumentEventType::EntityDeleted,
        [&count3](const DocumentEvent&) { count3++; }
    );

    DocumentEvent create_event(DocumentEventType::EntityCreated, EntityId(EntityId::Type::Part), "Created");
    bus.publish(create_event);

    assert(count1 == 1);
    assert(count2 == 1);
    assert(count3 == 0);

    DocumentEvent delete_event(DocumentEventType::EntityDeleted, EntityId(EntityId::Type::Part), "Deleted");
    bus.publish(delete_event);

    assert(count3 == 1);

    std::cout << "✓ Multiple subscribers passed\n";
}

void test_unsubscribe() {
    std::cout << "Testing unsubscribe...\n";

    DocumentEventBus bus;
    int count = 0;

    auto sub = bus.subscribe(
        DocumentEventType::EntityModified,
        [&count](const DocumentEvent&) { count++; }
    );

    DocumentEvent event(DocumentEventType::EntityModified, EntityId(EntityId::Type::Part), "Modified");
    bus.publish(event);
    assert(count == 1);

    bus.unsubscribe(sub);
    bus.publish(event);
    assert(count == 1);  // Shouldn't increase

    std::cout << "✓ Unsubscribe passed\n";
}

void test_event_types() {
    std::cout << "Testing all event types...\n";

    DocumentEventBus bus;
    std::vector<DocumentEventType> received_types;

    for (int i = 0; i <= 4; ++i) {
        DocumentEventType type = static_cast<DocumentEventType>(i);
        bus.subscribe(
            type,
            [&received_types, type](const DocumentEvent& event) {
                assert(event.type == type);
                received_types.push_back(type);
            }
        );
    }

    bus.publish(DocumentEvent(DocumentEventType::EntityCreated, EntityId(EntityId::Type::Part), ""));
    bus.publish(DocumentEvent(DocumentEventType::EntityDeleted, EntityId(EntityId::Type::Part), ""));
    bus.publish(DocumentEvent(DocumentEventType::EntityModified, EntityId(EntityId::Type::Part), ""));
    bus.publish(DocumentEvent(DocumentEventType::HierarchyChanged, EntityId(EntityId::Type::Part), ""));
    bus.publish(DocumentEvent(DocumentEventType::DocumentRevisionChanged, EntityId(EntityId::Type::Part), ""));

    assert(received_types.size() == 5);

    std::cout << "✓ All event types passed\n";
}

void test_batch_events() {
    std::cout << "Testing batch events...\n";

    DocumentEventBus bus;
    int event_count = 0;

    bus.subscribe(
        DocumentEventType::EntityCreated,
        [&event_count](const DocumentEvent&) { event_count++; }
    );

    bus.begin_batch();
    assert(bus.in_batch());

    // Publish several events in batch mode
    for (int i = 0; i < 5; ++i) {
        bus.publish(DocumentEvent(DocumentEventType::EntityCreated, EntityId(EntityId::Type::Part), ""));
    }

    // Events should be queued but not delivered yet
    assert(event_count == 0);

    bus.end_batch();
    assert(!bus.in_batch());

    // Events should now be delivered
    assert(event_count == 5);

    std::cout << "✓ Batch events passed\n";
}

int main() {
    std::cout << "=== CAD-CORE-008: DocumentEventBus Tests ===\n\n";

    test_subscribe_publish();
    test_multiple_subscribers();
    test_unsubscribe();
    test_event_types();
    test_batch_events();

    std::cout << "\n✓ All CAD-CORE-008 DocumentEventBus tests passed!\n";
    return 0;
}
