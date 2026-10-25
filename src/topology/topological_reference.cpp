/**
 * GAP-008: Топологические ссылки - реализация
 */

#include "bendcore/topology/topological_reference.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace bendcore::topology {

// ========== TopologicalSignature ==========

TopologicalSignature TopologicalSignature::compute_for_vertex(const Point3D& point) {
    TopologicalSignature sig;
    sig.type = ReferenceType::Vertex;
    sig.position = point;
    return sig;
}

TopologicalSignature TopologicalSignature::compute_for_edge(
    const Edge& edge,
    const BRepBuilder& builder
) {
    TopologicalSignature sig;
    sig.type = ReferenceType::Edge;
    sig.edge_length = edge.length;
    sig.edge_start_vertex = edge.start_vertex;
    sig.edge_end_vertex = edge.end_vertex;

    // Вычислить середину
    auto start_opt = builder.get_vertex(edge.start_vertex);
    auto end_opt = builder.get_vertex(edge.end_vertex);

    if (start_opt && end_opt) {
        Point3D start = start_opt->point;
        Point3D end = end_opt->point;
        Point3D midpoint;
        midpoint.x = (start.x + end.x) * 0.5;
        midpoint.y = (start.y + end.y) * 0.5;
        midpoint.z = (start.z + end.z) * 0.5;
        sig.edge_midpoint = midpoint;
    }

    return sig;
}

TopologicalSignature TopologicalSignature::compute_for_face(
    const Face& face,
    const BRepBuilder& builder
) {
    TopologicalSignature sig;
    sig.type = ReferenceType::Face;
    sig.face_area = face.area;
    sig.face_normal = face.normal;
    sig.face_edge_count = face.boundary_edges.size();
    return sig;
}

bool TopologicalSignature::is_compatible_with(const TopologicalSignature& other) const {
    if (type != other.type) {
        return false;
    }

    switch (type) {
        case ReferenceType::Vertex:
            if (position && other.position) {
                return position->distance_to(*other.position) < position_tolerance;
            }
            break;
        case ReferenceType::Edge:
            if (edge_length && other.edge_length) {
                return std::abs(*edge_length - *other.edge_length) < 1e-6;
            }
            break;
        case ReferenceType::Face:
            if (face_area && other.face_area) {
                return std::abs(*face_area - *other.face_area) < 1e-6;
            }
            break;
        default:
            return true;
    }

    return true;
}

// ========== TopologicalReferenceManager ==========

TopologicalReferenceManager::TopologicalReferenceManager(BRepBuilder& builder)
    : builder_(builder) {}

// ========== Создание ссылок ==========

TopologicalReferenceId TopologicalReferenceManager::create_vertex_reference(VertexId vertex_id) {
    auto vertex_opt = builder_.get_vertex(vertex_id);
    if (!vertex_opt) {
        return "";
    }

    auto ref_id = generate_reference_id(ReferenceType::Vertex);
    auto signature = TopologicalSignature::compute_for_vertex(vertex_opt->point);

    reference_signatures_[ref_id] = signature;
    reference_types_[ref_id] = ReferenceType::Vertex;
    current_element_ids_[ref_id] = vertex_id;

    return ref_id;
}

TopologicalReferenceId TopologicalReferenceManager::create_edge_reference(EdgeId edge_id) {
    auto edge_opt = builder_.get_edge(edge_id);
    if (!edge_opt) {
        return "";
    }

    auto ref_id = generate_reference_id(ReferenceType::Edge);
    auto signature = TopologicalSignature::compute_for_edge(*edge_opt, builder_);

    reference_signatures_[ref_id] = signature;
    reference_types_[ref_id] = ReferenceType::Edge;
    current_element_ids_[ref_id] = edge_id;

    return ref_id;
}

TopologicalReferenceId TopologicalReferenceManager::create_face_reference(FaceId face_id) {
    auto face_opt = builder_.get_face(face_id);
    if (!face_opt) {
        return "";
    }

    auto ref_id = generate_reference_id(ReferenceType::Face);
    auto signature = TopologicalSignature::compute_for_face(*face_opt, builder_);

    reference_signatures_[ref_id] = signature;
    reference_types_[ref_id] = ReferenceType::Face;
    current_element_ids_[ref_id] = face_id;

    return ref_id;
}

TopologicalReferenceId TopologicalReferenceManager::create_shell_reference(ShellId shell_id) {
    auto ref_id = generate_reference_id(ReferenceType::Shell);
    TopologicalSignature signature;
    signature.type = ReferenceType::Shell;

    reference_signatures_[ref_id] = signature;
    reference_types_[ref_id] = ReferenceType::Shell;
    current_element_ids_[ref_id] = shell_id;

    return ref_id;
}

TopologicalReferenceId TopologicalReferenceManager::create_solid_reference(SolidId solid_id) {
    auto ref_id = generate_reference_id(ReferenceType::Solid);
    TopologicalSignature signature;
    signature.type = ReferenceType::Solid;

    reference_signatures_[ref_id] = signature;
    reference_types_[ref_id] = ReferenceType::Solid;
    current_element_ids_[ref_id] = solid_id;

    return ref_id;
}

// ========== Разрешение ссылок ==========

ReferenceResolution TopologicalReferenceManager::resolve_reference(
    const TopologicalReferenceId& ref_id
) const {
    ReferenceResolution result;

    auto it = current_element_ids_.find(ref_id);
    if (it != current_element_ids_.end()) {
        // Если текущий ID ещё валиден, использовать его
        result.element_id = it->second;
        result.found = true;
        result.confidence = 1.0;
        successful_resolutions_++;
        return result;
    }

    // Если текущий ID невалиден, попытаться найти по сигнатуре
    auto sig_it = reference_signatures_.find(ref_id);
    if (sig_it == reference_signatures_.end()) {
        result.found = false;
        resolution_failures_++;
        return result;
    }

    auto candidates = find_candidate_elements(sig_it->second);
    if (candidates.empty()) {
        result.found = false;
        resolution_failures_++;
        return result;
    }

    result.candidates = candidates;
    result.element_id = candidates[0];
    result.found = true;
    result.confidence = compute_signature_match_score(sig_it->second, sig_it->second);

    successful_resolutions_++;
    return result;
}

std::optional<VertexId> TopologicalReferenceManager::resolve_vertex_reference(
    const TopologicalReferenceId& ref_id
) const {
    auto resolution = resolve_reference(ref_id);
    if (resolution.found) {
        return static_cast<VertexId>(resolution.element_id);
    }
    return std::nullopt;
}

std::optional<EdgeId> TopologicalReferenceManager::resolve_edge_reference(
    const TopologicalReferenceId& ref_id
) const {
    auto resolution = resolve_reference(ref_id);
    if (resolution.found) {
        return static_cast<EdgeId>(resolution.element_id);
    }
    return std::nullopt;
}

std::optional<FaceId> TopologicalReferenceManager::resolve_face_reference(
    const TopologicalReferenceId& ref_id
) const {
    auto resolution = resolve_reference(ref_id);
    if (resolution.found) {
        return static_cast<FaceId>(resolution.element_id);
    }
    return std::nullopt;
}

// ========== Валидация и диагностика ==========

bool TopologicalReferenceManager::is_reference_valid(const TopologicalReferenceId& ref_id) const {
    auto resolution = resolve_reference(ref_id);
    return resolution.found && resolution.confidence > 0.5;
}

std::optional<ReferenceType> TopologicalReferenceManager::get_reference_type(
    const TopologicalReferenceId& ref_id
) const {
    auto it = reference_types_.find(ref_id);
    if (it != reference_types_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<TopologicalSignature> TopologicalReferenceManager::get_reference_signature(
    const TopologicalReferenceId& ref_id
) const {
    auto it = reference_signatures_.find(ref_id);
    if (it != reference_signatures_.end()) {
        return it->second;
    }
    return std::nullopt;
}

TopologicalReferenceManager::AmbiguityDiagnosis TopologicalReferenceManager::diagnose_ambiguity(
    const TopologicalReferenceId& ref_id
) const {
    AmbiguityDiagnosis diagnosis;

    auto resolution = resolve_reference(ref_id);
    if (resolution.candidates.size() > 1) {
        diagnosis.has_ambiguity = true;
        diagnosis.candidates = resolution.candidates;

        // Вычислить доверие для каждого кандидата
        auto sig_it = reference_signatures_.find(ref_id);
        if (sig_it != reference_signatures_.end()) {
            for (int candidate : resolution.candidates) {
                double score = 0.5;  // Упрощённо
                diagnosis.confidence_scores.push_back(score);
            }
        }

        diagnosis.suggestion = "Multiple candidates found; use closest match or verify geometry";
    }

    return diagnosis;
}

// ========== Управление ссылками ==========

bool TopologicalReferenceManager::delete_reference(const TopologicalReferenceId& ref_id) {
    reference_signatures_.erase(ref_id);
    reference_types_.erase(ref_id);
    current_element_ids_.erase(ref_id);
    return true;
}

bool TopologicalReferenceManager::rename_reference(
    const TopologicalReferenceId& old_id,
    const TopologicalReferenceId& new_id
) {
    auto sig_it = reference_signatures_.find(old_id);
    auto type_it = reference_types_.find(old_id);
    auto id_it = current_element_ids_.find(old_id);

    if (sig_it == reference_signatures_.end()) {
        return false;
    }

    reference_signatures_[new_id] = sig_it->second;
    reference_types_[new_id] = type_it->second;
    current_element_ids_[new_id] = id_it->second;

    [[maybe_unused]] bool deleted = delete_reference(old_id);
    return true;
}

std::vector<TopologicalReferenceId> TopologicalReferenceManager::get_references_to_element(
    ReferenceType type,
    int element_id
) const {
    std::vector<TopologicalReferenceId> results;

    for (const auto& [ref_id, elem_id] : current_element_ids_) {
        auto type_it = reference_types_.find(ref_id);
        if (type_it != reference_types_.end() && type_it->second == type && elem_id == element_id) {
            results.push_back(ref_id);
        }
    }

    return results;
}

std::vector<TopologicalReferenceId> TopologicalReferenceManager::get_all_references() const {
    std::vector<TopologicalReferenceId> results;

    for (const auto& [ref_id, _] : reference_types_) {
        results.push_back(ref_id);
    }

    return results;
}

void TopologicalReferenceManager::clear_all_references() {
    reference_signatures_.clear();
    reference_types_.clear();
    current_element_ids_.clear();
    element_mapping_history_.clear();
}

// ========== Поддержка перестроения ==========

size_t TopologicalReferenceManager::update_references_after_rebuild() {
    size_t updated = 0;

    for (auto& [ref_id, elem_id] : current_element_ids_) {
        // Попытаться разрешить ссылку заново
        auto resolution = resolve_reference(ref_id);
        if (resolution.found && resolution.element_id != elem_id) {
            elem_id = resolution.element_id;
            updated++;
        }
    }

    return updated;
}

void TopologicalReferenceManager::track_element_mapping(
    ReferenceType type,
    int old_id,
    int new_id
) {
    element_mapping_history_[type][old_id] = new_id;
}

std::map<int, int> TopologicalReferenceManager::get_element_mapping_history(ReferenceType type) const {
    auto it = element_mapping_history_.find(type);
    if (it != element_mapping_history_.end()) {
        return it->second;
    }
    return {};
}

// ========== Статистика ==========

TopologicalReferenceManager::ReferenceStats TopologicalReferenceManager::get_statistics() const {
    ReferenceStats stats;
    stats.total_references = reference_types_.size();

    for (const auto& [_, type] : reference_types_) {
        switch (type) {
            case ReferenceType::Vertex:
                stats.vertex_references++;
                break;
            case ReferenceType::Edge:
                stats.edge_references++;
                break;
            case ReferenceType::Face:
                stats.face_references++;
                break;
            case ReferenceType::Shell:
                stats.shell_references++;
                break;
            case ReferenceType::Solid:
                stats.solid_references++;
                break;
        }
    }

    // Подсчитать неразрешённые и неоднозначные ссылки
    for (const auto& ref_id : get_all_references()) {
        if (!is_reference_valid(ref_id)) {
            stats.unresolved_references++;
        }

        auto diagnosis = diagnose_ambiguity(ref_id);
        if (diagnosis.has_ambiguity) {
            stats.ambiguous_references++;
        }
    }

    return stats;
}

TopologicalReferenceManager::HealthReport TopologicalReferenceManager::get_health_report() const {
    HealthReport report;
    report.is_healthy = true;

    auto stats = get_statistics();

    if (stats.unresolved_references > 0) {
        report.is_healthy = false;
        report.issues.push_back(
            "Found " + std::to_string(stats.unresolved_references) + " unresolved references"
        );
    }

    if (stats.ambiguous_references > 0) {
        report.warnings.push_back(
            "Found " + std::to_string(stats.ambiguous_references) + " ambiguous references"
        );
    }

    // Вычислить очко здоровья
    if (stats.total_references > 0) {
        double unresolved_ratio = static_cast<double>(stats.unresolved_references) / stats.total_references;
        report.health_score = 1.0 - unresolved_ratio;
    }

    return report;
}

// ========== Вспомогательные методы ==========

TopologicalReferenceId TopologicalReferenceManager::generate_reference_id(ReferenceType type) {
    std::ostringstream oss;
    oss << "ref_";

    switch (type) {
        case ReferenceType::Vertex:
            oss << "v";
            break;
        case ReferenceType::Edge:
            oss << "e";
            break;
        case ReferenceType::Face:
            oss << "f";
            break;
        case ReferenceType::Shell:
            oss << "s";
            break;
        case ReferenceType::Solid:
            oss << "d";
            break;
    }

    oss << "_" << next_ref_id_counter_++;

    return oss.str();
}

std::vector<int> TopologicalReferenceManager::find_candidate_elements(
    const TopologicalSignature& signature
) const {
    std::vector<int> candidates;

    switch (signature.type) {
        case ReferenceType::Vertex: {
            if (signature.position) {
                for (int i = 0; i < builder_.vertex_count(); i++) {
                    auto v_opt = builder_.get_vertex(i);
                    if (v_opt && v_opt->point.distance_to(*signature.position) < 1e-6) {
                        candidates.push_back(i);
                    }
                }
            }
            break;
        }
        case ReferenceType::Edge: {
            if (signature.edge_length) {
                for (int i = 0; i < builder_.edge_count(); i++) {
                    auto e_opt = builder_.get_edge(i);
                    if (e_opt && std::abs(e_opt->length - *signature.edge_length) < 1e-6) {
                        candidates.push_back(i);
                    }
                }
            }
            break;
        }
        case ReferenceType::Face: {
            if (signature.face_area) {
                for (int i = 0; i < builder_.face_count(); i++) {
                    auto f_opt = builder_.get_face(i);
                    if (f_opt && std::abs(f_opt->area - *signature.face_area) < 1e-6) {
                        candidates.push_back(i);
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    return candidates;
}

double TopologicalReferenceManager::compute_signature_match_score(
    const TopologicalSignature& sig1,
    const TopologicalSignature& sig2
) const {
    // Упрощённо: если они совместимы, вернуть 1.0
    return sig1.is_compatible_with(sig2) ? 1.0 : 0.0;
}

void TopologicalReferenceManager::record_resolution_failure(const TopologicalReferenceId& ref_id) {
    // В реальной реализации записать в журнал
}

}  // namespace bendcore::topology
