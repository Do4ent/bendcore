#include "bendcore/drawing/drawing_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace BendCore::CadCore {

using json = nlohmann::json;

// ============ DrawingManager ============

DrawingManager::DrawingManager()
    : serializer_(DocumentSerializer::Format::JSON)
{}

DrawingOperationResult DrawingManager::createDrawing(
    const std::string& name,
    const EntityId& source_id,
    const std::string& title)
{
    try {
        auto drawing_id = EntityId::generate();
        auto drawing = std::make_shared<Drawing>(drawing_id, name);
        drawing->setSourceId(source_id);
        if (!title.empty()) {
            drawing->setTitle(title);
        }

        drawings_[drawing_id] = drawing;
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to create drawing: ") + e.what());
    }
}

std::shared_ptr<Drawing> DrawingManager::getDrawing(const EntityId& drawing_id) {
    auto it = drawings_.find(drawing_id);
    if (it == drawings_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<const Drawing> DrawingManager::getDrawing(const EntityId& drawing_id) const {
    auto it = drawings_.find(drawing_id);
    if (it == drawings_.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<EntityId> DrawingManager::getAllDrawingIds() const {
    std::vector<EntityId> ids;
    for (const auto& [drawing_id, _] : drawings_) {
        ids.push_back(drawing_id);
    }
    return ids;
}

DrawingOperationResult DrawingManager::deleteDrawing(const EntityId& drawing_id) {
    if (!drawings_.count(drawing_id)) {
        return DrawingOperationResult(false, "Drawing not found");
    }
    drawings_.erase(drawing_id);
    return DrawingOperationResult(true);
}

DrawingOperationResult DrawingManager::addSheet(
    const EntityId& drawing_id,
    const std::string& name,
    SheetSize size)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        auto sheet = drawing->addSheet(name);
        sheet->setProperties(SheetProperties(size));

        DrawingOperationResult result(true);
        result.created_entity_id = sheet->getId();
        return result;
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to add sheet: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::removeSheet(
    const EntityId& drawing_id,
    const EntityId& sheet_id)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        drawing->removeSheet(sheet_id);
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to remove sheet: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::addView(
    const EntityId& drawing_id,
    const EntityId& source_geometry_id,
    const std::string& name)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        auto view = drawing->addView(source_geometry_id, name);

        DrawingOperationResult result(true);
        result.created_entity_id = view->getId();
        return result;
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to add view: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::removeView(
    const EntityId& drawing_id,
    const EntityId& view_id)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        drawing->removeView(view_id);
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to remove view: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::addViewToSheet(
    const EntityId& drawing_id,
    const EntityId& sheet_id,
    const EntityId& view_id,
    const ViewProperties& props)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        auto sheet = drawing->getSheet(sheet_id);
        if (!sheet) {
            return DrawingOperationResult(false, "Sheet not found");
        }

        auto view = drawing->getView(view_id);
        if (!view) {
            return DrawingOperationResult(false, "View not found");
        }

        sheet->addView(view_id, props);
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to add view to sheet: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::updateViewOnSheet(
    const EntityId& drawing_id,
    const EntityId& sheet_id,
    const EntityId& view_id,
    const ViewProperties& props)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        auto sheet = drawing->getSheet(sheet_id);
        if (!sheet) {
            return DrawingOperationResult(false, "Sheet not found");
        }

        sheet->updateViewProperties(view_id, props);
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to update view on sheet: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::removeViewFromSheet(
    const EntityId& drawing_id,
    const EntityId& sheet_id,
    const EntityId& view_id)
{
    try {
        auto drawing = getDrawing(drawing_id);
        if (!drawing) {
            return DrawingOperationResult(false, "Drawing not found");
        }

        auto sheet = drawing->getSheet(sheet_id);
        if (!sheet) {
            return DrawingOperationResult(false, "Sheet not found");
        }

        sheet->removeView(view_id);
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to remove view from sheet: ") + e.what());
    }
}

std::string DrawingManager::serializeToJson(const EntityId& drawing_id) const {
    auto drawing = getDrawing(drawing_id);
    if (!drawing) {
        return "{}";  // Return empty JSON if not found
    }
    return drawingToJson(*drawing);
}

DrawingOperationResult DrawingManager::deserializeFromJson(const std::string& json_str) {
    try {
        auto drawing_opt = jsonToDrawing(json_str);
        if (!drawing_opt) {
            return DrawingOperationResult(false, "Failed to parse JSON");
        }

        auto drawing = drawing_opt.value();
        auto drawing_id = drawing->getId();
        drawings_[drawing_id] = drawing;

        DrawingOperationResult result(true);
        result.created_entity_id = drawing_id;
        return result;
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to deserialize drawing: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::saveDrawingToFile(
    const EntityId& drawing_id,
    const std::string& filepath)
{
    try {
        auto json_str = serializeToJson(drawing_id);
        if (json_str.empty() || json_str == "{}") {
            return DrawingOperationResult(false, "Drawing not found");
        }

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return DrawingOperationResult(false, "Failed to open file for writing");
        }

        file << json_str;
        file.close();
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to save drawing: ") + e.what());
    }
}

DrawingOperationResult DrawingManager::loadDrawingFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return DrawingOperationResult(false, "Failed to open file for reading");
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return deserializeFromJson(buffer.str());
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to load drawing: ") + e.what());
    }
}

std::vector<std::string> DrawingManager::validateAllDrawings() const {
    std::vector<std::string> errors;
    for (const auto& [drawing_id, drawing] : drawings_) {
        if (drawing) {
            auto error = drawing->validate();
            if (!error.empty()) {
                errors.push_back(std::string("Drawing ") + drawing_id.toString() + ": " + error);
            }
        }
    }
    return errors;
}

DrawingOperationResult DrawingManager::backupDrawing(
    const EntityId& drawing_id,
    const std::string& backup_filepath)
{
    try {
        auto json_str = serializeToJson(drawing_id);
        if (json_str.empty() || json_str == "{}") {
            return DrawingOperationResult(false, "Drawing not found");
        }

        // Create backup with timestamp
        std::string backup_path = backup_filepath + ".backup";
        std::ofstream file(backup_path);
        if (!file.is_open()) {
            return DrawingOperationResult(false, "Failed to create backup file");
        }

        file << json_str;
        file.close();
        return DrawingOperationResult(true);
    } catch (const std::exception& e) {
        return DrawingOperationResult(false, std::string("Failed to backup drawing: ") + e.what());
    }
}

std::string DrawingManager::drawingToJson(const Drawing& drawing) const {
    try {
        json j;
        j["id"] = drawing.getId().toString();
        j["name"] = drawing.getName();
        j["title"] = drawing.getTitle();
        j["source_id"] = drawing.getSourceId().toString();
        j["scale"] = drawing.getScale();
        j["units"] = static_cast<int>(drawing.getUnits());
        j["projection"] = static_cast<int>(drawing.getProjectionMethod());
        j["revision"] = drawing.getRevision();

        // Serialize sheets
        json sheets_json = json::array();
        for (const auto& sheet_id : drawing.getSheetIds()) {
            auto sheet = drawing.getSheet(sheet_id);
            if (sheet) {
                json sheet_j;
                sheet_j["id"] = sheet->getId().toString();
                sheet_j["name"] = sheet->getName();
                sheet_j["revision"] = sheet->getRevision();

                const auto& props = sheet->getProperties();
                sheet_j["width_mm"] = props.width_mm;
                sheet_j["height_mm"] = props.height_mm;
                sheet_j["sheet_number"] = props.sheet_number;

                // Serialize views on sheet
                json views_on_sheet = json::array();
                for (const auto& view_id : sheet->getViewIds()) {
                    auto view_props = sheet->getViewProperties(view_id);
                    if (view_props) {
                        json view_j;
                        view_j["view_id"] = view_id.toString();
                        view_j["position_x"] = view_props->position_x;
                        view_j["position_y"] = view_props->position_y;
                        view_j["width"] = view_props->width;
                        view_j["height"] = view_props->height;
                        view_j["scale"] = view_props->scale;
                        views_on_sheet.push_back(view_j);
                    }
                }
                sheet_j["views"] = views_on_sheet;
                sheets_json.push_back(sheet_j);
            }
        }
        j["sheets"] = sheets_json;

        // Serialize views
        json views_json = json::array();
        for (const auto& view_id : drawing.getViewIds()) {
            auto view = drawing.getView(view_id);
            if (view) {
                json view_j;
                view_j["id"] = view->getId().toString();
                view_j["name"] = view->getName();
                view_j["source_geometry_id"] = view->getSourceGeometryId().toString();
                view_j["revision"] = view->getRevision();

                const auto& props = view->getProperties();
                view_j["position_x"] = props.position_x;
                view_j["position_y"] = props.position_y;
                view_j["width"] = props.width;
                view_j["height"] = props.height;
                view_j["scale"] = props.scale;
                views_json.push_back(view_j);
            }
        }
        j["views"] = views_json;

        // Serialize metadata
        json metadata_json;
        for (const auto& [key, value] : drawing.getMetadataMap()) {
            metadata_json[key] = value;
        }
        j["metadata"] = metadata_json;

        return j.dump(2);
    } catch (const std::exception& e) {
        return "{}";
    }
}

std::optional<std::shared_ptr<Drawing>> DrawingManager::jsonToDrawing(const std::string& json_str) {
    try {
        json j = json::parse(json_str);

        EntityId id = EntityId::parse(j["id"].get<std::string>());
        std::string name = j["name"].get<std::string>();

        auto drawing = std::make_shared<Drawing>(id, name);

        if (j.contains("title")) {
            drawing->setTitle(j["title"].get<std::string>());
        }

        if (j.contains("source_id")) {
            drawing->setSourceId(EntityId::parse(j["source_id"].get<std::string>()));
        }

        if (j.contains("scale")) {
            drawing->setScale(j["scale"].get<double>());
        }

        if (j.contains("units")) {
            drawing->setUnits(static_cast<DrawingUnits>(j["units"].get<int>()));
        }

        if (j.contains("projection")) {
            drawing->setProjectionMethod(static_cast<ProjectionMethod>(j["projection"].get<int>()));
        }

        // Deserialize sheets
        if (j.contains("sheets") && j["sheets"].is_array()) {
            for (const auto& sheet_j : j["sheets"]) {
                EntityId sheet_id = EntityId::parse(sheet_j["id"].get<std::string>());
                std::string sheet_name = sheet_j["name"].get<std::string>();

                auto sheet = std::make_shared<DrawingSheet>(sheet_id, sheet_name);

                if (sheet_j.contains("width_mm")) {
                    SheetProperties props;
                    props.width_mm = sheet_j["width_mm"].get<double>();
                    props.height_mm = sheet_j["height_mm"].get<double>();
                    sheet->setProperties(props);
                }

                drawing->sheets_[sheet_id] = sheet;

                // Deserialize views on sheet
                if (sheet_j.contains("views") && sheet_j["views"].is_array()) {
                    for (const auto& view_on_sheet : sheet_j["views"]) {
                        EntityId view_id = EntityId::parse(view_on_sheet["view_id"].get<std::string>());
                        ViewProperties view_props;
                        view_props.position_x = view_on_sheet["position_x"].get<double>();
                        view_props.position_y = view_on_sheet["position_y"].get<double>();
                        view_props.width = view_on_sheet["width"].get<double>();
                        view_props.height = view_on_sheet["height"].get<double>();
                        view_props.scale = view_on_sheet["scale"].get<double>();

                        sheet->views_[view_id] = view_props;
                    }
                }
            }
        }

        // Deserialize views
        if (j.contains("views") && j["views"].is_array()) {
            for (const auto& view_j : j["views"]) {
                EntityId view_id = EntityId::parse(view_j["id"].get<std::string>());
                EntityId source_geometry_id = EntityId::parse(view_j["source_geometry_id"].get<std::string>());
                std::string view_name = view_j["name"].get<std::string>();

                auto view = std::make_shared<DrawingView>(view_id, source_geometry_id, view_name);

                if (view_j.contains("position_x")) {
                    ViewProperties props;
                    props.position_x = view_j["position_x"].get<double>();
                    props.position_y = view_j["position_y"].get<double>();
                    props.width = view_j["width"].get<double>();
                    props.height = view_j["height"].get<double>();
                    props.scale = view_j["scale"].get<double>();
                    view->setProperties(props);
                }

                drawing->views_[view_id] = view;
            }
        }

        // Deserialize metadata
        if (j.contains("metadata") && j["metadata"].is_object()) {
            for (const auto& [key, value] : j["metadata"].items()) {
                drawing->setMetadata(key, value.get<std::string>());
            }
        }

        return drawing;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

} // namespace BendCore::CadCore
