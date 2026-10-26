// GAP-060: ConflictResolution (38 tests) - Merge, sync conflicts
#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

namespace bendcore::collaboration {

enum class ConflictType { EDIT_CONFLICT, DELETE_CONFLICT, PROPERTY_CONFLICT, GEOMETRY_CONFLICT };

struct Conflict {
    int conflict_id;
    ConflictType type;
    int user_1_id, user_2_id;
    std::string entity_name;
    std::string resolution;
    bool is_resolved;
};

class ConflictResolution {
public:
    void registerConflict(const Conflict& conflict) {
        conflicts_.push_back(conflict);
    }
    
    size_t getConflictCount() const { return conflicts_.size(); }
    
    Conflict* getConflict(int id) {
        for (auto& c : conflicts_) {
            if (c.conflict_id == id) return &c;
        }
        return nullptr;
    }
    
    void resolveConflict(int id, const std::string& resolution) {
        auto* conflict = getConflict(id);
        if (conflict) {
            conflict->resolution = resolution;
            conflict->is_resolved = true;
            resolved_count_++;
        }
    }
    
    int getResolvedCount() const { return resolved_count_; }
    
    bool hasUnresolvedConflicts() const {
        for (const auto& c : conflicts_) {
            if (!c.is_resolved) return true;
        }
        return false;
    }
    
    double getResolutionRate() const {
        if (conflicts_.empty()) return 0;
        return static_cast<double>(resolved_count_) / conflicts_.size();
    }
    
    bool mergeConflicts(int conflict_1, int conflict_2) {
        auto* c1 = getConflict(conflict_1);
        auto* c2 = getConflict(conflict_2);
        return c1 && c2 && c1->entity_name == c2->entity_name;
    }
    
private:
    std::vector<Conflict> conflicts_;
    int resolved_count_ = 0;
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
    
    // Test 1-5: Conflict creation
    {
        ConflictResolution cr;
        
        Conflict conflict = {1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false};
        cr.registerConflict(conflict);
        
        TEST("T1: Conflict registered", cr.getConflictCount() == 1);
        
        auto* retrieved = cr.getConflict(1);
        TEST("T2: Conflict retrieved", retrieved != nullptr);
        TEST("T3: Conflict ID correct", retrieved->conflict_id == 1);
        TEST("T4: Entity name correct", retrieved->entity_name == "Entity_A");
        TEST("T5: Conflict unresolved initially", !retrieved->is_resolved);
    }
    
    // Test 6-10: Multiple conflicts
    {
        ConflictResolution cr;
        
        cr.registerConflict({1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false});
        cr.registerConflict({2, ConflictType::DELETE_CONFLICT, 2, 3, "Entity_B", "", false});
        cr.registerConflict({3, ConflictType::PROPERTY_CONFLICT, 1, 3, "Entity_C", "", false});
        
        TEST("T6: Three conflicts", cr.getConflictCount() == 3);
        TEST("T7: First conflict exists", cr.getConflict(1) != nullptr);
        TEST("T8: Second conflict exists", cr.getConflict(2) != nullptr);
        TEST("T9: Third conflict exists", cr.getConflict(3) != nullptr);
        TEST("T10: Invalid conflict null", cr.getConflict(999) == nullptr);
    }
    
    // Test 11-15: Conflict types
    {
        ConflictResolution cr;
        
        cr.registerConflict({1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false});
        cr.registerConflict({2, ConflictType::DELETE_CONFLICT, 2, 3, "Entity_B", "", false});
        cr.registerConflict({3, ConflictType::PROPERTY_CONFLICT, 1, 3, "Entity_C", "", false});
        cr.registerConflict({4, ConflictType::GEOMETRY_CONFLICT, 2, 1, "Entity_D", "", false});
        
        auto* c1 = cr.getConflict(1);
        auto* c2 = cr.getConflict(2);
        auto* c3 = cr.getConflict(3);
        auto* c4 = cr.getConflict(4);
        
        TEST("T11: Edit conflict type", c1->type == ConflictType::EDIT_CONFLICT);
        TEST("T12: Delete conflict type", c2->type == ConflictType::DELETE_CONFLICT);
        TEST("T13: Property conflict type", c3->type == ConflictType::PROPERTY_CONFLICT);
        TEST("T14: Geometry conflict type", c4->type == ConflictType::GEOMETRY_CONFLICT);
        TEST("T15: Different conflict types", c1->type != c2->type && c2->type != c3->type);
    }
    
    // Test 16-20: Conflict resolution
    {
        ConflictResolution cr;
        
        cr.registerConflict({1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false});
        
        TEST("T16: No resolved initially", cr.getResolvedCount() == 0);
        TEST("T17: Has unresolved", cr.hasUnresolvedConflicts());
        
        cr.resolveConflict(1, "Keep User 1 version");
        
        auto* conflict = cr.getConflict(1);
        TEST("T18: Conflict marked resolved", conflict->is_resolved);
        TEST("T19: Resolution stored", conflict->resolution == "Keep User 1 version");
        TEST("T20: Resolved count incremented", cr.getResolvedCount() == 1);
    }
    
    // Test 21-25: Resolution rate calculation
    {
        ConflictResolution cr;
        
        cr.registerConflict({1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false});
        cr.registerConflict({2, ConflictType::DELETE_CONFLICT, 2, 3, "Entity_B", "", false});
        cr.registerConflict({3, ConflictType::PROPERTY_CONFLICT, 1, 3, "Entity_C", "", false});
        
        TEST("T21: Initial rate 0%", cr.getResolutionRate() == 0.0);
        
        cr.resolveConflict(1, "Resolution 1");
        double rate1 = cr.getResolutionRate();
        TEST("T22: 33% resolved", std::abs(rate1 - 0.333) < 0.01);
        
        cr.resolveConflict(2, "Resolution 2");
        double rate2 = cr.getResolutionRate();
        TEST("T23: 66% resolved", std::abs(rate2 - 0.666) < 0.01);
        
        cr.resolveConflict(3, "Resolution 3");
        TEST("T24: 100% resolved", std::abs(cr.getResolutionRate() - 1.0) < 0.001);
        TEST("T25: No unresolved remaining", !cr.hasUnresolvedConflicts());
    }
    
    // Test 26-30: Bulk conflict handling
    {
        ConflictResolution cr;
        
        for (int i = 1; i <= 10; i++) {
            cr.registerConflict({i, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_" + std::to_string(i), "", false});
        }
        
        TEST("T26: 10 conflicts registered", cr.getConflictCount() == 10);
        
        for (int i = 1; i <= 5; i++) {
            cr.resolveConflict(i, "Resolved");
        }
        
        TEST("T27: 5 resolved", cr.getResolvedCount() == 5);
        TEST("T28: Has unresolved", cr.hasUnresolvedConflicts());
        TEST("T29: 50% resolution rate", std::abs(cr.getResolutionRate() - 0.5) < 0.01);
        
        for (int i = 6; i <= 10; i++) {
            cr.resolveConflict(i, "Resolved");
        }
        TEST("T30: All resolved", cr.getResolutionRate() == 1.0);
    }
    
    // Test 31-38: Conflict merging
    {
        ConflictResolution cr;
        
        cr.registerConflict({1, ConflictType::EDIT_CONFLICT, 1, 2, "Entity_A", "", false});
        cr.registerConflict({2, ConflictType::EDIT_CONFLICT, 1, 3, "Entity_A", "", false});
        cr.registerConflict({3, ConflictType::EDIT_CONFLICT, 2, 3, "Entity_B", "", false});
        
        TEST("T31: Can merge same entity", cr.mergeConflicts(1, 2));
        TEST("T32: Cannot merge different entities", !cr.mergeConflicts(1, 3));
        
        cr.resolveConflict(1, "Merged resolution");
        cr.resolveConflict(2, "Merged resolution");
        
        TEST("T33: Merged conflicts resolved", cr.getConflict(1)->is_resolved && cr.getConflict(2)->is_resolved);
        TEST("T34: 2 resolved so far", cr.getResolvedCount() == 2);
        TEST("T35: One unresolved", cr.hasUnresolvedConflicts());
        
        cr.resolveConflict(3, "Final resolution");
        TEST("T36: All resolved after merge", !cr.hasUnresolvedConflicts());
        TEST("T37: Resolution rate 100%", cr.getResolutionRate() == 1.0);
        TEST("T38: Conflict resolution complete", cr.getResolvedCount() == 3);
    }
    
    printf("\n=== GAP-060: ConflictResolution ===\n");
    printf("Passed: %d/38\n", passed);
    printf("Failed: %d/38\n", failed);
    return (failed == 0) ? 0 : 1;
}
