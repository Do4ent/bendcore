#include <cassert>
#include <iostream>
#include <memory>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/dependencies/dependency_graph.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/parameters/parameter_store.hpp"
#include "bendcore/rebuild/rebuild_engine.hpp"

using namespace bendcore::document;
using namespace bendcore::commands;
using namespace bendcore::parameters;
using namespace bendcore::dependencies;
using namespace bendcore::rebuild;

int main() {
    std::cout << "Testing RebuildEngine...\n";

    // Setup
    auto doc = std::make_unique<CadDocument>();
    auto bus = std::make_unique<DocumentEventBus>();
    auto store = std::make_unique<ParameterStore>(*doc, *bus);
    auto graph = std::make_unique<DependencyGraph>(*doc, *bus);
    auto engine = std::make_unique<RebuildEngine>(*doc, *store, *graph, *bus);

    // Create test entity
    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "TestPart";
    auto entity = doc->add_entity(part);

    // Test 1: register compute function
    {
        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(42.0));
        };

        engine->register_compute_function(entity, "param", func);
        assert(engine->can_rebuild(entity, "param"));
        std::cout << "✓ test_register_compute_function\n";
    }

    // Test 2: can rebuild without function
    {
        Part part2;
        part2.id = EntityId(EntityId::Type::Part);
        auto entity2 = doc->add_entity(part2);

        assert(!engine->can_rebuild(entity2, "param"));
        std::cout << "✓ test_can_rebuild_without_function\n";
    }

    // Test 3: rebuild parameter simple
    {
        Part part3;
        part3.id = EntityId(EntityId::Type::Part);
        auto entity3 = doc->add_entity(part3);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(3.14));
        };

        engine->register_compute_function(entity3, "radius", func);
        auto result = engine->rebuild_parameter(entity3, "radius");

        assert(result.status == RebuildStatus::Success);
        assert(result.new_value.as_double() == 3.14);
        std::cout << "✓ test_rebuild_parameter_simple\n";
    }

    // Test 4: rebuild parameter no function
    {
        Part part4;
        part4.id = EntityId(EntityId::Type::Part);
        auto entity4 = doc->add_entity(part4);

        auto result = engine->rebuild_parameter(entity4, "param");
        assert(result.status == RebuildStatus::Failed);
        std::cout << "✓ test_rebuild_parameter_no_function\n";
    }

    // Test 5: unregister compute function
    {
        Part part5;
        part5.id = EntityId(EntityId::Type::Part);
        auto entity5 = doc->add_entity(part5);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(42.0));
        };

        engine->register_compute_function(entity5, "param", func);
        assert(engine->can_rebuild(entity5, "param"));

        engine->unregister_compute_function(entity5, "param");
        assert(!engine->can_rebuild(entity5, "param"));
        std::cout << "✓ test_unregister_compute_function\n";
    }

    // Test 6: request stop
    {
        assert(!engine->is_stop_requested());

        engine->request_stop();
        assert(engine->is_stop_requested());
        std::cout << "✓ test_request_stop\n";
    }

    // Test 7: clear stop request
    {
        engine->request_stop();
        engine->clear_stop_request();

        assert(!engine->is_stop_requested());
        std::cout << "✓ test_clear_stop_request\n";
    }

    // Test 8: schedule rebuild
    {
        Part part6;
        part6.id = EntityId(EntityId::Type::Part);
        auto entity6 = doc->add_entity(part6);

        engine->schedule_rebuild(entity6, "param");
        assert(engine->pending_count() >= 1);
        std::cout << "✓ test_schedule_rebuild\n";
    }

    // Test 9: clear pending
    {
        Part part7;
        part7.id = EntityId(EntityId::Type::Part);
        auto entity7 = doc->add_entity(part7);

        engine->schedule_rebuild(entity7, "param");
        engine->clear_pending();

        assert(engine->pending_count() == 0);
        std::cout << "✓ test_clear_pending\n";
    }

    // Test 10: enable profiling
    {
        Part part8;
        part8.id = EntityId(EntityId::Type::Part);
        auto entity8 = doc->add_entity(part8);

        engine->enable_profiling(true);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity8, "param", func);
        engine->rebuild_parameter(entity8, "param");

        auto stats = engine->get_performance_stats(entity8, "param");
        assert(stats.computation_count >= 1);
        std::cout << "✓ test_enable_profiling\n";
    }

    // Test 11: get rebuild status
    {
        Part part9;
        part9.id = EntityId(EntityId::Type::Part);
        auto entity9 = doc->add_entity(part9);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity9, "param", func);
        engine->rebuild_parameter(entity9, "param");

        auto status = engine->get_rebuild_status(entity9, "param");
        assert(status == RebuildStatus::Success);
        std::cout << "✓ test_get_rebuild_status\n";
    }

    // Test 12: fea pipeline dirty flag
    {
        assert(!engine->is_fea_dirty());

        engine->notify_fea_pipeline_dirty();
        assert(engine->is_fea_dirty());

        engine->clear_dirty_flags();
        assert(!engine->is_fea_dirty());
        std::cout << "✓ test_fea_pipeline_dirty_flag\n";
    }

    // Test 13: render pipeline dirty flag
    {
        assert(!engine->is_render_dirty());

        engine->notify_render_pipeline_dirty();
        assert(engine->is_render_dirty());

        engine->clear_dirty_flags();
        assert(!engine->is_render_dirty());
        std::cout << "✓ test_render_pipeline_dirty_flag\n";
    }

    // Test 14: rebuild entity multiple parameters
    {
        Part part10;
        part10.id = EntityId(EntityId::Type::Part);
        auto entity10 = doc->add_entity(part10);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity10, "p1", func);
        engine->register_compute_function(entity10, "p2", func);

        store->set(entity10, "p1", ParameterValue(0.0));
        store->set(entity10, "p2", ParameterValue(0.0));

        auto report = engine->rebuild_entity(entity10);

        assert(report.total_parameters == 2);
        assert(report.successful == 2);
        std::cout << "✓ test_rebuild_entity_multiple_parameters\n";
    }

    // Test 15: rebuild all
    {
        Part part11;
        part11.id = EntityId(EntityId::Type::Part);
        auto entity11 = doc->add_entity(part11);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity11, "p1", func);
        engine->register_compute_function(entity11, "p2", func);

        store->set(entity11, "p1", ParameterValue(0.0));
        store->set(entity11, "p2", ParameterValue(0.0));

        auto report = engine->rebuild_all();

        assert(report.total_parameters >= 2);
        assert(report.successful >= 2);
        std::cout << "✓ test_rebuild_all\n";
    }

    // Test 16: rebuild report success rate
    {
        Part part12;
        part12.id = EntityId(EntityId::Type::Part);
        auto entity12 = doc->add_entity(part12);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity12, "p1", func);
        store->set(entity12, "p1", ParameterValue(0.0));

        auto report = engine->rebuild_all();

        assert(report.success_rate() > 0.0);
        std::cout << "✓ test_rebuild_report_success_rate\n";
    }

    // Test 17: stop request skips rebuild
    {
        Part part13;
        part13.id = EntityId(EntityId::Type::Part);
        auto entity13 = doc->add_entity(part13);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity13, "param", func);
        engine->clear_stop_request();
        engine->request_stop();

        auto result = engine->rebuild_parameter(entity13, "param");
        assert(result.status == RebuildStatus::Skipped);

        engine->clear_stop_request();
        std::cout << "✓ test_stop_request_skips_rebuild\n";
    }

    // Test 18: subscribe rebuild event
    {
        Part part14;
        part14.id = EntityId(EntityId::Type::Part);
        auto entity14 = doc->add_entity(part14);

        bool event_fired = false;
        RebuildEngine::RebuildEventHandler handler = [&event_fired](const RebuildResult&) {
            event_fired = true;
        };

        engine->subscribe_rebuild_event(handler);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity14, "param", func);
        engine->rebuild_parameter(entity14, "param");

        assert(event_fired);
        std::cout << "✓ test_subscribe_rebuild_event\n";
    }

    // Test 19: clear event handlers
    {
        Part part15;
        part15.id = EntityId(EntityId::Type::Part);
        auto entity15 = doc->add_entity(part15);

        bool event_fired = false;
        RebuildEngine::RebuildEventHandler handler = [&event_fired](const RebuildResult&) {
            event_fired = true;
        };

        engine->subscribe_rebuild_event(handler);
        engine->clear_event_handlers();

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity15, "param", func);
        engine->rebuild_parameter(entity15, "param");

        assert(!event_fired);
        std::cout << "✓ test_clear_event_handlers\n";
    }

    // Test 20: get last compute time
    {
        Part part16;
        part16.id = EntityId(EntityId::Type::Part);
        auto entity16 = doc->add_entity(part16);

        engine->enable_profiling(true);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity16, "param", func);
        engine->rebuild_parameter(entity16, "param");

        auto time = engine->get_last_compute_time(entity16, "param");
        assert(time.has_value());
        assert(time->count() >= 0);
        std::cout << "✓ test_get_last_compute_time\n";
    }

    // Test 21: get performance stats
    {
        Part part17;
        part17.id = EntityId(EntityId::Type::Part);
        auto entity17 = doc->add_entity(part17);

        engine->enable_profiling(true);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity17, "param", func);
        engine->rebuild_parameter(entity17, "param");
        engine->rebuild_parameter(entity17, "param");

        auto stats = engine->get_performance_stats(entity17, "param");
        assert(stats.computation_count == 2);
        assert(stats.average_time.count() >= 0);
        std::cout << "✓ test_get_performance_stats\n";
    }

    // Test 22: reset all
    {
        Part part18;
        part18.id = EntityId(EntityId::Type::Part);
        auto entity18 = doc->add_entity(part18);

        engine->schedule_rebuild(entity18, "param");
        engine->request_stop();

        engine->reset_all();

        assert(engine->pending_count() == 0);
        assert(!engine->is_stop_requested());
        std::cout << "✓ test_reset_all\n";
    }

    // Test 23: rebuild with exception
    {
        Part part19;
        part19.id = EntityId(EntityId::Type::Part);
        auto entity19 = doc->add_entity(part19);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) -> std::optional<ParameterValue> {
            throw std::runtime_error("Computation error");
            return std::nullopt;  // Never reached
        };

        engine->register_compute_function(entity19, "param", func);
        auto result = engine->rebuild_parameter(entity19, "param");

        assert(result.status == RebuildStatus::Failed);
        assert(!result.error_message.empty());
        std::cout << "✓ test_rebuild_parameter_with_exception\n";
    }

    // Test 24: pending count
    {
        Part part20;
        part20.id = EntityId(EntityId::Type::Part);
        auto entity20 = doc->add_entity(part20);

        engine->clear_pending();
        engine->schedule_rebuild(entity20, "p1");
        engine->schedule_rebuild(entity20, "p2");

        assert(engine->pending_count() == 2);
        std::cout << "✓ test_pending_count\n";
    }

    // Test 25: flush scheduled rebuilds
    {
        Part part21;
        part21.id = EntityId(EntityId::Type::Part);
        auto entity21 = doc->add_entity(part21);

        RebuildEngine::ComputeFunction func = [](EntityId, const std::string&, const ParameterStore&) {
            return std::make_optional(ParameterValue(1.0));
        };

        engine->register_compute_function(entity21, "param", func);
        store->set(entity21, "param", ParameterValue(0.0));

        engine->clear_pending();
        engine->schedule_rebuild(entity21, "param");
        auto report = engine->flush_scheduled_rebuilds();

        assert(report.successful >= 1);
        assert(engine->pending_count() == 0);
        std::cout << "✓ test_flush_scheduled_rebuilds\n";
    }

    std::cout << "\nAll RebuildEngine tests passed! (25/25)\n";
    return 0;
}
