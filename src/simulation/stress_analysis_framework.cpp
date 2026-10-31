#include "bendcore/simulation/stress_analysis_framework.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace bendcore::simulation {

StressAnalysisResult StressAnalysisFramework::analyzeLoadCase(const LoadCase& load) noexcept {
    StressAnalysisResult result;

    // Calculate component stresses
    double axial_stress = calculateAxialStress(load.axial_force);
    double bending_stress_y = calculateBendingStress(load.bending_moment_y);
    double bending_stress_z = calculateBendingStress(load.bending_moment_z);
    double shear_stress_y = calculateShearStress(load.shear_force_y);
    double shear_stress_z = calculateShearStress(load.shear_force_z);

    // Build stress tensor (simplified 2D analysis for now)
    result.stress_tensor.set_xx(axial_stress + bending_stress_y);
    result.stress_tensor.set_yy(bending_stress_z);
    result.stress_tensor.set_zz(0.0);
    result.stress_tensor.set_xy(shear_stress_y);
    result.stress_tensor.set_yz(shear_stress_z);
    result.stress_tensor.set_xz(0.0);

    // Calculate equivalent stress
    result.von_mises_equivalent = result.stress_tensor.vonMisesStress();
    result.max_principal_stress = result.stress_tensor.maxPrincipalStress();

    // Calculate deformation
    result.deformation = calculateDeformation(load);

    // Calculate strain energy
    result.strain_energy = calculateStrainEnergy(result.stress_tensor);

    // Calculate safety factor
    result.safety_factor = calculateSafetyFactor(result.stress_tensor);

    // Determine safety
    result.is_safe = result.safety_factor > 1.5;

    return result;
}

std::vector<StressAnalysisResult> StressAnalysisFramework::analyzeMultipleLoadCases(
    const std::vector<LoadCase>& loads) noexcept {
    std::vector<StressAnalysisResult> results;
    results.reserve(loads.size());

    for (const auto& load : loads) {
        results.push_back(analyzeLoadCase(load));
    }

    return results;
}

double StressAnalysisFramework::calculateDeformation(const LoadCase& load) const noexcept {
    // δ = FL / (EA) for axial deformation
    double e = material_.young_modulus;
    if (e < 1e-12) return 0.0;

    double axial_deform = (load.axial_force * geometry_.length) / (e * geometry_.cross_section_area);

    // Bending deflection δ = ML² / (3EI)
    double max_moment = std::max(std::abs(load.bending_moment_y), std::abs(load.bending_moment_z));
    double bending_deform = (max_moment * geometry_.length * geometry_.length) / (3.0 * e * geometry_.moment_of_inertia);

    return std::sqrt(axial_deform * axial_deform + bending_deform * bending_deform);
}

double StressAnalysisFramework::calculateStrainEnergy(const StressTensor& stress) const noexcept {
    // U = (1/2) * σ:ε * Volume
    // For linear elastic: σ:ε = σ² / E (simplified)

    double e = material_.young_modulus;
    double volume = geometry_.cross_section_area * geometry_.length;

    if (e < 1e-12 || volume < 1e-12) return 0.0;

    // Von Mises stress energy
    double vm = stress.vonMisesStress();
    double strain_energy = (vm * vm / (2.0 * e)) * volume;

    return strain_energy;
}

}  // namespace bendcore::simulation
