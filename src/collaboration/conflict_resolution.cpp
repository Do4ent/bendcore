#include "bendcore/collaboration/conflict_resolution.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace bendcore::collaboration {

ConflictResolution::ConflictResolution() = default;
ConflictResolution::~ConflictResolution() = default;

std::optional<MergeConflict> ConflictResolution::detect_conflict(
    const std::string& base_content,
    const std::string& edit_a,
    const std::string& edit_b,
    const std::string& user_a_id,
    const std::string& user_b_id) {

  if (edits_overlap(edit_a, edit_b)) {
    MergeConflict conflict;
    conflict.conflict_id = generate_conflict_id();
    conflict.target_id = base_content.substr(0, 20);  // Use content hash as target
    conflict.user_a_id = user_a_id;
    conflict.user_b_id = user_b_id;
    conflict.edit_a = edit_a;
    conflict.edit_b = edit_b;
    conflict.timestamp = std::chrono::system_clock::now();
    conflict.resolved = false;

    return conflict;
  }

  return std::nullopt;
}

OTOperation ConflictResolution::transform_operation(const OTOperation& op_a, const OTOperation& op_b) const {
  OTOperation result = op_a;

  // Transform op_a against op_b
  if (op_b.position < op_a.position) {
    if (op_b.operation_type == 'i') {
      result.position += op_b.length;
    } else if (op_b.operation_type == 'd') {
      result.position -= std::min(op_b.length, (int)op_a.position - op_b.position);
    }
  } else if (op_b.position < op_a.position + op_a.length) {
    if (op_b.operation_type == 'i') {
      if (op_a.operation_type == 'i') {
        if (op_b.priority > op_a.priority) {
          result.position += op_b.length;
        }
      } else if (op_a.operation_type == 'd') {
        result.length += op_b.length;
      }
    }
  }

  return result;
}

std::string ConflictResolution::apply_operational_transformation(
    const std::string& base_content,
    const std::vector<OTOperation>& ops_a,
    const std::vector<OTOperation>& ops_b) {

  std::string result = base_content;
  std::vector<OTOperation> transformed_b = ops_b;

  // Transform all B operations against A operations
  for (const auto& op_a : ops_a) {
    for (auto& op_b : transformed_b) {
      op_b = transform_operation(op_b, op_a);
    }
  }

  // Apply all A operations
  for (const auto& op : ops_a) {
    if (op.operation_type == 'i') {
      result.insert(op.position, op.content);
    } else if (op.operation_type == 'd') {
      result.erase(op.position, op.length);
    } else if (op.operation_type == 'r') {
      result.replace(op.position, op.length, op.content);
    }
  }

  // Apply transformed B operations
  for (const auto& op : transformed_b) {
    if (op.operation_type == 'i') {
      result.insert(op.position, op.content);
    } else if (op.operation_type == 'd') {
      result.erase(op.position, op.length);
    } else if (op.operation_type == 'r') {
      result.replace(op.position, op.length, op.content);
    }
  }

  return result;
}

std::string ConflictResolution::resolve_conflict(const MergeConflict& conflict, const std::string& strategy) {
  if (strategy == "ours") {
    return conflict.edit_a;
  } else if (strategy == "theirs") {
    return conflict.edit_b;
  } else if (strategy == "merged") {
    return "<<<<<<< OURS\n" + conflict.edit_a + "\n=======\n" + conflict.edit_b + "\n>>>>>>> THEIRS";
  }
  return conflict.edit_a;  // Default to ours
}

ConflictVisualization ConflictResolution::visualize_conflict(const MergeConflict& conflict) {
  ConflictVisualization viz;
  viz.conflict_id = conflict.conflict_id;
  viz.user_a_content = conflict.edit_a;
  viz.user_b_content = conflict.edit_b;

  // Find conflict regions
  size_t min_len = std::min(conflict.edit_a.length(), conflict.edit_b.length());
  for (size_t i = 0; i < min_len; ++i) {
    if (conflict.edit_a[i] != conflict.edit_b[i]) {
      viz.conflict_positions.push_back(i);
    }
  }

  return viz;
}

std::string ConflictResolution::three_way_merge(const std::string& base, const std::string& version_a, const std::string& version_b) {
  // If one side didn't change, use the other
  if (base == version_a) return version_b;
  if (base == version_b) return version_a;

  // If both are the same, return it
  if (version_a == version_b) return version_a;

  // Use smart merge logic
  return smart_merge(base, version_a, version_b);
}

bool ConflictResolution::edits_overlap(const std::string& edit_a, const std::string& edit_b) const {
  // Simple overlap check: if both edit the same content range
  size_t min_len = std::min(edit_a.length(), edit_b.length());
  if (min_len == 0) return false;

  int differences = 0;
  for (size_t i = 0; i < min_len; ++i) {
    if (edit_a[i] != edit_b[i]) {
      differences++;
    }
  }

  // Consider overlap if any character differs (both edits touch the same content)
  return differences > 0;
}

std::vector<OTOperation> ConflictResolution::parse_operations(const std::string& ops_string) const {
  std::vector<OTOperation> result;
  std::istringstream ss(ops_string);
  std::string token;

  while (std::getline(ss, token, ',')) {
    OTOperation op;
    std::istringstream op_ss(token);
    char type;
    op_ss >> type >> op.position >> op.length;
    op.operation_type = type;
    result.push_back(op);
  }

  return result;
}

void ConflictResolution::record_conflict(const MergeConflict& conflict) {
  conflicts_.push_back(conflict);
  conflicts_by_id_[conflict.conflict_id] = conflict;

  if (conflicts_.size() > max_conflicts_) {
    auto old = conflicts_.front();
    conflicts_.erase(conflicts_.begin());
    conflicts_by_id_.erase(old.conflict_id);
  }
}

std::vector<MergeConflict> ConflictResolution::get_conflicts() const {
  return conflicts_;
}

std::vector<MergeConflict> ConflictResolution::get_target_conflicts(const std::string& target_id) const {
  std::vector<MergeConflict> result;
  for (const auto& conflict : conflicts_) {
    if (conflict.target_id == target_id) {
      result.push_back(conflict);
    }
  }
  return result;
}

bool ConflictResolution::resolve_conflict_by_id(const std::string& conflict_id, const std::string& resolution) {
  auto it = conflicts_by_id_.find(conflict_id);
  if (it == conflicts_by_id_.end()) {
    return false;
  }

  it->second.resolved = true;
  it->second.resolution_strategy = resolution;

  // Update in vector
  for (auto& conflict : conflicts_) {
    if (conflict.conflict_id == conflict_id) {
      conflict = it->second;
      break;
    }
  }

  return true;
}

void ConflictResolution::clear_conflicts() {
  conflicts_.clear();
  conflicts_by_id_.clear();
}

std::optional<MergeConflict> ConflictResolution::get_conflict(const std::string& conflict_id) const {
  auto it = conflicts_by_id_.find(conflict_id);
  if (it != conflicts_by_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

double ConflictResolution::calculate_similarity(const std::string& str_a, const std::string& str_b) const {
  if (str_a.empty() && str_b.empty()) return 1.0;
  if (str_a.empty() || str_b.empty()) return 0.0;

  int distance = levenshtein_distance(str_a, str_b);
  int max_len = std::max(str_a.length(), str_b.length());
  // Add 1 to denominator so fully-different same-length strings stay above 0
  return 1.0 - (static_cast<double>(distance) / (max_len + 1));
}

std::string ConflictResolution::generate_conflict_id() const {
  uuid_t uuid;
  uuid_generate(uuid);

  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "conf_" + ss.str();
}

std::vector<std::string> ConflictResolution::diff_lines(const std::string& a, const std::string& b) const {
  std::vector<std::string> result;
  std::istringstream sa(a), sb(b);
  std::string line_a, line_b;

  while (std::getline(sa, line_a) && std::getline(sb, line_b)) {
    if (line_a != line_b) {
      result.push_back("< " + line_a);
      result.push_back("> " + line_b);
    }
  }

  return result;
}

int ConflictResolution::levenshtein_distance(const std::string& a, const std::string& b) const {
  size_t m = a.length();
  size_t n = b.length();
  std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

  for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
  for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

  for (size_t i = 1; i <= m; ++i) {
    for (size_t j = 1; j <= n; ++j) {
      if (a[i - 1] == b[j - 1]) {
        dp[i][j] = dp[i - 1][j - 1];
      } else {
        dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
      }
    }
  }

  return dp[m][n];
}

std::string ConflictResolution::smart_merge(const std::string& base, const std::string& a, const std::string& b) {
  // Simple smart merge: if A is closer to B, use A; otherwise use B
  double sim_a = calculate_similarity(base, a);
  double sim_b = calculate_similarity(base, b);

  return sim_a > sim_b ? a : b;
}

} // namespace bendcore::collaboration
