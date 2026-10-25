/**
 * GAP-008: Топологические ссылки (Topological References)
 * Система для создания и управления устойчивыми ссылками на элементы геометрии
 *
 * Проблема: рёбра, грани и вершины могут изменять свои ID после операций перестроения.
 * Решение: использовать устойчивые ссылки, основанные на топологических свойствах
 * (геометрическое положение, соседство, нормали и т.д.)
 */

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "bendcore/geometry/brep_builder.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"

namespace bendcore::topology {

using namespace bendcore::geometry;

/**
 * Типы топологических ссылок
 */
enum class ReferenceType {
    Vertex = 0,
    Edge = 1,
    Face = 2,
    Shell = 3,
    Solid = 4
};

/**
 * Идентификатор устойчивой ссылки
 */
using TopologicalReferenceId = std::string;

/**
 * Информация для идентификации элемента геометрии по топологическим свойствам
 */
struct TopologicalSignature {
    ReferenceType type;

    // Для вершин
    std::optional<Point3D> position;
    double position_tolerance = 1e-6;

    // Для рёбер
    std::optional<double> edge_length;
    std::optional<Point3D> edge_midpoint;
    std::optional<VertexId> edge_start_vertex;
    std::optional<VertexId> edge_end_vertex;

    // Для граней
    std::optional<double> face_area;
    std::optional<Vector3D> face_normal;
    std::optional<Point3D> face_centroid;
    std::optional<size_t> face_edge_count;

    // Для Shell и Solid
    std::optional<size_t> element_count;
    std::optional<double> volume;
    std::optional<bool> is_closed;

    // Контекст (соседние элементы)
    std::vector<int> neighbor_ids;

    /**
     * Вычислить сигнатуру для элемента
     */
    static TopologicalSignature compute_for_vertex(const Point3D& point);
    static TopologicalSignature compute_for_edge(
        const Edge& edge,
        const BRepBuilder& builder
    );
    static TopologicalSignature compute_for_face(
        const Face& face,
        const BRepBuilder& builder
    );

    /**
     * Проверить совместимость двух сигнатур
     */
    [[nodiscard]] bool is_compatible_with(const TopologicalSignature& other) const;
};

/**
 * Результат разрешения ссылки
 */
struct ReferenceResolution {
    bool found = false;
    int element_id = -1;
    double confidence = 0.0;  // От 0.0 до 1.0
    std::vector<int> candidates;  // Альтернативные кандидаты
};

/**
 * TopologicalReferenceManager - менеджер устойчивых ссылок
 */
class TopologicalReferenceManager {
public:
    explicit TopologicalReferenceManager(BRepBuilder& builder);
    ~TopologicalReferenceManager() = default;

    // ========== Создание ссылок ==========

    /**
     * Создать ссылку на вершину
     */
    [[nodiscard]] TopologicalReferenceId create_vertex_reference(VertexId vertex_id);

    /**
     * Создать ссылку на ребро
     */
    [[nodiscard]] TopologicalReferenceId create_edge_reference(EdgeId edge_id);

    /**
     * Создать ссылку на грань
     */
    [[nodiscard]] TopologicalReferenceId create_face_reference(FaceId face_id);

    /**
     * Создать ссылку на shell
     */
    [[nodiscard]] TopologicalReferenceId create_shell_reference(ShellId shell_id);

    /**
     * Создать ссылку на solid
     */
    [[nodiscard]] TopologicalReferenceId create_solid_reference(SolidId solid_id);

    // ========== Разрешение ссылок ==========

    /**
     * Разрешить ссылку - найти текущий ID элемента по устойчивой ссылке
     */
    [[nodiscard]] ReferenceResolution resolve_reference(
        const TopologicalReferenceId& ref_id
    ) const;

    /**
     * Разрешить ссылку на вершину
     */
    [[nodiscard]] std::optional<VertexId> resolve_vertex_reference(
        const TopologicalReferenceId& ref_id
    ) const;

    /**
     * Разрешить ссылку на ребро
     */
    [[nodiscard]] std::optional<EdgeId> resolve_edge_reference(
        const TopologicalReferenceId& ref_id
    ) const;

    /**
     * Разрешить ссылку на грань
     */
    [[nodiscard]] std::optional<FaceId> resolve_face_reference(
        const TopologicalReferenceId& ref_id
    ) const;

    // ========== Валидация и диагностика ==========

    /**
     * Проверить, валидна ли ссылка
     */
    [[nodiscard]] bool is_reference_valid(const TopologicalReferenceId& ref_id) const;

    /**
     * Получить тип ссылки
     */
    [[nodiscard]] std::optional<ReferenceType> get_reference_type(
        const TopologicalReferenceId& ref_id
    ) const;

    /**
     * Получить сигнатуру ссылки
     */
    [[nodiscard]] std::optional<TopologicalSignature> get_reference_signature(
        const TopologicalReferenceId& ref_id
    ) const;

    /**
     * Диагностировать неоднозначность при разрешении ссылки
     */
    struct AmbiguityDiagnosis {
        bool has_ambiguity = false;
        std::vector<int> candidates;
        std::vector<double> confidence_scores;
        std::string suggestion;
    };

    [[nodiscard]] AmbiguityDiagnosis diagnose_ambiguity(
        const TopologicalReferenceId& ref_id
    ) const;

    // ========== Управление ссылками ==========

    /**
     * Удалить ссылку
     */
    [[nodiscard]] bool delete_reference(const TopologicalReferenceId& ref_id);

    /**
     * Переименовать ссылку
     */
    [[nodiscard]] bool rename_reference(
        const TopologicalReferenceId& old_id,
        const TopologicalReferenceId& new_id
    );

    /**
     * Получить все ссылки на элемент
     */
    [[nodiscard]] std::vector<TopologicalReferenceId> get_references_to_element(
        ReferenceType type,
        int element_id
    ) const;

    /**
     * Получить все ссылки
     */
    [[nodiscard]] std::vector<TopologicalReferenceId> get_all_references() const;

    /**
     * Очистить все ссылки
     */
    void clear_all_references();

    // ========== Поддержка перестроения ==========

    /**
     * Обновить ссылки после перестроения модели
     * Вызывается после операций, которые могут изменить ID элементов
     */
    [[nodiscard]] size_t update_references_after_rebuild();

    /**
     * Отследить связь между старыми и новыми ID элементов
     * Помогает в переходе от одной версии модели к другой
     */
    void track_element_mapping(
        ReferenceType type,
        int old_id,
        int new_id
    );

    /**
     * Получить историю отображений
     */
    [[nodiscard]] std::map<int, int> get_element_mapping_history(ReferenceType type) const;

    // ========== Статистика ==========

    struct ReferenceStats {
        size_t total_references;
        size_t vertex_references;
        size_t edge_references;
        size_t face_references;
        size_t shell_references;
        size_t solid_references;
        size_t unresolved_references;
        size_t ambiguous_references;
    };

    [[nodiscard]] ReferenceStats get_statistics() const;

    /**
     * Отчёт о здоровье системы ссылок
     */
    struct HealthReport {
        bool is_healthy = true;
        std::vector<std::string> issues;
        std::vector<std::string> warnings;
        double health_score = 1.0;  // 0.0 до 1.0
    };

    [[nodiscard]] HealthReport get_health_report() const;

private:
    BRepBuilder& builder_;

    // Хранилище сигнатур для всех ссылок
    std::map<TopologicalReferenceId, TopologicalSignature> reference_signatures_;
    std::map<TopologicalReferenceId, ReferenceType> reference_types_;

    // Отображение текущих ID элементов (для быстрого доступа)
    std::map<TopologicalReferenceId, int> current_element_ids_;

    // История отображений при перестройке
    std::map<ReferenceType, std::map<int, int>> element_mapping_history_;

    // Счётчик для генерации уникальных ID
    int next_ref_id_counter_ = 0;

    // Статистика (mutable для const методов)
    mutable size_t resolution_failures_ = 0;
    mutable size_t successful_resolutions_ = 0;

    // Вспомогательные методы
    [[nodiscard]] TopologicalReferenceId generate_reference_id(ReferenceType type);

    [[nodiscard]] std::vector<int> find_candidate_elements(
        const TopologicalSignature& signature
    ) const;

    [[nodiscard]] double compute_signature_match_score(
        const TopologicalSignature& sig1,
        const TopologicalSignature& sig2
    ) const;

    void record_resolution_failure(const TopologicalReferenceId& ref_id);
};

}  // namespace bendcore::topology
