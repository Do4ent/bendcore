#include <gtest/gtest.h>
#include "bendcore/collaboration/version_control.hpp"

using namespace bendcore::collaboration;

class VersionControlTest : public ::testing::Test {
 protected:
  VersionControl vc;
  void SetUp() override { vc.clear_history(); }
};

TEST_F(VersionControlTest, CreateRevision) {
  auto rev = vc.create_revision("content", "user1", "Initial commit");
  EXPECT_EQ(rev.version, 0);
  EXPECT_EQ(rev.author_id, "user1");
  EXPECT_EQ(rev.content, "content");
}

TEST_F(VersionControlTest, GetRevision) {
  vc.create_revision("v1", "user1", "msg1");
  auto rev = vc.get_revision(0);
  EXPECT_TRUE(rev.has_value());
  EXPECT_EQ(rev->content, "v1");
}

TEST_F(VersionControlTest, GetNonExistentRevision) {
  auto rev = vc.get_revision(99);
  EXPECT_FALSE(rev.has_value());
}

TEST_F(VersionControlTest, GetRevisionById) {
  auto created = vc.create_revision("content", "user1", "msg");
  auto rev = vc.get_revision_by_id(created.revision_id);
  EXPECT_TRUE(rev.has_value());
}

TEST_F(VersionControlTest, GetHistory) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");
  auto history = vc.get_history();
  EXPECT_EQ(history.size(), 2);
}

TEST_F(VersionControlTest, GetHistoryWithLimit) {
  for (int i = 0; i < 10; ++i) {
    vc.create_revision("v" + std::to_string(i), "user1", "msg");
  }
  auto history = vc.get_history(5);
  EXPECT_EQ(history.size(), 5);
}

TEST_F(VersionControlTest, GetAuthorRevisions) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user2", "msg2");
  vc.create_revision("v3", "user1", "msg3");

  auto revs = vc.get_author_revisions("user1");
  EXPECT_EQ(revs.size(), 2);
}

TEST_F(VersionControlTest, GetDiff) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");

  std::string diff = vc.get_diff(0, 1);
  EXPECT_NE(diff, "");
}

TEST_F(VersionControlTest, RevertToVersion) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");

  bool reverted = vc.revert_to_version(0, "user2");
  EXPECT_TRUE(reverted);
  EXPECT_EQ(vc.get_current_version(), 3);
}

TEST_F(VersionControlTest, CreateTag) {
  vc.create_revision("v1", "user1", "msg1");
  bool created = vc.create_tag("v1.0", 0, "Release v1.0");
  EXPECT_TRUE(created);
}

TEST_F(VersionControlTest, GetTag) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_tag("v1.0", 0, "Release");

  auto tag = vc.get_tag("v1.0");
  EXPECT_TRUE(tag.has_value());
  EXPECT_EQ(tag->tag_name, "v1.0");
}

TEST_F(VersionControlTest, GetAllTags) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");

  vc.create_tag("v1.0", 0, "Release");
  vc.create_tag("v1.1", 1, "Patch");

  auto tags = vc.get_all_tags();
  EXPECT_EQ(tags.size(), 2);
}

TEST_F(VersionControlTest, CreateBranch) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");

  bool created = vc.create_branch("feature", 0);
  EXPECT_TRUE(created);
}

TEST_F(VersionControlTest, GetBranch) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_branch("feature", 0);

  auto branch = vc.get_branch("feature");
  EXPECT_TRUE(branch.has_value());
}

TEST_F(VersionControlTest, GetAllBranches) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_branch("feature1", 0);
  vc.create_branch("feature2", 0);

  auto branches = vc.get_all_branches();
  EXPECT_EQ(branches.size(), 2);
}

TEST_F(VersionControlTest, GetCurrentVersion) {
  EXPECT_EQ(vc.get_current_version(), 0);
  vc.create_revision("v1", "user1", "msg1");
  EXPECT_EQ(vc.get_current_version(), 1);
}

TEST_F(VersionControlTest, GetRevisionCount) {
  EXPECT_EQ(vc.get_revision_count(), 0);
  vc.create_revision("v1", "user1", "msg1");
  EXPECT_EQ(vc.get_revision_count(), 1);
}

TEST_F(VersionControlTest, ParentRevision) {
  auto r1 = vc.create_revision("v1", "user1", "msg1");
  auto r2 = vc.create_revision("v2", "user1", "msg2");

  EXPECT_EQ(r2.parent_id, r1.revision_id);
}

TEST_F(VersionControlTest, MultipleRevisions) {
  for (int i = 0; i < 5; ++i) {
    vc.create_revision("v" + std::to_string(i), "user1", "msg");
  }
  EXPECT_EQ(vc.get_revision_count(), 5);
}

TEST_F(VersionControlTest, RevisionTimestamps) {
  auto rev = vc.create_revision("content", "user1", "msg");
  EXPECT_LE(rev.created_at, std::chrono::system_clock::now());
}

TEST_F(VersionControlTest, UniqueRevisionIds) {
  auto r1 = vc.create_revision("v1", "user1", "msg1");
  auto r2 = vc.create_revision("v2", "user1", "msg2");

  EXPECT_NE(r1.revision_id, r2.revision_id);
}

TEST_F(VersionControlTest, TagWithInvalidVersion) {
  bool created = vc.create_tag("v1.0", 99, "msg");
  EXPECT_FALSE(created);
}

TEST_F(VersionControlTest, BranchWithInvalidVersion) {
  bool created = vc.create_branch("feature", 99);
  EXPECT_FALSE(created);
}

TEST_F(VersionControlTest, ClearHistory) {
  vc.create_revision("v1", "user1", "msg1");
  vc.clear_history();
  EXPECT_EQ(vc.get_revision_count(), 0);
}

TEST_F(VersionControlTest, NonExistentTag) {
  auto tag = vc.get_tag("nonexistent");
  EXPECT_FALSE(tag.has_value());
}

TEST_F(VersionControlTest, NonExistentBranch) {
  auto branch = vc.get_branch("nonexistent");
  EXPECT_FALSE(branch.has_value());
}

TEST_F(VersionControlTest, RevertNonExistent) {
  bool reverted = vc.revert_to_version(99, "user1");
  EXPECT_FALSE(reverted);
}

TEST_F(VersionControlTest, RevisionMessages) {
  auto rev = vc.create_revision("content", "user1", "Important change");
  EXPECT_EQ(rev.message, "Important change");
}

TEST_F(VersionControlTest, HistoryOrder) {
  vc.create_revision("v1", "user1", "msg1");
  vc.create_revision("v2", "user1", "msg2");
  vc.create_revision("v3", "user1", "msg3");

  auto history = vc.get_history();
  EXPECT_EQ(history[0].version, 0);
  EXPECT_EQ(history[2].version, 2);
}

TEST_F(VersionControlTest, AuthorTracking) {
  vc.create_revision("v1", "alice", "msg1");
  vc.create_revision("v2", "bob", "msg2");

  auto alice_revs = vc.get_author_revisions("alice");
  auto bob_revs = vc.get_author_revisions("bob");

  EXPECT_EQ(alice_revs.size(), 1);
  EXPECT_EQ(bob_revs.size(), 1);
}

TEST_F(VersionControlTest, VersionSequence) {
  for (int i = 0; i < 10; ++i) {
    auto rev = vc.create_revision("v" + std::to_string(i), "user1", "msg");
    EXPECT_EQ(rev.version, i);
  }
}
