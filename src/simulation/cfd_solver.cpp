#include "bendcore/simulation/cfd_solver.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <set>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>

namespace BendCore::CadCore::Simulation {

// ==================== CFDMesh Implementation ====================

void CFDMesh::addPoint(const MeshPoint& point) {
    points_.push_back(point);
}

void CFDMesh::addElement(const MeshElement& element) {
    elements_.push_back(element);
}

void CFDMesh::addFace(const MeshFace& face) {
    faces_.push_back(face);
}

void CFDMesh::clear() {
    points_.clear();
    elements_.clear();
    faces_.clear();
}

double CFDMesh::getTotalVolume() const {
    double total = 0.0;
    for (const auto& elem : elements_) {
        total += elem.volume;
    }
    return total;
}

// ==================== CFDSolver Implementation ====================

CFDSolver::CFDSolver()
    : fluid_density_(1.225), fluid_viscosity_(1.81e-5),
      max_iterations_(1000), relaxation_factor_(0.7),
      convergence_tolerance_(1e-6), converged_(false),
      residual_(1.0), iteration_count_(0) {}

void CFDSolver::setTurbulenceModel(TurbulenceModel model) {
    turbulence_model_ = model;
}

void CFDSolver::setFluidDensity(double density) {
    fluid_density_ = density;
}

void CFDSolver::setFluidViscosity(double viscosity) {
    fluid_viscosity_ = viscosity;
}

void CFDSolver::setNumberOfIterations(size_t iterations) {
    max_iterations_ = iterations;
}

void CFDSolver::setRelaxationFactor(double factor) {
    relaxation_factor_ = factor;
}

void CFDSolver::setMesh(const std::shared_ptr<CFDMesh>& mesh) {
    mesh_ = mesh;
}

void CFDSolver::addBoundaryCondition(const BoundaryCondition& bc) {
    boundary_conditions_.push_back(bc);
}

namespace {

constexpr double kAtmosphere = 101325.0;     // Pa, pressure reference
constexpr double kAirCp = 1006.0;            // J/(kg K)
constexpr double kAirConductivity = 0.026;   // W/(m K)
constexpr double kPrandtlTurbulent = 0.85;
// Standard k-epsilon constants (Launder & Spalding 1974)
constexpr double kCmu = 0.09, kC1 = 1.44, kC2 = 1.92, kSigmaK = 1.0, kSigmaE = 1.3;
constexpr double kInletIntensity = 0.05;
constexpr double kTurbFloor = 1e-12;

struct Edge {
    size_t j;
    double dist;
    Vector3D dir;  // unit vector i -> j
};
using Graph = std::vector<std::vector<Edge>>;

Graph buildGraph(const CFDMesh& mesh) {
    const auto& pts = mesh.getPoints();
    const size_t n = pts.size();
    std::vector<std::set<size_t>> adj(n);

    if (!mesh.getElements().empty()) {
        for (const auto& el : mesh.getElements()) {
            for (size_t a = 0; a < 4; ++a) {
                for (size_t b = a + 1; b < 4; ++b) {
                    size_t i = el.vertex_indices[a], j = el.vertex_indices[b];
                    if (i < n && j < n && i != j) { adj[i].insert(j); adj[j].insert(i); }
                }
            }
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            double h = std::numeric_limits<double>::max();
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                double d = (pts[j].position - pts[i].position).magnitude();
                if (d > 1e-12) h = std::min(h, d);
            }
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                double d = (pts[j].position - pts[i].position).magnitude();
                if (d > 1e-12 && d <= 1.5 * h) { adj[i].insert(j); adj[j].insert(i); }
            }
        }
    }

    Graph g(n);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j : adj[i]) {
            Vector3D diff = pts[j].position - pts[i].position;
            double d = diff.magnitude();
            if (d <= 1e-12) continue;
            g[i].push_back({j, d, diff * (1.0 / d)});
        }
    }
    return g;
}

// Least-squares gradient of a nodal field (handles 1-D / 2-D clouds via
// a rank-revealing solve).
Eigen::Vector3d gradientAt(const Graph& g, const std::vector<Vector3D>& x,
                           const std::vector<double>& phi, size_t i) {
    Eigen::Matrix3d A = Eigen::Matrix3d::Zero();
    Eigen::Vector3d b = Eigen::Vector3d::Zero();
    for (const auto& e : g[i]) {
        Eigen::Vector3d dx(x[e.j].x - x[i].x, x[e.j].y - x[i].y, x[e.j].z - x[i].z);
        double w = 1.0 / (e.dist * e.dist);
        A += w * dx * dx.transpose();
        b += w * dx * (phi[e.j] - phi[i]);
    }
    if (g[i].empty()) return Eigen::Vector3d::Zero();
    return A.completeOrthogonalDecomposition().solve(b);
}

// Steady advection-diffusion-reaction for one scalar:
//   rho u.grad(phi) = div(gamma grad phi) + S - Sp * phi
// Upwind convection and positive diffusion weights give an M-matrix, so
// the discrete solution obeys a maximum principle.
// Nodes not connected to any fixed node (and with no reaction) keep phi.
class ScalarSystem {
public:
    ScalarSystem(const Graph& g, const std::vector<Vector3D>& u_adv, double rho,
                 const std::vector<double>& gamma, const std::vector<double>& sp,
                 const std::vector<bool>& fixed)
        : g_(g), fixed_(fixed) {
        const size_t n = g.size();
        // Which nodes are anchored (reachable from a fixed or reacting node)
        anchored_.assign(n, false);
        std::vector<size_t> stack;
        for (size_t i = 0; i < n; ++i) {
            if (fixed[i] || sp[i] > 0.0) { anchored_[i] = true; stack.push_back(i); }
        }
        while (!stack.empty()) {
            size_t i = stack.back(); stack.pop_back();
            for (const auto& e : g[i]) {
                if (!anchored_[e.j]) { anchored_[e.j] = true; stack.push_back(e.j); }
            }
        }

        std::vector<Eigen::Triplet<double>> t;
        coeff_.assign(n, {});
        for (size_t i = 0; i < n; ++i) {
            if (fixed[i] || !anchored_[i]) {
                t.emplace_back(i, i, 1.0);
                continue;
            }
            const Vector3D& ui = u_adv[i];
            double umag = ui.magnitude();
            double s_sum = 0.0;
            if (umag > 0.0) {
                for (const auto& e : g[i]) s_sum += std::max(0.0, -(ui.dot(e.dir)) / umag);
            }
            double diag = sp[i];
            for (const auto& e : g[i]) {
                double a = 0.5 * (gamma[i] + gamma[e.j]) / (e.dist * e.dist);
                if (s_sum > 0.0) {
                    double s = std::max(0.0, -(ui.dot(e.dir)) / umag);
                    a += rho * umag * s / (e.dist * s_sum);
                }
                diag += a;
                coeff_[i].push_back({e.j, a});
                if (!fixed[e.j] && anchored_[e.j]) t.emplace_back(i, e.j, -a);
            }
            t.emplace_back(i, i, diag);
        }
        A_.resize(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
        A_.setFromTriplets(t.begin(), t.end());
        lu_.compute(A_);
    }

    [[nodiscard]] bool ok() const { return lu_.info() == Eigen::Success; }

    // phi holds fixed values on fixed nodes on input; returns false on failure.
    bool solve(std::vector<double>& phi, const std::vector<double>& source) {
        const size_t n = g_.size();
        Eigen::VectorXd rhs(static_cast<Eigen::Index>(n));
        for (size_t i = 0; i < n; ++i) {
            if (fixed_[i] || !anchored_[i]) { rhs[i] = phi[i]; continue; }
            double r = source[i];
            for (const auto& [j, a] : coeff_[i]) {
                if (fixed_[j] || !anchored_[j]) r += a * phi[j];
            }
            rhs[i] = r;
        }
        Eigen::VectorXd x = lu_.solve(rhs);
        if (lu_.info() != Eigen::Success || !x.allFinite()) return false;
        for (size_t i = 0; i < n; ++i) phi[i] = x[i];
        return true;
    }

private:
    const Graph& g_;
    const std::vector<bool>& fixed_;
    std::vector<bool> anchored_;
    std::vector<std::vector<std::pair<size_t, double>>> coeff_;
    Eigen::SparseMatrix<double> A_;
    Eigen::SparseLU<Eigen::SparseMatrix<double>> lu_;
};

}  // namespace

bool CFDSolver::solve() {
    converged_ = false;
    iteration_count_ = 0;
    residual_ = 1.0;
    if (!mesh_ || mesh_->getNumPoints() == 0) {
        return false;
    }
    // Only laminar flow and standard k-epsilon have equations here (see the header).
    if (turbulence_model_ != TurbulenceModel::LAMINAR &&
        turbulence_model_ != TurbulenceModel::K_EPSILON) {
        return false;
    }

    initializeFields();

    const size_t n = mesh_->getNumPoints();
    const Graph graph = buildGraph(*mesh_);
    std::vector<Vector3D> x(n);
    for (size_t i = 0; i < n; ++i) x[i] = mesh_->getPoints()[i].position;

    // ---- Boundary conditions -> fixed nodes
    std::vector<bool> fix_u(n, false), fix_t(n, false), fix_k(n, false), fix_p(n, false);
    std::vector<double> p_fixed(n, kAtmosphere), k_in(n, 0.0), e_in(n, 0.0);

    Vector3D lo = x[0], hi = x[0];
    for (const auto& p : x) {
        lo = Vector3D(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
        hi = Vector3D(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
    }
    double length_scale = (hi - lo).magnitude();
    if (length_scale <= 0.0) length_scale = 1.0;
    const double mixing_length = 0.07 * length_scale;

    for (const auto& bc : boundary_conditions_) {
        for (size_t idx : bc.face_indices) {
            if (idx >= n) continue;
            if (bc.type == BoundaryConditionType::DIRICHLET) {
                fix_u[idx] = true;
                fix_p[idx] = true;
                p_fixed[idx] = kAtmosphere + bc.pressure;
                if (bc.temperature > 0.0) fix_t[idx] = true;
                double umag = std::sqrt(bc.velocity_x * bc.velocity_x +
                                        bc.velocity_y * bc.velocity_y +
                                        bc.velocity_z * bc.velocity_z);
                double k = std::max(1.5 * std::pow(kInletIntensity * umag, 2), kTurbFloor);
                fix_k[idx] = true;
                k_in[idx] = k;
                e_in[idx] = std::max(std::pow(kCmu, 0.75) * std::pow(k, 1.5) / mixing_length, kTurbFloor);
            } else if (bc.type == BoundaryConditionType::NO_SLIP) {
                fix_u[idx] = true;
                velocity_field_[idx].velocity_x = 0.0;
                velocity_field_[idx].velocity_y = 0.0;
                velocity_field_[idx].velocity_z = 0.0;
                if (bc.temperature > 0.0) {
                    fix_t[idx] = true;
                    temperature_field_[idx] = bc.temperature;
                }
                fix_k[idx] = true;  // k -> 0 at a wall; epsilon left free
                k_in[idx] = kTurbFloor;
            }
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (fix_k[i]) { turbulent_ke_field_[i] = k_in[i]; if (e_in[i] > 0) turbulent_diss_field_[i] = e_in[i]; }
    }
    std::vector<bool> fix_e(n, false);
    for (size_t i = 0; i < n; ++i) fix_e[i] = fix_k[i] && e_in[i] > 0.0;

    const bool turbulent = turbulence_model_ != TurbulenceModel::LAMINAR;
    const double rho = fluid_density_, mu = fluid_viscosity_;
    std::vector<double> mu_t(n, 0.0);
    std::vector<Vector3D> u_adv(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& f = velocity_field_[i];
        u_adv[i] = Vector3D(f.velocity_x, f.velocity_y, f.velocity_z);
    }

    auto component = [&](int c) {
        std::vector<double> v(n);
        for (size_t i = 0; i < n; ++i) {
            const auto& f = velocity_field_[i];
            v[i] = c == 0 ? f.velocity_x : (c == 1 ? f.velocity_y : f.velocity_z);
        }
        return v;
    };
    const std::vector<double> zero(n, 0.0);

    // ---- Picard iterations
    for (size_t iter = 0; iter < max_iterations_; ++iter) {
        const std::vector<FlowField> previous = velocity_field_;

        // Momentum (three components share one matrix)
        std::vector<double> gamma_u(n);
        for (size_t i = 0; i < n; ++i) gamma_u[i] = mu + mu_t[i];
        ScalarSystem mom(graph, u_adv, rho, gamma_u, zero, fix_u);
        if (!mom.ok()) return false;
        std::vector<double> ux = component(0), uy = component(1), uz = component(2);
        if (!mom.solve(ux, zero) || !mom.solve(uy, zero) || !mom.solve(uz, zero)) return false;
        for (size_t i = 0; i < n; ++i) {
            velocity_field_[i].velocity_x = ux[i];
            velocity_field_[i].velocity_y = uy[i];
            velocity_field_[i].velocity_z = uz[i];
        }

        // Turbulence: k-epsilon with Picard-linearised sinks
        if (turbulent) {
            std::vector<double> pk(n, 0.0);
            for (size_t i = 0; i < n; ++i) {
                Eigen::Matrix3d grad;
                grad.row(0) = gradientAt(graph, x, ux, i);
                grad.row(1) = gradientAt(graph, x, uy, i);
                grad.row(2) = gradientAt(graph, x, uz, i);
                Eigen::Matrix3d S = 0.5 * (grad + grad.transpose());
                pk[i] = mu_t[i] * 2.0 * (S.array() * S.array()).sum();
            }
            std::vector<double> gk(n), ge(n), sp_k(n), sp_e(n), src_e(n);
            for (size_t i = 0; i < n; ++i) {
                double k = turbulent_ke_field_[i], e = turbulent_diss_field_[i];
                gk[i] = mu + mu_t[i] / kSigmaK;
                ge[i] = mu + mu_t[i] / kSigmaE;
                sp_k[i] = rho * e / k;
                sp_e[i] = kC2 * rho * e / k;
                src_e[i] = kC1 * (e / k) * pk[i];
            }
            ScalarSystem ks(graph, u_adv, rho, gk, sp_k, fix_k);
            ScalarSystem es(graph, u_adv, rho, ge, sp_e, fix_e);
            if (!ks.ok() || !es.ok()) return false;
            if (!ks.solve(turbulent_ke_field_, pk) || !es.solve(turbulent_diss_field_, src_e)) return false;
            for (size_t i = 0; i < n; ++i) {
                turbulent_ke_field_[i] = std::max(turbulent_ke_field_[i], kTurbFloor);
                turbulent_diss_field_[i] = std::max(turbulent_diss_field_[i], kTurbFloor);
                double k = turbulent_ke_field_[i], e = turbulent_diss_field_[i];
                // Cap mu_t to keep the linearisation well conditioned
                mu_t[i] = std::min(rho * kCmu * k * k / e, 1e5 * mu);
                velocity_field_[i].turbulent_kinetic_energy = k;
                velocity_field_[i].turbulent_dissipation = e;
            }
        }

        // Energy
        {
            std::vector<double> gamma_t(n);
            for (size_t i = 0; i < n; ++i) gamma_t[i] = kAirConductivity / kAirCp + mu_t[i] / kPrandtlTurbulent;
            ScalarSystem en(graph, u_adv, rho, gamma_t, zero, fix_t);
            if (!en.ok() || !en.solve(temperature_field_, zero)) return false;
            for (size_t i = 0; i < n; ++i) velocity_field_[i].temperature = temperature_field_[i];
        }

        // Relax the lagged advecting velocity (Picard damping)
        for (size_t i = 0; i < n; ++i) {
            Vector3D u_new(ux[i], uy[i], uz[i]);
            u_adv[i] = u_adv[i] + (u_new - u_adv[i]) * relaxation_factor_;
        }

        iteration_count_ = iter + 1;
        residual_ = computeResidual(previous);
        if (iter > 0 && residual_ < convergence_tolerance_) {
            converged_ = true;
            break;
        }
    }

    // ---- Pressure Poisson equation on the final velocity:
    //   lap(p) = -rho * d_i u_j d_j u_i
    {
        std::vector<double> ux = component(0), uy = component(1), uz = component(2);
        std::vector<double> src(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            Eigen::Matrix3d grad;
            grad.row(0) = gradientAt(graph, x, ux, i);
            grad.row(1) = gradientAt(graph, x, uy, i);
            grad.row(2) = gradientAt(graph, x, uz, i);
            // div(gamma grad p) = -S with gamma = 1 -> S = rho * tr(grad^2)
            src[i] = rho * (grad * grad).trace();
        }
        bool any_fixed = std::any_of(fix_p.begin(), fix_p.end(), [](bool b) { return b; });
        if (!any_fixed) fix_p[0] = true;  // reference pressure
        std::vector<double> ones(n, 1.0);
        std::vector<Vector3D> still(n);
        ScalarSystem ps(graph, still, 1.0, ones, zero, fix_p);
        for (size_t i = 0; i < n; ++i) pressure_field_[i] = fix_p[i] ? p_fixed[i] : kAtmosphere;
        if (!ps.ok() || !ps.solve(pressure_field_, src)) return false;
        for (size_t i = 0; i < n; ++i) velocity_field_[i].pressure = pressure_field_[i];
    }

    return converged_;
}

void CFDSolver::initializeFields() {
    size_t num_points = mesh_->getNumPoints();

    velocity_field_.assign(num_points, FlowField{});
    pressure_field_.assign(num_points, kAtmosphere);
    temperature_field_.assign(num_points, 293.15);  // 20 C
    turbulent_ke_field_.assign(num_points, 1e-6);
    turbulent_diss_field_.assign(num_points, 1e-6);

    for (auto& f : velocity_field_) {
        f.pressure = kAtmosphere;
        f.temperature = 293.15;
        f.turbulent_kinetic_energy = 1e-6;
        f.turbulent_dissipation = 1e-6;
    }

    for (const auto& bc : boundary_conditions_) {
        if (bc.type != BoundaryConditionType::DIRICHLET) continue;
        for (size_t idx : bc.face_indices) {
            if (idx >= num_points) continue;
            velocity_field_[idx].velocity_x = bc.velocity_x;
            velocity_field_[idx].velocity_y = bc.velocity_y;
            velocity_field_[idx].velocity_z = bc.velocity_z;
            if (bc.temperature > 0.0) {
                temperature_field_[idx] = bc.temperature;
                velocity_field_[idx].temperature = bc.temperature;
            }
        }
    }
}

double CFDSolver::computeResidual(const std::vector<FlowField>& previous) const {
    // Max change of velocity between Picard iterations, relative to max |u|
    double change = 0.0, scale = 0.0;
    for (size_t i = 0; i < velocity_field_.size(); ++i) {
        const auto& a = velocity_field_[i];
        const auto& b = previous[i];
        double dx = a.velocity_x - b.velocity_x, dy = a.velocity_y - b.velocity_y,
               dz = a.velocity_z - b.velocity_z;
        change = std::max(change, std::sqrt(dx * dx + dy * dy + dz * dz));
        scale = std::max(scale, std::sqrt(a.velocity_x * a.velocity_x +
                                          a.velocity_y * a.velocity_y +
                                          a.velocity_z * a.velocity_z));
    }
    return scale > 0.0 ? change / scale : change;
}

double CFDSolver::getMaxVelocity() const {
    double max_vel = 0.0;
    for (const auto& field : velocity_field_) {
        double vel = std::sqrt(
            field.velocity_x * field.velocity_x +
            field.velocity_y * field.velocity_y +
            field.velocity_z * field.velocity_z
        );
        max_vel = std::max(max_vel, vel);
    }
    return max_vel;
}

double CFDSolver::getMaxPressure() const {
    if (pressure_field_.empty()) return 0.0;
    return *std::max_element(pressure_field_.begin(), pressure_field_.end());
}

double CFDSolver::getMaxTemperature() const {
    if (temperature_field_.empty()) return 0.0;
    return *std::max_element(temperature_field_.begin(), temperature_field_.end());
}

double CFDSolver::getMinTemperature() const {
    if (temperature_field_.empty()) return 0.0;
    return *std::min_element(temperature_field_.begin(), temperature_field_.end());
}

}  // namespace BendCore::CadCore::Simulation
