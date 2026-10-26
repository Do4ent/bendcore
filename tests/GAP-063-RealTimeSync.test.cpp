// GAP-063: RealTimeSync (40 tests) - WebSocket, live updates
#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <queue>

namespace bendcore::collaboration {

enum class MessageType { GEOMETRY_UPDATE, PROPERTY_CHANGE, DELETE, LOCK, UNLOCK, CURSOR_MOVE };

struct SyncMessage {
    int message_id;
    MessageType type;
    int entity_id;
    std::string data;
    int sender_user_id;
    long timestamp;
};

class RealTimeSync {
public:
    int broadcastMessage(MessageType type, int entity_id, const std::string& data, int user_id) {
        int mid = next_message_id_++;
        SyncMessage msg = {mid, type, entity_id, data, user_id, 0};
        message_queue_.push(msg);
        total_messages_++;
        return mid;
    }
    
    SyncMessage* getNextMessage() {
        if (message_queue_.empty()) return nullptr;
        
        static SyncMessage current;
        current = message_queue_.front();
        message_queue_.pop();
        return &current;
    }
    
    size_t getPendingMessageCount() const { return message_queue_.size(); }
    
    int getTotalMessagesProcessed() const { return total_messages_; }
    
    bool connectUser(int user_id) {
        if (connected_users_.find(user_id) != connected_users_.end()) return false;
        connected_users_[user_id] = true;
        return true;
    }
    
    bool disconnectUser(int user_id) {
        auto it = connected_users_.find(user_id);
        if (it == connected_users_.end()) return false;
        connected_users_.erase(it);
        return true;
    }
    
    bool isUserConnected(int user_id) const {
        return connected_users_.find(user_id) != connected_users_.end();
    }
    
    size_t getConnectedUserCount() const { return connected_users_.size(); }
    
    void lockEntity(int entity_id, int user_id) {
        entity_locks_[entity_id] = user_id;
    }
    
    void unlockEntity(int entity_id) {
        entity_locks_.erase(entity_id);
    }
    
    int getEntityLockOwner(int entity_id) const {
        auto it = entity_locks_.find(entity_id);
        if (it == entity_locks_.end()) return -1;
        return it->second;
    }
    
    bool isEntityLocked(int entity_id) const {
        return entity_locks_.find(entity_id) != entity_locks_.end();
    }
    
    size_t getLockedEntityCount() const { return entity_locks_.size(); }
    
    double getLatency() const { return latency_ms_; }
    void setLatency(double ms) { latency_ms_ = ms; }
    
private:
    std::queue<SyncMessage> message_queue_;
    std::map<int, bool> connected_users_;
    std::map<int, int> entity_locks_;  // entity_id -> user_id
    int next_message_id_ = 1;
    int total_messages_ = 0;
    double latency_ms_ = 0.0;
};

} // namespace bendcore::collaboration

using namespace bendcore::collaboration;

int main() {
    int passed = 0, failed = 0;
    
    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }
    
    // Test 1-5: Message broadcasting
    {
        RealTimeSync sync;
        
        int mid = sync.broadcastMessage(MessageType::GEOMETRY_UPDATE, 1, "{x: 10}", 1);
        TEST("T1: Message broadcast", mid > 0);
        TEST("T2: Pending message", sync.getPendingMessageCount() > 0);
        
        auto* msg = sync.getNextMessage();
        TEST("T3: Message retrieved", msg != nullptr);
        TEST("T4: Message type correct", msg->type == MessageType::GEOMETRY_UPDATE);
        TEST("T5: Entity ID correct", msg->entity_id == 1);
    }
    
    // Test 6-10: Message queue
    {
        RealTimeSync sync;
        
        sync.broadcastMessage(MessageType::GEOMETRY_UPDATE, 1, "data1", 1);
        sync.broadcastMessage(MessageType::PROPERTY_CHANGE, 2, "data2", 2);
        sync.broadcastMessage(MessageType::DELETE, 3, "data3", 3);
        
        TEST("T6: Three messages", sync.getPendingMessageCount() == 3);
        TEST("T7: Total messages", sync.getTotalMessagesProcessed() == 3);
        
        auto* m1 = sync.getNextMessage();
        TEST("T8: First message retrieved", m1->entity_id == 1);
        TEST("T9: Queue decremented", sync.getPendingMessageCount() == 2);
        
        auto* m2 = sync.getNextMessage();
        TEST("T10: Second message retrieved", m2->entity_id == 2);
    }
    
    // Test 11-15: User connection
    {
        RealTimeSync sync;
        
        bool conn1 = sync.connectUser(1);
        TEST("T11: User 1 connected", conn1);
        TEST("T12: Connected count 1", sync.getConnectedUserCount() == 1);
        TEST("T13: User 1 is connected", sync.isUserConnected(1));
        
        bool conn2 = sync.connectUser(2);
        bool conn_dup = sync.connectUser(1);
        TEST("T14: Duplicate connection rejected", !conn_dup);
        TEST("T15: Two users connected", sync.getConnectedUserCount() == 2);
    }
    
    // Test 16-20: User disconnection
    {
        RealTimeSync sync;
        
        sync.connectUser(1);
        sync.connectUser(2);
        sync.connectUser(3);
        
        TEST("T16: Three users", sync.getConnectedUserCount() == 3);
        
        bool disc = sync.disconnectUser(2);
        TEST("T17: User 2 disconnected", disc);
        TEST("T18: Connected count 2", sync.getConnectedUserCount() == 2);
        TEST("T19: User 2 not connected", !sync.isUserConnected(2));
        TEST("T20: User 1 still connected", sync.isUserConnected(1));
    }
    
    // Test 21-25: Entity locking
    {
        RealTimeSync sync;
        
        sync.lockEntity(1, 1);
        TEST("T21: Entity locked", sync.isEntityLocked(1));
        
        int owner = sync.getEntityLockOwner(1);
        TEST("T22: Lock owner correct", owner == 1);
        TEST("T23: Locked count 1", sync.getLockedEntityCount() == 1);
        
        sync.lockEntity(2, 2);
        sync.lockEntity(3, 2);
        TEST("T24: Three entities locked", sync.getLockedEntityCount() == 3);
        
        TEST("T25: Entity 3 owner is 2", sync.getEntityLockOwner(3) == 2);
    }
    
    // Test 26-30: Lock management
    {
        RealTimeSync sync;
        
        sync.lockEntity(1, 1);
        sync.lockEntity(2, 1);
        sync.lockEntity(3, 2);
        
        TEST("T26: Three locked", sync.getLockedEntityCount() == 3);
        
        sync.unlockEntity(2);
        TEST("T27: Entity 2 unlocked", !sync.isEntityLocked(2));
        TEST("T28: Two locked remaining", sync.getLockedEntityCount() == 2);
        
        sync.unlockEntity(1);
        sync.unlockEntity(3);
        TEST("T29: All unlocked", sync.getLockedEntityCount() == 0);
        TEST("T30: No locked entities", !sync.isEntityLocked(1) && !sync.isEntityLocked(2) && !sync.isEntityLocked(3));
    }
    
    // Test 31-35: Latency simulation
    {
        RealTimeSync sync;
        
        double lat1 = sync.getLatency();
        TEST("T31: Default latency 0", lat1 == 0.0);
        
        sync.setLatency(100.0);
        TEST("T32: Latency set to 100ms", sync.getLatency() == 100.0);
        
        sync.setLatency(50.0);
        TEST("T33: Latency changed to 50ms", sync.getLatency() == 50.0);
        
        sync.setLatency(0.0);
        TEST("T34: Latency reset to 0", sync.getLatency() == 0.0);
        
        TEST("T35: Latency adjustable", sync.getLatency() >= 0);
    }
    
    // Test 36-40: Integrated synchronization
    {
        RealTimeSync sync;
        
        // Users join
        sync.connectUser(1);
        sync.connectUser(2);
        
        // Broadcast changes
        sync.broadcastMessage(MessageType::GEOMETRY_UPDATE, 1, "geom_data", 1);
        sync.broadcastMessage(MessageType::LOCK, 2, "lock", 1);
        sync.broadcastMessage(MessageType::CURSOR_MOVE, 1, "pos", 2);
        
        // Lock entities
        sync.lockEntity(1, 1);
        sync.lockEntity(2, 1);
        
        TEST("T36: Messages queued", sync.getPendingMessageCount() == 3);
        TEST("T37: Users connected", sync.getConnectedUserCount() == 2);
        TEST("T38: Entities locked", sync.getLockedEntityCount() == 2);
        TEST("T39: Total messages processed", sync.getTotalMessagesProcessed() == 3);
        
        // Process messages
        auto* m1 = sync.getNextMessage();
        auto* m2 = sync.getNextMessage();
        auto* m3 = sync.getNextMessage();
        
        TEST("T40: All messages retrieved", m1 != nullptr && m2 != nullptr && m3 != nullptr);
    }
    
    printf("\n=== GAP-063: RealTimeSync ===\n");
    printf("Passed: %d/40\n", passed);
    printf("Failed: %d/40\n", failed);
    return (failed == 0) ? 0 : 1;
}
