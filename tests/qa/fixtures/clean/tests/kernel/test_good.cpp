// Фикстура самотеста check_no_stubs: тест сверяет результат с аналитическим эталоном.
#include <cassert>
#include <cmath>

static double box_volume(double a, double b, double c) { return a * b * c; }

struct FakeClock {
  double now(int) const { return 0.0; }   // test double: allowed in tests
  void reset() {}
};

int main() {
  const double v = box_volume(2.0, 3.0, 4.0);
  assert(std::abs(v - 24.0) < 1e-12);
  FakeClock clock;
  clock.reset();
  assert(clock.now(1) == 0.0);
  return 0;
}
