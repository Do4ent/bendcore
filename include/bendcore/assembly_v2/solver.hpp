#pragma once

// Решатель сопряжений: разбиение на подсистемы, Левенберг — Марквардт, ранговая диагностика
// (степени свободы, избыточные и противоречивые сопряжения), перетаскивание с мягкой целью.
// Алгоритм и параметры — docs/assembly/SOLVER.md.

#include <bendcore/assembly_v2/assembly.hpp>
#include <bendcore/kernel/result.hpp>

#include <vector>

namespace bendcore::assembly_v2 {

struct SolverOptions {
    int maxIterations = 200;
    // Сходимость: max |r_i| ≤ residualTolerance (мм для расстояний, безразмерно для направлений).
    double residualTolerance = 1e-10;
    // Остановка в стационарной точке: max_j |(Jᵀr)_j| / (‖J_j‖·‖r‖) ≤ gradientTolerance.
    double gradientTolerance = 1e-10;
    // Остановка по малому шагу: ‖h‖ ≤ stepTolerance·(1 + Σ‖t_c‖) по компонентам подсистемы.
    double stepTolerance = 1e-13;
    // Начальное демпфирование μ0 = initialDamping · max diag(JᵀJ).
    double initialDamping = 1e-3;
    // Наибольший поворот компонента за шаг, рад (шаг масштабируется целиком).
    double maxRotationStep = 0.3;
    // Порог ранга (относительно наибольшего ведущего элемента QR / единичной нормы строки).
    double rankTolerance = 1e-8;
    // Сопряжение противоречиво, если ‖r_mate‖ > max(conflictAbsolute, conflictRelative·max‖r‖).
    double conflictAbsolute = 1e-7;
    double conflictRelative = 1e-4;
};

enum class SubsystemStatus {
    Solved,        // невязка ≤ residualTolerance
    Conflicting,   // достигнут минимум суммы квадратов с ненулевой невязкой
    NotConverged,  // исчерпан лимит итераций
};

const char* toString(SubsystemStatus status) noexcept;

struct MateDiagnostic {
    MateId id = 0;
    int equations = 0;    // строк невязки
    int rank = 0;         // ранг строк самого сопряжения
    int independent = 0;  // насколько сопряжение повышает ранг при добавлении по порядку
    double residualNorm = 0.0;
};

struct SubsystemReport {
    std::vector<ComponentId> components;  // нефиксированные компоненты подсистемы
    std::vector<MateId> mates;
    SubsystemStatus status = SubsystemStatus::Solved;
    int iterations = 0;
    double residualNorm = 0.0;  // ‖r‖₂ по сопряжениям подсистемы
    int rank = 0;               // ранг якобиана в решении
    int dof = 0;                // 6n − rank
    // Сопряжения, не добавляющие ни одного независимого уравнения к предыдущим (по порядку
    // добавления), при нулевой собственной невязке.
    std::vector<MateId> redundantMates;
    // Сопряжения с ненулевой невязкой в точке минимума (только при Conflicting).
    std::vector<MateId> conflictingMates;
    std::vector<MateDiagnostic> mateDiagnostics;
};

struct SolveResult {
    std::vector<Transform> poses;  // по ComponentId
    std::vector<SubsystemReport> subsystems;
    int totalDof = 0;
    int totalIterations = 0;
    double residualNorm = 0.0;
    bool success() const;  // все подсистемы Solved
};

// Подсистема: связная компонента графа «нефиксированный компонент — сопряжение».
// Сопряжения только между фиксированными компонентами образуют подсистему без компонентов.
struct Subsystem {
    std::vector<ComponentId> components;
    std::vector<std::size_t> mateIndices;  // индексы в Assembly::mates(), по возрастанию
};

std::vector<Subsystem> partitionSubsystems(const Assembly& assembly);

SolveResult solve(const Assembly& assembly, const SolverOptions& options = {});

// Мягкая цель перетаскивания: точка localPoint компонента component стремится к target.
struct DragGoal {
    ComponentId component = 0;
    Vec3 localPoint = Vec3::Zero();
    Vec3 target = Vec3::Zero();
    double weight = 1e-3;
};

// Этап 1: МНК с жёсткими сопряжениями и строками weight·(p − target); этап 2: решение только
// жёстких сопряжений из полученного положения. Ошибка, если компонент неизвестен или фиксирован.
kernel::Result<SolveResult> drag(const Assembly& assembly, const DragGoal& goal,
                                 const SolverOptions& options = {});

}  // namespace bendcore::assembly_v2
