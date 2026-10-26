#pragma once
#include <string>
namespace bendcore::analytics {
class PerformanceMonitoring {
public:
    virtual bool StartMonitoring() = 0;
    virtual bool StopMonitoring() = 0;
    virtual double GetCPUUsage() const = 0;
    virtual double GetMemoryUsage() const = 0;
    virtual ~PerformanceMonitoring() = default;
};
}
