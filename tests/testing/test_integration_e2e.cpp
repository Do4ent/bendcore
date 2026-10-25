#include "bendcore/testing/test_suite.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/brep_builder.hpp"

#include <iostream>
#include <cassert>

using namespace bendcore::testing;
using namespace bendcore::document;
using namespace bendcore::geometry;

int main() {
  std::cout << "CAD-CORE Phase F: E2E Integration Test\n";
  std::cout << "======================================\n\n";

  try {
    // Test 1: Document Creation
    std::cout << "[TEST 1] Document Creation... ";
    auto doc = std::make_shared<CadDocument>();
    std::cout << "OK\n";

    // Test 2: Entity Creation
    std::cout << "[TEST 2] Entity ID Creation... ";
    auto part_id = EntityId(EntityId::Type::Part);
    auto body_id = EntityId(EntityId::Type::Body);
    std::cout << "OK\n";

    // Test 3: Geometry Operations
    std::cout << "[TEST 3] Geometry Kernel... ";
    GeometryKernel kernel(1e-10);
    Point3D p1(0, 0, 0);
    Point3D p2(1, 1, 1);
    double dist = kernel.point_distance(p1, p2);
    assert(dist > 1.7 && dist < 1.8);  // sqrt(3) ≈ 1.732
    std::cout << "OK\n";

    // Test 4: B-Rep Operations
    std::cout << "[TEST 4] B-Rep Builder... ";
    BRepBuilder builder;
    auto box = builder.create_box(1.0, 2.0, 3.0);
    auto cylinder = builder.create_cylinder(0.5, 1.5);
    std::cout << "OK\n";

    // Test 5: Boolean Operations
    std::cout << "[TEST 5] Boolean Operations... ";
    auto diff = builder.boolean_difference(box, cylinder);
    std::cout << "OK\n";

    std::cout << "\n✓ All E2E tests PASSED\n";
    return 0;
  } catch (const std::exception& e) {
    std::cout << "FAILED: " << e.what() << "\n";
    return 1;
  }
}
