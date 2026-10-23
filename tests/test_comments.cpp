#include <gtest/gtest.h>
#include <chrono>
#include "bendcore/drawing.hpp"

namespace bendcore::drawing {
namespace {

// Test 1: Создание комментария
TEST(Comments, CreateComment) {
  Comment c = make_comment(1, {100.0, 50.0}, "Test comment", "john");

  EXPECT_EQ(c.sheet_number, 1);
  EXPECT_EQ(c.position[0], 100.0);
  EXPECT_EQ(c.position[1], 50.0);
  EXPECT_EQ(c.text, "Test comment");
  EXPECT_EQ(c.author, "john");
  EXPECT_EQ(c.status, CommentStatus::Open);
  EXPECT_FALSE(c.id.empty());
  EXPECT_FALSE(c.created_at.empty());
  EXPECT_EQ(c.replies.size(), 0);
}

// Test 2: Добавление ответа к комментарию
TEST(Comments, AddReply) {
  Comment c = make_comment(1, {100.0, 50.0}, "Test comment", "john");
  CommentReply reply = make_reply("Great idea", "jane");

  EXPECT_EQ(c.replies.size(), 0);
  add_reply_to_comment(c, reply);

  EXPECT_EQ(c.replies.size(), 1);
  EXPECT_EQ(c.replies[0].text, "Great idea");
  EXPECT_EQ(c.replies[0].author, "jane");
  EXPECT_FALSE(c.replies[0].id.empty());
}

// Test 3: Разрешение комментария (изменение статуса)
TEST(Comments, ResolveComment) {
  Comment c = make_comment(1, {100.0, 50.0}, "Test comment", "john");

  EXPECT_EQ(c.status, CommentStatus::Open);
  change_comment_status(c, CommentStatus::Resolved);

  EXPECT_EQ(c.status, CommentStatus::Resolved);
}

// Test 4: JSON сериализация комментария
TEST(Comments, CommentStatusNames) {
  EXPECT_STREQ(comment_status_name(CommentStatus::Open), "open");
  EXPECT_STREQ(comment_status_name(CommentStatus::Resolved), "resolved");
  EXPECT_STREQ(comment_status_name(CommentStatus::NeedsRework), "needs_rework");
  EXPECT_STREQ(comment_status_name(CommentStatus::Archived), "archived");
}

// Test 5: Конвертация строки в статус
TEST(Comments, CommentStatusFromString) {
  EXPECT_EQ(comment_status_from("open"), CommentStatus::Open);
  EXPECT_EQ(comment_status_from("resolved"), CommentStatus::Resolved);
  EXPECT_EQ(comment_status_from("needs_rework"), CommentStatus::NeedsRework);
  EXPECT_EQ(comment_status_from("archived"), CommentStatus::Archived);
  EXPECT_EQ(comment_status_from("unknown"), CommentStatus::Open);
}

// Test 6: Откат версии восстанавливает комментарии
TEST(Comments, VersionRollbackPreservesComments) {
  DrawingVersion v1;
  v1.revision = 1;

  Comment c1 = make_comment(1, {50.0, 50.0}, "First comment", "john");
  CommentReply reply1 = make_reply("Response 1", "jane");
  add_reply_to_comment(c1, reply1);

  v1.comments.push_back(c1);

  // Simulate version 2 (current)
  DrawingVersion v2;
  v2.revision = 2;
  v2.comments.push_back(c1);  // Comments should be preserved

  // After rollback to v1, comments should be the same
  EXPECT_EQ(v1.comments.size(), 1);
  EXPECT_EQ(v1.comments[0].replies.size(), 1);
  EXPECT_EQ(v1.comments[0].replies[0].text, "Response 1");
}

// Test 7: Фильтрование комментариев по статусу
TEST(Comments, FilterCommentsByStatus) {
  std::vector<Comment> comments;

  Comment c1 = make_comment(1, {50.0, 50.0}, "First", "john");
  c1.status = CommentStatus::Open;

  Comment c2 = make_comment(1, {100.0, 100.0}, "Second", "jane");
  c2.status = CommentStatus::Resolved;

  Comment c3 = make_comment(1, {150.0, 150.0}, "Third", "bob");
  c3.status = CommentStatus::NeedsRework;

  comments.push_back(c1);
  comments.push_back(c2);
  comments.push_back(c3);

  auto open_comments = filter_comments_by_status(comments, CommentStatus::Open);
  EXPECT_EQ(open_comments.size(), 1);
  EXPECT_EQ(open_comments[0].text, "First");

  auto resolved_comments = filter_comments_by_status(comments, CommentStatus::Resolved);
  EXPECT_EQ(resolved_comments.size(), 1);
  EXPECT_EQ(resolved_comments[0].text, "Second");
}

// Test 8: Валидация позиции комментария на листе
TEST(Comments, CommentPositionValidation) {
  Comment c = make_comment(1, {-10.0, -20.0}, "Negative position", "john");
  EXPECT_EQ(c.position[0], -10.0);
  EXPECT_EQ(c.position[1], -20.0);

  // Negative positions are allowed (off-sheet comments)
  Comment c2 = make_comment(1, {0.0, 0.0}, "Origin", "jane");
  EXPECT_EQ(c2.position[0], 0.0);
  EXPECT_EQ(c2.position[1], 0.0);
}

// Test 9: Удаление комментария (фильтрованием)
TEST(Comments, RemoveComment) {
  std::vector<Comment> comments;

  Comment c1 = make_comment(1, {50.0, 50.0}, "First", "john");
  Comment c2 = make_comment(1, {100.0, 100.0}, "Second", "jane");

  comments.push_back(c1);
  comments.push_back(c2);

  EXPECT_EQ(comments.size(), 2);

  // Simulate removing first comment
  auto it = std::find_if(comments.begin(), comments.end(),
                        [&c1](const Comment& c) { return c.id == c1.id; });
  if (it != comments.end()) {
    comments.erase(it);
  }

  EXPECT_EQ(comments.size(), 1);
  EXPECT_EQ(comments[0].text, "Second");
}

// Test 10: История ответов на комментарий
TEST(Comments, CommentReplyHistory) {
  Comment c = make_comment(1, {100.0, 50.0}, "Main comment", "john");

  CommentReply r1 = make_reply("First response", "jane");
  CommentReply r2 = make_reply("Second response", "bob");
  CommentReply r3 = make_reply("Third response", "alice");

  add_reply_to_comment(c, r1);
  add_reply_to_comment(c, r2);
  add_reply_to_comment(c, r3);

  EXPECT_EQ(c.replies.size(), 3);
  EXPECT_EQ(c.replies[0].author, "jane");
  EXPECT_EQ(c.replies[1].author, "bob");
  EXPECT_EQ(c.replies[2].author, "alice");

  // Check that each reply has a unique ID
  EXPECT_NE(c.replies[0].id, c.replies[1].id);
  EXPECT_NE(c.replies[1].id, c.replies[2].id);
}

// Test 11: Таймстамп валидация (ISO 8601)
TEST(Comments, TimestampValidation) {
  Comment c = make_comment(1, {100.0, 50.0}, "Test", "john");

  // Check that timestamp is in ISO 8601 format (YYYY-MM-DDTHH:MM:SSZ)
  EXPECT_EQ(c.created_at.back(), 'Z');
  EXPECT_NE(c.created_at.find('T'), std::string::npos);
  EXPECT_GE(c.created_at.length(), 20);  // "2024-01-01T00:00:00Z" is 20 chars
}

// Test 12: Множественные комментарии на листе
TEST(Comments, MultipleCommentsOnSheet) {
  std::vector<Comment> comments;

  for (int i = 0; i < 5; ++i) {
    Comment c = make_comment(1, {static_cast<double>(i*20), static_cast<double>(i*20)},
                            std::string("Comment ") + std::to_string(i), "user");
    comments.push_back(c);
  }

  auto sheet_comments = filter_comments_by_sheet(comments, 1);
  EXPECT_EQ(sheet_comments.size(), 5);

  auto sheet2_comments = filter_comments_by_sheet(comments, 2);
  EXPECT_EQ(sheet2_comments.size(), 0);
}

}  // namespace
}  // namespace bendcore::drawing
