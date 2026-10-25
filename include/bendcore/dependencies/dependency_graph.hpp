#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"

namespace bendcore::dependencies {

using namespace bendcore::document;
using namespace bendcore::commands;

/**
 * Узел в графе зависимостей параметров
 */
struct DependencyNode {
    EntityId entity_id;
    std::string param_name;

    // Входящие ребра (зависит от)
    std::set<std::pair<EntityId, std::string>> predecessors;

    // Исходящие ребра (влияет на)
    std::set<std::pair<EntityId, std::string>> successors;

    // Статус кэширования
    bool is_dirty = true;
    int depth = -1;  // Глубина в DAG (для оптимизации)

    [[nodiscard]] bool operator<(const DependencyNode& other) const;
    [[nodiscard]] bool operator==(const DependencyNode& other) const;
};

/**
 * Путь зависимостей (для анализа циклов и влияния)
 */
struct DependencyPath {
    std::vector<std::pair<EntityId, std::string>> nodes;
    size_t cost = 0;  // Число переходов

    [[nodiscard]] bool is_empty() const { return nodes.empty(); }
    [[nodiscard]] size_t length() const { return nodes.size(); }
};

/**
 * DependencyGraph — ориентированный ациклический граф зависимостей параметров
 * CAD-CORE-010: Parameter dependency management and cycle detection
 *
 * Функциональность:
 * - Управление зависимостями между параметрами
 * - Обнаружение циклических зависимостей через топологическую сортировку
 * - Отслеживание путей зависимостей (для анализа влияния изменений)
 * - Уведомление через DocumentEventBus об изменениях графа
 * - Кэширование для оптимизации запросов
 * - Параллельная поддержка (потокобезопасное readonly чтение)
 */
class DependencyGraph {
public:
    explicit DependencyGraph(CadDocument& doc, DocumentEventBus& bus);
    ~DependencyGraph() = default;

    // ==================== Основной API ====================

    /**
     * Добавить зависимость: dependent зависит от dependency
     */
    [[nodiscard]] bool add_dependency(
        EntityId dependent_entity,
        const std::string& dependent_param,
        EntityId dependency_entity,
        const std::string& dependency_param
    );

    /**
     * Удалить зависимость
     */
    [[nodiscard]] bool remove_dependency(
        EntityId dependent_entity,
        const std::string& dependent_param,
        EntityId dependency_entity,
        const std::string& dependency_param
    );

    /**
     * Получить все зависимости параметра (на какие параметры он влияет)
     */
    [[nodiscard]] std::set<std::pair<EntityId, std::string>> get_dependents(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Получить все предшественники параметра (от каких параметров он зависит)
     */
    [[nodiscard]] std::set<std::pair<EntityId, std::string>> get_predecessors(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    // ==================== Обнаружение циклов ====================

    /**
     * Проверить наличие циклических зависимостей в графе
     */
    [[nodiscard]] bool has_cycle() const;

    /**
     * Проверить, добавит ли данная зависимость цикл
     */
    [[nodiscard]] bool would_create_cycle(
        EntityId dependent_entity,
        const std::string& dependent_param,
        EntityId dependency_entity,
        const std::string& dependency_param
    ) const;

    /**
     * Найти цикл, если он существует
     */
    [[nodiscard]] std::optional<DependencyPath> find_cycle() const;

    // ==================== Анализ путей и влияния ====================

    /**
     * Найти путь от source к target (если существует)
     */
    [[nodiscard]] std::optional<DependencyPath> find_path(
        EntityId source_entity,
        const std::string& source_param,
        EntityId target_entity,
        const std::string& target_param
    ) const;

    /**
     * Получить все параметры, на которые влияет данный параметр (транзитивное замыкание)
     */
    [[nodiscard]] std::set<std::pair<EntityId, std::string>> get_transitive_dependents(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Получить все параметры, от которых зависит данный параметр (транзитивное замыкание)
     */
    [[nodiscard]] std::set<std::pair<EntityId, std::string>> get_transitive_predecessors(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Получить уровень параметра в графе (топологический порядок)
     */
    [[nodiscard]] int get_level(EntityId entity_id, const std::string& param_name) const;

    // ==================== Топологическая сортировка ====================

    /**
     * Получить топологический порядок (для оптимального вычисления параметров)
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::string>> topological_sort() const;

    /**
     * Инвалидировать кэш параметра (отметить его и все зависимые как грязные)
     */
    void invalidate(EntityId entity_id, const std::string& param_name);

    /**
     * Получить все параметры, требующие пересчета (грязные)
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::string>> get_dirty_parameters() const;

    // ==================== Управление графом ====================

    /**
     * Удалить все зависимости параметра (при удалении параметра)
     */
    void remove_parameter(EntityId entity_id, const std::string& param_name);

    /**
     * Удалить все зависимости сущности (при удалении сущности)
     */
    void remove_entity(EntityId entity_id);

    /**
     * Получить все узлы графа
     */
    [[nodiscard]] std::vector<DependencyNode> get_all_nodes() const;

    // ==================== Статистика ====================

    /**
     * Количество узлов в графе
     */
    [[nodiscard]] size_t node_count() const;

    /**
     * Количество ребер в графе
     */
    [[nodiscard]] size_t edge_count() const;

    /**
     * Получить глубину графа (максимальный уровень)
     */
    [[nodiscard]] int max_depth() const;

    /**
     * Количество диаграмм без входящих ребер (source nodes)
     */
    [[nodiscard]] size_t source_count() const;

    /**
     * Получить исходные узлы (для начала вычислений)
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::string>> get_source_nodes() const;

    // ==================== Пакетные операции ====================

    /**
     * Начать пакетное изменение (отложить уведомления)
     */
    void begin_batch_update();

    /**
     * Завершить пакетное изменение (отправить все отложенные уведомления)
     */
    void end_batch_update();

    [[nodiscard]] bool in_batch() const { return batch_mode_; }

    // ==================== Сохранение и восстановление ====================

    /**
     * Экспортировать граф в JSON для сохранения
     */
    [[nodiscard]] std::string export_to_json() const;

    /**
     * Импортировать граф из JSON
     */
    [[nodiscard]] bool import_from_json(const std::string& json_str);

private:
    CadDocument& document_;
    DocumentEventBus& event_bus_;

    // Основное хранилище узлов
    std::map<std::pair<EntityId, std::string>, DependencyNode> nodes_;

    // Кэш топологического порядка
    mutable std::optional<std::vector<std::pair<EntityId, std::string>>> topo_cache_;
    mutable bool topo_dirty_ = true;

    // Режим пакетного обновления
    bool batch_mode_ = false;
    std::vector<std::pair<EntityId, std::string>> batch_invalidated_;

    // Вспомогательные методы
    void invalidate_topo_cache();
    void mark_successors_dirty(EntityId entity_id, const std::string& param_name);
    [[nodiscard]] bool dfs_has_cycle(
        const std::pair<EntityId, std::string>& node,
        std::set<std::pair<EntityId, std::string>>& visited,
        std::set<std::pair<EntityId, std::string>>& rec_stack
    ) const;
    [[nodiscard]] bool dfs_find_path(
        const std::pair<EntityId, std::string>& current,
        const std::pair<EntityId, std::string>& target,
        std::vector<std::pair<EntityId, std::string>>& path,
        std::set<std::pair<EntityId, std::string>>& visited
    ) const;
    void calculate_depth(EntityId entity_id, const std::string& param_name);
    void notify_graph_changed();
};

}  // namespace bendcore::dependencies
