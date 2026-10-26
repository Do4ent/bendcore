#pragma once
#include <string>
#include <functional>
namespace bendcore::cloud {
class CloudComputing {
public:
    virtual bool SubmitJob(const std::string& code) = 0;
    virtual bool WaitForResult(const std::string& job_id) = 0;
    virtual std::string GetResult(const std::string& job_id) = 0;
    virtual ~CloudComputing() = default;
};
}
