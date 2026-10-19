// 2D-эскиз чертежа (7.9): точки, отрезки, окружности и ограничения поверх PlaneGCS (LGPL-2.1-or-later).
// Эскиз не хранит решатель между вызовами: каждый solve() строит систему PlaneGCS заново по текущему состоянию.
// Номер ограничения — его tag в диагностике (конфликты, избыточность).
#pragma once

#include <string>
#include <vector>

namespace bendcore::sketch2d {

using Id = int;   // номер точки, отрезка, окружности или ограничения (≥ 1 для ограничений)

struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

struct Report {
    std::string status;              // определено | недоопределено | избыточно | конфликт
    int dof = 0;                     // степени свободы (по диагностике PlaneGCS)
    std::vector<Id> conflicting;     // ограничения, входящие в конфликт
    std::vector<Id> redundant;       // избыточные ограничения
};

class Sketch {
public:
    Id add_point(double x, double y);
    void fix_point(Id p);            // закрепить текущие координаты точки
    Id add_line(Id a, Id b);         // отрезок между точками
    Id add_circle(Id center, double radius);

    Id constrain_coincident(Id a, Id b);
    Id constrain_horizontal(Id a, Id b);
    Id constrain_vertical(Id a, Id b);
    Id constrain_distance(Id a, Id b, double d);
    Id constrain_radius(Id circle, double r);
    Id constrain_parallel(Id l1, Id l2);             // отрезки ∥
    Id constrain_perpendicular(Id l1, Id l2);        // отрезки ⊥
    Id constrain_equal_length(Id l1, Id l2);         // равенство длин отрезков
    Id constrain_angle(Id l1, Id l2, double deg);    // угол между отрезками, градусы
    Id constrain_point_on_line(Id p, Id line);
    Id constrain_point_on_circle(Id p, Id circle);
    Id constrain_equal_radius(Id c1, Id c2);
    Id constrain_symmetric(Id p1, Id p2, Id axis);   // p1 и p2 симметричны относительно отрезка axis
    void remove_constraint(Id c);

    Report solve();                  // при успехе координаты обновляются
    Point2 point(Id p) const;
    double circle_radius(Id c) const;

private:
    struct Constraint {
        std::string kind;
        Id a = 0;
        Id b = 0;
        Id c = -1;                // третий объект (ось симметрии), если нужен
        double value = 0.0;
        bool removed = false;
    };
    struct Circle {
        Id center;
        double radius;
    };
    struct Line {
        Id a;
        Id b;
    };

    std::vector<Point2> points_;
    std::vector<bool> fixed_;
    std::vector<Point2> pins_;       // координаты закреплённых точек
    std::vector<Line> lines_;
    std::vector<Circle> circles_;
    std::vector<Constraint> constraints_;   // номер ограничения = индекс + 1

    Id add_constraint(const std::string& kind, Id a, Id b, double value, Id c = -1);
};

}  // namespace bendcore::sketch2d
