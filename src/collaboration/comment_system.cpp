#include "bendcore/collaboration/comment_system.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::collaboration {

CommentSystem::CommentSystem() = default;
CommentSystem::~CommentSystem() = default;

Comment CommentSystem::add_comment(const std::string& content, const std::string& author_id, const std::string& target_id, int line) {
  Comment comment;
  comment.comment_id = generate_comment_id();
  comment.author_id = author_id;
  comment.content = content;
  comment.target_id = target_id;
  comment.line_number = line;
  comment.created_at = std::chrono::system_clock::now();
  comment.updated_at = comment.created_at;
  comment.resolved = false;

  comments_.push_back(comment);
  comments_by_id_[comment.comment_id] = comment;

  return comment;
}

std::optional<Comment> CommentSystem::get_comment(const std::string& comment_id) const {
  auto it = comments_by_id_.find(comment_id);
  if (it != comments_by_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Comment> CommentSystem::get_target_comments(const std::string& target_id) const {
  std::vector<Comment> result;
  for (const auto& comment : comments_) {
    if (comment.target_id == target_id && comment.parent_id.empty()) {
      result.push_back(comment);
    }
  }
  return result;
}

std::vector<Comment> CommentSystem::get_author_comments(const std::string& author_id) const {
  std::vector<Comment> result;
  for (const auto& comment : comments_) {
    if (comment.author_id == author_id) {
      result.push_back(comment);
    }
  }
  return result;
}

Comment CommentSystem::reply_to_comment(const std::string& parent_id, const std::string& content, const std::string& author_id) {
  auto parent = get_comment(parent_id);

  Comment reply;
  reply.comment_id = generate_comment_id();
  reply.parent_id = parent_id;
  reply.author_id = author_id;
  reply.content = content;
  reply.target_id = parent ? parent->target_id : "";
  reply.created_at = std::chrono::system_clock::now();
  reply.updated_at = reply.created_at;

  comments_.push_back(reply);
  comments_by_id_[reply.comment_id] = reply;
  replies_[parent_id].push_back(reply.comment_id);

  return reply;
}

std::vector<Comment> CommentSystem::get_comment_replies(const std::string& comment_id) const {
  std::vector<Comment> result;
  auto it = replies_.find(comment_id);
  if (it != replies_.end()) {
    for (const auto& reply_id : it->second) {
      auto reply = get_comment(reply_id);
      if (reply) {
        result.push_back(*reply);
      }
    }
  }
  return result;
}

bool CommentSystem::resolve_comment(const std::string& comment_id, const std::string& resolution_notes) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  it->second.resolved = true;
  it->second.resolution_notes = resolution_notes;
  it->second.updated_at = std::chrono::system_clock::now();

  for (auto& comment : comments_) {
    if (comment.comment_id == comment_id) {
      comment = it->second;
      break;
    }
  }

  return true;
}

bool CommentSystem::unresolve_comment(const std::string& comment_id) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  it->second.resolved = false;
  it->second.resolution_notes = "";
  it->second.updated_at = std::chrono::system_clock::now();

  for (auto& comment : comments_) {
    if (comment.comment_id == comment_id) {
      comment = it->second;
      break;
    }
  }

  return true;
}

std::vector<Comment> CommentSystem::get_resolved_comments() const {
  std::vector<Comment> result;
  for (const auto& comment : comments_) {
    if (comment.resolved) {
      result.push_back(comment);
    }
  }
  return result;
}

std::vector<Comment> CommentSystem::get_unresolved_comments() const {
  std::vector<Comment> result;
  for (const auto& comment : comments_) {
    if (!comment.resolved) {
      result.push_back(comment);
    }
  }
  return result;
}

bool CommentSystem::edit_comment(const std::string& comment_id, const std::string& new_content) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  it->second.content = new_content;
  it->second.updated_at = std::chrono::system_clock::now();

  for (auto& comment : comments_) {
    if (comment.comment_id == comment_id) {
      comment = it->second;
      break;
    }
  }

  return true;
}

bool CommentSystem::delete_comment(const std::string& comment_id) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  comments_by_id_.erase(it);

  comments_.erase(
      std::remove_if(comments_.begin(), comments_.end(),
                     [&comment_id](const Comment& c) { return c.comment_id == comment_id; }),
      comments_.end());

  return true;
}

CommentThread CommentSystem::create_thread(const std::string& target_id) {
  CommentThread thread;
  thread.thread_id = generate_thread_id();
  thread.target_id = target_id;
  thread.created_at = std::chrono::system_clock::now();

  threads_.push_back(thread);
  return thread;
}

std::optional<CommentThread> CommentSystem::get_thread(const std::string& thread_id) const {
  for (const auto& thread : threads_) {
    if (thread.thread_id == thread_id) {
      return thread;
    }
  }
  return std::nullopt;
}

std::vector<CommentThread> CommentSystem::get_target_threads(const std::string& target_id) const {
  std::vector<CommentThread> result;
  for (const auto& thread : threads_) {
    if (thread.target_id == target_id) {
      result.push_back(thread);
    }
  }
  return result;
}

bool CommentSystem::add_tag(const std::string& comment_id, const std::string& tag) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  it->second.tags.push_back(tag);

  for (auto& comment : comments_) {
    if (comment.comment_id == comment_id) {
      comment = it->second;
      break;
    }
  }

  return true;
}

bool CommentSystem::remove_tag(const std::string& comment_id, const std::string& tag) {
  auto it = comments_by_id_.find(comment_id);
  if (it == comments_by_id_.end()) {
    return false;
  }

  auto& tags = it->second.tags;
  tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());

  for (auto& comment : comments_) {
    if (comment.comment_id == comment_id) {
      comment = it->second;
      break;
    }
  }

  return true;
}

std::vector<Comment> CommentSystem::get_comments_with_tag(const std::string& tag) const {
  std::vector<Comment> result;
  for (const auto& comment : comments_) {
    for (const auto& t : comment.tags) {
      if (t == tag) {
        result.push_back(comment);
        break;
      }
    }
  }
  return result;
}

int CommentSystem::get_total_comments() const {
  return comments_.size();
}

int CommentSystem::get_unresolved_count() const {
  return get_unresolved_comments().size();
}

void CommentSystem::clear_comments() {
  comments_.clear();
  comments_by_id_.clear();
  replies_.clear();
  threads_.clear();
}

std::string CommentSystem::generate_comment_id() const {
  uuid_t uuid;
  uuid_generate(uuid);
  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "cmt_" + ss.str();
}

std::string CommentSystem::generate_thread_id() const {
  uuid_t uuid;
  uuid_generate(uuid);
  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "thr_" + ss.str();
}

} // namespace bendcore::collaboration
