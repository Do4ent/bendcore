#pragma once

// Внутренние средства генератора редуктора: построение документа детали по шагам с проверкой
// каждой операции и поиск элементов тела по геометрии (положение, нормаль, радиус), а не по
// карте имён. Ошибки — исключение BuildError, которое внешние функции переводят в Result.

#include "gearbox.hpp"

#include <bendcore/modeling/part_features.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::examples::gearbox::detail {

struct BuildError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const std::string& message);

std::string num(double v);

// Документ детали с пошаговым построением: каждая операция добавляется и документ
// перестраивается; операция не в состоянии Ok/Warning — BuildError с её сообщением.
class PartBuilder {
public:
    explicit PartBuilder(std::string part);
    modeling::PartDocument& doc() { return *doc_; }
    std::unique_ptr<modeling::PartDocument> release() { return std::move(doc_); }

    void var(const std::string& name, double value);
    void var(const std::string& name, const std::string& expression);
    double value(const std::string& name) const;

    // Добавляет операцию, перестраивает, проверяет состояние; возвращает итоговое тело.
    const modeling::NamedShape& add(modeling::FeatureSpec spec);
    const modeling::NamedShape& shape() const { return shape_; }

    // Эскиз на базовой плоскости (XY, XZ, YZ) со смещением.
    void sketch(const std::string& id, const std::string& plane, const std::string& offset, modeling::SketchData data,
                bool open = false);
    // Эскиз на плоской грани; начало — проекция точки (ox, oy, oz).
    void faceSketch(const std::string& id, const TopoDS_Shape& face, const std::string& ox, const std::string& oy,
                    const std::string& oz, modeling::SketchData data);
    void extrude(const std::string& id, const std::string& sketch, std::map<std::string, std::string> options,
                 std::map<std::string, std::string> params);
    void revolve(const std::string& id, const std::string& sketch, const std::string& axis, const std::string& mode = "boss");
    void chamfer(const std::string& id, const std::vector<TopoDS_Shape>& edges, const std::string& distance);
    void fillet(const std::string& id, const std::vector<TopoDS_Shape>& edges, const std::string& radius);

    modeling::TopoReference ref(const TopoDS_Shape& element) const;

private:
    std::string part_;
    std::unique_ptr<modeling::PartDocument> doc_;
    modeling::NamedShape shape_;
};

// ---- зонды геометрии (точно один элемент, иначе BuildError с описанием)

// Плоская грань с наружной нормалью n в плоскости n·p = d; при заданной точке — содержащая её.
TopoDS_Shape planarFace(const TopoDS_Shape& shape, const gp_Dir& n, double d);
TopoDS_Shape planarFaceAt(const TopoDS_Shape& shape, const gp_Dir& n, const gp_Pnt& inside);
// Рёбра-окружности радиуса r с центром c (ось — любая).
std::vector<TopoDS_Shape> circleEdges(const TopoDS_Shape& shape, const gp_Pnt& c, double r);
TopoDS_Shape circleEdge(const TopoDS_Shape& shape, const gp_Pnt& c, double r);
// Прямолинейные рёбра между точками a и b (в любом порядке).
TopoDS_Shape lineEdge(const TopoDS_Shape& shape, const gp_Pnt& a, const gp_Pnt& b);
// Прямолинейные рёбра, параллельные dir и проходящие через точку p (любой длины).
std::vector<TopoDS_Shape> linesThrough(const TopoDS_Shape& shape, const gp_Pnt& p, const gp_Dir& dir);
// Цилиндрическая грань радиуса r на оси (p, dir); при нескольких — содержащая точку inside.
TopoDS_Shape cylinderFace(const TopoDS_Shape& shape, double r, const gp_Pnt& p, const gp_Dir& dir);
TopoDS_Shape cylinderFaceAt(const TopoDS_Shape& shape, double r, const gp_Pnt& p, const gp_Dir& dir, const gp_Pnt& inside);

// Данные эскиза.
modeling::SketchData polygon(const std::vector<std::pair<std::string, std::string>>& corners);
modeling::SketchData circles(const std::vector<std::tuple<std::string, std::string, std::string>>& c);
modeling::SketchData points(const std::vector<std::pair<std::string, std::string>>& p);
// Паз (шпоночный, исполнение 1): центр (xc, yc), длина l вдоль оси x, ширина b.
modeling::SketchData slot(const std::string& xc, const std::string& yc, const std::string& l, const std::string& b);
// Прямоугольник с центром (cx, cy) и сторонами a × b.
modeling::SketchData rect(const std::string& cx, const std::string& cy, const std::string& a, const std::string& b);
// Прямоугольная рамка: внешний прямоугольник (x0, y0)–(x1, y1) и внутренний (u0, v0)–(u1, v1).
modeling::SketchData frame(const std::string& x0, const std::string& y0, const std::string& x1, const std::string& y1,
                           const std::string& u0, const std::string& v0, const std::string& u1, const std::string& v1);

double volumeOf(const TopoDS_Shape& shape);

template <class F>
auto guard(F&& f) -> decltype(f()) {
    try {
        return f();
    } catch (const BuildError& e) {
        return Error{ErrorCode::OperationFailed, e.what()};
    }
}

}  // namespace bendcore::examples::gearbox::detail
