#include "bendcore/collaboration/version_control.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::collaboration {

VersionControl::VersionControl() = default;
VersionControl::~VersionControl() = default;

Revision VersionControl::create_revision(const std::string& content, const std::string& author_id, const std::string& message) {
  Revision rev;
  rev.version = current_version_++;
  rev.revision_id = generate_revision_id();
  rev.content = content;
  rev.author_id = author_id;
  rev.created_at = std::chrono::system_clock::now();
  rev.message = message;

  if (!revisions_.empty()) {
    rev.parent_id = revisions_.back().revision_id;
  }

  revisions_.push_back(rev);
  revisions_by_id_[rev.revision_id] = rev;

  return rev;
}

std::optional<Revision> VersionControl::get_revision(int version) const {
  if (version >= 0 && version < (int)revisions_.size()) {
    return revisions_[version];
  }
  return std::nullopt;
}

std::optional<Revision> VersionControl::get_revision_by_id(const std::string& revision_id) const {
  auto it = revisions_by_id_.find(revision_id);
  if (it != revisions_by_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Revision> VersionControl::get_history(int limit) const {
  std::vector<Revision> result;
  size_t start = (limit <= 0 || revisions_.size() <= (size_t)limit) ? 0 : revisions_.size() - limit;

  for (size_t i = start; i < revisions_.size(); ++i) {
    result.push_back(revisions_[i]);
  }
  return result;
}

std::vector<Revision> VersionControl::get_author_revisions(const std::string& author_id) const {
  std::vector<Revision> result;
  for (const auto& rev : revisions_) {
    if (rev.author_id == author_id) {
      result.push_back(rev);
    }
  }
  return result;
}

std::string VersionControl::get_diff(int version_a, int version_b) const {
  auto rev_a = get_revision(version_a);
  auto rev_b = get_revision(version_b);

  if (!rev_a || !rev_b) return "";

  std::string diff = "--- Version " + std::to_string(version_a) + "\n";
  diff += "+++ Version " + std::to_string(version_b) + "\n";

  return diff;
}

bool VersionControl::revert_to_version(int version, const std::string& author_id) {
  auto rev = get_revision(version);
  if (!rev) return false;

  create_revision(rev->content, author_id, "Revert to version " + std::to_string(version));
  return true;
}

bool VersionControl::create_tag(const std::string& tag_name, int version, const std::string& message) {
  auto rev = get_revision(version);
  if (!rev) return false;

  VersionTag tag;
  tag.tag_name = tag_name;
  tag.version = version;
  tag.revision_id = rev->revision_id;
  tag.message = message;

  tags_[tag_name] = tag;
  return true;
}

std::optional<VersionTag> VersionControl::get_tag(const std::string& tag_name) const {
  auto it = tags_.find(tag_name);
  if (it != tags_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<VersionTag> VersionControl::get_all_tags() const {
  std::vector<VersionTag> result;
  for (const auto& pair : tags_) {
    result.push_back(pair.second);
  }
  return result;
}

bool VersionControl::create_branch(const std::string& branch_name, int from_version) {
  auto rev = get_revision(from_version);
  if (!rev) return false;

  std::vector<Revision> branch;
  for (int i = from_version; i < (int)revisions_.size(); ++i) {
    branch.push_back(revisions_[i]);
  }

  branches_[branch_name] = branch;
  return true;
}

std::optional<std::vector<Revision>> VersionControl::get_branch(const std::string& branch_name) const {
  auto it = branches_.find(branch_name);
  if (it != branches_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<std::string> VersionControl::get_all_branches() const {
  std::vector<std::string> result;
  for (const auto& pair : branches_) {
    result.push_back(pair.first);
  }
  return result;
}

int VersionControl::get_current_version() const {
  return current_version_;
}

int VersionControl::get_revision_count() const {
  return revisions_.size();
}

void VersionControl::clear_history() {
  revisions_.clear();
  revisions_by_id_.clear();
  tags_.clear();
  branches_.clear();
  current_version_ = 0;
}

std::string VersionControl::generate_revision_id() const {
  uuid_t uuid;
  uuid_generate(uuid);

  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "rev_" + ss.str();
}

} // namespace bendcore::collaboration
