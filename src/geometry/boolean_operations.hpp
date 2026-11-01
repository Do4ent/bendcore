#pragma once

#include <vector>
#include "bendcore/geometry_kernel.hpp"

namespace bendcore::geometry {

enum class BooleanOperation {
    UNION,
    DIFFERENCE,
    INTERSECTION
};

class BooleanOperations {
public:
    struct Result {
        bool success = false;
        std::string error_message;
        Solid result_solid;
    };

    static Result boolean_operation(
        const Solid& solid_a,
        const Solid& solid_b,
        BooleanOperation op
    );

    static Result unite(const Solid& a, const Solid& b);
    static Result subtract(const Solid& a, const Solid& b);
    static Result intersect(const Solid& a, const Solid& b);

private:
    static std::vector<Face> compute_union_faces(
        const std::vector<Face>& faces_a,
        const std::vector<Face>& faces_b
    );
    static std::vector<Face> compute_difference_faces(
        const std::vector<Face>& faces_a,
        const std::vector<Face>& faces_b
    );
    static std::vector<Face> compute_intersection_faces(
        const std::vector<Face>& faces_a,
        const std::vector<Face>& faces_b
    );
};

}
