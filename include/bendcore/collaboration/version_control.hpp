#ifndef BENDCORE_COLLABORATION_VERSION_CONTROL_HPP
#define BENDCORE_COLLABORATION_VERSION_CONTROL_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <optional>

namespace bendcore::collaboration {

struct Revision {
  int version = 0;
  std::string revision_id;
  std::string content;
  std::string author_id;
  std::chrono::system_clock::time_point created_at;
  std::string message;
  std::string parent_id;
  std::vector<std::string> tags;
};

struct VersionTag {
  std::string tag_name;
  int version = 0;
  std::string revision_id;
  std::string message;
};

class VersionControl {
 public:
  VersionControl();
  ~VersionControl();

  Revision create_revision(const std::string& content, const std::string& author_id, const std::string& message);
  std::optional<Revision> get_revision(int version) const;
  std::optional<Revision> get_revision_by_id(const std::string& revision_id) const;
  std::vector<Revision> get_history(int limit = 100) const;
  std::vector<Revision> get_author_revisions(const std::string& author_id) const;

  std::string get_diff(int version_a, int version_b) const;
  bool revert_to_version(int version, const std::string& author_id);
  bool create_tag(const std::string& tag_name, int version, const std::string& message);
  std::optional<VersionTag> get_tag(const std::string& tag_name) const;
  std::vector<VersionTag> get_all_tags() const;

  bool create_branch(const std::string& branch_name, int from_version);
  std::optional<std::vector<Revision>> get_branch(const std::string& branch_name) const;
  std::vector<std::string> get_all_branches() const;

  int get_current_version() const;
  int get_revision_count() const;

  void clear_history();

 private:
  std::vector<Revision> revisions_;
  std::map<std::string, Revision> revisions_by_id_;
  std::map<std::string, VersionTag> tags_;
  std::map<std::string, std::vector<Revision>> branches_;
  int current_version_ = 0;

  std::string generate_revision_id() const;
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_VERSION_CONTROL_HPP
