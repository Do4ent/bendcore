#ifndef BENDCORE_COLLABORATION_REALTIME_SYNC_HPP
#define BENDCORE_COLLABORATION_REALTIME_SYNC_HPP

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <chrono>
#include <optional>
#include <functional>

namespace bendcore::collaboration {

struct SyncMessage {
  std::string message_id;
  std::string type;  // "update", "heartbeat", "sync", "ack"
  std::string sender_id;
  std::string content;
  int sequence = 0;
  std::chrono::system_clock::time_point timestamp;
  bool acknowledged = false;
};

struct ConnectionState {
  std::string connection_id;
  std::string user_id;
  bool connected = false;
  std::chrono::system_clock::time_point last_heartbeat;
  int message_count = 0;
};

using SyncCallback = std::function<void(const SyncMessage&)>;
using ConnectionCallback = std::function<void(const ConnectionState&)>;

class RealtimeSync {
 public:
  RealtimeSync();
  ~RealtimeSync();

  std::string establish_connection(const std::string& user_id);
  bool close_connection(const std::string& connection_id);
  std::optional<ConnectionState> get_connection(const std::string& connection_id) const;
  std::vector<ConnectionState> get_active_connections() const;

  SyncMessage queue_message(const std::string& connection_id, const std::string& type, const std::string& content);
  std::optional<SyncMessage> dequeue_message();
  std::vector<SyncMessage> get_pending_messages() const;
  int get_queue_size() const;

  bool acknowledge_message(const std::string& message_id);
  std::vector<SyncMessage> get_unacknowledged_messages() const;

  bool send_heartbeat(const std::string& connection_id);
  bool check_connection_health();
  std::vector<std::string> get_stale_connections() const;

  void on_message_received(SyncCallback callback);
  void on_connection_established(ConnectionCallback callback);
  void on_connection_closed(ConnectionCallback callback);

  bool broadcast_message(const std::string& type, const std::string& content);
  bool send_to_connection(const std::string& connection_id, const std::string& type, const std::string& content);

  int get_message_count(const std::string& connection_id) const;
  int get_total_messages_sent() const;

  void clear_queue();
  void clear_connections();

 private:
  std::map<std::string, ConnectionState> connections_;
  std::queue<SyncMessage> message_queue_;
  std::vector<SyncMessage> sent_messages_;

  int sequence_number_ = 0;
  int total_sent_ = 0;
  std::chrono::seconds heartbeat_interval_{30};
  std::chrono::seconds connection_timeout_{120};

  SyncCallback on_message_;
  ConnectionCallback on_connect_;
  ConnectionCallback on_disconnect_;

  std::string generate_connection_id() const;
  std::string generate_message_id() const;
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_REALTIME_SYNC_HPP
