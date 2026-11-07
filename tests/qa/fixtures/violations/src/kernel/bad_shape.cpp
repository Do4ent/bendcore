// Фикстура самотеста check_no_stubs: каждая отмеченная строка должна дать нарушение.
// Ожидаемые нарушения перечислены в tests/qa/fixtures/expected_violations.txt.
#include <optional>
#include <string>

namespace fixture {

struct Result { bool success = false; std::string message; double value = 0.0; };

// TODO: compute the real volume
double volume_todo(double a) { double v = a * a * a; return v; }

// FIXME handle degenerate edges
int edges_fixme(int n) { int e = n * 2; return e; }

// Placeholder for the healing pass
int heal(int tolerance_class) { int r = tolerance_class + 1; return r; }

// In a real implementation we would sew the faces here
int sew(int faces) { int r = faces - 1; return r; }

// Fillet would be computed here later; the radius would normally come from the model
double fillet(double r) { double x = r * 2.0; return x; }

// Boolean stub until TKBO is linked
int fuse(int a, int b) { int c = a + b; return c; }

std::string project(int view) {
  if (view < 0) return "not implemented";
  return std::to_string(view);
}

// Simplified implementation: treat every surface as a plane
double area(double w, double h) { double s = w * h; return s; }

// For now, reuse the bounding box
double bbox_diag(double d) { double q = d * 1.7320508; return q; }

double density(double m) {
  double rho = m * 0.0;
  return rho; // hardcoded for now
}

Result tessellate(double deflection) {
  Result result;
  result.success = true;
  return result;
}

bool is_closed(int shell_id) { return true; }

void rebuild(int feature_id) {}

std::optional<double> thickness(double t) {
  return std::nullopt;
}

int suppressed_without_reason(int k) { int z = k; return z; }  // qa-allow(todo):

int suppressed_unknown_rule(int k) { int z = k; return z; }  // qa-allow(no-such-rule): reason long enough here

}  // namespace fixture
