#pragma once

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cmath>

namespace bendcore::pipe {

enum class FittingType {
    ELBOW_45,
    ELBOW_90,
    TEE,
    CROSS,
    REDUCER,
    ADAPTER,
    CAP,
    UNION,
    SWIVEL,
    BALL_VALVE
};

struct FittingDimension {
    double port1Diameter;      // mm
    double port2Diameter;      // mm
    std::optional<double> port3Diameter;  // for T, cross fittings
    std::optional<double> port4Diameter;  // for cross fittings
    double length;             // mm
    double angle;              // degrees (for elbows)
    std::string material;      // steel, aluminum, brass, etc.
};

class PipeFitting {
public:
    PipeFitting(FittingType type, const std::string& designation,
                const FittingDimension& dims, double cost = 0.0)
        : type_(type), designation_(designation), dimensions_(dims), cost_(cost) {}

    FittingType getType() const { return type_; }
    const std::string& getDesignation() const { return designation_; }
    const FittingDimension& getDimensions() const { return dimensions_; }
    double getCost() const { return cost_; }

    double getRadius() const;
    double getBendRadius() const;
    std::string getTypeString() const;

private:
    FittingType type_;
    std::string designation_;
    FittingDimension dimensions_;
    double cost_;
};

class FittingLibrary {
public:
    static std::vector<PipeFitting> getStandardElbows();
    static std::vector<PipeFitting> getStandardTees();
    static std::vector<PipeFitting> getStandardReducers();
    static std::vector<PipeFitting> getStandardAdapters();
    static std::optional<PipeFitting> findFitting(const std::string& designation);
    static std::vector<PipeFitting> findFittingsByType(FittingType type);
    static std::vector<PipeFitting> findFittingsByPort(double portDiameter);
};

} // namespace bendcore::pipe
