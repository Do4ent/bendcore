#pragma once
#include <string>
namespace bendcore::analytics {
class ReportGenerator {
public:
    virtual std::string GenerateReport() = 0;
    virtual bool ExportReport(const std::string& path) = 0;
    virtual ~ReportGenerator() = default;
};
}
