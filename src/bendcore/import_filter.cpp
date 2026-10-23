/**
 * BendCore Import Filter Implementation
 */

#include "import_filter.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace bendcore {

/**
 * Apply grouping rules to imported solids
 */
std::vector<Group> ImportFilter::apply_rules(
    const std::vector<SolidGeometry>& solids,
    const std::vector<GroupingRule>& rules) {
  std::vector<Group> result;

  for (const auto& rule : rules) {
    switch (rule) {
      case GroupingRule::RULE_1: {
        // Group by diameter
        auto group = group_by_diameter(solids);
        result.push_back(group);
        break;
      }

      case GroupingRule::RULE_2: {
        // Group by material
        auto groups = group_by_material(solids);
        result.insert(result.end(), groups.begin(), groups.end());
        break;
      }

      case GroupingRule::RULE_3: {
        // Group by bending profile
        auto groups = group_by_bending_profile(solids);
        result.insert(result.end(), groups.begin(), groups.end());
        break;
      }

      case GroupingRule::RULE_4: {
        // Group contact bodies
        auto groups = group_contact_bodies(solids);
        result.insert(result.end(), groups.begin(), groups.end());
        break;
      }

      case GroupingRule::RULE_5: {
        // Find isolated tubes
        auto group = find_isolated_tubes(solids);
        if (!group.solids.empty()) {
          result.push_back(group);
        }
        break;
      }
    }
  }

  return result;
}

/**
 * Detect round tubes in assembly
 */
std::vector<SolidGeometry> ImportFilter::detect_tubes(
    const std::vector<SolidGeometry>& solids) {
  std::vector<SolidGeometry> tubes;

  for (const auto& solid : solids) {
    if (is_cylindrical_geometry(solid) && is_hollow_geometry(solid)) {
      tubes.push_back(solid);
    }
  }

  return tubes;
}

/**
 * Filter by diameter
 */
std::vector<SolidGeometry> ImportFilter::filter_by_diameter(
    const std::vector<SolidGeometry>& solids,
    double min_od,
    double max_od) {
  std::vector<SolidGeometry> filtered;

  for (const auto& solid : solids) {
    if (solid.outer_diameter >= min_od && solid.outer_diameter <= max_od) {
      filtered.push_back(solid);
    }
  }

  return filtered;
}

/**
 * Filter by material
 */
std::vector<SolidGeometry> ImportFilter::filter_by_material(
    const std::vector<SolidGeometry>& solids,
    const std::string& material) {
  std::vector<SolidGeometry> filtered;

  for (const auto& solid : solids) {
    if (solid.material == material) {
      filtered.push_back(solid);
    }
  }

  return filtered;
}

/**
 * Detect contacts between solids
 */
std::vector<Contact> ImportFilter::detect_contacts(
    const std::vector<SolidGeometry>& solids,
    double tolerance) {
  std::vector<Contact> contacts;

  for (size_t i = 0; i < solids.size(); ++i) {
    for (size_t j = i + 1; j < solids.size(); ++j) {
      Contact contact;
      if (are_in_contact(solids[i], solids[j], tolerance, &contact)) {
        contacts.push_back(contact);
      }
    }
  }

  return contacts;
}

/**
 * Check if two solids are in contact
 */
bool ImportFilter::are_in_contact(
    const SolidGeometry& solid1,
    const SolidGeometry& solid2,
    double tolerance,
    Contact* out_contact) {
  const auto& bbox1 = solid1.bounding_box;
  const auto& bbox2 = solid2.bounding_box;

  // AABB intersection test with tolerance
  bool x_overlap = (bbox1[0] - tolerance <= bbox2[3]) &&
                   (bbox2[0] - tolerance <= bbox1[3]);
  bool y_overlap = (bbox1[1] - tolerance <= bbox2[4]) &&
                   (bbox2[1] - tolerance <= bbox1[4]);
  bool z_overlap = (bbox1[2] - tolerance <= bbox2[5]) &&
                   (bbox2[2] - tolerance <= bbox1[5]);

  if (x_overlap && y_overlap && z_overlap) {
    if (out_contact) {
      out_contact->body1_id = solid1.id;
      out_contact->body2_id = solid2.id;
      out_contact->contact_area = 0.0;  // Simplified
    }
    return true;
  }

  return false;
}

/**
 * Check if geometry is cylindrical
 */
bool ImportFilter::is_cylindrical_geometry(const SolidGeometry& solid) {
  // Simplified: check if bounding box suggests cylinder
  const auto& bbox = solid.bounding_box;
  double dx = bbox[3] - bbox[0];
  double dy = bbox[4] - bbox[1];
  double dz = bbox[5] - bbox[2];

  // One dimension significantly larger = axis-aligned cylinder
  int major_axes = 0;
  if (dx > std::max(dy, dz) * 2.0) major_axes++;
  if (dy > std::max(dx, dz) * 2.0) major_axes++;
  if (dz > std::max(dx, dy) * 2.0) major_axes++;

  return major_axes == 1;
}

/**
 * Check if geometry is hollow (tube)
 */
bool ImportFilter::is_hollow_geometry(const SolidGeometry& solid) {
  // Check if has both outer and inner diameter
  return solid.is_hollow && solid.outer_diameter > 0.0 &&
         solid.inner_diameter > 0.0 &&
         solid.outer_diameter > solid.inner_diameter;
}

/**
 * Group by diameter
 */
Group ImportFilter::group_by_diameter(const std::vector<SolidGeometry>& solids) {
  Group group;
  group.rule = GroupingRule::RULE_1;
  group.group_name = "By Diameter";
  group.description = "Tubes grouped by outer diameter";

  std::vector<SolidGeometry> tubes = detect_tubes(solids);

  // Sort by diameter
  std::sort(tubes.begin(), tubes.end(),
            [](const SolidGeometry& a, const SolidGeometry& b) {
              return a.outer_diameter < b.outer_diameter;
            });

  group.solids = tubes;
  return group;
}

/**
 * Group by material
 */
std::vector<Group> ImportFilter::group_by_material(
    const std::vector<SolidGeometry>& solids) {
  std::map<std::string, Group> material_groups;
  std::vector<SolidGeometry> tubes = detect_tubes(solids);

  for (const auto& tube : tubes) {
    std::string mat = extract_material(tube);
    if (material_groups.find(mat) == material_groups.end()) {
      Group g;
      g.rule = GroupingRule::RULE_2;
      g.group_name = "Material: " + mat;
      g.description = "Tubes made of " + mat;
      material_groups[mat] = g;
    }
    material_groups[mat].solids.push_back(tube);
  }

  std::vector<Group> result;
  for (auto& pair : material_groups) {
    result.push_back(pair.second);
  }
  return result;
}

/**
 * Group by bending profile
 */
std::vector<Group> ImportFilter::group_by_bending_profile(
    const std::vector<SolidGeometry>& solids) {
  // Simplified implementation
  // In production, would match against known profiles
  std::vector<Group> result;

  Group profile_group;
  profile_group.rule = GroupingRule::RULE_3;
  profile_group.group_name = "By Profile";
  profile_group.description = "Tubes grouped by bending profile";
  profile_group.solids = detect_tubes(solids);

  if (!profile_group.solids.empty()) {
    result.push_back(profile_group);
  }

  return result;
}

/**
 * Group contact bodies
 */
std::vector<Group> ImportFilter::group_contact_bodies(
    const std::vector<SolidGeometry>& solids) {
  std::vector<Group> result;
  auto contacts = detect_contacts(solids, 0.01);

  // Build contact graph
  std::map<int, std::set<int>> contact_graph;
  for (const auto& contact : contacts) {
    contact_graph[contact.body1_id].insert(contact.body2_id);
    contact_graph[contact.body2_id].insert(contact.body1_id);
  }

  // Find connected components (groups)
  std::set<int> visited;
  for (const auto& solid : solids) {
    if (visited.find(solid.id) != visited.end()) continue;

    // BFS to find connected component
    Group group;
    group.rule = GroupingRule::RULE_4;
    group.group_name = "Contact Group";
    group.description = "Bodies in contact";

    std::vector<int> queue = {solid.id};
    while (!queue.empty()) {
      int current_id = queue.back();
      queue.pop_back();

      if (visited.find(current_id) != visited.end()) continue;
      visited.insert(current_id);

      for (const auto& s : solids) {
        if (s.id == current_id) {
          group.solids.push_back(s);
          break;
        }
      }

      // Add neighbors
      if (contact_graph.find(current_id) != contact_graph.end()) {
        for (int neighbor_id : contact_graph[current_id]) {
          if (visited.find(neighbor_id) == visited.end()) {
            queue.push_back(neighbor_id);
          }
        }
      }
    }

    if (group.solids.size() > 1) {
      result.push_back(group);
    }
  }

  return result;
}

/**
 * Find isolated tubes
 */
Group ImportFilter::find_isolated_tubes(
    const std::vector<SolidGeometry>& solids) {
  Group group;
  group.rule = GroupingRule::RULE_5;
  group.group_name = "Isolated Tubes";
  group.description = "Tubes with no contact";

  auto contacts = detect_contacts(solids, 0.01);
  std::set<int> contacted_ids;

  for (const auto& contact : contacts) {
    contacted_ids.insert(contact.body1_id);
    contacted_ids.insert(contact.body2_id);
  }

  auto tubes = detect_tubes(solids);
  for (const auto& tube : tubes) {
    if (contacted_ids.find(tube.id) == contacted_ids.end()) {
      group.solids.push_back(tube);
    }
  }

  return group;
}

/**
 * Extract material from solid
 */
std::string ImportFilter::extract_material(const SolidGeometry& solid) {
  if (!solid.material.empty()) {
    return solid.material;
  }
  return "Unknown";
}

/**
 * Get analysis summary
 */
json ImportFilter::get_analysis_summary(
    const std::vector<SolidGeometry>& solids) {
  auto tubes = detect_tubes(solids);
  auto contacts = detect_contacts(solids, 0.01);

  json summary;
  summary["total_solids"] = solids.size();
  summary["detected_tubes"] = tubes.size();
  summary["total_contacts"] = contacts.size();

  // Diameter distribution
  json diameters = json::array();
  for (const auto& tube : tubes) {
    diameters.push_back({{"id", tube.id},
                         {"od", tube.outer_diameter},
                         {"id_inner", tube.inner_diameter},
                         {"name", tube.name}});
  }
  summary["diameter_distribution"] = diameters;

  // Material distribution
  std::map<std::string, int> material_count;
  for (const auto& tube : tubes) {
    material_count[extract_material(tube)]++;
  }
  json materials = json::object();
  for (const auto& pair : material_count) {
    materials[pair.first] = pair.second;
  }
  summary["materials"] = materials;

  return summary;
}

}  // namespace bendcore
