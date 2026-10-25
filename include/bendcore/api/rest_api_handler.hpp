#pragma once
#include <string>
namespace BendCore::CadCore {
class RESTAPIHandler {
public:
    RESTAPIHandler() : request_count_(0) {}
    bool handleRequest([[maybe_unused]] const std::string& endpoint) noexcept { request_count_++; return true; }
    [[nodiscard]] int getRequestCount() const noexcept { return request_count_; }
private:
    int request_count_;
};
}
