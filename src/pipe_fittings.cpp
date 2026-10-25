#include "bendcore/pipe_fittings.h"

namespace bendcore::pipe {

double PipeFitting::getRadius() const {
    return dimensions_.port1Diameter / 2.0;
}

double PipeFitting::getBendRadius() const {
    // For 90 degree elbow: typical bend radius is 1-1.5 times OD
    // For 45 degree: proportionally smaller
    if (type_ == FittingType::ELBOW_90) {
        return dimensions_.port1Diameter * 1.25;
    } else if (type_ == FittingType::ELBOW_45) {
        return dimensions_.port1Diameter * 0.65;
    }
    return 0.0;
}

std::string PipeFitting::getTypeString() const {
    switch (type_) {
        case FittingType::ELBOW_45: return "Elbow 45°";
        case FittingType::ELBOW_90: return "Elbow 90°";
        case FittingType::TEE: return "Tee";
        case FittingType::CROSS: return "Cross";
        case FittingType::REDUCER: return "Reducer";
        case FittingType::ADAPTER: return "Adapter";
        case FittingType::CAP: return "Cap";
        case FittingType::UNION: return "Union";
        case FittingType::SWIVEL: return "Swivel";
        case FittingType::BALL_VALVE: return "Ball Valve";
        default: return "Unknown";
    }
}

std::vector<PipeFitting> FittingLibrary::getStandardElbows() {
    std::vector<PipeFitting> fittings;

    // DIN/ISO 90 degree elbows
    fittings.push_back(PipeFitting(
        FittingType::ELBOW_90, "ISO-90-M6",
        {6.0, 6.0, std::nullopt, std::nullopt, 15.0, 90.0, "steel"},
        2.50
    ));
    fittings.push_back(PipeFitting(
        FittingType::ELBOW_90, "ISO-90-M10",
        {10.0, 10.0, std::nullopt, std::nullopt, 20.0, 90.0, "steel"},
        4.20
    ));
    fittings.push_back(PipeFitting(
        FittingType::ELBOW_90, "ISO-90-M16",
        {16.0, 16.0, std::nullopt, std::nullopt, 28.0, 90.0, "steel"},
        7.80
    ));

    // 45 degree elbows
    fittings.push_back(PipeFitting(
        FittingType::ELBOW_45, "ISO-45-M10",
        {10.0, 10.0, std::nullopt, std::nullopt, 18.0, 45.0, "steel"},
        3.50
    ));

    return fittings;
}

std::vector<PipeFitting> FittingLibrary::getStandardTees() {
    std::vector<PipeFitting> fittings;

    fittings.push_back(PipeFitting(
        FittingType::TEE, "ISO-TEE-M10",
        {10.0, 10.0, 10.0, std::nullopt, 25.0, 0.0, "steel"},
        6.50
    ));
    fittings.push_back(PipeFitting(
        FittingType::TEE, "ISO-TEE-M16",
        {16.0, 16.0, 16.0, std::nullopt, 35.0, 0.0, "steel"},
        12.30
    ));

    return fittings;
}

std::vector<PipeFitting> FittingLibrary::getStandardReducers() {
    std::vector<PipeFitting> fittings;

    fittings.push_back(PipeFitting(
        FittingType::REDUCER, "ISO-RED-10-6",
        {10.0, 6.0, std::nullopt, std::nullopt, 12.0, 0.0, "steel"},
        3.75
    ));
    fittings.push_back(PipeFitting(
        FittingType::REDUCER, "ISO-RED-16-10",
        {16.0, 10.0, std::nullopt, std::nullopt, 16.0, 0.0, "steel"},
        5.40
    ));

    return fittings;
}

std::vector<PipeFitting> FittingLibrary::getStandardAdapters() {
    std::vector<PipeFitting> fittings;

    fittings.push_back(PipeFitting(
        FittingType::ADAPTER, "ISO-ADAPTER-M10-SAE",
        {10.0, 9.525, std::nullopt, std::nullopt, 10.0, 0.0, "steel"},
        2.80
    ));

    return fittings;
}

std::optional<PipeFitting> FittingLibrary::findFitting(const std::string& designation) {
    auto elbows = getStandardElbows();
    auto tees = getStandardTees();
    auto reducers = getStandardReducers();
    auto adapters = getStandardAdapters();

    for (const auto& f : elbows) {
        if (f.getDesignation() == designation) return f;
    }
    for (const auto& f : tees) {
        if (f.getDesignation() == designation) return f;
    }
    for (const auto& f : reducers) {
        if (f.getDesignation() == designation) return f;
    }
    for (const auto& f : adapters) {
        if (f.getDesignation() == designation) return f;
    }

    return std::nullopt;
}

std::vector<PipeFitting> FittingLibrary::findFittingsByType(FittingType type) {
    std::vector<PipeFitting> result;
    auto all = getStandardElbows();
    auto tees = getStandardTees();
    auto reducers = getStandardReducers();
    auto adapters = getStandardAdapters();

    all.insert(all.end(), tees.begin(), tees.end());
    all.insert(all.end(), reducers.begin(), reducers.end());
    all.insert(all.end(), adapters.begin(), adapters.end());

    for (const auto& f : all) {
        if (f.getType() == type) result.push_back(f);
    }
    return result;
}

std::vector<PipeFitting> FittingLibrary::findFittingsByPort(double portDiameter) {
    std::vector<PipeFitting> result;
    auto all = getStandardElbows();
    auto tees = getStandardTees();
    auto reducers = getStandardReducers();
    auto adapters = getStandardAdapters();

    all.insert(all.end(), tees.begin(), tees.end());
    all.insert(all.end(), reducers.begin(), reducers.end());
    all.insert(all.end(), adapters.begin(), adapters.end());

    const double tolerance = 0.1; // mm tolerance
    for (const auto& f : all) {
        const auto& dims = f.getDimensions();
        if (std::abs(dims.port1Diameter - portDiameter) < tolerance ||
            std::abs(dims.port2Diameter - portDiameter) < tolerance) {
            result.push_back(f);
        }
    }
    return result;
}

} // namespace bendcore::pipe
