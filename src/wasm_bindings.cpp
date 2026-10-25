#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <bendcore/document/cad_document.h>
#include <bendcore/document/entity_id.h>
#include <bendcore/commands/command_manager.h>
#include <bendcore/geometry/geometry_engine.h>
#include <bendcore/topology/brep.h>
#include <bendcore/features/feature_system.h>
#include <bendcore/serialization/serializer.h>
#include <memory>
#include <vector>
#include <string>

using namespace emscripten;
using namespace bendcore;

// Global document instance
std::shared_ptr<CADDocument> g_document = nullptr;
std::unique_ptr<CommandManager> g_command_manager = nullptr;
std::unique_ptr<GeometryEngine> g_geometry_engine = nullptr;

// Initialize document
void initDocument() {
    g_document = std::make_shared<CADDocument>();
    g_command_manager = std::make_unique<CommandManager>(*g_document);
    g_geometry_engine = std::make_unique<GeometryEngine>();
}

// Get document reference
CADDocument& getDocument() {
    if (!g_document) {
        initDocument();
    }
    return *g_document;
}

// Create an entity
val createEntity(std::string type, std::string name) {
    auto& doc = getDocument();

    // Create entity based on type
    EntityID id = doc.createEntity(type, name);

    val result = val::object();
    result.set("id", id.toString());
    result.set("type", type);
    result.set("name", name);

    return result;
}

// Delete entity
bool deleteEntity(std::string entityId) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);
    return doc.deleteEntity(id);
}

// Get entity
val getEntity(std::string entityId) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);

    auto entity = doc.getEntity(id);
    if (!entity) {
        return val::null();
    }

    val result = val::object();
    result.set("id", id.toString());
    result.set("name", entity->name);
    result.set("type", entity->type);

    return result;
}

// Get all entities
val getAllEntities() {
    auto& doc = getDocument();
    auto entities = doc.getAllEntities();

    val result = val::array();
    int index = 0;

    for (const auto& entity : entities) {
        val item = val::object();
        item.set("id", entity.id.toString());
        item.set("name", entity.name);
        item.set("type", entity.type);
        result.set(index++, item);
    }

    return result;
}

// Set parameter
bool setParameter(std::string entityId, std::string paramName, double value) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);

    return doc.setParameter(id, paramName, value);
}

// Get parameter
double getParameter(std::string entityId, std::string paramName) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);

    return doc.getParameter(id, paramName);
}

// Execute command
bool executeCommand(std::string command, val params) {
    if (!g_command_manager) {
        return false;
    }

    try {
        // Execute command with parameters
        g_command_manager->execute(command, params);
        return true;
    } catch (...) {
        return false;
    }
}

// Undo
bool undo() {
    auto& doc = getDocument();
    return doc.undo();
}

// Redo
bool redo() {
    auto& doc = getDocument();
    return doc.redo();
}

// Get geometry
val getGeometry(std::string entityId) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);

    if (!g_geometry_engine) {
        g_geometry_engine = std::make_unique<GeometryEngine>();
    }

    auto geom = g_geometry_engine->computeGeometry(id, doc);

    val result = val::object();
    result.set("vertices", val::array());
    result.set("faces", val::array());
    result.set("edges", val::array());

    return result;
}

// Triangulate geometry
val triangulateGeometry(std::string entityId) {
    auto& doc = getDocument();
    EntityID id = EntityID::fromString(entityId);

    if (!g_geometry_engine) {
        g_geometry_engine = std::make_unique<GeometryEngine>();
    }

    auto triangles = g_geometry_engine->triangulate(id, doc);

    val result = val::object();
    result.set("vertices", val::array());
    result.set("indices", val::array());

    return result;
}

// Save document
val saveDocument() {
    auto& doc = getDocument();

    try {
        Serializer serializer;
        std::string json = serializer.serialize(doc);

        val result = val::object();
        result.set("success", true);
        result.set("data", json);

        return result;
    } catch (const std::exception& e) {
        val result = val::object();
        result.set("success", false);
        result.set("error", std::string(e.what()));

        return result;
    }
}

// Load document
val loadDocument(std::string json) {
    try {
        Serializer serializer;
        auto doc = serializer.deserialize(json);
        g_document = doc;
        g_command_manager = std::make_unique<CommandManager>(*g_document);

        val result = val::object();
        result.set("success", true);
        result.set("message", "Document loaded successfully");

        return result;
    } catch (const std::exception& e) {
        val result = val::object();
        result.set("success", false);
        result.set("error", std::string(e.what()));

        return result;
    }
}

// Get document stats
val getDocumentStats() {
    auto& doc = getDocument();

    val stats = val::object();
    stats.set("entityCount", doc.getEntityCount());
    stats.set("transactionCount", 0); // Would need transaction tracking
    stats.set("hasPendingChanges", doc.hasPendingChanges());

    return stats;
}

// Bindings
EMSCRIPTEN_BINDINGS(bendcore_bindings) {
    function("initDocument", &initDocument);
    function("createEntity", &createEntity);
    function("deleteEntity", &deleteEntity);
    function("getEntity", &getEntity);
    function("getAllEntities", &getAllEntities);
    function("setParameter", &setParameter);
    function("getParameter", &getParameter);
    function("executeCommand", &executeCommand);
    function("undo", &undo);
    function("redo", &redo);
    function("getGeometry", &getGeometry);
    function("triangulateGeometry", &triangulateGeometry);
    function("saveDocument", &saveDocument);
    function("loadDocument", &loadDocument);
    function("getDocumentStats", &getDocumentStats);
}
