// Тесты для системы сотрудничества (Phase 25 Stage 3)
#include "bendcore/collaboration.hpp"

#include <cstdio>
#include <ctime>

using namespace bendcore::collaboration;

struct TestCase {
    const char* name;
    int passed = 0;
    int failed = 0;
};

void assert(TestCase& tc, bool condition, const char* message) {
    if (condition) {
        tc.passed++;
    } else {
        printf("  FAIL: %s\n", message);
        tc.failed++;
    }
}

int main() {
    TestCase tc{"collaboration"};

    // Тест 1: Создание ChangeTracker
    {
        ChangeTracker tracker;
        assert(tc, true, "ChangeTracker создан успешно");
    }

    // Тест 2: Запись изменений
    {
        ChangeTracker tracker;

        uint64_t id1 = tracker.recordChange(
            "user1", "John Doe", ChangeType::Modified,
            "tube1", "Main Tube", "Changed radius",
            "50.0", "55.0"
        );

        uint64_t id2 = tracker.recordChange(
            "user2", "Jane Smith", ChangeType::PropertyChanged,
            "tube1", "Main Tube", "Material changed",
            "copper", "steel"
        );

        assert(tc, id1 > 0, "Первое изменение записано");
        assert(tc, id2 > id1, "ID изменений увеличиваются");
    }

    // Тест 3: История изменений
    {
        ChangeTracker tracker;

        tracker.recordChange("user1", "John", ChangeType::Created, "obj1", "Object 1", "Created");
        tracker.recordChange("user1", "John", ChangeType::Modified, "obj1", "Object 1", "Modified");
        tracker.recordChange("user2", "Jane", ChangeType::Modified, "obj2", "Object 2", "Modified");

        auto history = tracker.getChangeHistory();
        assert(tc, history.size() == 3, "История содержит все изменения");

        auto obj1Changes = tracker.getChangeHistory("obj1");
        assert(tc, obj1Changes.size() == 2, "Фильтрация по объекту работает");

        auto user2Changes = tracker.getChangesByUser("user2", 0, 0);
        assert(tc, user2Changes.size() == 1, "Фильтрация по пользователю работает");
    }

    // Тест 4: Откат изменений
    {
        ChangeTracker tracker;

        uint64_t id = tracker.recordChange(
            "user1", "John", ChangeType::Modified,
            "tube1", "Tube", "Change"
        );

        auto before = tracker.getChangeHistory();
        bool rolled = tracker.rollbackChange(id, "user1");
        auto after = tracker.getChangeHistory();

        assert(tc, rolled, "Откат выполнен");
        assert(tc, after.size() < before.size(), "Отыкученное изменение исключено из истории");
    }

    // Тест 5: Статистика изменений
    {
        ChangeTracker tracker;

        tracker.recordChange("user1", "John", ChangeType::Created, "obj1", "Obj", "");
        tracker.recordChange("user1", "John", ChangeType::Modified, "obj1", "Obj", "");
        tracker.recordChange("user2", "Jane", ChangeType::PropertyChanged, "obj2", "Obj", "");

        auto stats = tracker.getChangeStats();
        assert(tc, stats.totalChanges == 3, "Общее количество изменений");
        assert(tc, stats.changesByUser.size() == 2, "Изменения по пользователям");
    }

    // Тест 6: VersionManager
    {
        VersionManager vm;

        auto v1 = vm.createVersion("First version", "user1");
        auto v2 = vm.createVersion("Second version", "user2");

        assert(tc, v1.major == 1 && v1.minor == 0 && v1.patch == 1, "Первая версия 1.0.1");
        assert(tc, v2.major == 1 && v2.minor == 0 && v2.patch == 2, "Вторая версия 1.0.2");
    }

    // Тест 7: Получение версий
    {
        VersionManager vm;

        vm.createVersion("V1", "user1");
        vm.createVersion("V2", "user2");
        vm.createVersion("V3", "user1");

        auto current = vm.getCurrentVersion();
        assert(tc, current.patch == 3, "Текущая версия - последняя");

        auto history = vm.getVersionHistory(10);
        assert(tc, history.size() == 3, "История версий полная");
    }

    // Тест 8: Теги версий
    {
        VersionManager vm;

        auto v1 = vm.createVersion("Stable", "user1");
        vm.tagVersion(v1.toString(), "stable");

        auto tagged = vm.getVersionByTag("stable");
        assert(tc, tagged.has_value(), "Версия найдена по тегу");
        assert(tc, tagged->patch == v1.patch, "Тег указывает на правильную версию");
    }

    // Тест 9: CommentManager
    {
        CommentManager cm;

        uint64_t c1 = cm.addComment("user1", "John", "john@example.com", "obj1", "This looks good");
        uint64_t c2 = cm.addComment("user2", "Jane", "jane@example.com", "obj1", "I agree", c1);

        assert(tc, c1 > 0, "Первый комментарий добавлен");
        assert(tc, c2 > c1, "Второй комментарий - ответ");
    }

    // Тест 10: Цепочки комментариев
    {
        CommentManager cm;

        uint64_t root = cm.addComment("user1", "John", "john@example.com", "obj1", "Main comment");
        uint64_t reply1 = cm.addComment("user2", "Jane", "jane@example.com", "obj1", "I agree", root);
        uint64_t reply2 = cm.addComment("user1", "John", "john@example.com", "obj1", "Thanks", root);

        auto comments = cm.getComments("obj1");
        assert(tc, comments.size() == 1, "Основной комментарий в списке");

        auto thread = cm.getCommentThread(root);
        assert(tc, thread.size() == 3, "Цепочка содержит все комментарии");
    }

    // Тест 11: Разрешение комментариев
    {
        CommentManager cm;

        uint64_t c1 = cm.addComment("user1", "John", "john@example.com", "obj1", "Issue");

        auto unresolved = cm.getUnresolvedComments();
        assert(tc, unresolved.size() == 1, "Неразрешённый комментарий найден");

        cm.resolveComment(c1);
        unresolved = cm.getUnresolvedComments();
        assert(tc, unresolved.size() == 0, "Разрешённый комментарий исключен");
    }

    // Тест 12: AccessManager
    {
        AccessManager am;

        am.addUser("user1", "John", "john@example.com", AccessLevel::Editor);
        am.addUser("user2", "Jane", "jane@example.com", AccessLevel::Viewer);

        auto john = am.getUserInfo("user1");
        assert(tc, john.has_value(), "Пользователь найден");
        assert(tc, john->accessLevel == AccessLevel::Editor, "Уровень доступа правильный");
    }

    // Тест 13: Проверка прав доступа
    {
        AccessManager am;

        am.addUser("editor", "Editor User", "editor@example.com", AccessLevel::Editor);
        am.addUser("viewer", "Viewer User", "viewer@example.com", AccessLevel::Viewer);

        assert(tc, am.hasPermission("editor", "edit"), "Editor может редактировать");
        assert(tc, !am.hasPermission("viewer", "edit"), "Viewer не может редактировать");
        assert(tc, am.hasPermission("viewer", "view"), "Viewer может просматривать");
    }

    // Тест 14: Активность пользователей
    {
        AccessManager am;

        am.addUser("user1", "John", "john@example.com", AccessLevel::Editor);
        am.logActivity("user1", "view", "obj1", "Viewed tube");
        am.logActivity("user1", "edit", "obj1", "Modified radius");

        auto activity = am.getUserActivity("user1", 0, 0);
        assert(tc, activity.size() == 2, "Активность записана");
    }

    // Тест 15: CollaborationManager интеграция
    {
        CollaborationManager cm;

        // Добавляем пользователей
        cm.accessManager.addUser("user1", "John", "john@example.com", AccessLevel::Editor);
        cm.accessManager.addUser("user2", "Jane", "jane@example.com", AccessLevel::Viewer);

        // Записываем изменение
        cm.changeTracker.recordChange(
            "user1", "John", ChangeType::Modified,
            "tube1", "Tube", "Modified"
        );

        // Добавляем комментарий
        cm.commentManager.addComment("user2", "Jane", "jane@example.com", "tube1", "Looks good");

        // Создаём версию
        auto version = cm.versionManager.createVersion("Updated", "user1");

        assert(tc, version.patch > 0, "Версия создана");
        assert(tc, cm.accessManager.getActiveUsers().size() == 2, "Пользователи зарегистрированы");
    }

    // Результаты
    printf("\n=== Test Collaboration ===\n");
    printf("Passed: %d\n", tc.passed);
    printf("Failed: %d\n", tc.failed);

    return tc.failed == 0 ? 0 : 1;
}
