/**
 * BendCore Properties Manager
 * Manages object properties and their persistence
 * Phase 32: Professional UI
 */

#ifndef BENDCORE_PROPERTIES_MANAGER_HPP
#define BENDCORE_PROPERTIES_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <any>

namespace bendcore {

/**
 * Property schema definition
 */
struct Property {
  std::string name;
  std::string label;
  std::string type;  // text, number, textarea, select, color, toggle, readonly
  std::any value;
  std::vector<std::pair<std::string, std::any>> options;  // For select type
  std::string unit;
  double min_value = 0.0;
  double max_value = 100.0;
  double step = 1.0;
  bool is_readonly = false;
};

/**
 * Property group for organizing properties
 */
struct PropertyGroup {
  std::string id;
  std::string title;
  std::vector<Property> properties;
  bool collapsed = false;
};

/**
 * Properties manager
 */
class PropertiesManager {
 public:
  PropertiesManager();
  ~PropertiesManager();

  // Get properties for different object types
  std::vector<PropertyGroup> get_document_properties() const;
  std::vector<PropertyGroup> get_tube_properties(const std::string& tube_id) const;
  std::vector<PropertyGroup> get_sketch_properties(const std::string& sketch_id) const;
  std::vector<PropertyGroup> get_port_properties(const std::string& port_id) const;
  std::vector<PropertyGroup> get_command_parameters(const std::string& command_type) const;

  // Set property value
  bool set_property(const std::string& object_id, const std::string& property_name,
                    const std::any& value);

  // Get property value
  std::any get_property(const std::string& object_id, const std::string& property_name) const;

  // Validate property value
  std::string validate_property(const std::string& property_type, const std::any& value) const;

  // Convert properties to JSON for IPC
  nlohmann::json properties_to_json(const std::vector<PropertyGroup>& groups) const;

 private:
  std::map<std::string, std::map<std::string, std::any>> object_properties_;

  // Property builders
  std::vector<PropertyGroup> build_document_properties() const;
  std::vector<PropertyGroup> build_tube_properties() const;
  std::vector<PropertyGroup> build_sketch_properties() const;
  std::vector<PropertyGroup> build_port_properties() const;
  std::vector<PropertyGroup> build_command_parameters(const std::string& command_type) const;

  // Helpers
  nlohmann::json property_to_json(const Property& prop) const;
  nlohmann::json group_to_json(const PropertyGroup& group) const;
};

}  // namespace bendcore

#endif  // BENDCORE_PROPERTIES_MANAGER_HPP
