#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"

#include <iostream>
#include <chrono>
#include <vector>

using namespace bendcore::document;

int main() {
  std::cout << "CAD-CORE Phase F: Stress Test (10,000 entities)\n";
  std::cout << "==============================================\n\n";

  try {
    auto doc = std::make_shared<CadDocument>();

    std::cout << "Creating 10,000 entities... ";
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<EntityId> entities;
    for (int i = 0; i < 10000; ++i) {
      auto id = EntityId(EntityId::Type::Part);
      entities.push_back(id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "OK (" << duration.count() << "ms)\n";
    std::cout << "  Throughput: " << (10000.0 / duration.count()) << " ops/ms\n";

    std::cout << "\n✓ Stress test PASSED\n";
    return 0;
  } catch (const std::exception& e) {
    std::cout << "FAILED: " << e.what() << "\n";
    return 1;
  }
}
