#ifndef BENDCORE_COLLABORATION_COMMENT_SYSTEM_HPP
#define BENDCORE_COLLABORATION_COMMENT_SYSTEM_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>
#include <memory>

namespace bendcore::collaboration {

struct Comment {
  std::string comment_id;
  std::string parent_id;
  std::string author_id;
  std::string content;
  std::string target_id;
  int line_number = 0;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
  bool resolved = false;
  std::string resolution_notes;
  std::vector<std::string> tags;
};

struct CommentThread {
  std::string thread_id;
  std::string target_id;
  std::vector<Comment> comments;
  bool resolved = false;
  std::chrono::system_clock::time_point created_at;
};

class CommentSystem {
 public:
  CommentSystem();
  ~CommentSystem();

  Comment add_comment(const std::string& content, const std::string& author_id, const std::string& target_id, int line = 0);
  std::optional<Comment> get_comment(const std::string& comment_id) const;
  std::vector<Comment> get_target_comments(const std::string& target_id) const;
  std::vector<Comment> get_author_comments(const std::string& author_id) const;

  Comment reply_to_comment(const std::string& parent_id, const std::string& content, const std::string& author_id);
  std::vector<Comment> get_comment_replies(const std::string& comment_id) const;

  bool resolve_comment(const std::string& comment_id, const std::string& resolution_notes);
  bool unresolve_comment(const std::string& comment_id);
  std::vector<Comment> get_resolved_comments() const;
  std::vector<Comment> get_unresolved_comments() const;

  bool edit_comment(const std::string& comment_id, const std::string& new_content);
  bool delete_comment(const std::string& comment_id);

  CommentThread create_thread(const std::string& target_id);
  std::optional<CommentThread> get_thread(const std::string& thread_id) const;
  std::vector<CommentThread> get_target_threads(const std::string& target_id) const;

  bool add_tag(const std::string& comment_id, const std::string& tag);
  bool remove_tag(const std::string& comment_id, const std::string& tag);
  std::vector<Comment> get_comments_with_tag(const std::string& tag) const;

  int get_total_comments() const;
  int get_unresolved_count() const;

  void clear_comments();

 private:
  std::vector<Comment> comments_;
  std::map<std::string, Comment> comments_by_id_;
  std::map<std::string, std::vector<std::string>> replies_;
  std::vector<CommentThread> threads_;

  std::string generate_comment_id() const;
  std::string generate_thread_id() const;
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_COMMENT_SYSTEM_HPP
