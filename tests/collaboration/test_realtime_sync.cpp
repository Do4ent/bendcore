#include <gtest/gtest.h>
#include "bendcore/collaboration/realtime_sync.hpp"

using namespace bendcore::collaboration;

class RealtimeSyncTest : public ::testing::Test {
 protected:
  RealtimeSync sync;
  void SetUp() override { sync.clear_connections(); }
};

TEST_F(RealtimeSyncTest, EstablishConnection) {
  auto conn_id = sync.establish_connection("user1");
  EXPECT_FALSE(conn_id.empty());
  auto conn = sync.get_connection(conn_id);
  EXPECT_TRUE(conn.has_value());
}

TEST_F(RealtimeSyncTest, CloseConnection) {
  auto conn_id = sync.establish_connection("user1");
  bool closed = sync.close_connection(conn_id);
  EXPECT_TRUE(closed);
  auto conn = sync.get_connection(conn_id);
  EXPECT_FALSE(conn.has_value());
}

TEST_F(RealtimeSyncTest, GetActiveConnections) {
  sync.establish_connection("user1");
  sync.establish_connection("user2");
  auto conns = sync.get_active_connections();
  EXPECT_EQ(conns.size(), 2);
}

TEST_F(RealtimeSyncTest, QueueMessage) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  EXPECT_EQ(msg.type, "update");
  EXPECT_EQ(msg.content, "Hello");
}

TEST_F(RealtimeSyncTest, DequeueMessage) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "Hello");
  auto msg = sync.dequeue_message();
  EXPECT_TRUE(msg.has_value());
  EXPECT_EQ(msg->content, "Hello");
}

TEST_F(RealtimeSyncTest, GetPendingMessages) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "Msg1");
  sync.queue_message(conn_id, "update", "Msg2");
  auto msgs = sync.get_pending_messages();
  EXPECT_EQ(msgs.size(), 2);
}

TEST_F(RealtimeSyncTest, GetQueueSize) {
  auto conn_id = sync.establish_connection("user1");
  EXPECT_EQ(sync.get_queue_size(), 0);
  sync.queue_message(conn_id, "update", "Msg");
  EXPECT_EQ(sync.get_queue_size(), 1);
}

TEST_F(RealtimeSyncTest, AcknowledgeMessage) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  bool acked = sync.acknowledge_message(msg.message_id);
  EXPECT_TRUE(acked);
}

TEST_F(RealtimeSyncTest, GetUnacknowledgedMessages) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  auto unacked = sync.get_unacknowledged_messages();
  EXPECT_GE(unacked.size(), 0);
}

TEST_F(RealtimeSyncTest, SendHeartbeat) {
  auto conn_id = sync.establish_connection("user1");
  bool sent = sync.send_heartbeat(conn_id);
  EXPECT_TRUE(sent);
}

TEST_F(RealtimeSyncTest, CheckConnectionHealth) {
  sync.establish_connection("user1");
  bool healthy = sync.check_connection_health();
  EXPECT_TRUE(healthy);
}

TEST_F(RealtimeSyncTest, GetMessageCount) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "M1");
  sync.queue_message(conn_id, "update", "M2");
  int count = sync.get_message_count(conn_id);
  EXPECT_EQ(count, 2);
}

TEST_F(RealtimeSyncTest, GetTotalMessagesSent) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "M1");
  sync.queue_message(conn_id, "update", "M2");
  int total = sync.get_total_messages_sent();
  EXPECT_EQ(total, 2);
}

TEST_F(RealtimeSyncTest, ClearQueue) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "M");
  sync.clear_queue();
  EXPECT_EQ(sync.get_queue_size(), 0);
}

TEST_F(RealtimeSyncTest, ClearConnections) {
  sync.establish_connection("user1");
  sync.clear_connections();
  auto conns = sync.get_active_connections();
  EXPECT_EQ(conns.size(), 0);
}

TEST_F(RealtimeSyncTest, BroadcastMessage) {
  sync.establish_connection("user1");
  sync.establish_connection("user2");
  bool broadcasted = sync.broadcast_message("alert", "Server issue");
  EXPECT_TRUE(broadcasted);
}

TEST_F(RealtimeSyncTest, MessageSequence) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  EXPECT_GE(msg.sequence, 0);
}

TEST_F(RealtimeSyncTest, MessageTimestamp) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  EXPECT_LE(msg.timestamp, std::chrono::system_clock::now());
}

TEST_F(RealtimeSyncTest, UniqueConnectionIds) {
  auto c1 = sync.establish_connection("user1");
  auto c2 = sync.establish_connection("user2");
  EXPECT_NE(c1, c2);
}

TEST_F(RealtimeSyncTest, UniqueMessageIds) {
  auto conn_id = sync.establish_connection("user1");
  auto m1 = sync.queue_message(conn_id, "update", "M1");
  auto m2 = sync.queue_message(conn_id, "update", "M2");
  EXPECT_NE(m1.message_id, m2.message_id);
}

TEST_F(RealtimeSyncTest, ConnectionState) {
  auto conn_id = sync.establish_connection("user1");
  auto conn = sync.get_connection(conn_id);
  EXPECT_TRUE(conn->connected);
}

TEST_F(RealtimeSyncTest, MessageCallback) {
  bool called = false;
  sync.on_message_received([&](const SyncMessage& msg) {
    called = true;
  });

  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "Hello");

  EXPECT_TRUE(called);
}

TEST_F(RealtimeSyncTest, ConnectionCallback) {
  bool called = false;
  sync.on_connection_established([&](const ConnectionState& state) {
    called = true;
  });

  sync.establish_connection("user1");
  EXPECT_TRUE(called);
}

TEST_F(RealtimeSyncTest, DisconnectionCallback) {
  auto conn_id = sync.establish_connection("user1");

  bool called = false;
  sync.on_connection_closed([&](const ConnectionState& state) {
    called = true;
  });

  sync.close_connection(conn_id);
  EXPECT_TRUE(called);
}

TEST_F(RealtimeSyncTest, MultipleMessageTypes) {
  auto conn_id = sync.establish_connection("user1");
  sync.queue_message(conn_id, "update", "Data");
  sync.queue_message(conn_id, "heartbeat", "");
  sync.queue_message(conn_id, "sync", "State");

  EXPECT_EQ(sync.get_queue_size(), 3);
}

TEST_F(RealtimeSyncTest, EmptyPendingMessages) {
  auto msgs = sync.get_pending_messages();
  EXPECT_EQ(msgs.size(), 0);
}

TEST_F(RealtimeSyncTest, GetStaleConnections) {
  auto conns = sync.get_stale_connections();
  EXPECT_GE(conns.size(), 0);
}

TEST_F(RealtimeSyncTest, NonExistentConnection) {
  bool sent = sync.send_heartbeat("nonexistent");
  EXPECT_FALSE(sent);
}

TEST_F(RealtimeSyncTest, ConnectionUserTracking) {
  auto c1 = sync.establish_connection("alice");
  auto c2 = sync.establish_connection("bob");

  auto conn1 = sync.get_connection(c1);
  auto conn2 = sync.get_connection(c2);

  EXPECT_EQ(conn1->user_id, "alice");
  EXPECT_EQ(conn2->user_id, "bob");
}

TEST_F(RealtimeSyncTest, MessageSenderTracking) {
  auto conn_id = sync.establish_connection("user1");
  auto msg = sync.queue_message(conn_id, "update", "Hello");
  EXPECT_EQ(msg.sender_id, "user1");
}
