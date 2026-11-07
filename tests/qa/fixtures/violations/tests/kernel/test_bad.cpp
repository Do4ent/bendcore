// Фикстура самотеста check_no_stubs: тест, проверяющий только «не упало».
#include <cassert>
#include <gtest/gtest.h>

double compute(double x) { return x * 2.0; }

TEST(Fixture, TrivialAssertions) {
  compute(1.0);
  EXPECT_TRUE(true);
  double v = compute(2.0);
  EXPECT_DOUBLE_EQ(v, v);
  assert(true);
  SUCCEED();
}
