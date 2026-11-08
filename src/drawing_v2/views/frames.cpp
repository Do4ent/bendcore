// Ориентация видов: ГОСТ 2.305-2008 (основные, проекционные, дополнительные виды) и
// ГОСТ 2.317-2011 (прямоугольная изометрия и диметрия).

#include <bendcore/drawing_v2/views.hpp>

#include <cmath>

namespace bendcore::drawing_v2::views {

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(double k, Vec3 a) { return {k * a.x, k * a.y, k * a.z}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(Vec3 a) { return std::sqrt(dot(a, a)); }

namespace {
Vec3 unit(Vec3 a) { return (1.0 / length(a)) * a; }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
}  // namespace

Point2 ViewFrame::project(Vec3 p) const { return {dot(p, right), dot(p, up())}; }

void Box2::add(Point2 p) {
    if (empty) {
        minX = maxX = p.x;
        minY = maxY = p.y;
        empty = false;
        return;
    }
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
}

Result<ViewFrame> makeFrame(Vec3 toward, Vec3 right) {
    if (!finite(toward) || !finite(right) || length(toward) < 1e-12 || length(right) < 1e-12)
        return Result<ViewFrame>::failure(ErrorCode::InvalidInput,
                                          "view frame: zero or non-finite direction");
    Vec3 n = unit(toward);
    Vec3 r = right - dot(right, n) * n;
    if (length(r) < 1e-9 * length(right))
        return Result<ViewFrame>::failure(ErrorCode::InvalidInput,
                                          "view frame: right is parallel to the view direction");
    ViewFrame f;
    f.toward = n;
    f.right = unit(r);
    return f;
}

ViewFrame defaultFrontFrame() { return ViewFrame{}; }

ViewFrame standardFrame(const ViewFrame& main, StandardView view) {
    const Vec3 n = main.toward, r = main.right, u = main.up();
    switch (view) {
        case StandardView::Front: return main;
        case StandardView::Top: return ViewFrame{u, r};            // наблюдатель сверху
        case StandardView::Bottom: return ViewFrame{-1.0 * u, r};  // снизу
        case StandardView::Left: return ViewFrame{-1.0 * r, n};    // слева
        case StandardView::Right: return ViewFrame{r, -1.0 * n};   // справа
        case StandardView::Rear: return ViewFrame{-1.0 * n, -1.0 * r};
    }
    return main;
}

std::optional<SheetSide> standardPlacement(StandardView view, ProjectionMethod method) {
    const bool first = method == ProjectionMethod::FirstAngle;
    switch (view) {
        case StandardView::Front: return std::nullopt;
        case StandardView::Top: return first ? SheetSide::Below : SheetSide::Above;
        case StandardView::Bottom: return first ? SheetSide::Above : SheetSide::Below;
        case StandardView::Left: return first ? SheetSide::RightOf : SheetSide::LeftOf;
        case StandardView::Right: return first ? SheetSide::LeftOf : SheetSide::RightOf;
        case StandardView::Rear: return SheetSide::RightOf;
    }
    return std::nullopt;
}

ViewFrame projectedFrame(const ViewFrame& parent, SheetSide side, ProjectionMethod method) {
    // Метод первого угла: вид ставится с противоположной стороны от наблюдателя
    // (вид сверху — под главным); третьего — с той же стороны.
    StandardView v = StandardView::Top;
    const bool first = method == ProjectionMethod::FirstAngle;
    switch (side) {
        case SheetSide::Below: v = first ? StandardView::Top : StandardView::Bottom; break;
        case SheetSide::Above: v = first ? StandardView::Bottom : StandardView::Top; break;
        case SheetSide::RightOf: v = first ? StandardView::Left : StandardView::Right; break;
        case SheetSide::LeftOf: v = first ? StandardView::Right : StandardView::Left; break;
    }
    return standardFrame(parent, v);
}

Result<ViewFrame> auxiliaryFrame(Vec3 sight, Vec3 upHint) {
    if (!finite(sight) || !finite(upHint) || length(sight) < 1e-12 || length(upHint) < 1e-12)
        return Result<ViewFrame>::failure(ErrorCode::InvalidInput,
                                          "auxiliary view: zero or non-finite direction");
    Vec3 n = -1.0 * unit(sight);
    Vec3 up = upHint - dot(upHint, n) * n;
    if (length(up) < 1e-9 * length(upHint))
        return Result<ViewFrame>::failure(ErrorCode::InvalidInput,
                                          "auxiliary view: up hint is parallel to the sight");
    up = unit(up);
    return ViewFrame{n, cross(up, n)};
}

ViewFrame axonometricFrame(const ViewFrame& main, Axonometry type, AxonometricCorner corner) {
    const double s = corner == AxonometricCorner::FrontRightTop ? 1.0 : -1.0;
    const Vec3 n = main.toward, r = main.right, u = main.up();
    Vec3 toward;
    if (type == Axonometry::Isometric) {
        toward = (1.0 / std::sqrt(3.0)) * (s * r + n + u);
    } else {
        // Диметрия: направляющие косинусы 1/3 (x, z) и √7/3 (y — глубина).
        toward = (1.0 / 3.0) * (s * r + std::sqrt(7.0) * n + u);
    }
    toward = unit(toward);
    Vec3 up = unit(u - dot(u, toward) * toward);  // ось z — вертикально (ГОСТ 2.317, рис. 1, 3)
    return ViewFrame{toward, cross(up, toward)};
}

double axonometricFactor(Axonometry type, bool reduced) {
    if (!reduced) return 1.0;
    return type == Axonometry::Isometric ? std::sqrt(1.5) : 3.0 / (2.0 * std::sqrt(2.0));
}

}  // namespace bendcore::drawing_v2::views
