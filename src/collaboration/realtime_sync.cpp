#include "bendcore/collaboration/realtime_sync.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::collaboration {

RealtimeSync::RealtimeSync() = default;
RealtimeSync::~RealtimeSync() = default;

std::string RealtimeSync::establish_connection(const std::string& user_id) {
  ConnectionState conn;
  conn.connection_id = generate_connection_id();
  conn.user_id = user_id;
  conn.connected = true;
  conn.last_heartbeat = std::chrono::system_clock::now();
  conn.message_count = 0;

  connections_[conn.connection_id] = conn;

  if (on_connect_) {
    on_connect_(conn);
  }

  return conn.connection_id;
}

bool RealtimeSync::close_connection(const std::string& connection_id) {
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) {
    return false;
  }

  ConnectionState conn = it->second;
  conn.connected = false;

  if (on_disconnect_) {
    on_disconnect_(conn);
  }

  connections_.erase(it);
  return true;
}

std::optional<ConnectionState> RealtimeSync::get_connection(const std::string& connection_id) const {
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<ConnectionState> RealtimeSync::get_active_connections() const {
  std::vector<ConnectionState> result;
  for (const auto& pair : connections_) {
    if (pair.second.connected) {
      result.push_back(pair.second);
    }
  }
  return result;
}

SyncMessage RealtimeSync::queue_message(const std::string& connection_id, const std::string& type, const std::string& content) {
  auto conn_it = connections_.find(connection_id);
  if (conn_it == connections_.end()) {
    return {};
  }

  SyncMessage msg;
  msg.message_id = generate_message_id();
  msg.type = type;
  msg.sender_id = conn_it->second.user_id;
  msg.content = content;
  msg.sequence = sequence_number_++;
  msg.timestamp = std::chrono::system_clock::now();
  msg.acknowledged = false;

  message_queue_.push(msg);
  conn_it->second.message_count++;
  sent_messages_.push_back(msg);
  total_sent_++;

  if (on_message_) {
    on_message_(msg);
  }

  return msg;
}

std::optional<SyncMessage> RealtimeSync::dequeue_message() {
  if (message_queue_.empty()) {
    return std::nullopt;
  }

  SyncMessage msg = message_queue_.front();
  message_queue_.pop();
  return msg;
}

std::vector<SyncMessage> RealtimeSync::get_pending_messages() const {
  std::vector<SyncMessage> result;
  std::queue<SyncMessage> temp = message_queue_;

  while (!temp.empty()) {
    result.push_back(temp.front());
    temp.pop();
  }

  return result;
}

int RealtimeSync::get_queue_size() const {
  return message_queue_.size();
}

bool RealtimeSync::acknowledge_message(const std::string& message_id) {
  for (auto& msg : sent_messages_) {
    if (msg.message_id == message_id) {
      msg.acknowledged = true;
      return true;
    }
  }
  return false;
}

std::vector<SyncMessage> RealtimeSync::get_unacknowledged_messages() const {
  std::vector<SyncMessage> result;
  for (const auto& msg : sent_messages_) {
    if (!msg.acknowledged) {
      result.push_back(msg);
    }
  }
  return result;
}

bool RealtimeSync::send_heartbeat(const std::string& connection_id) {
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) {
    return false;
  }

  it->second.last_heartbeat = std::chrono::system_clock::now();
  return true;
}

bool RealtimeSync::check_connection_health() {
  auto now = std::chrono::system_clock::now();
  std::vector<std::string> stale;

  for (auto& pair : connections_) {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.last_heartbeat);
    if (duration > connection_timeout_) {
      stale.push_back(pair.first);
    }
  }

  for (const auto& conn_id : stale) {
    close_connection(conn_id);
  }

  return stale.empty();
}

std::vector<std::string> RealtimeSync::get_stale_connections() const {
  std::vector<std::string> result;
  auto now = std::chrono::system_clock::now();

  for (const auto& pair : connections_) {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.last_heartbeat);
    if (duration > connection_timeout_) {
      result.push_back(pair.first);
    }
  }

  return result;
}

void RealtimeSync::on_message_received(SyncCallback callback) {
  on_message_ = callback;
}

void RealtimeSync::on_connection_established(ConnectionCallback callback) {
  on_connect_ = callback;
}

void RealtimeSync::on_connection_closed(ConnectionCallback callback) {
  on_disconnect_ = callback;
}

bool RealtimeSync::broadcast_message(const std::string& type, const std::string& content) {
  bool success = false;
  for (auto& pair : connections_) {
    SyncMessage msg;
    msg.message_id = generate_message_id();
    msg.type = type;
    msg.sender_id = "system";
    msg.content = content;
    msg.sequence = sequence_number_++;
    msg.timestamp = std::chrono::system_clock::now();

    message_queue_.push(msg);
    sent_messages_.push_back(msg);
    total_sent_++;
    success = true;
  }
  return success;
}

bool RealtimeSync::send_to_connection(const std::string& connection_id, const std::string& type, const std::string& content) {
  return dequeue_message().has_value();
}

int RealtimeSync::get_message_count(const std::string& connection_id) const {
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    return it->second.message_count;
  }
  return 0;
}

int RealtimeSync::get_total_messages_sent() const {
  return total_sent_;
}

void RealtimeSync::clear_queue() {
  while (!message_queue_.empty()) {
    message_queue_.pop();
  }
}

void RealtimeSync::clear_connections() {
  connections_.clear();
  message_queue_ = {};
  sent_messages_.clear();
  sequence_number_ = 0;
  total_sent_ = 0;
}

std::string RealtimeSync::generate_connection_id() const {
  uuid_t uuid;
  uuid_generate(uuid);
  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "ws_" + ss.str();
}

std::string RealtimeSync::generate_message_id() const {
  uuid_t uuid;
  uuid_generate(uuid);
  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "msg_" + ss.str();
}

} // namespace bendcore::collaboration
