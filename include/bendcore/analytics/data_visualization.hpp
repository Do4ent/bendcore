#pragma once
#include <string>
namespace bendcore::analytics {
class DataVisualization {
public:
    virtual std::string GenerateChart(const std::string& data) = 0;
    virtual bool SaveChart(const std::string& filename) = 0;
    virtual ~DataVisualization() = default;
};
}
