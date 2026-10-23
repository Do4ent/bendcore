/**
 * BendCore Properties Manager Implementation
 * Manages object properties and their persistence
 */

#include "properties_manager.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace bendcore {

PropertiesManager::PropertiesManager() = default;

PropertiesManager::~PropertiesManager() = default;

std::vector<PropertyGroup> PropertiesManager::get_document_properties() const {
  return build_document_properties();
}

std::vector<PropertyGroup> PropertiesManager::get_tube_properties(
    const std::string& tube_id) const {
  return build_tube_properties();
}

std::vector<PropertyGroup> PropertiesManager::get_sketch_properties(
    const std::string& sketch_id) const {
  return build_sketch_properties();
}

std::vector<PropertyGroup> PropertiesManager::get_port_properties(
    const std::string& port_id) const {
  return build_port_properties();
}

std::vector<PropertyGroup> PropertiesManager::get_command_parameters(
    const std::string& command_type) const {
  return build_command_parameters(command_type);
}

bool PropertiesManager::set_property(const std::string& object_id,
                                      const std::string& property_name,
                                      const std::any& value) {
  object_properties_[object_id][property_name] = value;
  return true;
}

std::any PropertiesManager::get_property(const std::string& object_id,
                                          const std::string& property_name) const {
  auto obj_it = object_properties_.find(object_id);
  if (obj_it != object_properties_.end()) {
    auto prop_it = obj_it->second.find(property_name);
    if (prop_it != obj_it->second.end()) {
      return prop_it->second;
    }
  }
  return std::any();
}

std::string PropertiesManager::validate_property(const std::string& property_type,
                                                  const std::any& value) const {
  if (property_type == "number") {
    try {
      std::any_cast<double>(value);
      return "";  // Valid
    } catch (...) {
      return "Invalid number format";
    }
  }

  if (property_type == "color") {
    const auto& str = std::any_cast<std::string>(value);
    if (str.length() == 7 && str[0] == '#') {
      return "";  // Valid hex color
    }
    return "Invalid color format (use #RRGGBB)";
  }

  return "";  // Valid for other types
}

std::vector<PropertyGroup> PropertiesManager::build_document_properties() const {
  PropertyGroup group;
  group.id = "document-props";
  group.title = "Document Properties";

  Property name_prop;
  name_prop.name = "documentName";
  name_prop.label = "Name";
  name_prop.type = "text";
  name_prop.value = std::string("Untitled Document");
  group.properties.push_back(name_prop);

  Property standard_prop;
  standard_prop.name = "standard";
  standard_prop.label = "Standard";
  standard_prop.type = "select";
  standard_prop.value = std::string("ISO");
  standard_prop.options = {
      {"ISO (International)", std::string("ISO")},
      {"GOST (Russian)", std::string("GOST")},
      {"DIN (German)", std::string("DIN")},
      {"ANSI (American)", std::string("ANSI")},
  };
  group.properties.push_back(standard_prop);

  Property description_prop;
  description_prop.name = "description";
  description_prop.label = "Description";
  description_prop.type = "textarea";
  description_prop.value = std::string("");
  group.properties.push_back(description_prop);

  return {group};
}

std::vector<PropertyGroup> PropertiesManager::build_tube_properties() const {
  std::vector<PropertyGroup> groups;

  // Basic properties
  PropertyGroup basic;
  basic.id = "tube-basic";
  basic.title = "Basic Properties";

  Property name_prop;
  name_prop.name = "tubeName";
  name_prop.label = "Name";
  name_prop.type = "text";
  name_prop.value = std::string("Tube_1");
  basic.properties.push_back(name_prop);

  Property material_prop;
  material_prop.name = "material";
  material_prop.label = "Material";
  material_prop.type = "select";
  material_prop.value = std::string("Steel");
  material_prop.options = {
      {"Steel", std::string("Steel")},
      {"Aluminum", std::string("Aluminum")},
      {"Copper", std::string("Copper")},
      {"Titanium", std::string("Titanium")},
      {"Stainless Steel", std::string("StainlessSteel")},
  };
  basic.properties.push_back(material_prop);

  groups.push_back(basic);

  // Dimensions
  PropertyGroup dimensions;
  dimensions.id = "tube-dimensions";
  dimensions.title = "Dimensions";

  Property od_prop;
  od_prop.name = "outerDiameter";
  od_prop.label = "Outer Diameter";
  od_prop.type = "number";
  od_prop.value = 12.7;
  od_prop.unit = "mm";
  od_prop.min_value = 0.5;
  od_prop.max_value = 100.0;
  od_prop.step = 0.1;
  dimensions.properties.push_back(od_prop);

  Property wt_prop;
  wt_prop.name = "wallThickness";
  wt_prop.label = "Wall Thickness";
  wt_prop.type = "number";
  wt_prop.value = 1.6;
  wt_prop.unit = "mm";
  wt_prop.min_value = 0.1;
  wt_prop.max_value = 10.0;
  wt_prop.step = 0.1;
  dimensions.properties.push_back(wt_prop);

  groups.push_back(dimensions);

  // Bending properties
  PropertyGroup bending;
  bending.id = "tube-bending";
  bending.title = "Bending Properties";

  Property bend_radius_prop;
  bend_radius_prop.name = "bendRadius";
  bend_radius_prop.label = "Bend Radius";
  bend_radius_prop.type = "number";
  bend_radius_prop.value = 20.0;
  bend_radius_prop.unit = "mm";
  bend_radius_prop.min_value = 5.0;
  bend_radius_prop.max_value = 500.0;
  bend_radius_prop.step = 0.5;
  bending.properties.push_back(bend_radius_prop);

  groups.push_back(bending);

  return groups;
}

std::vector<PropertyGroup> PropertiesManager::build_sketch_properties() const {
  PropertyGroup group;
  group.id = "sketch-basic";
  group.title = "Sketch Properties";

  Property name_prop;
  name_prop.name = "sketchName";
  name_prop.label = "Name";
  name_prop.type = "text";
  name_prop.value = std::string("Sketch_1");
  group.properties.push_back(name_prop);

  Property plane_prop;
  plane_prop.name = "plane";
  plane_prop.label = "Plane";
  plane_prop.type = "readonly";
  plane_prop.value = std::string("XY Plane");
  plane_prop.is_readonly = true;
  group.properties.push_back(plane_prop);

  return {group};
}

std::vector<PropertyGroup> PropertiesManager::build_port_properties() const {
  PropertyGroup group;
  group.id = "port-props";
  group.title = "Port Properties";

  Property name_prop;
  name_prop.name = "portName";
  name_prop.label = "Name";
  name_prop.type = "text";
  name_prop.value = std::string("Port_1");
  group.properties.push_back(name_prop);

  Property type_prop;
  type_prop.name = "portType";
  type_prop.label = "Port Type";
  type_prop.type = "select";
  type_prop.value = std::string("connector");
  type_prop.options = {
      {"Connector", std::string("connector")},
      {"Flange", std::string("flange")},
      {"Threaded", std::string("threaded")},
      {"Welded", std::string("welded")},
  };
  group.properties.push_back(type_prop);

  return {group};
}

std::vector<PropertyGroup> PropertiesManager::build_command_parameters(
    const std::string& command_type) const {
  PropertyGroup group;
  group.id = "command-params";
  group.title = command_type + " Parameters";

  if (command_type == "new-tube") {
    Property diameter;
    diameter.name = "diameter";
    diameter.label = "Outer Diameter";
    diameter.type = "number";
    diameter.value = 12.7;
    diameter.unit = "mm";
    diameter.min_value = 0.5;
    diameter.max_value = 100.0;
    diameter.step = 0.1;
    group.properties.push_back(diameter);

    Property thickness;
    thickness.name = "thickness";
    thickness.label = "Wall Thickness";
    thickness.type = "number";
    thickness.value = 1.6;
    thickness.unit = "mm";
    thickness.min_value = 0.1;
    thickness.max_value = 10.0;
    thickness.step = 0.1;
    group.properties.push_back(thickness);
  }

  return {group};
}

json PropertiesManager::property_to_json(const Property& prop) const {
  json j;
  j["name"] = prop.name;
  j["label"] = prop.label;
  j["type"] = prop.type;
  j["unit"] = prop.unit;
  j["readonly"] = prop.is_readonly;
  j["min"] = prop.min_value;
  j["max"] = prop.max_value;
  j["step"] = prop.step;

  // Convert value to JSON
  if (prop.value.type() == typeid(std::string)) {
    j["value"] = std::any_cast<std::string>(prop.value);
  } else if (prop.value.type() == typeid(double)) {
    j["value"] = std::any_cast<double>(prop.value);
  } else if (prop.value.type() == typeid(int)) {
    j["value"] = std::any_cast<int>(prop.value);
  } else if (prop.value.type() == typeid(bool)) {
    j["value"] = std::any_cast<bool>(prop.value);
  }

  // Convert options
  if (!prop.options.empty()) {
    j["options"] = json::array();
    for (const auto& opt : prop.options) {
      json opt_j;
      opt_j["label"] = opt.first;
      if (opt.second.type() == typeid(std::string)) {
        opt_j["value"] = std::any_cast<std::string>(opt.second);
      }
      j["options"].push_back(opt_j);
    }
  }

  return j;
}

json PropertiesManager::group_to_json(const PropertyGroup& group) const {
  json j;
  j["id"] = group.id;
  j["title"] = group.title;
  j["collapsed"] = group.collapsed;
  j["properties"] = json::array();

  for (const auto& prop : group.properties) {
    j["properties"].push_back(property_to_json(prop));
  }

  return j;
}

json PropertiesManager::properties_to_json(
    const std::vector<PropertyGroup>& groups) const {
  json j = json::array();
  for (const auto& group : groups) {
    j.push_back(group_to_json(group));
  }
  return j;
}

}  // namespace bendcore
