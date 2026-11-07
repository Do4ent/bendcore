#include <bendcore/assembly_v2/solver.hpp>

#include <Eigen/Cholesky>
#include <Eigen/QR>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <string>

namespace bendcore::assembly_v2 {

using kernel::ErrorCode;

const char* toString(SubsystemStatus status) noexcept {
    switch (status) {
        case SubsystemStatus::Solved: return "Solved";
        case SubsystemStatus::Conflicting: return "Conflicting";
        case SubsystemStatus::NotConverged: return "NotConverged";
    }
    return "Unknown";
}

bool SolveResult::success() const {
    return std::all_of(subsystems.begin(), subsystems.end(),
                       [](const SubsystemReport& s) { return s.status == SubsystemStatus::Solved; });
}

// ---------------------------------------------------------------------------------------------
// Разбиение на подсистемы

namespace {

struct DisjointSet {
    std::vector<std::size_t> parent;
    explicit DisjointSet(std::size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    std::size_t find(std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent[std::max(a, b)] = std::min(a, b);
    }
};

}  // namespace

std::vector<Subsystem> partitionSubsystems(const Assembly& assembly) {
    const auto& comps = assembly.components();
    const auto& mates = assembly.mates();
    DisjointSet ds(comps.size());
    for (const Mate& m : mates) {
        if (!comps[m.a.component].fixed && !comps[m.b.component].fixed) ds.unite(m.a.component, m.b.component);
    }
    std::map<std::size_t, Subsystem> byRoot;
    for (const Component& c : comps) {
        if (!c.fixed) byRoot[ds.find(c.id)].components.push_back(c.id);
    }
    Subsystem fixedOnly;
    for (std::size_t i = 0; i < mates.size(); ++i) {
        const Mate& m = mates[i];
        if (!comps[m.a.component].fixed) {
            byRoot[ds.find(m.a.component)].mateIndices.push_back(i);
        } else if (!comps[m.b.component].fixed) {
            byRoot[ds.find(m.b.component)].mateIndices.push_back(i);
        } else {
            fixedOnly.mateIndices.push_back(i);
        }
    }
    std::vector<Subsystem> out;
    out.reserve(byRoot.size() + 1);
    for (auto& [root, s] : byRoot) out.push_back(std::move(s));
    if (!fixedOnly.mateIndices.empty()) out.push_back(std::move(fixedOnly));
    return out;
}

// ---------------------------------------------------------------------------------------------
// Задача одной подсистемы

namespace {

struct Problem {
    std::vector<ComponentId> comps;
    std::vector<int> block;          // по ComponentId: номер блока переменных или −1
    std::vector<Mate> mates;         // с разрешённой ориентацией
    std::vector<MateId> mateIds;
    std::vector<int> rowOffset;
    int hardRows = 0;
    const DragGoal* goal = nullptr;  // мягкая цель (только этап 1 перетаскивания)

    int vars() const { return 6 * static_cast<int>(comps.size()); }
    int rows() const { return hardRows + (goal ? 3 : 0); }
};

Problem buildProblem(const Assembly& assembly, const Subsystem& sub, const std::vector<Transform>& poses) {
    Problem p;
    p.comps = sub.components;
    p.block.assign(assembly.components().size(), -1);
    for (std::size_t k = 0; k < p.comps.size(); ++k) p.block[p.comps[k]] = static_cast<int>(k);
    for (std::size_t idx : sub.mateIndices) {
        const Mate& m = assembly.mates()[idx];
        p.mates.push_back(resolveAlignment(m, poses[m.a.component], poses[m.b.component]));
        p.mateIds.push_back(m.id);
        p.rowOffset.push_back(p.hardRows);
        p.hardRows += equationCount(m.form);
    }
    return p;
}

void evaluate(const Problem& p, const std::vector<Transform>& poses, Eigen::VectorXd& r, Eigen::MatrixXd& J) {
    r.resize(p.rows());
    J.setZero(p.rows(), p.vars());
    MateEvaluation ev;
    for (std::size_t k = 0; k < p.mates.size(); ++k) {
        const Mate& m = p.mates[k];
        detail::evaluateResolved(m, poses[m.a.component], poses[m.b.component], ev);
        const int off = p.rowOffset[k];
        const int n = static_cast<int>(ev.residual.size());
        r.segment(off, n) = ev.residual;
        if (const int ia = p.block[m.a.component]; ia >= 0) J.block(off, 6 * ia, n, 6) += ev.jacobianA;
        if (const int ib = p.block[m.b.component]; ib >= 0) J.block(off, 6 * ib, n, 6) += ev.jacobianB;
    }
    if (p.goal) {
        const Transform& pose = poses[p.goal->component];
        const Vec3 arm = pose.rotation * p.goal->localPoint;
        const double w = p.goal->weight;
        r.segment<3>(p.hardRows) = w * (arm + pose.position - p.goal->target);
        const int ib = p.block[p.goal->component];
        J.block<3, 3>(p.hardRows, 6 * ib).setIdentity();
        J.block<3, 3>(p.hardRows, 6 * ib + 3) = -skew(arm);
        J.block(p.hardRows, 6 * ib, 3, 6) *= w;
    }
}

std::vector<Transform> stepped(const Problem& p, const std::vector<Transform>& poses, const Eigen::VectorXd& h) {
    std::vector<Transform> out = poses;
    for (std::size_t k = 0; k < p.comps.size(); ++k) {
        const ComponentId c = p.comps[k];
        out[c] = applyIncrement(poses[c], h.segment<6>(6 * static_cast<Eigen::Index>(k)));
    }
    return out;
}

double maxAbs(const Eigen::VectorXd& v, int n) { return n == 0 ? 0.0 : v.head(n).cwiseAbs().maxCoeff(); }

// Косинусный критерий стационарности: max_j |g_j| / (‖J_j‖·‖r‖).
double stationarity(const Eigen::MatrixXd& J, const Eigen::VectorXd& r, const Eigen::VectorXd& g) {
    const double rn = r.norm();
    if (rn == 0.0) return 0.0;
    double worst = 0.0;
    for (Eigen::Index j = 0; j < J.cols(); ++j) {
        const double cn = J.col(j).norm();
        if (cn > 0.0) worst = std::max(worst, std::abs(g(j)) / (cn * rn));
    }
    return worst;
}

std::vector<double> characteristicLengths(const Problem& p);

struct LmOutcome {
    int iterations = 0;
    bool converged = false;   // жёсткая невязка ≤ допуска
    bool stationary = false;  // остановка в стационарной точке / по малому шагу
};

// Левенберг — Марквардт (схема Нильсена обновления μ, метрика демпфирования diag(1, L²)).
LmOutcome levenbergMarquardt(const Problem& p, std::vector<Transform>& poses, const SolverOptions& opt) {
    LmOutcome out;
    const int n = p.vars();
    const bool soft = p.goal != nullptr;
    Eigen::VectorXd r, rNew;
    Eigen::MatrixXd J, JNew;
    evaluate(p, poses, r, J);
    const auto hardConverged = [&](const Eigen::VectorXd& res) {
        return maxAbs(res, p.hardRows) <= opt.residualTolerance &&
               (!soft || res.tail(3).norm() <= opt.residualTolerance);
    };
    if (hardConverged(r)) {
        out.converged = true;
        return out;
    }
    if (n == 0) {
        out.stationary = true;  // нет переменных: невязку сопряжений фиксированных тел не изменить
        return out;
    }
    Eigen::MatrixXd A = J.transpose() * J;
    Eigen::VectorXd g = J.transpose() * r;
    double F = 0.5 * r.squaredNorm();
    const double maxDiag = A.diagonal().maxCoeff();
    if (!(maxDiag > 0.0)) {
        out.stationary = true;  // якобиан нулевой: невязку не изменить
        return out;
    }
    // Метрика демпфирования (Левенберг): сдвиг в мм и поворот, умноженный на характерную длину L,
    // равноценны. Шаг по почти свободным направлениям остаётся малым — минимальное движение тел.
    const std::vector<double> L = characteristicLengths(p);
    Eigen::VectorXd D(n);
    for (int k = 0; k < n / 6; ++k) {
        D.segment<3>(6 * k).setConstant(1.0);
        D.segment<3>(6 * k + 3).setConstant(L[k] * L[k]);
    }
    double mu = opt.initialDamping * A.diagonal().cwiseQuotient(D).maxCoeff();
    double nu = 2.0;
    Eigen::LDLT<Eigen::MatrixXd> ldlt(n);

    while (out.iterations < opt.maxIterations) {
        if (stationarity(J, r, g) <= opt.gradientTolerance) {
            out.stationary = true;
            break;
        }
        Eigen::MatrixXd M = A;
        M.diagonal() += mu * D;
        ldlt.compute(M);
        Eigen::VectorXd h = ldlt.solve(-g);
        // Ограничение поворота за шаг: линеаризация exp(ω) годится лишь при малых |ω|, а
        // направления, близкие к свободным степеням свободы, почти не демпфируются.
        double maxRot = 0.0;
        for (Eigen::Index k = 0; k < h.size() / 6; ++k) maxRot = std::max(maxRot, h.segment<3>(6 * k + 3).norm());
        if (maxRot > opt.maxRotationStep) h *= opt.maxRotationStep / maxRot;
        ++out.iterations;
        double scale = 1.0;
        for (ComponentId c : p.comps) scale += poses[c].position.norm();
        if (!h.allFinite()) {
            mu *= nu;
            nu *= 2.0;
            continue;
        }
        if (h.norm() <= opt.stepTolerance * scale) {
            out.stationary = true;
            break;
        }
        std::vector<Transform> trial = stepped(p, poses, h);
        evaluate(p, trial, rNew, JNew);
        const double FNew = 0.5 * rNew.squaredNorm();
        // Предсказанное убывание линейной модели: F − ½‖r + J h‖² = −gᵀh − ½ hᵀA h.
        const double predicted = -g.dot(h) - 0.5 * h.dot(A * h);
        const double rho = predicted > 0.0 ? (F - FNew) / predicted : -1.0;
        if (rho > 0.0 && std::isfinite(FNew)) {
            poses.swap(trial);
            r.swap(rNew);
            J.swap(JNew);
            A.noalias() = J.transpose() * J;
            g.noalias() = J.transpose() * r;
            F = FNew;
            const double t = 2.0 * rho - 1.0;
            mu *= std::max(1.0 / 3.0, 1.0 - t * t * t);
            nu = 2.0;
            if (hardConverged(r)) {
                out.converged = true;
                break;
            }
        } else {
            mu *= nu;
            nu *= 2.0;
            if (mu > 1e30 * (1.0 + maxDiag)) {
                out.stationary = true;
                break;
            }
        }
    }
    if (!out.converged && hardConverged(r)) out.converged = true;
    return out;
}

// ---------------------------------------------------------------------------------------------
// Диагностика

std::vector<double> characteristicLengths(const Problem& p) {
    // Для каждого блока — наибольшее плечо |p − t| = |R·p_local| = |p_local| его сущностей (не меньше 1):
    // поворот на ω смещает точки на ~L·|ω|.
    std::vector<double> L(p.comps.size(), 1.0);
    const auto note = [&](ComponentId c, double arm) {
        if (const int b = p.block[c]; b >= 0) L[b] = std::max(L[b], arm);
    };
    for (const Mate& m : p.mates) {
        if (m.form == MateForm::Rigid) {
            note(m.a.component, m.relative.value_or(Transform::identity()).position.norm());
        } else {
            note(m.a.component, m.a.entity.point.norm());
            note(m.b.component, m.b.entity.point.norm());
        }
    }
    if (p.goal) note(p.goal->component, p.goal->localPoint.norm());
    // В невязки входят и расстояния между сущностями разных тел (например, nb·(pa − pb)), поэтому
    // берётся общая для подсистемы длина: на эталонных механизмах это сходится быстрее поблочной.
    const double common = L.empty() ? 1.0 : *std::max_element(L.begin(), L.end());
    std::fill(L.begin(), L.end(), common);
    return L;
}

void diagnose(const Problem& p, const std::vector<Transform>& poses, const SolverOptions& opt,
              SubsystemReport& rep) {
    Eigen::VectorXd r;
    Eigen::MatrixXd J;
    Problem hard = p;
    hard.goal = nullptr;
    evaluate(hard, poses, r, J);
    rep.residualNorm = r.norm();

    // Масштаб: столбцы поворота делятся на характерную длину L (плечо ~ L), строки нормируются.
    const std::vector<double> L = characteristicLengths(hard);
    Eigen::MatrixXd Js = J;
    for (std::size_t k = 0; k < hard.comps.size(); ++k) Js.middleCols<3>(6 * static_cast<Eigen::Index>(k) + 3) /= L[k];
    for (Eigen::Index i = 0; i < Js.rows(); ++i) {
        const double nrm = Js.row(i).norm();
        if (nrm > 1e-14) {
            Js.row(i) /= nrm;
        } else {
            Js.row(i).setZero();
        }
    }
    const int n = hard.vars();
    if (n > 0 && Js.rows() > 0) {
        Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(Js);
        qr.setThreshold(opt.rankTolerance);
        rep.rank = static_cast<int>(qr.rank());
    } else {
        rep.rank = 0;
    }
    rep.dof = n - rep.rank;

    // Вклад каждого сопряжения: последовательная ортогонализация строк (CGS2) в порядке добавления.
    Eigen::MatrixXd basis(n, n);
    int basisSize = 0;
    std::vector<double> mateRes(hard.mates.size());
    for (std::size_t k = 0; k < hard.mates.size(); ++k) {
        const int off = hard.rowOffset[k];
        const int rowsK = equationCount(hard.mates[k].form);
        MateDiagnostic d;
        d.id = hard.mateIds[k];
        d.equations = rowsK;
        d.residualNorm = r.segment(off, rowsK).norm();
        mateRes[k] = d.residualNorm;
        if (n > 0) {
            const Eigen::MatrixXd own = Js.middleRows(off, rowsK);
            Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qrOwn(own);
            qrOwn.setThreshold(opt.rankTolerance);
            d.rank = static_cast<int>(qrOwn.rank());
            for (int i = 0; i < rowsK && basisSize < n; ++i) {
                Eigen::VectorXd v = own.row(i).transpose();
                if (v.norm() < 0.5) continue;  // нулевая строка (нормированные строки имеют норму 1)
                for (int pass = 0; pass < 2; ++pass) {
                    if (basisSize > 0) {
                        const auto Q = basis.leftCols(basisSize);
                        v -= Q * (Q.transpose() * v);
                    }
                }
                const double vn = v.norm();
                if (vn > opt.rankTolerance) {
                    basis.col(basisSize++) = v / vn;
                    ++d.independent;
                }
            }
        }
        rep.mateDiagnostics.push_back(d);
    }

    const double maxRes = mateRes.empty() ? 0.0 : *std::max_element(mateRes.begin(), mateRes.end());
    const double conflictThreshold = std::max(opt.conflictAbsolute, opt.conflictRelative * maxRes);
    for (const MateDiagnostic& d : rep.mateDiagnostics) {
        const bool conflicting = rep.status == SubsystemStatus::Conflicting && d.residualNorm > conflictThreshold;
        if (conflicting) {
            rep.conflictingMates.push_back(d.id);
        } else if (d.independent == 0 && d.residualNorm <= conflictThreshold) {
            rep.redundantMates.push_back(d.id);
        }
    }
}

SubsystemReport solveSubsystem(const Assembly& assembly, const Subsystem& sub, std::vector<Transform>& poses,
                               const SolverOptions& opt, const DragGoal* goal) {
    SubsystemReport rep;
    rep.components = sub.components;
    for (std::size_t idx : sub.mateIndices) rep.mates.push_back(assembly.mates()[idx].id);

    if (goal) {
        // Этап 1: мягкая цель вместе с жёсткими сопряжениями.
        Problem soft = buildProblem(assembly, sub, poses);
        soft.goal = goal;
        rep.iterations += levenbergMarquardt(soft, poses, opt).iterations;
    }
    // Ориентации Closest разрешаются по положениям на входе этого этапа.
    Problem p = buildProblem(assembly, sub, poses);
    const LmOutcome lm = levenbergMarquardt(p, poses, opt);
    rep.iterations += lm.iterations;
    if (lm.converged) {
        rep.status = SubsystemStatus::Solved;
    } else if (lm.stationary) {
        rep.status = SubsystemStatus::Conflicting;
    } else {
        rep.status = SubsystemStatus::NotConverged;
    }
    diagnose(p, poses, opt, rep);
    return rep;
}

SolveResult solveFrom(const Assembly& assembly, std::vector<Transform> poses, const SolverOptions& opt,
                      const DragGoal* goal) {
    SolveResult result;
    double sq = 0.0;
    for (const Subsystem& sub : partitionSubsystems(assembly)) {
        const bool hasGoal = goal && std::find(sub.components.begin(), sub.components.end(), goal->component) !=
                                         sub.components.end();
        SubsystemReport rep = solveSubsystem(assembly, sub, poses, opt, hasGoal ? goal : nullptr);
        result.totalDof += rep.dof;
        result.totalIterations += rep.iterations;
        sq += rep.residualNorm * rep.residualNorm;
        result.subsystems.push_back(std::move(rep));
    }
    result.residualNorm = std::sqrt(sq);
    result.poses = std::move(poses);
    return result;
}

std::vector<Transform> currentPoses(const Assembly& assembly) {
    std::vector<Transform> poses;
    poses.reserve(assembly.components().size());
    for (const Component& c : assembly.components()) poses.push_back(c.pose);
    return poses;
}

}  // namespace

SolveResult solve(const Assembly& assembly, const SolverOptions& options) {
    return solveFrom(assembly, currentPoses(assembly), options, nullptr);
}

kernel::Result<SolveResult> drag(const Assembly& assembly, const DragGoal& goal, const SolverOptions& options) {
    if (!assembly.hasComponent(goal.component)) {
        return kernel::Result<SolveResult>::failure(ErrorCode::InvalidInput, "drag: unknown component id");
    }
    if (assembly.component(goal.component).fixed) {
        return kernel::Result<SolveResult>::failure(ErrorCode::InvalidInput, "drag: component is fixed");
    }
    if (!(goal.weight > 0.0) || !goal.target.allFinite() || !goal.localPoint.allFinite()) {
        return kernel::Result<SolveResult>::failure(ErrorCode::InvalidInput,
                                                    "drag: weight must be positive and points finite");
    }
    return solveFrom(assembly, currentPoses(assembly), options, &goal);
}

}  // namespace bendcore::assembly_v2
