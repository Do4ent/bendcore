#include <gtest/gtest.h>
#include "bendcore/collaboration/multi_user_manager.hpp"
#include <thread>
#include <chrono>

using namespace bendcore::collaboration;

class MultiUserManagerTest : public ::testing::Test {
 protected:
  MultiUserManager manager;

  void SetUp() override {
    manager.clear_sessions();
    manager.clear_edits();
  }
};

// Test session creation
TEST_F(MultiUserManagerTest, CreateSession) {
  UserSession session = manager.create_session("user1", "User One");

  EXPECT_FALSE(session.user_id.empty());
  EXPECT_FALSE(session.session_id.empty());
  EXPECT_EQ(session.user_id, "user1");
  EXPECT_EQ(session.user_name, "User One");
  EXPECT_TRUE(session.is_active);
}

// Test get session by ID
TEST_F(MultiUserManagerTest, GetSessionById) {
  UserSession created = manager.create_session("user1", "User One");
  auto retrieved = manager.get_session(created.session_id);

  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->session_id, created.session_id);
  EXPECT_EQ(retrieved->user_id, "user1");
}

// Test get non-existent session
TEST_F(MultiUserManagerTest, GetNonExistentSession) {
  auto result = manager.get_session("nonexistent");
  EXPECT_FALSE(result.has_value());
}

// Test get active sessions
TEST_F(MultiUserManagerTest, GetActiveSessions) {
  manager.create_session("user1", "User One");
  manager.create_session("user2", "User Two");
  auto sessions = manager.create_session("user3", "User Three");

  auto active = manager.get_active_sessions();
  EXPECT_EQ(active.size(), 3);
}

// Test get user sessions
TEST_F(MultiUserManagerTest, GetUserSessions) {
  manager.create_session("user1", "User One");
  manager.create_session("user1", "User One");
  manager.create_session("user2", "User Two");

  auto user1_sessions = manager.get_user_sessions("user1");
  EXPECT_EQ(user1_sessions.size(), 2);

  auto user2_sessions = manager.get_user_sessions("user2");
  EXPECT_EQ(user2_sessions.size(), 1);
}

// Test update session activity
TEST_F(MultiUserManagerTest, UpdateSessionActivity) {
  UserSession session = manager.create_session("user1", "User One");
  auto initial_activity = session.last_activity;

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  bool updated = manager.update_session_activity(session.session_id);

  EXPECT_TRUE(updated);
  auto updated_session = manager.get_session(session.session_id);
  EXPECT_GT(updated_session->last_activity, initial_activity);
}

// Test deactivate session
TEST_F(MultiUserManagerTest, DeactivateSession) {
  UserSession session = manager.create_session("user1", "User One");
  bool deactivated = manager.deactivate_session(session.session_id);

  EXPECT_TRUE(deactivated);
  auto retrieved = manager.get_session(session.session_id);
  EXPECT_FALSE(retrieved->is_active);
}

// Test close session
TEST_F(MultiUserManagerTest, CloseSession) {
  UserSession session = manager.create_session("user1", "User One");
  bool closed = manager.close_session(session.session_id);

  EXPECT_TRUE(closed);
  auto retrieved = manager.get_session(session.session_id);
  EXPECT_FALSE(retrieved.has_value());
}

// Test close non-existent session
TEST_F(MultiUserManagerTest, CloseNonExistentSession) {
  bool result = manager.close_session("nonexistent");
  EXPECT_FALSE(result);
}

// Test record edit
TEST_F(MultiUserManagerTest, RecordEdit) {
  ConcurrentEdit edit;
  edit.user_id = "user1";
  edit.session_id = "sess123";
  edit.timestamp = std::chrono::system_clock::now();
  edit.operation_type = "insert";
  edit.target_id = "doc1";
  edit.content = "Hello";
  edit.version = 1;

  manager.record_edit(edit);
  auto edits = manager.get_recent_edits(10);

  EXPECT_EQ(edits.size(), 1);
  EXPECT_EQ(edits[0].user_id, "user1");
  EXPECT_EQ(edits[0].content, "Hello");
}

// Test get recent edits with limit
TEST_F(MultiUserManagerTest, GetRecentEditsWithLimit) {
  for (int i = 0; i < 10; ++i) {
    ConcurrentEdit edit;
    edit.user_id = "user1";
    edit.content = "edit" + std::to_string(i);
    edit.version = i;
    manager.record_edit(edit);
  }

  auto edits = manager.get_recent_edits(5);
  EXPECT_EQ(edits.size(), 5);
  EXPECT_EQ(edits[0].content, "edit5");
}

// Test get user edits
TEST_F(MultiUserManagerTest, GetUserEdits) {
  for (int i = 0; i < 5; ++i) {
    ConcurrentEdit edit;
    edit.user_id = "user1";
    edit.content = "edit" + std::to_string(i);
    manager.record_edit(edit);
  }

  for (int i = 0; i < 3; ++i) {
    ConcurrentEdit edit;
    edit.user_id = "user2";
    edit.content = "edit" + std::to_string(i);
    manager.record_edit(edit);
  }

  auto user1_edits = manager.get_user_edits("user1");
  auto user2_edits = manager.get_user_edits("user2");

  EXPECT_EQ(user1_edits.size(), 5);
  EXPECT_EQ(user2_edits.size(), 3);
}

// Test get target edits
TEST_F(MultiUserManagerTest, GetTargetEdits) {
  for (int i = 0; i < 5; ++i) {
    ConcurrentEdit edit;
    edit.target_id = "doc1";
    edit.version = i;
    manager.record_edit(edit);
  }

  for (int i = 0; i < 3; ++i) {
    ConcurrentEdit edit;
    edit.target_id = "doc2";
    edit.version = i;
    manager.record_edit(edit);
  }

  auto doc1_edits = manager.get_target_edits("doc1");
  auto doc2_edits = manager.get_target_edits("doc2");

  EXPECT_EQ(doc1_edits.size(), 5);
  EXPECT_EQ(doc2_edits.size(), 3);
}

// Test synchronize state
TEST_F(MultiUserManagerTest, SynchronizeState) {
  manager.create_session("user1", "User One");
  manager.create_session("user2", "User Two");

  SessionState state = manager.synchronize_state();

  EXPECT_EQ(state.active_users.size(), 2);
  EXPECT_GT(state.version, -1);
}

// Test update state
TEST_F(MultiUserManagerTest, UpdateState) {
  manager.update_state("key1", "value1");
  manager.update_state("key2", "value2");

  SessionState state = manager.get_state();
  EXPECT_EQ(state.state_data["key1"], "value1");
  EXPECT_EQ(state.state_data["key2"], "value2");
}

// Test state version increment
TEST_F(MultiUserManagerTest, StateVersionIncrement) {
  int v1 = manager.get_state_version();
  manager.update_state("key", "value");
  int v2 = manager.get_state_version();

  EXPECT_GT(v2, v1);
}

// Test get active user count
TEST_F(MultiUserManagerTest, GetActiveUserCount) {
  manager.create_session("user1", "User One");
  manager.create_session("user1", "User One");
  manager.create_session("user2", "User Two");

  EXPECT_EQ(manager.get_active_user_count(), 2);
}

// Test is user active
TEST_F(MultiUserManagerTest, IsUserActive) {
  manager.create_session("user1", "User One");

  EXPECT_TRUE(manager.is_user_active("user1"));
  EXPECT_FALSE(manager.is_user_active("user2"));
}

// Test set and get session metadata
TEST_F(MultiUserManagerTest, SessionMetadata) {
  UserSession session = manager.create_session("user1", "User One");

  bool set = manager.set_session_metadata(session.session_id, "role", "editor");
  EXPECT_TRUE(set);

  auto value = manager.get_session_metadata(session.session_id, "role");
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), "editor");
}

// Test get non-existent metadata
TEST_F(MultiUserManagerTest, GetNonExistentMetadata) {
  UserSession session = manager.create_session("user1", "User One");
  auto value = manager.get_session_metadata(session.session_id, "nonexistent");
  EXPECT_FALSE(value.has_value());
}

// Test clear sessions
TEST_F(MultiUserManagerTest, ClearSessions) {
  manager.create_session("user1", "User One");
  manager.create_session("user2", "User Two");

  manager.clear_sessions();

  auto sessions = manager.get_active_sessions();
  EXPECT_EQ(sessions.size(), 0);
}

// Test clear edits
TEST_F(MultiUserManagerTest, ClearEdits) {
  ConcurrentEdit edit;
  edit.user_id = "user1";
  manager.record_edit(edit);

  manager.clear_edits();

  auto edits = manager.get_recent_edits();
  EXPECT_EQ(edits.size(), 0);
}

// Test session joined callback
TEST_F(MultiUserManagerTest, SessionJoinedCallback) {
  bool callback_called = false;
  manager.on_session_joined([&](const UserSession& session) {
    callback_called = true;
  });

  manager.create_session("user1", "User One");
  EXPECT_TRUE(callback_called);
}

// Test session left callback
TEST_F(MultiUserManagerTest, SessionLeftCallback) {
  UserSession session = manager.create_session("user1", "User One");

  bool callback_called = false;
  manager.on_session_left([&](const UserSession& s) {
    callback_called = true;
  });

  manager.close_session(session.session_id);
  EXPECT_TRUE(callback_called);
}

// Test edit recorded callback
TEST_F(MultiUserManagerTest, EditRecordedCallback) {
  bool callback_called = false;
  manager.on_edit_recorded([&](const ConcurrentEdit& edit) {
    callback_called = true;
  });

  ConcurrentEdit edit;
  edit.user_id = "user1";
  manager.record_edit(edit);

  EXPECT_TRUE(callback_called);
}

// Test state updated callback
TEST_F(MultiUserManagerTest, StateUpdatedCallback) {
  bool callback_called = false;
  manager.on_state_updated([&](const SessionState& state) {
    callback_called = true;
  });

  manager.update_state("key", "value");
  EXPECT_TRUE(callback_called);
}

// Test get concurrent edit count
TEST_F(MultiUserManagerTest, GetConcurrentEditCount) {
  for (int i = 0; i < 3; ++i) {
    ConcurrentEdit edit;
    edit.user_id = "user" + std::to_string(i);
    edit.target_id = "doc1";
    manager.record_edit(edit);
  }

  int count = manager.get_concurrent_edit_count("doc1");
  EXPECT_EQ(count, 3);
}

// Test get max concurrent editors
TEST_F(MultiUserManagerTest, GetMaxConcurrentEditors) {
  int max = manager.get_max_concurrent_editors();
  EXPECT_GT(max, 0);
}

// Test concurrent access to sessions
TEST_F(MultiUserManagerTest, ConcurrentSessionAccess) {
  std::vector<std::thread> threads;

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([this, i]() {
      manager.create_session("user" + std::to_string(i), "User " + std::to_string(i));
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(manager.get_active_user_count(), 10);
}

// Test get target edits with version filter
TEST_F(MultiUserManagerTest, GetTargetEditsWithVersionFilter) {
  for (int i = 0; i < 5; ++i) {
    ConcurrentEdit edit;
    edit.target_id = "doc1";
    edit.version = i;
    manager.record_edit(edit);
  }

  auto edits = manager.get_target_edits("doc1", 2);
  EXPECT_EQ(edits.size(), 3); // versions 2, 3, 4
}

// Test unique session IDs
TEST_F(MultiUserManagerTest, UniqueSessionIds) {
  UserSession s1 = manager.create_session("user1", "User One");
  UserSession s2 = manager.create_session("user1", "User One");

  EXPECT_NE(s1.session_id, s2.session_id);
}

// Test unique connection IDs
TEST_F(MultiUserManagerTest, UniqueConnectionIds) {
  UserSession s1 = manager.create_session("user1", "User One");
  UserSession s2 = manager.create_session("user1", "User One");

  EXPECT_NE(s1.connection_id, s2.connection_id);
}

// Test session timestamps
TEST_F(MultiUserManagerTest, SessionTimestamps) {
  UserSession session = manager.create_session("user1", "User One");

  EXPECT_LE(session.created_at, session.last_activity);
  EXPECT_LE(session.created_at, std::chrono::system_clock::now());
}

// Test state synchronization completeness
TEST_F(MultiUserManagerTest, StateSynchronizationCompleteness) {
  UserSession session = manager.create_session("user1", "User One");
  manager.update_state("config", "value");

  ConcurrentEdit edit;
  edit.user_id = "user1";
  manager.record_edit(edit);

  SessionState state = manager.synchronize_state();

  EXPECT_GT(state.active_users.size(), 0);
  EXPECT_GT(state.state_data.size(), 0);
  EXPECT_GT(state.recent_edits.size(), 0);
}
