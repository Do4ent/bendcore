// Система сотрудничества: отслеживание изменений, версии, комментарии
// Phase 25 Stage 3: Collaboration Features
#pragma once

#include "bendcore/tube_model.hpp"
#include "bendcore/vec3.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <chrono>

namespace bendcore::collaboration {

// Типы изменений
enum class ChangeType {
    Created,           // Объект создан
    Modified,          // Объект изменён
    Deleted,           // Объект удалён
    Moved,             // Объект переместили
    PropertyChanged,   // Свойство изменилось
    ConstraintAdded,   // Ограничение добавлено
    ConstraintRemoved, // Ограничение удалено
    PortAdded,         // Порт добавлен
    PortRemoved,       // Порт удален
    MaterialChanged,   // Материал изменён
    MachineChanged     // Станок изменён
};

// Уровень доступа
enum class AccessLevel {
    None,       // Нет доступа
    Viewer,     // Только просмотр
    Commenter,  // Просмотр + комментарии
    Editor,     // Редактирование
    Admin       // Администратор
};

// Запись об изменении
struct Change {
    uint64_t id = 0;                        // Уникальный ID изменения
    std::string userId;                     // ID пользователя
    std::string userName;                   // Имя пользователя
    ChangeType type = ChangeType::Modified; // Тип изменения
    std::string objectId;                   // ID объекта (труба, путь, и т.д.)
    std::string objectName;                 // Имя объекта
    std::string description;                // Описание изменения
    std::string beforeValue;                // Значение до изменения
    std::string afterValue;                 // Значение после изменения
    int64_t timestamp = 0;                  // Время изменения (Unix time)
    std::string commitHash;                 // Git commit hash
    bool isRolledBack = false;              // Откачено ли изменение
};

// Версия документа
struct DocumentVersion {
    uint32_t major = 1;         // Основной номер версии
    uint32_t minor = 0;         // Дополнительный номер версии
    uint32_t patch = 0;         // Номер патча
    std::string hash;           // SHA-1 хеш содержимого
    std::string description;    // Описание версии
    std::string createdBy;      // Кто создал версию
    int64_t timestamp = 0;      // Время создания (Unix time)
    std::vector<uint64_t> changeIds;  // IDs изменений в этой версии

    std::string toString() const;
};

// Комментарий на объекте/версии
struct Comment {
    uint64_t id = 0;                   // Уникальный ID комментария
    std::string userId;                // ID пользователя
    std::string userName;              // Имя пользователя
    std::string userEmail;             // Email пользователя
    std::string objectId;              // ID объекта (что комментируем)
    std::string text;                  // Текст комментария
    int64_t timestamp = 0;             // Время создания (Unix time)
    std::optional<uint64_t> parentId;  // ID родительского комментария (для потоков)
    std::vector<uint64_t> replies;     // IDs ответов
    bool isResolved = false;           // Разрешён ли комментарий
    std::vector<std::string> mentions; // Упоминания пользователей (@username)
    std::string attachmentPath;        // Путь к вложению (скриншот, файл)
};

// Информация о пользователе
struct UserInfo {
    std::string userId;
    std::string userName;
    std::string email;
    std::string role;              // "admin", "designer", "engineer", "viewer"
    AccessLevel accessLevel = AccessLevel::Viewer;
    bool isActive = true;
    int64_t lastSeenTimestamp = 0;
    std::string avatarUrl;
};

// Активность пользователя
struct UserActivity {
    std::string userId;
    std::string action;            // "view", "edit", "comment", "share"
    std::string objectId;          // На каком объекте
    int64_t timestamp = 0;
    std::string details;
};

// Трекер изменений
class ChangeTracker {
public:
    explicit ChangeTracker();
    ~ChangeTracker();

    // Регистрация изменения
    uint64_t recordChange(
        const std::string& userId,
        const std::string& userName,
        ChangeType type,
        const std::string& objectId,
        const std::string& objectName,
        const std::string& description,
        const std::string& beforeValue = "",
        const std::string& afterValue = ""
    );

    // Получение истории изменений
    std::vector<Change> getChangeHistory(
        std::optional<std::string> objectId = {},
        std::optional<std::string> userId = {},
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Откат изменения
    bool rollbackChange(uint64_t changeId, const std::string& userId);

    // Получение всех изменений за период
    std::vector<Change> getChangesByUser(
        const std::string& userId,
        int64_t fromTimestamp,
        int64_t toTimestamp
    ) const;

    // Получение статистики изменений
    struct ChangeStats {
        int totalChanges = 0;
        int changesByType[11] = {0};  // По каждому типу ChangeType
        std::map<std::string, int> changesByUser;
        int64_t oldestTimestamp = 0;
        int64_t newestTimestamp = 0;
    };

    ChangeStats getChangeStats() const;

    // Очистка старых изменений
    void purgeOldChanges(int64_t keepAfterTimestamp);

private:
    std::vector<Change> changes;
    uint64_t nextChangeId = 1;
};

// Менеджер версий
class VersionManager {
public:
    explicit VersionManager();
    ~VersionManager();

    // Создание новой версии
    DocumentVersion createVersion(
        const std::string& description,
        const std::string& createdBy,
        const std::vector<uint64_t>& changeIds = {}
    );

    // Получение текущей версии
    DocumentVersion getCurrentVersion() const;

    // Получение определённой версии
    std::optional<DocumentVersion> getVersion(const std::string& versionString) const;

    // Получение истории версий
    std::vector<DocumentVersion> getVersionHistory(int limit = 50) const;

    // Откат на предыдущую версию
    bool revertToVersion(const std::string& versionString, const std::string& userId);

    // Сравнение версий
    struct VersionDiff {
        std::vector<Change> added;      // Новые изменения
        std::vector<Change> removed;    // Удалённые изменения
        int totalDifferences = 0;
    };

    VersionDiff compareVersions(
        const std::string& version1,
        const std::string& version2
    ) const;

    // Тегирование версии
    void tagVersion(const std::string& versionString, const std::string& tagName);

    // Получение версии по тегу
    std::optional<DocumentVersion> getVersionByTag(const std::string& tagName) const;

    // Получение списка тегов
    std::map<std::string, std::string> getAllTags() const;

private:
    std::vector<DocumentVersion> versions;
    std::map<std::string, std::string> tags;  // tagName -> versionString
    uint32_t currentMajor = 1;
    uint32_t currentMinor = 0;
    uint32_t currentPatch = 0;
};

// Менеджер комментариев
class CommentManager {
public:
    explicit CommentManager();
    ~CommentManager();

    // Добавление комментария
    uint64_t addComment(
        const std::string& userId,
        const std::string& userName,
        const std::string& userEmail,
        const std::string& objectId,
        const std::string& text,
        std::optional<uint64_t> parentId = {}
    );

    // Получение комментариев объекта
    std::vector<Comment> getComments(const std::string& objectId) const;

    // Получение цепочки комментариев
    std::vector<Comment> getCommentThread(uint64_t parentId) const;

    // Получение комментариев пользователя
    std::vector<Comment> getCommentsByUser(const std::string& userId) const;

    // Редактирование комментария
    bool editComment(uint64_t commentId, const std::string& newText);

    // Удаление комментария
    bool deleteComment(uint64_t commentId);

    // Отмечание комментария как разрешённого
    bool resolveComment(uint64_t commentId);

    // Получение неразрешённых комментариев
    std::vector<Comment> getUnresolvedComments() const;

    // Добавление вложения к комментарию
    bool attachFile(uint64_t commentId, const std::string& filePath);

    // Получение статистики комментариев
    struct CommentStats {
        int totalComments = 0;
        int unresolvedComments = 0;
        int commentsByUser[100] = {0};  // Примерный массив
        std::map<std::string, int> commentsByObject;
    };

    CommentStats getCommentStats() const;

private:
    std::vector<Comment> comments;
    uint64_t nextCommentId = 1;
};

// Менеджер доступа и прав
class AccessManager {
public:
    explicit AccessManager();
    ~AccessManager();

    // Добавление пользователя
    void addUser(
        const std::string& userId,
        const std::string& userName,
        const std::string& email,
        AccessLevel accessLevel
    );

    // Получение информации о пользователе
    std::optional<UserInfo> getUserInfo(const std::string& userId) const;

    // Изменение уровня доступа
    bool setAccessLevel(const std::string& userId, AccessLevel accessLevel);

    // Проверка прав доступа
    bool hasPermission(
        const std::string& userId,
        const std::string& action
    ) const;

    // Получение списка пользователей с доступом
    std::vector<UserInfo> getUsers(
        std::optional<AccessLevel> filterByLevel = {}
    ) const;

    // Отключение/включение пользователя
    bool setUserActive(const std::string& userId, bool active);

    // Отслеживание активности пользователя
    void logActivity(
        const std::string& userId,
        const std::string& action,
        const std::string& objectId,
        const std::string& details = ""
    );

    // Получение активности пользователя
    std::vector<UserActivity> getUserActivity(
        const std::string& userId,
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Получение активности на объекте
    std::vector<UserActivity> getObjectActivity(
        const std::string& objectId,
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

private:
    std::vector<UserInfo> users;
    std::vector<UserActivity> activities;
};

// Интегрированный менеджер сотрудничества
class CollaborationManager {
public:
    explicit CollaborationManager();
    ~CollaborationManager();

    // Компоненты
    ChangeTracker changeTracker;
    VersionManager versionManager;
    CommentManager commentManager;
    AccessManager accessManager;

    // Сохранение сеанса сотрудничества
    bool saveSession(const std::string& sessionFile) const;

    // Загрузка сеанса
    bool loadSession(const std::string& sessionFile);

    // Получение активных пользователей
    std::vector<UserInfo> getActiveUsers() const;

    // Уведомление о конфликте (когда два пользователя редактируют одно)
    struct ConflictInfo {
        std::string objectId;
        std::string user1;
        std::string user2;
        int64_t timestamp = 0;
        std::string resolution;  // "merge", "user1", "user2", "manual"
    };

    void reportConflict(const ConflictInfo& conflict);
    std::vector<ConflictInfo> getUnresolvedConflicts() const;
    bool resolveConflict(const std::string& objectId, const std::string& resolution);
};

}  // namespace bendcore::collaboration
