#ifndef BENDCORE_COLLABORATION_CONFLICT_RESOLUTION_HPP
#define BENDCORE_COLLABORATION_CONFLICT_RESOLUTION_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace bendcore::collaboration {

/**
 * Represents a merge conflict between concurrent edits
 */
struct MergeConflict {
  std::string conflict_id;
  std::string target_id;
  std::string user_a_id;
  std::string user_b_id;
  std::string edit_a;
  std::string edit_b;
  std::chrono::system_clock::time_point timestamp;
  bool resolved = false;
  std::string resolution_strategy;
};

/**
 * Operational transformation operation
 */
struct OTOperation {
  int position = 0;
  int length = 0;
  std::string content;
  char operation_type;  // 'i'=insert, 'd'=delete, 'r'=replace
  int priority = 0;
  std::string user_id;
};

/**
 * Conflict visualization data for UI display
 */
struct ConflictVisualization {
  std::string conflict_id;
  std::string base_content;
  std::string user_a_content;
  std::string user_b_content;
  std::vector<int> conflict_positions;
  std::vector<std::string> highlighted_regions;
};

/**
 * ConflictResolution handles detection and resolution of merge conflicts
 * resulting from concurrent user edits. Implements operational transformation
 * to resolve concurrent modifications.
 */
class ConflictResolution {
 public:
  ConflictResolution();
  ~ConflictResolution();

  /**
   * Detect conflicts between two concurrent edits
   *
   * @param base_content Original content before edits
   * @param edit_a First user's edit
   * @param edit_b Second user's edit
   * @param user_a_id First user ID
   * @param user_b_id Second user ID
   * @return MergeConflict structure if conflict detected
   */
  std::optional<MergeConflict> detect_conflict(
      const std::string& base_content,
      const std::string& edit_a,
      const std::string& edit_b,
      const std::string& user_a_id,
      const std::string& user_b_id);

  /**
   * Transform operation A against operation B (operational transformation)
   *
   * @param op_a First operation
   * @param op_b Second operation
   * @return Transformed operation A
   */
  OTOperation transform_operation(const OTOperation& op_a, const OTOperation& op_b) const;

  /**
   * Apply operational transformation to resolve conflict
   *
   * @param base_content Original content
   * @param ops_a Operations from user A
   * @param ops_b Operations from user B
   * @return Merged content
   */
  std::string apply_operational_transformation(
      const std::string& base_content,
      const std::vector<OTOperation>& ops_a,
      const std::vector<OTOperation>& ops_b);

  /**
   * Resolve conflict using specified strategy
   *
   * @param conflict The conflict to resolve
   * @param strategy Resolution strategy ("ours", "theirs", "merged", "manual")
   * @return Resolved content
   */
  std::string resolve_conflict(const MergeConflict& conflict, const std::string& strategy);

  /**
   * Generate visualization data for conflict display
   *
   * @param conflict The conflict to visualize
   * @return ConflictVisualization data
   */
  ConflictVisualization visualize_conflict(const MergeConflict& conflict);

  /**
   * Three-way merge using base, A, and B versions
   *
   * @param base Base content
   * @param version_a User A's version
   * @param version_b User B's version
   * @return Merged result
   */
  std::string three_way_merge(const std::string& base, const std::string& version_a, const std::string& version_b);

  /**
   * Check if two edits overlap (potential conflict area)
   *
   * @param edit_a First edit
   * @param edit_b Second edit
   * @return True if edits overlap
   */
  bool edits_overlap(const std::string& edit_a, const std::string& edit_b) const;

  /**
   * Parse operation string into OTOperation objects
   *
   * @param ops_string Comma-separated operation string
   * @return Vector of OTOperation objects
   */
  std::vector<OTOperation> parse_operations(const std::string& ops_string) const;

  /**
   * Record a conflict for audit trail
   *
   * @param conflict The conflict to record
   */
  void record_conflict(const MergeConflict& conflict);

  /**
   * Get all recorded conflicts
   *
   * @return Vector of MergeConflict objects
   */
  std::vector<MergeConflict> get_conflicts() const;

  /**
   * Get conflicts for a specific target
   *
   * @param target_id Target identifier
   * @return Vector of MergeConflict objects
   */
  std::vector<MergeConflict> get_target_conflicts(const std::string& target_id) const;

  /**
   * Resolve conflict by ID
   *
   * @param conflict_id Conflict identifier
   * @param resolution Resolution content/strategy
   * @return True if conflict was resolved
   */
  bool resolve_conflict_by_id(const std::string& conflict_id, const std::string& resolution);

  /**
   * Clear all conflicts (for testing)
   */
  void clear_conflicts();

  /**
   * Get conflict by ID
   *
   * @param conflict_id Conflict identifier
   * @return Optional MergeConflict if found
   */
  std::optional<MergeConflict> get_conflict(const std::string& conflict_id) const;

  /**
   * Calculate similarity between two strings (0.0 to 1.0)
   *
   * @param str_a First string
   * @param str_b Second string
   * @return Similarity score
   */
  double calculate_similarity(const std::string& str_a, const std::string& str_b) const;

 private:
  std::vector<MergeConflict> conflicts_;
  std::map<std::string, MergeConflict> conflicts_by_id_;

  int max_conflicts_ = 1000;

  std::string generate_conflict_id() const;
  std::vector<std::string> diff_lines(const std::string& a, const std::string& b) const;
  int levenshtein_distance(const std::string& a, const std::string& b) const;
  std::string smart_merge(const std::string& base, const std::string& a, const std::string& b);
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_CONFLICT_RESOLUTION_HPP
