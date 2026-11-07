#include <bendcore/assembly_v2/mate.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace bendcore::assembly_v2 {

using kernel::ErrorCode;

const char* toString(MateType type) noexcept {
    switch (type) {
        case MateType::Coincident: return "Coincident";
        case MateType::Concentric: return "Concentric";
        case MateType::Distance: return "Distance";
        case MateType::Angle: return "Angle";
        case MateType::Parallel: return "Parallel";
        case MateType::Perpendicular: return "Perpendicular";
        case MateType::Tangent: return "Tangent";
        case MateType::Rigid: return "Rigid";
    }
    return "Unknown";
}

const char* toString(MateForm form) noexcept {
    switch (form) {
        case MateForm::Unprepared: return "Unprepared";
        case MateForm::PointPoint: return "PointPoint";
        case MateForm::PointPlane: return "PointPlane";
        case MateForm::PointLine: return "PointLine";
        case MateForm::PlanePlane: return "PlanePlane";
        case MateForm::LineLine: return "LineLine";
        case MateForm::PointPointDistance: return "PointPointDistance";
        case MateForm::Angle: return "Angle";
        case MateForm::Parallel: return "Parallel";
        case MateForm::Perpendicular: return "Perpendicular";
        case MateForm::TangentPlaneCylinder: return "TangentPlaneCylinder";
        case MateForm::TangentCylinderCylinder: return "TangentCylinderCylinder";
        case MateForm::TangentPlaneSphere: return "TangentPlaneSphere";
        case MateForm::Rigid: return "Rigid";
    }
    return "Unknown";
}

int equationCount(MateForm form) noexcept {
    switch (form) {
        case MateForm::Unprepared: return 0;
        case MateForm::PointPoint: return 3;
        case MateForm::PointPlane: return 1;
        case MateForm::PointLine: return 2;
        case MateForm::PlanePlane: return 4;
        case MateForm::LineLine: return 5;
        case MateForm::PointPointDistance: return 1;
        case MateForm::Angle: return 1;
        case MateForm::Parallel: return 3;
        case MateForm::Perpendicular: return 1;
        case MateForm::TangentPlaneCylinder: return 2;
        case MateForm::TangentCylinderCylinder: return 4;
        case MateForm::TangentPlaneSphere: return 1;
        case MateForm::Rigid: return 9;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Конструкторы описаний

namespace {
Mate makeMate(MateType type, const Feature& a, const Feature& b, double value, Alignment al) {
    Mate m;
    m.type = type;
    m.a = a;
    m.b = b;
    m.value = value;
    m.alignment = al;
    return m;
}
}  // namespace

Mate Mate::coincident(const Feature& a, const Feature& b, Alignment al) {
    return makeMate(MateType::Coincident, a, b, 0.0, al);
}
Mate Mate::concentric(const Feature& a, const Feature& b, Alignment al) {
    return makeMate(MateType::Concentric, a, b, 0.0, al);
}
Mate Mate::distance(const Feature& a, const Feature& b, double value, Alignment al) {
    return makeMate(MateType::Distance, a, b, value, al);
}
Mate Mate::angle(const Feature& a, const Feature& b, double radians) {
    return makeMate(MateType::Angle, a, b, radians, Alignment::Closest);
}
Mate Mate::parallel(const Feature& a, const Feature& b, Alignment al) {
    return makeMate(MateType::Parallel, a, b, 0.0, al);
}
Mate Mate::perpendicular(const Feature& a, const Feature& b) {
    return makeMate(MateType::Perpendicular, a, b, 0.0, Alignment::Closest);
}
Mate Mate::tangent(const Feature& a, const Feature& b, Alignment side, bool internal) {
    Mate m = makeMate(MateType::Tangent, a, b, 0.0, side);
    m.internalTangency = internal;
    return m;
}
Mate Mate::rigid(ComponentId a, ComponentId b, std::optional<Transform> relative) {
    Mate m;
    m.type = MateType::Rigid;
    m.a.component = a;
    m.b.component = b;
    m.relative = relative;
    return m;
}

// ---------------------------------------------------------------------------------------------
// Проверка и каноническая форма

namespace {

kernel::Result<Mate> fail(ErrorCode code, const Mate& m, const std::string& why) {
    return kernel::Result<Mate>::failure(code, std::string(toString(m.type)) + ": " + why);
}

std::string pairName(const Mate& m) {
    return std::string(toString(m.a.entity.type)) + "-" + toString(m.b.entity.type);
}

// Нормализует направление, проверяет конечность и радиус. Пустая строка — без ошибки.
std::string sanitize(Entity& e) {
    if (!e.point.allFinite() || !e.direction.allFinite() || !std::isfinite(e.radius)) {
        return "entity has non-finite coordinates";
    }
    if (e.hasDirection()) {
        const double n = e.direction.norm();
        if (n < 1e-12) return std::string(toString(e.type)) + " has zero direction";
        e.direction /= n;
    }
    if ((e.type == EntityType::Cylinder || e.type == EntityType::Sphere) && !(e.radius > 0.0)) {
        return std::string(toString(e.type)) + " radius must be positive";
    }
    return {};
}

void swapAB(Mate& m) { std::swap(m.a, m.b); }

}  // namespace

kernel::Result<Mate> prepareMate(Mate m) {
    if (m.type == MateType::Rigid) {
        if (m.a.component == m.b.component) {
            return fail(ErrorCode::InvalidInput, m, "components must differ");
        }
        if (m.relative && (!m.relative->position.allFinite() || !m.relative->rotation.coeffs().allFinite() ||
                           m.relative->rotation.norm() < 1e-12)) {
            return fail(ErrorCode::InvalidInput, m, "relative transform is not finite");
        }
        if (m.relative) m.relative->rotation.normalize();
        m.form = MateForm::Rigid;
        return m;
    }
    if (auto err = sanitize(m.a.entity); !err.empty()) return fail(ErrorCode::InvalidInput, m, err);
    if (auto err = sanitize(m.b.entity); !err.empty()) return fail(ErrorCode::InvalidInput, m, err);
    if (!std::isfinite(m.value)) return fail(ErrorCode::InvalidInput, m, "value is not finite");

    const Entity& ea = m.a.entity;
    const Entity& eb = m.b.entity;
    const auto is = [](const Entity& e, EntityType t) { return e.type == t; };

    switch (m.type) {
        case MateType::Coincident:
        case MateType::Distance: {
            const bool distance = m.type == MateType::Distance;
            if (!distance) m.value = 0.0;
            if (ea.isPointLike() && eb.isPointLike()) {
                if (!distance) {
                    m.form = MateForm::PointPoint;
                } else {
                    if (!(m.value > 0.0)) {
                        return fail(ErrorCode::InvalidInput, m,
                                    "point-point distance must be positive (use Coincident for 0)");
                    }
                    m.form = MateForm::PointPointDistance;
                }
                return m;
            }
            if ((ea.isPointLike() && is(eb, EntityType::Plane)) ||
                (is(ea, EntityType::Plane) && eb.isPointLike())) {
                if (is(ea, EntityType::Plane)) swapAB(m);
                m.form = MateForm::PointPlane;
                return m;
            }
            if (is(ea, EntityType::Plane) && is(eb, EntityType::Plane)) {
                m.form = MateForm::PlanePlane;
                return m;
            }
            if (!distance && ((ea.isPointLike() && eb.isLineLike()) || (ea.isLineLike() && eb.isPointLike()))) {
                if (ea.isLineLike()) swapAB(m);
                m.form = MateForm::PointLine;
                return m;
            }
            if (!distance && ea.isLineLike() && eb.isLineLike()) {
                m.form = MateForm::LineLine;
                return m;
            }
            return fail(ErrorCode::Unsupported, m, "unsupported entity pair " + pairName(m));
        }
        case MateType::Concentric:
            if (ea.isLineLike() && eb.isLineLike()) {
                m.form = MateForm::LineLine;
                return m;
            }
            if (is(ea, EntityType::Sphere) && is(eb, EntityType::Sphere)) {
                m.form = MateForm::PointPoint;
                return m;
            }
            return fail(ErrorCode::Unsupported, m, "unsupported entity pair " + pairName(m));
        case MateType::Angle:
            if (!ea.hasDirection() || !eb.hasDirection()) {
                return fail(ErrorCode::InvalidInput, m, "both entities need a direction, got " + pairName(m));
            }
            if (!(m.value > 1e-6 && m.value < std::numbers::pi - 1e-6)) {
                return fail(ErrorCode::InvalidInput, m,
                            "angle must lie in (0, pi); use Parallel for 0 or pi");
            }
            m.form = MateForm::Angle;
            return m;
        case MateType::Parallel:
        case MateType::Perpendicular:
            if (!ea.hasDirection() || !eb.hasDirection()) {
                return fail(ErrorCode::InvalidInput, m, "both entities need a direction, got " + pairName(m));
            }
            m.form = m.type == MateType::Parallel ? MateForm::Parallel : MateForm::Perpendicular;
            return m;
        case MateType::Tangent:
            if ((is(ea, EntityType::Plane) && is(eb, EntityType::Cylinder)) ||
                (is(ea, EntityType::Cylinder) && is(eb, EntityType::Plane))) {
                if (is(ea, EntityType::Cylinder)) swapAB(m);
                m.form = MateForm::TangentPlaneCylinder;
                return m;
            }
            if (is(ea, EntityType::Cylinder) && is(eb, EntityType::Cylinder)) {
                const double d = m.internalTangency ? std::abs(ea.radius - eb.radius) : ea.radius + eb.radius;
                if (d < 1e-9) {
                    return fail(ErrorCode::InvalidInput, m,
                                "internal tangency of equal radii is coaxiality; use Concentric");
                }
                m.form = MateForm::TangentCylinderCylinder;
                return m;
            }
            if ((is(ea, EntityType::Plane) && is(eb, EntityType::Sphere)) ||
                (is(ea, EntityType::Sphere) && is(eb, EntityType::Plane))) {
                if (is(ea, EntityType::Sphere)) swapAB(m);
                m.form = MateForm::TangentPlaneSphere;
                return m;
            }
            return fail(ErrorCode::Unsupported, m, "unsupported entity pair " + pairName(m));
        case MateType::Rigid:
            break;
    }
    return fail(ErrorCode::InvalidInput, m, "unknown mate type");
}

// ---------------------------------------------------------------------------------------------
// Ориентация

namespace {
Alignment bySign(double x) { return x >= 0.0 ? Alignment::Aligned : Alignment::AntiAligned; }
double signOf(Alignment al) { return al == Alignment::AntiAligned ? -1.0 : 1.0; }
}  // namespace

Mate resolveAlignment(const Mate& mate, const Transform& poseA, const Transform& poseB) {
    Mate m = mate;
    if (m.alignment != Alignment::Closest) return m;
    const Vec3 da = poseA.applyToDirection(m.a.entity.direction);
    const Vec3 db = poseB.applyToDirection(m.b.entity.direction);
    const Vec3 pa = poseA.applyToPoint(m.a.entity.point);
    const Vec3 pb = poseB.applyToPoint(m.b.entity.point);
    switch (m.form) {
        case MateForm::PlanePlane:
        case MateForm::LineLine:
        case MateForm::Parallel:
        case MateForm::TangentCylinderCylinder:
            m.alignment = bySign(da.dot(db));
            break;
        case MateForm::TangentPlaneCylinder:
        case MateForm::TangentPlaneSphere:
            m.alignment = bySign(da.dot(pb - pa));
            break;
        default:
            m.alignment = Alignment::Aligned;  // форма не зависит от ориентации
            break;
    }
    return m;
}

// ---------------------------------------------------------------------------------------------
// Невязки и якобианы. Обозначения: P(r) = [I, −[r]×] — производная мировой точки с плечом
// r = p − t по (dt, ω); D(d) = [0, −[d]×] — производная мирового направления.

namespace {

using Row6 = Eigen::Matrix<double, 1, 6>;
using Mat36 = Eigen::Matrix<double, 3, 6>;

Mat36 pointJac(const Vec3& arm) {
    Mat36 j;
    j.leftCols<3>().setIdentity();
    j.rightCols<3>() = -skew(arm);
    return j;
}

Mat36 dirJac(const Vec3& d) {
    Mat36 j;
    j.leftCols<3>().setZero();
    j.rightCols<3>() = -skew(d);
    return j;
}

struct WorldEntity {
    Vec3 p;      // точка
    Vec3 d;      // направление
    Vec3 u, v;   // базис, перпендикулярный d (для прямых)
    Mat36 P;     // ∂p
    Mat36 D;     // ∂d
    Mat36 U, V;  // ∂u, ∂v
    double radius;
};

WorldEntity toWorld(const Entity& e, const Transform& pose, bool needBasis) {
    WorldEntity w;
    w.p = pose.applyToPoint(e.point);
    w.d = pose.applyToDirection(e.direction);
    w.P = pointJac(w.p - pose.position);
    w.D = dirJac(w.d);
    w.radius = e.radius;
    if (needBasis) {
        Vec3 ul, vl;
        perpendicularBasis(e.direction, ul, vl);
        w.u = pose.applyToDirection(ul);
        w.v = pose.applyToDirection(vl);
        w.U = dirJac(w.u);
        w.V = dirJac(w.v);
    }
    return w;
}

// Строки «da − s·db».
void directionRows(const WorldEntity& A, const WorldEntity& B, double s, int row, MateEvaluation& out) {
    out.residual.segment<3>(row) = A.d - s * B.d;
    out.jacobianA.middleRows<3>(row) = A.D;
    out.jacobianB.middleRows<3>(row) = -s * B.D;
}

// Строка «n·(pa − pb) − value», где n — направление сущности N (принадлежит a или b).
void planeOffsetRow(const WorldEntity& A, const WorldEntity& B, double value, int row, MateEvaluation& out) {
    const Vec3 delta = A.p - B.p;
    out.residual(row) = B.d.dot(delta) - value;
    out.jacobianA.row(row) = B.d.transpose() * A.P;
    out.jacobianB.row(row) = delta.transpose() * B.D - B.d.transpose() * B.P;
}

// Строки «ub·(pa − pb), vb·(pa − pb)» — расстояние точки a до прямой b.
void lineOffsetRows(const WorldEntity& A, const WorldEntity& B, int row, MateEvaluation& out) {
    const Vec3 delta = A.p - B.p;
    out.residual(row) = B.u.dot(delta);
    out.residual(row + 1) = B.v.dot(delta);
    out.jacobianA.row(row) = B.u.transpose() * A.P;
    out.jacobianA.row(row + 1) = B.v.transpose() * A.P;
    out.jacobianB.row(row) = delta.transpose() * B.U - B.u.transpose() * B.P;
    out.jacobianB.row(row + 1) = delta.transpose() * B.V - B.v.transpose() * B.P;
}

}  // namespace

namespace detail {

void evaluateResolved(const Mate& m, const Transform& poseA, const Transform& poseB, MateEvaluation& out) {
    const int n = equationCount(m.form);
    out.residual.resize(n);
    out.jacobianA.resize(n, 6);
    out.jacobianB.resize(n, 6);
    const double s = signOf(m.alignment);

    if (m.form == MateForm::Rigid) {
        const Transform rel = m.relative.value_or(Transform::identity());
        const Vec3 arm = poseA.rotation * rel.position;  // плечо целевой точки относительно ta
        const Vec3 target = arm + poseA.position;
        out.residual.segment<3>(0) = poseB.position - target;
        out.jacobianA.middleRows<3>(0) = -pointJac(arm);
        out.jacobianB.middleRows<3>(0) = pointJac(Vec3::Zero());
        const Quat targetRot = poseA.rotation * rel.rotation;
        for (int k = 0; k < 2; ++k) {
            const Vec3 axis = k == 0 ? Vec3::UnitX() : Vec3::UnitY();
            const Vec3 eb = poseB.rotation * axis;
            const Vec3 et = targetRot * axis;
            const int row = 3 + 3 * k;
            out.residual.segment<3>(row) = eb - et;
            out.jacobianB.middleRows<3>(row) = dirJac(eb);
            out.jacobianA.middleRows<3>(row) = -dirJac(et);
        }
        return;
    }

    const bool basisB = m.form == MateForm::PointLine || m.form == MateForm::LineLine;
    const WorldEntity A = toWorld(m.a.entity, poseA, false);
    const WorldEntity B = toWorld(m.b.entity, poseB, basisB);

    switch (m.form) {
        case MateForm::PointPoint:
            out.residual = A.p - B.p;
            out.jacobianA = A.P;
            out.jacobianB = -B.P;
            break;
        case MateForm::PointPlane:
            planeOffsetRow(A, B, m.value, 0, out);
            break;
        case MateForm::PointLine:
            lineOffsetRows(A, B, 0, out);
            break;
        case MateForm::PlanePlane:
            directionRows(A, B, s, 0, out);
            planeOffsetRow(A, B, m.value, 3, out);
            break;
        case MateForm::LineLine:
            directionRows(A, B, s, 0, out);
            lineOffsetRows(A, B, 3, out);
            break;
        case MateForm::PointPointDistance: {
            const Vec3 delta = A.p - B.p;
            const double dist = m.value;
            out.residual(0) = (delta.squaredNorm() - dist * dist) / (2.0 * dist);
            out.jacobianA.row(0) = delta.transpose() * A.P / dist;
            out.jacobianB.row(0) = -delta.transpose() * B.P / dist;
            break;
        }
        case MateForm::Angle:
        case MateForm::Perpendicular: {
            const double c = m.form == MateForm::Angle ? std::cos(m.value) : 0.0;
            out.residual(0) = A.d.dot(B.d) - c;
            out.jacobianA.row(0) = B.d.transpose() * A.D;
            out.jacobianB.row(0) = A.d.transpose() * B.D;
            break;
        }
        case MateForm::Parallel:
            directionRows(A, B, s, 0, out);
            break;
        case MateForm::TangentPlaneCylinder: {
            // a — плоскость (pa, na), b — цилиндр (pb, db, ρ).
            out.residual(0) = A.d.dot(B.d);
            out.jacobianA.row(0) = B.d.transpose() * A.D;
            out.jacobianB.row(0) = A.d.transpose() * B.D;
            const Vec3 delta = B.p - A.p;
            out.residual(1) = A.d.dot(delta) - s * B.radius;
            out.jacobianA.row(1) = delta.transpose() * A.D - A.d.transpose() * A.P;
            out.jacobianB.row(1) = A.d.transpose() * B.P;
            break;
        }
        case MateForm::TangentCylinderCylinder: {
            directionRows(A, B, s, 0, out);
            const double dist = m.internalTangency ? std::abs(A.radius - B.radius) : A.radius + B.radius;
            const Vec3 delta = A.p - B.p;
            const double axial = B.d.dot(delta);
            const Vec3 w = delta - B.d * axial;
            out.residual(3) = (w.squaredNorm() - dist * dist) / (2.0 * dist);
            out.jacobianA.row(3) = w.transpose() * A.P / dist;
            out.jacobianB.row(3) = (-w.transpose() * B.P - axial * delta.transpose() * B.D) / dist;
            break;
        }
        case MateForm::TangentPlaneSphere: {
            const Vec3 delta = B.p - A.p;
            out.residual(0) = A.d.dot(delta) - s * B.radius;
            out.jacobianA.row(0) = delta.transpose() * A.D - A.d.transpose() * A.P;
            out.jacobianB.row(0) = A.d.transpose() * B.P;
            break;
        }
        case MateForm::Unprepared:
        case MateForm::Rigid:
            break;
    }
}

}  // namespace detail

kernel::Result<MateEvaluation> evaluateMate(const Mate& mate, const Transform& poseA, const Transform& poseB) {
    if (mate.form == MateForm::Unprepared) {
        return kernel::Result<MateEvaluation>::failure(ErrorCode::InvalidInput,
                                                       "mate is not prepared (call prepareMate)");
    }
    const Mate resolved = resolveAlignment(mate, poseA, poseB);
    MateEvaluation ev;
    detail::evaluateResolved(resolved, poseA, poseB, ev);
    return ev;
}

}  // namespace bendcore::assembly_v2
