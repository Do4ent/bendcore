#pragma once
#include <string>
#include <vector>
namespace BendCore::CadCore {
class HelpSystem {
public:
    HelpSystem() : topic_count_(0) {}
    bool addTopic([[maybe_unused]] const std::string& title) noexcept { topic_count_++; return true; }
    [[nodiscard]] int getTopicCount() const noexcept { return topic_count_; }
private:
    int topic_count_;
};
}
