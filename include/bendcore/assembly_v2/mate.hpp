#pragma once

// Сопряжения: описание, проверка допустимости, функции невязки и аналитический якобиан.
// Формулы — docs/assembly/SOLVER.md, раздел 2.

#include <bendcore/assembly_v2/geometry.hpp>
#include <bendcore/kernel/result.hpp>

#include <Eigen/Core>

#include <optional>

namespace bendcore::assembly_v2 {

enum class MateType {
    Coincident,     // точка-точка, точка-плоскость, точка-ось, плоскость-плоскость, ось-ось
    Concentric,     // ось-ось (оси, цилиндры)
    Distance,       // плоскость-плоскость, точка-плоскость, точка-точка
    Angle,          // между направлениями (ось, нормаль, ось цилиндра)
    Parallel,       // направления параллельны
    Perpendicular,  // направления перпендикулярны
    Tangent,        // плоскость-цилиндр, цилиндр-цилиндр, плоскость-сфера
    Rigid,          // жёсткая связь двух компонентов
};

const char* toString(MateType type) noexcept;

// Ориентация направлений (плоскость-плоскость, соосность, параллельность, цилиндр-цилиндр)
// или сторона касания (плоскость-цилиндр, плоскость-сфера):
//  Aligned     — направления сонаправлены / тело касания на стороне нормали плоскости;
//  AntiAligned — встречные / тело с обратной стороны;
//  Closest     — выбирается по текущим положениям в начале решения.
enum class Alignment { Aligned, AntiAligned, Closest };

// Конкретная форма уравнений, определяется по типу сопряжения и типам сущностей.
enum class MateForm {
    Unprepared,
    PointPoint,               // 3: pa − pb
    PointPlane,               // 1: nb·(pa − pb) − value
    PointLine,                // 2: ub·(pa − pb), vb·(pa − pb)
    PlanePlane,               // 4: na − s·nb; nb·(pa − pb) − value
    LineLine,                 // 5: da − s·db; ub·(pa − pb), vb·(pa − pb)
    PointPointDistance,       // 1: (|pa − pb|² − D²) / (2D)
    Angle,                    // 1: da·db − cos θ
    Parallel,                 // 3: da − s·db
    Perpendicular,            // 1: da·db
    TangentPlaneCylinder,     // 2: na·db; na·(pb − pa) − σ·ρ
    TangentCylinderCylinder,  // 4: da − s·db; (|w|² − D²) / (2D), w = (I − db dbᵀ)(pa − pb)
    TangentPlaneSphere,       // 1: na·(cb − pa) − σ·ρ
    Rigid,                    // 9: tb − (Ra t_rel + ta); Rb ex − Ra R_rel ex; Rb ey − Ra R_rel ey
};

const char* toString(MateForm form) noexcept;

// Число уравнений (строк невязки) формы.
int equationCount(MateForm form) noexcept;

struct Mate {
    MateId id = 0;  // назначает Assembly::addMate
    MateType type = MateType::Coincident;
    Feature a;
    Feature b;
    // Distance: расстояние (мм) — для плоскость-плоскость и точка-плоскость со знаком
    // (точка/плоскость a на стороне нормали b при value > 0), для точка-точка > 0.
    // Angle: угол между направлениями, рад, в (0, π).
    double value = 0.0;
    Alignment alignment = Alignment::Closest;
    // Tangent цилиндр-цилиндр: внутреннее касание (расстояние осей |ρa − ρb|) вместо внешнего.
    bool internalTangency = false;
    // Rigid: положение b в СК a. Если не задано, Assembly::addMate берёт текущее.
    std::optional<Transform> relative;

    // Заполняется prepareMate.
    MateForm form = MateForm::Unprepared;

    static Mate coincident(const Feature& a, const Feature& b, Alignment al = Alignment::Closest);
    static Mate concentric(const Feature& a, const Feature& b, Alignment al = Alignment::Closest);
    static Mate distance(const Feature& a, const Feature& b, double value,
                         Alignment al = Alignment::Closest);
    static Mate angle(const Feature& a, const Feature& b, double radians);
    static Mate parallel(const Feature& a, const Feature& b, Alignment al = Alignment::Closest);
    static Mate perpendicular(const Feature& a, const Feature& b);
    static Mate tangent(const Feature& a, const Feature& b, Alignment side = Alignment::Closest,
                        bool internal = false);
    static Mate rigid(ComponentId a, ComponentId b, std::optional<Transform> relative = {});
};

// Проверяет типы сущностей и параметры, приводит порядок a/b к каноническому (например,
// точка — a, плоскость — b) и заполняет form. Ошибка InvalidInput/Unsupported с причиной.
kernel::Result<Mate> prepareMate(Mate mate);

// Заменяет Alignment::Closest на конкретное значение по положениям компонентов.
Mate resolveAlignment(const Mate& mate, const Transform& poseA, const Transform& poseB);

// Невязка и якобиан сопряжения по переменным (dt, ω) компонентов a и b (см. applyIncrement).
struct MateEvaluation {
    Eigen::VectorXd residual;
    Eigen::Matrix<double, Eigen::Dynamic, 6> jacobianA;
    Eigen::Matrix<double, Eigen::Dynamic, 6> jacobianB;
};

// mate должен быть подготовлен (prepareMate). Alignment::Closest разрешается по переданным
// положениям. Ошибка, если сопряжение не подготовлено.
kernel::Result<MateEvaluation> evaluateMate(const Mate& mate, const Transform& poseA,
                                            const Transform& poseB);

namespace detail {
// Без проверок: mate подготовлен и Closest уже разрешён. out переразмечается по форме.
void evaluateResolved(const Mate& mate, const Transform& poseA, const Transform& poseB,
                      MateEvaluation& out);
}  // namespace detail

}  // namespace bendcore::assembly_v2
