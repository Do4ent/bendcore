#include <gtest/gtest.h>
#include "bendcore/collaboration/comment_system.hpp"

using namespace bendcore::collaboration;

class CommentSystemTest : public ::testing::Test {
 protected:
  CommentSystem cs;
  void SetUp() override { cs.clear_comments(); }
};

TEST_F(CommentSystemTest, AddComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1", 10);
  EXPECT_EQ(c.author_id, "user1");
  EXPECT_EQ(c.content, "Hello");
  EXPECT_EQ(c.target_id, "doc1");
  EXPECT_EQ(c.line_number, 10);
}

TEST_F(CommentSystemTest, GetComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->content, "Hello");
}

TEST_F(CommentSystemTest, GetTargetComments) {
  cs.add_comment("C1", "user1", "doc1");
  cs.add_comment("C2", "user2", "doc1");
  cs.add_comment("C3", "user1", "doc2");

  auto comments = cs.get_target_comments("doc1");
  EXPECT_EQ(comments.size(), 2);
}

TEST_F(CommentSystemTest, GetAuthorComments) {
  cs.add_comment("C1", "user1", "doc1");
  cs.add_comment("C2", "user2", "doc1");
  cs.add_comment("C3", "user1", "doc2");

  auto comments = cs.get_author_comments("user1");
  EXPECT_EQ(comments.size(), 2);
}

TEST_F(CommentSystemTest, ReplyToComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  auto reply = cs.reply_to_comment(c.comment_id, "Reply", "user2");

  EXPECT_EQ(reply.parent_id, c.comment_id);
  EXPECT_EQ(reply.content, "Reply");
}

TEST_F(CommentSystemTest, GetCommentReplies) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  cs.reply_to_comment(c.comment_id, "Reply1", "user2");
  cs.reply_to_comment(c.comment_id, "Reply2", "user2");

  auto replies = cs.get_comment_replies(c.comment_id);
  EXPECT_EQ(replies.size(), 2);
}

TEST_F(CommentSystemTest, ResolveComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  bool resolved = cs.resolve_comment(c.comment_id, "Fixed");

  EXPECT_TRUE(resolved);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_TRUE(retrieved->resolved);
}

TEST_F(CommentSystemTest, UnresolveComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  cs.resolve_comment(c.comment_id, "Fixed");
  bool unresolved = cs.unresolve_comment(c.comment_id);

  EXPECT_TRUE(unresolved);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_FALSE(retrieved->resolved);
}

TEST_F(CommentSystemTest, GetResolvedComments) {
  auto c1 = cs.add_comment("C1", "user1", "doc1");
  auto c2 = cs.add_comment("C2", "user1", "doc1");

  cs.resolve_comment(c1.comment_id, "Fixed");

  auto resolved = cs.get_resolved_comments();
  EXPECT_EQ(resolved.size(), 1);
}

TEST_F(CommentSystemTest, GetUnresolvedComments) {
  auto c1 = cs.add_comment("C1", "user1", "doc1");
  auto c2 = cs.add_comment("C2", "user1", "doc1");

  cs.resolve_comment(c1.comment_id, "Fixed");

  auto unresolved = cs.get_unresolved_comments();
  EXPECT_EQ(unresolved.size(), 1);
}

TEST_F(CommentSystemTest, EditComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  bool edited = cs.edit_comment(c.comment_id, "Modified");

  EXPECT_TRUE(edited);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_EQ(retrieved->content, "Modified");
}

TEST_F(CommentSystemTest, DeleteComment) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  bool deleted = cs.delete_comment(c.comment_id);

  EXPECT_TRUE(deleted);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_FALSE(retrieved.has_value());
}

TEST_F(CommentSystemTest, CreateThread) {
  auto thread = cs.create_thread("doc1");
  EXPECT_EQ(thread.target_id, "doc1");
  EXPECT_FALSE(thread.thread_id.empty());
}

TEST_F(CommentSystemTest, GetThread) {
  auto thread = cs.create_thread("doc1");
  auto retrieved = cs.get_thread(thread.thread_id);
  EXPECT_TRUE(retrieved.has_value());
}

TEST_F(CommentSystemTest, GetTargetThreads) {
  cs.create_thread("doc1");
  cs.create_thread("doc1");
  cs.create_thread("doc2");

  auto threads = cs.get_target_threads("doc1");
  EXPECT_EQ(threads.size(), 2);
}

TEST_F(CommentSystemTest, AddTag) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  bool added = cs.add_tag(c.comment_id, "bug");

  EXPECT_TRUE(added);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_EQ(retrieved->tags.size(), 1);
}

TEST_F(CommentSystemTest, RemoveTag) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  cs.add_tag(c.comment_id, "bug");
  bool removed = cs.remove_tag(c.comment_id, "bug");

  EXPECT_TRUE(removed);
  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_EQ(retrieved->tags.size(), 0);
}

TEST_F(CommentSystemTest, GetCommentsWithTag) {
  auto c1 = cs.add_comment("C1", "user1", "doc1");
  auto c2 = cs.add_comment("C2", "user1", "doc1");

  cs.add_tag(c1.comment_id, "bug");
  cs.add_tag(c2.comment_id, "feature");

  auto bugs = cs.get_comments_with_tag("bug");
  EXPECT_EQ(bugs.size(), 1);
}

TEST_F(CommentSystemTest, GetTotalComments) {
  EXPECT_EQ(cs.get_total_comments(), 0);
  cs.add_comment("C1", "user1", "doc1");
  cs.add_comment("C2", "user1", "doc1");
  EXPECT_EQ(cs.get_total_comments(), 2);
}

TEST_F(CommentSystemTest, GetUnresolvedCount) {
  auto c1 = cs.add_comment("C1", "user1", "doc1");
  auto c2 = cs.add_comment("C2", "user1", "doc1");

  cs.resolve_comment(c1.comment_id, "Fixed");

  EXPECT_EQ(cs.get_unresolved_count(), 1);
}

TEST_F(CommentSystemTest, CommentTimestamps) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  EXPECT_LE(c.created_at, c.updated_at);
}

TEST_F(CommentSystemTest, UniqueCommentIds) {
  auto c1 = cs.add_comment("C1", "user1", "doc1");
  auto c2 = cs.add_comment("C2", "user1", "doc1");

  EXPECT_NE(c1.comment_id, c2.comment_id);
}

TEST_F(CommentSystemTest, ClearComments) {
  cs.add_comment("C1", "user1", "doc1");
  cs.clear_comments();
  EXPECT_EQ(cs.get_total_comments(), 0);
}

TEST_F(CommentSystemTest, MultipleReplies) {
  auto c = cs.add_comment("Hello", "user1", "doc1");
  for (int i = 0; i < 5; ++i) {
    cs.reply_to_comment(c.comment_id, "Reply" + std::to_string(i), "user2");
  }

  auto replies = cs.get_comment_replies(c.comment_id);
  EXPECT_EQ(replies.size(), 5);
}

TEST_F(CommentSystemTest, CommentLineNumbers) {
  auto c = cs.add_comment("Comment at line 42", "user1", "doc1", 42);
  EXPECT_EQ(c.line_number, 42);
}

TEST_F(CommentSystemTest, ResolutionNotes) {
  auto c = cs.add_comment("Issue", "user1", "doc1");
  cs.resolve_comment(c.comment_id, "Fixed in v1.2");

  auto retrieved = cs.get_comment(c.comment_id);
  EXPECT_EQ(retrieved->resolution_notes, "Fixed in v1.2");
}
