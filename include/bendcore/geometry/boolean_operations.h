#pragma once

#include <memory>
#include <vector>

namespace bendcore {

class BooleanOperations {
public:
    enum class Operation {
        Union,
        Difference,
        Intersection
    };

    // Placeholder for future implementation
    static void executeOperation(Operation op);
};

} // namespace bendcore
