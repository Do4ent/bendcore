// Фикстура самотеста check_no_stubs: тестовый файл без единой проверки.
#include <cstdio>

static double compute(double x) { return x * 3.0; }

int main() {
  const double v = compute(4.0);
  std::printf("%f\n", v);
  return 0;
}
