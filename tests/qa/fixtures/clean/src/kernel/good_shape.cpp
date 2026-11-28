// Фикстура самотеста check_no_stubs: законные конструкции, которые НЕ должны давать нарушений.
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace fixture {

struct Result {
  bool success = false;
  bool valid = false;
  std::string message;
  double value = 0.0;
  static Result success_result(double v);
};

class Healer {
public:
  Healer() {}
  explicit Healer(double tol) : tol_(tol), passes_{} {}
  ~Healer() {}
  Healer(const Healer&) = default;
  virtual void on_progress(int) = 0;

private:
  double tol_ = 1e-7;
  std::vector<int> passes_;
};

// Factory of a successful result: sets the flag together with the computed value.
Result Result::success_result(double v) {
  Result r;
  r.success = true;
  r.value = v;
  return r;
}

// Validity starts as true and every failed check clears it.
Result validate(const std::vector<double>& lengths) {
  Result result;
  result.valid = true;
  for (double l : lengths) {
    if (l <= 0.0) {
      result.valid = false;
      result.message = "non-positive edge length";
    }
  }
  return result;
}

// Otherwise the face normal would be inverted after the mirror transform.
double mirrored_sign(double s) { return -s; }

// Simplify the wire before offsetting: UnifySameDomain merges coplanar faces.
int merge_count(int faces) { return faces > 1 ? faces - 1 : 0; }

// The stubborn tolerance problem is handled by the healing pass below.
double heal(double tol) { return std::max(tol * 10.0, 1e-6); }

int bind_example() {
  using namespace std::placeholders;
  auto add = std::bind(std::plus<int>(), _1, 2);
  auto noop = [](int) {};
  noop(0);
  return add(3);
}

std::vector<std::string> todo_list_names(int n) {
  std::vector<std::string> todo_list;
  for (int i = 0; i < n; ++i) todo_list.push_back("item" + std::to_string(i));
  return todo_list;
}

int legacy_bridge(int code) {  // qa-allow(literal-return): documented boundary, kept for ABI
  return code == 0 ? 1 : code;
}

// qa-allow(stub): word refers to the STEP header record "stub file" name in spec
const char* kHeaderNote = "stub file entity";

}  // namespace fixture
