#include <cassert>
#include <iostream>
#include <memory>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/parameters/parameter_store.hpp"

using namespace bendcore::document;
using namespace bendcore::commands;
using namespace bendcore::parameters;

int main() {
    std::cout << "Testing ParameterStore...\n";

    // Setup
    auto doc = std::make_unique<CadDocument>();
    auto bus = std::make_unique<DocumentEventBus>();
    auto store = std::make_unique<ParameterStore>(*doc, *bus);

    // Create test entity
    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "TestPart";
    auto entity = doc->add_entity(part);

    // Test 1: set and get double
    {
        ParameterValue val(3.14);
        assert(store->set(entity, "radius", val));

        auto result = store->get(entity, "radius");
        assert(result.has_value());
        assert(result->as_double() == 3.14);
        std::cout << "✓ test_set_and_get_double\n";
    }

    // Test 2: set and get integer
    {
        ParameterValue val(42);
        assert(store->set(entity, "count", val));

        auto result = store->get(entity, "count");
        assert(result.has_value());
        assert(result->as_integer() == 42);
        std::cout << "✓ test_set_and_get_integer\n";
    }

    // Test 3: set and get string
    {
        ParameterValue val("TestString");
        assert(store->set(entity, "name", val));

        auto result = store->get(entity, "name");
        assert(result.has_value());
        assert(result->as_string() == "TestString");
        std::cout << "✓ test_set_and_get_string\n";
    }

    // Test 4: set and get boolean
    {
        ParameterValue val(true);
        assert(store->set(entity, "enabled", val));

        auto result = store->get(entity, "enabled");
        assert(result.has_value());
        assert(result->as_boolean() == true);
        std::cout << "✓ test_set_and_get_boolean\n";
    }

    // Test 5: get nonexistent parameter
    {
        auto result = store->get(entity, "nonexistent");
        assert(!result.has_value());
        std::cout << "✓ test_get_nonexistent_parameter\n";
    }

    // Test 6: get all names
    {
        store->set(entity, "p1", ParameterValue(1.0));
        store->set(entity, "p2", ParameterValue(2.0));
        store->set(entity, "p3", ParameterValue(3.0));

        auto names = store->get_all_names(entity);
        assert(names.size() >= 3);
        std::cout << "✓ test_get_all_names\n";
    }

    // Test 7: count parameters
    {
        Part part2;
        part2.id = EntityId(EntityId::Type::Part);
        auto entity2 = doc->add_entity(part2);

        store->set(entity2, "p1", ParameterValue(1.0));
        store->set(entity2, "p2", ParameterValue(2.0));

        assert(store->count_parameters(entity2) == 2);
        std::cout << "✓ test_count_parameters\n";
    }

    // Test 8: has parameter
    {
        Part part3;
        part3.id = EntityId(EntityId::Type::Part);
        auto entity3 = doc->add_entity(part3);

        store->set(entity3, "existing", ParameterValue(1.0));

        assert(store->has_parameter(entity3, "existing"));
        assert(!store->has_parameter(entity3, "nonexistent"));
        std::cout << "✓ test_has_parameter\n";
    }

    // Test 9: validate type mismatch
    {
        Part part4;
        part4.id = EntityId(EntityId::Type::Part);
        auto entity4 = doc->add_entity(part4);

        ParameterMetadata meta;
        meta.name = "name";
        meta.type = ParameterType::String;
        store->register_parameter(entity4, meta);

        ParameterValue invalid(42);
        assert(!store->validate(entity4, "name", invalid));
        std::cout << "✓ test_validate_type_mismatch\n";
    }

    // Test 10: parameter history
    {
        Part part5;
        part5.id = EntityId(EntityId::Type::Part);
        auto entity5 = doc->add_entity(part5);

        store->set(entity5, "param", ParameterValue(1.0));
        store->set(entity5, "param", ParameterValue(2.0));

        auto history = store->get_history(entity5, "param");
        assert(history.size() == 2);
        assert(history[0].new_value.as_double() == 1.0);
        assert(history[1].new_value.as_double() == 2.0);
        std::cout << "✓ test_parameter_history_recording\n";
    }

    // Test 11: undo last change
    {
        Part part6;
        part6.id = EntityId(EntityId::Type::Part);
        auto entity6 = doc->add_entity(part6);

        store->set(entity6, "param", ParameterValue(1.0));
        store->set(entity6, "param", ParameterValue(2.0));

        assert(store->undo_last_change(entity6, "param"));

        auto result = store->get(entity6, "param");
        assert(result.has_value());
        assert(result->as_double() == 1.0);
        std::cout << "✓ test_undo_last_change\n";
    }

    // Test 12: register and get metadata
    {
        Part part7;
        part7.id = EntityId(EntityId::Type::Part);
        auto entity7 = doc->add_entity(part7);

        ParameterMetadata meta;
        meta.name = "radius";
        meta.display_name = "Radius";
        meta.description = "Tube radius";
        meta.type = ParameterType::Double;

        store->register_parameter(entity7, meta);

        auto retrieved = store->get_metadata(entity7, "radius");
        assert(retrieved.has_value());
        assert(retrieved->name == "radius");
        assert(retrieved->display_name == "Radius");
        std::cout << "✓ test_register_and_get_metadata\n";
    }

    // Test 13: invalidate cache
    {
        Part part8;
        part8.id = EntityId(EntityId::Type::Part);
        auto entity8 = doc->add_entity(part8);

        store->set(entity8, "param", ParameterValue(1.0));
        store->invalidate_cache(entity8, "param");

        auto result = store->get(entity8, "param");
        assert(result.has_value());
        std::cout << "✓ test_invalidate_cache\n";
    }

    // Test 14: batch mode
    {
        Part part9;
        part9.id = EntityId(EntityId::Type::Part);
        auto entity9 = doc->add_entity(part9);

        assert(!store->in_batch());

        store->begin_batch_update();
        assert(store->in_batch());

        store->set(entity9, "p1", ParameterValue(1.0));
        store->set(entity9, "p2", ParameterValue(2.0));

        store->end_batch_update();
        assert(!store->in_batch());

        assert(store->count_parameters(entity9) == 2);
        std::cout << "✓ test_batch_mode_begin_end\n";
    }

    // Test 15: remove parameter
    {
        Part part10;
        part10.id = EntityId(EntityId::Type::Part);
        auto entity10 = doc->add_entity(part10);

        store->set(entity10, "param", ParameterValue(1.0));
        assert(store->has_parameter(entity10, "param"));

        assert(store->remove_parameter(entity10, "param"));
        assert(!store->has_parameter(entity10, "param"));
        std::cout << "✓ test_remove_parameter\n";
    }

    // Test 16: parameter type conversion
    {
        ParameterValue double_val(3.14);
        assert(double_val.type == ParameterType::Double);
        assert(double_val.type_name() == "Double");
        std::cout << "✓ test_parameter_value_type_conversion\n";
    }

    // Test 17: clear history
    {
        Part part11;
        part11.id = EntityId(EntityId::Type::Part);
        auto entity11 = doc->add_entity(part11);

        store->set(entity11, "param", ParameterValue(1.0));
        store->set(entity11, "param", ParameterValue(2.0));

        store->clear_history(entity11, "param");

        auto history = store->get_history(entity11, "param");
        assert(history.empty());
        std::cout << "✓ test_clear_history\n";
    }

    // Test 18: parameter value with formula
    {
        Part part12;
        part12.id = EntityId(EntityId::Type::Part);
        auto entity12 = doc->add_entity(part12);

        ParameterValue val(5.0);
        val.formula = "radius * 2";
        store->set(entity12, "diameter", val);

        auto result = store->get(entity12, "diameter");
        assert(result.has_value());
        assert(result->formula.has_value());
        assert(*result->formula == "radius * 2");
        std::cout << "✓ test_parameter_value_with_formula\n";
    }

    // Test 19: validate double constraints
    {
        Part part13;
        part13.id = EntityId(EntityId::Type::Part);
        auto entity13 = doc->add_entity(part13);

        ParameterMetadata meta;
        meta.name = "radius";
        meta.type = ParameterType::Double;
        meta.min_value = 0.0;
        meta.max_value = 10.0;
        store->register_parameter(entity13, meta);

        ParameterValue valid(5.0);
        assert(store->validate(entity13, "radius", valid));

        ParameterValue invalid(-1.0);
        assert(!store->validate(entity13, "radius", invalid));
        std::cout << "✓ test_validate_double_constraints\n";
    }

    // Test 20: multiple entities
    {
        Part part14;
        part14.id = EntityId(EntityId::Type::Part);
        auto entity14 = doc->add_entity(part14);

        Part part15;
        part15.id = EntityId(EntityId::Type::Part);
        auto entity15 = doc->add_entity(part15);

        store->set(entity14, "radius", ParameterValue(1.5));
        store->set(entity15, "radius", ParameterValue(2.5));

        auto val1 = store->get(entity14, "radius");
        auto val2 = store->get(entity15, "radius");

        assert(val1.has_value());
        assert(val2.has_value());
        assert(val1->as_double() == 1.5);
        assert(val2->as_double() == 2.5);
        std::cout << "✓ test_multiple_entities\n";
    }

    // Test 21: clear all cache
    {
        Part part16;
        part16.id = EntityId(EntityId::Type::Part);
        auto entity16 = doc->add_entity(part16);

        Part part17;
        part17.id = EntityId(EntityId::Type::Part);
        auto entity17 = doc->add_entity(part17);

        store->set(entity16, "p1", ParameterValue(1.0));
        store->set(entity17, "p2", ParameterValue(2.0));

        store->clear_cache();

        assert(store->has_parameter(entity16, "p1"));
        assert(store->has_parameter(entity17, "p2"));
        std::cout << "✓ test_clear_all_cache\n";
    }

    // Test 22: set same value twice
    {
        Part part18;
        part18.id = EntityId(EntityId::Type::Part);
        auto entity18 = doc->add_entity(part18);

        store->set(entity18, "param", ParameterValue(1.0));
        store->set(entity18, "param", ParameterValue(1.0));

        auto result = store->get(entity18, "param");
        assert(result.has_value());
        assert(result->as_double() == 1.0);

        auto history = store->get_history(entity18, "param");
        assert(history.size() == 2);
        std::cout << "✓ test_set_same_value_twice\n";
    }

    // Test 23: remove nonexistent parameter
    {
        Part part19;
        part19.id = EntityId(EntityId::Type::Part);
        auto entity19 = doc->add_entity(part19);

        assert(!store->remove_parameter(entity19, "nonexistent"));
        std::cout << "✓ test_remove_nonexistent_parameter\n";
    }

    // Test 24: validate enum values
    {
        Part part20;
        part20.id = EntityId(EntityId::Type::Part);
        auto entity20 = doc->add_entity(part20);

        ParameterMetadata meta;
        meta.name = "color";
        meta.type = ParameterType::String;
        meta.allowed_values = {"red", "green", "blue"};
        store->register_parameter(entity20, meta);

        ParameterValue valid("red");
        assert(store->validate(entity20, "color", valid));

        ParameterValue invalid("yellow");
        assert(!store->validate(entity20, "color", invalid));
        std::cout << "✓ test_validate_enum_values\n";
    }

    // Test 25: metadata modification count
    {
        Part part21;
        part21.id = EntityId(EntityId::Type::Part);
        auto entity21 = doc->add_entity(part21);

        ParameterMetadata meta;
        meta.name = "param";
        meta.type = ParameterType::Double;
        store->register_parameter(entity21, meta);

        store->set(entity21, "param", ParameterValue(1.0));
        store->set(entity21, "param", ParameterValue(2.0));

        auto updated_meta = store->get_metadata(entity21, "param");
        assert(updated_meta.has_value());
        assert(updated_meta->modification_count >= 2);
        std::cout << "✓ test_metadata_modification_count\n";
    }

    std::cout << "\nAll ParameterStore tests passed! (25/25)\n";
    return 0;
}
