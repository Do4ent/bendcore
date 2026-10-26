// GAP-059: MultiUserSupport (36 tests) - Sessions, users, sync
#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <map>
#include <vector>

namespace bendcore::collaboration {

struct User {
    int user_id;
    std::string username;
    std::string email;
    bool is_active;
};

struct Session {
    int session_id;
    std::string project_name;
    int host_user_id;
    std::vector<int> connected_users;
    bool is_active;
    long created_time;
};

class MultiUserSupport {
public:
    void createUser(int id, const std::string& name, const std::string& email) {
        users_[id] = {id, name, email, false};
    }
    
    User* getUser(int id) {
        auto it = users_.find(id);
        if (it != users_.end()) return &it->second;
        return nullptr;
    }
    
    void activateUser(int id) {
        auto* user = getUser(id);
        if (user) user->is_active = true;
    }
    
    int createSession(const std::string& project, int host_id) {
        int sid = next_session_id_++;
        sessions_[sid] = {sid, project, host_id, {host_id}, true, 0};
        return sid;
    }
    
    Session* getSession(int id) {
        auto it = sessions_.find(id);
        if (it != sessions_.end()) return &it->second;
        return nullptr;
    }
    
    void addUserToSession(int session_id, int user_id) {
        auto* session = getSession(session_id);
        if (session) {
            auto* user = getUser(user_id);
            if (user) {
                session->connected_users.push_back(user_id);
                user->is_active = true;
            }
        }
    }
    
    int getUserCountInSession(int session_id) {
        auto* session = getSession(session_id);
        if (!session) return 0;
        return session->connected_users.size();
    }
    
    bool isSessionActive(int id) {
        auto* session = getSession(id);
        if (!session) return false;
        return session->is_active;
    }
    
    size_t getUserCount() const { return users_.size(); }
    size_t getSessionCount() const { return sessions_.size(); }
    
private:
    std::map<int, User> users_;
    std::map<int, Session> sessions_;
    int next_session_id_ = 1;
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
    
    // Test 1-5: User creation
    {
        MultiUserSupport mup;
        
        mup.createUser(1, "Alice", "alice@example.com");
        auto* user = mup.getUser(1);
        TEST("T1: User created", user != nullptr);
        TEST("T2: Username correct", user->username == "Alice");
        TEST("T3: Email correct", user->email == "alice@example.com");
        TEST("T4: User inactive initially", !user->is_active);
        TEST("T5: User count is 1", mup.getUserCount() == 1);
    }
    
    // Test 6-10: Multiple users
    {
        MultiUserSupport mup;
        
        mup.createUser(1, "Alice", "alice@example.com");
        mup.createUser(2, "Bob", "bob@example.com");
        mup.createUser(3, "Charlie", "charlie@example.com");
        
        TEST("T6: Three users created", mup.getUserCount() == 3);
        TEST("T7: User 1 exists", mup.getUser(1) != nullptr);
        TEST("T8: User 2 exists", mup.getUser(2) != nullptr);
        TEST("T9: User 3 exists", mup.getUser(3) != nullptr);
        TEST("T10: Invalid user not found", mup.getUser(999) == nullptr);
    }
    
    // Test 11-15: User activation
    {
        MultiUserSupport mup;
        mup.createUser(1, "Alice", "alice@example.com");
        
        TEST("T11: User inactive initially", !mup.getUser(1)->is_active);
        
        mup.activateUser(1);
        TEST("T12: User activated", mup.getUser(1)->is_active);
        
        mup.createUser(2, "Bob", "bob@example.com");
        mup.activateUser(2);
        TEST("T13: Bob activated", mup.getUser(2)->is_active);
        
        TEST("T14: Alice still active", mup.getUser(1)->is_active);
        TEST("T15: Multiple active users", mup.getUser(1)->is_active && mup.getUser(2)->is_active);
    }
    
    // Test 16-20: Session creation
    {
        MultiUserSupport mup;
        mup.createUser(1, "Alice", "alice@example.com");
        
        int sid = mup.createSession("Project A", 1);
        TEST("T16: Session created", sid > 0);
        
        auto* session = mup.getSession(sid);
        TEST("T17: Session retrieved", session != nullptr);
        TEST("T18: Project name correct", session->project_name == "Project A");
        TEST("T19: Host user correct", session->host_user_id == 1);
        TEST("T20: Session active", session->is_active);
    }
    
    // Test 21-25: Session management
    {
        MultiUserSupport mup;
        
        mup.createUser(1, "Alice", "alice@example.com");
        mup.createUser(2, "Bob", "bob@example.com");
        mup.createUser(3, "Charlie", "charlie@example.com");
        
        int sid1 = mup.createSession("Project A", 1);
        int sid2 = mup.createSession("Project B", 2);
        
        TEST("T21: Two sessions created", mup.getSessionCount() == 2);
        TEST("T22: First session active", mup.isSessionActive(sid1));
        TEST("T23: Second session active", mup.isSessionActive(sid2));
        TEST("T24: Unique session IDs", sid1 != sid2);
        TEST("T25: Invalid session not active", !mup.isSessionActive(999));
    }
    
    // Test 26-30: User session joining
    {
        MultiUserSupport mup;
        
        mup.createUser(1, "Alice", "alice@example.com");
        mup.createUser(2, "Bob", "bob@example.com");
        mup.createUser(3, "Charlie", "charlie@example.com");
        
        int sid = mup.createSession("Project A", 1);
        
        TEST("T26: Initial user count is 1", mup.getUserCountInSession(sid) == 1);
        
        mup.addUserToSession(sid, 2);
        TEST("T27: User added to session", mup.getUserCountInSession(sid) == 2);
        
        mup.addUserToSession(sid, 3);
        TEST("T28: Another user added", mup.getUserCountInSession(sid) == 3);
        
        TEST("T29: All users in session", mup.getUserCountInSession(sid) == 3);
        TEST("T30: Session members tracked", mup.getUserCountInSession(sid) > 1);
    }
    
    // Test 31-36: Multi-session support
    {
        MultiUserSupport mup;
        
        mup.createUser(1, "Alice", "alice@example.com");
        mup.createUser(2, "Bob", "bob@example.com");
        
        int sid1 = mup.createSession("ProjectA", 1);
        int sid2 = mup.createSession("ProjectB", 2);
        
        mup.addUserToSession(sid1, 2);
        mup.addUserToSession(sid2, 1);
        
        TEST("T31: Session 1 has 2 users", mup.getUserCountInSession(sid1) == 2);
        TEST("T32: Session 2 has 2 users", mup.getUserCountInSession(sid2) == 2);
        
        auto* s1 = mup.getSession(sid1);
        auto* s2 = mup.getSession(sid2);
        
        TEST("T33: Session 1 project name", s1->project_name == "ProjectA");
        TEST("T34: Session 2 project name", s2->project_name == "ProjectB");
        TEST("T35: Different hosts", s1->host_user_id != s2->host_user_id);
        TEST("T36: Multi-session working", mup.getSessionCount() == 2);
    }
    
    printf("\n=== GAP-059: MultiUserSupport ===\n");
    printf("Passed: %d/36\n", passed);
    printf("Failed: %d/36\n", failed);
    return (failed == 0) ? 0 : 1;
}
