#include <cassert>
#include <iostream>

#include "bendcore/document/entity_id.hpp"

using namespace bendcore::document;

int main() {
    std::cout << "Testing EntityId creation...\n";

    // Test 1: Invalid ID
    EntityId invalid;
    assert(!invalid.is_valid());
    std::cout << "✓ Invalid ID test passed\n";

    // Test 2: Create Part ID
    EntityId part_id(EntityId::Type::Part);
    assert(part_id.is_valid());
    assert(part_id.type() == EntityId::Type::Part);

    std::cout << "Part ID: " << part_id.debug_str() << "\n";
    std::cout << "UUID: " << part_id.uuid() << "\n";
    std::cout << "UUID length: " << part_id.uuid().length() << "\n";

    assert(part_id.uuid().length() == 36);
    std::cout << "✓ Part ID creation passed\n";

    // Test 3: Serialization
    std::string serialized = part_id.to_string();
    std::cout << "Serialized: " << serialized << "\n";

    auto restored = EntityId::from_string(serialized);
    if (!restored.has_value()) {
        std::cout << "ERROR: from_string returned nullopt!\n";
        return 1;
    }

    assert(restored.value() == part_id);
    std::cout << "✓ Serialization test passed\n";

    std::cout << "\n✓ All simple tests passed!\n";
    return 0;
}
