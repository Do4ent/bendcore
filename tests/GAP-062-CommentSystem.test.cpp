// GAP-062: CommentSystem (32 tests) - Comments, annotations, markup
#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>

namespace bendcore::collaboration {

enum class AnnotationType { TEXT, ARROW, CIRCLE, HIGHLIGHT };

struct Comment {
    int comment_id;
    std::string author;
    std::string text;
    int entity_id;
    bool is_resolved;
    long timestamp;
};

struct Annotation {
    int annotation_id;
    AnnotationType type;
    double x, y, z;
    std::string label;
    int associated_comment;
};

class CommentSystem {
public:
    int addComment(const std::string& author, const std::string& text, int entity_id) {
        int cid = next_comment_id_++;
        comments_[cid] = {cid, author, text, entity_id, false, 0};
        return cid;
    }
    
    Comment* getComment(int id) {
        auto it = comments_.find(id);
        if (it != comments_.end()) return &it->second;
        return nullptr;
    }
    
    void resolveComment(int id) {
        auto* comment = getComment(id);
        if (comment) comment->is_resolved = true;
    }
    
    size_t getCommentCount() const { return comments_.size(); }
    
    size_t getCommentCountForEntity(int entity_id) const {
        int count = 0;
        for (const auto& pair : comments_) {
            if (pair.second.entity_id == entity_id) count++;
        }
        return count;
    }
    
    int addAnnotation(AnnotationType type, double x, double y, double z, const std::string& label) {
        int aid = next_annotation_id_++;
        annotations_[aid] = {aid, type, x, y, z, label, -1};
        return aid;
    }
    
    Annotation* getAnnotation(int id) {
        auto it = annotations_.find(id);
        if (it != annotations_.end()) return &it->second;
        return nullptr;
    }
    
    void linkAnnotationToComment(int annotation_id, int comment_id) {
        auto* annotation = getAnnotation(annotation_id);
        if (annotation && getComment(comment_id)) {
            annotation->associated_comment = comment_id;
        }
    }
    
    size_t getAnnotationCount() const { return annotations_.size(); }
    
    int getResolvedCommentCount() const {
        int count = 0;
        for (const auto& pair : comments_) {
            if (pair.second.is_resolved) count++;
        }
        return count;
    }
    
private:
    std::map<int, Comment> comments_;
    std::map<int, Annotation> annotations_;
    int next_comment_id_ = 1;
    int next_annotation_id_ = 1;
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
    
    // Test 1-5: Comment creation
    {
        CommentSystem cs;
        
        int cid = cs.addComment("Alice", "This needs fixing", 1);
        TEST("T1: Comment added", cid > 0);
        
        auto* comment = cs.getComment(cid);
        TEST("T2: Comment retrieved", comment != nullptr);
        TEST("T3: Author correct", comment->author == "Alice");
        TEST("T4: Text correct", comment->text == "This needs fixing");
        TEST("T5: Entity ID correct", comment->entity_id == 1);
    }
    
    // Test 6-10: Multiple comments
    {
        CommentSystem cs;
        
        int c1 = cs.addComment("Alice", "Comment 1", 1);
        int c2 = cs.addComment("Bob", "Comment 2", 1);
        int c3 = cs.addComment("Charlie", "Comment 3", 2);
        
        TEST("T6: Three comments", cs.getCommentCount() == 3);
        TEST("T7: Unique IDs", c1 != c2 && c2 != c3);
        TEST("T8: Entity 1 has 2 comments", cs.getCommentCountForEntity(1) == 2);
        TEST("T9: Entity 2 has 1 comment", cs.getCommentCountForEntity(2) == 1);
        TEST("T10: Entity 3 has 0 comments", cs.getCommentCountForEntity(3) == 0);
    }
    
    // Test 11-15: Comment resolution
    {
        CommentSystem cs;
        
        int c1 = cs.addComment("Alice", "Fix this", 1);
        int c2 = cs.addComment("Bob", "Also this", 1);
        
        TEST("T11: No resolved initially", cs.getResolvedCommentCount() == 0);
        
        cs.resolveComment(c1);
        TEST("T12: Comment marked resolved", cs.getComment(c1)->is_resolved);
        TEST("T13: Resolved count is 1", cs.getResolvedCommentCount() == 1);
        
        cs.resolveComment(c2);
        TEST("T14: Second comment resolved", cs.getComment(c2)->is_resolved);
        TEST("T15: Resolved count is 2", cs.getResolvedCommentCount() == 2);
    }
    
    // Test 16-20: Annotation creation
    {
        CommentSystem cs;
        
        int aid = cs.addAnnotation(AnnotationType::TEXT, 10.0, 20.0, 30.0, "Dimension");
        TEST("T16: Annotation added", aid > 0);
        
        auto* annotation = cs.getAnnotation(aid);
        TEST("T17: Annotation retrieved", annotation != nullptr);
        TEST("T18: Type TEXT", annotation->type == AnnotationType::TEXT);
        TEST("T19: Label correct", annotation->label == "Dimension");
        TEST("T20: Position stored", annotation->x == 10.0 && annotation->y == 20.0 && annotation->z == 30.0);
    }
    
    // Test 21-25: Multiple annotations
    {
        CommentSystem cs;
        
        int a1 = cs.addAnnotation(AnnotationType::TEXT, 0, 0, 0, "Label1");
        int a2 = cs.addAnnotation(AnnotationType::ARROW, 10, 10, 0, "Arrow1");
        int a3 = cs.addAnnotation(AnnotationType::CIRCLE, 20, 20, 0, "Circle1");
        int a4 = cs.addAnnotation(AnnotationType::HIGHLIGHT, 30, 30, 0, "Highlight1");
        
        TEST("T21: Four annotations", cs.getAnnotationCount() == 4);
        TEST("T22: TEXT annotation", cs.getAnnotation(a1)->type == AnnotationType::TEXT);
        TEST("T23: ARROW annotation", cs.getAnnotation(a2)->type == AnnotationType::ARROW);
        TEST("T24: CIRCLE annotation", cs.getAnnotation(a3)->type == AnnotationType::CIRCLE);
        TEST("T25: HIGHLIGHT annotation", cs.getAnnotation(a4)->type == AnnotationType::HIGHLIGHT);
    }
    
    // Test 26-30: Comment-annotation linking
    {
        CommentSystem cs;
        
        int c1 = cs.addComment("Alice", "See this annotation", 1);
        int a1 = cs.addAnnotation(AnnotationType::ARROW, 10, 20, 30, "Arrow");
        
        cs.linkAnnotationToComment(a1, c1);
        
        auto* annotation = cs.getAnnotation(a1);
        TEST("T26: Annotation linked", annotation->associated_comment == c1);
        
        int a2 = cs.addAnnotation(AnnotationType::CIRCLE, 15, 25, 35, "Circle");
        cs.linkAnnotationToComment(a2, c1);
        
        TEST("T27: Multiple annotations linked", cs.getAnnotation(a2)->associated_comment == c1);
        TEST("T28: Different comments linked", cs.getComment(c1) != nullptr);
        
        int c2 = cs.addComment("Bob", "Another comment", 2);
        cs.linkAnnotationToComment(a1, c2);
        TEST("T29: Link updated", cs.getAnnotation(a1)->associated_comment == c2);
        TEST("T30: Linking works", cs.getAnnotation(a2)->associated_comment > 0);
    }
    
    // Test 31-32: System completeness
    {
        CommentSystem cs;
        
        for (int i = 1; i <= 5; i++) {
            cs.addComment("User", "Comment " + std::to_string(i), 1);
            cs.addAnnotation(AnnotationType::TEXT, i*10, i*10, 0, "Label" + std::to_string(i));
        }
        
        TEST("T31: 5 comments created", cs.getCommentCount() == 5);
        TEST("T32: 5 annotations created", cs.getAnnotationCount() == 5);
    }
    
    printf("\n=== GAP-062: CommentSystem ===\n");
    printf("Passed: %d/32\n", passed);
    printf("Failed: %d/32\n", failed);
    return (failed == 0) ? 0 : 1;
}
