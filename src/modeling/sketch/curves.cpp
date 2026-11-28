#include "sketch_internal.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>

#include <ElCLib.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2dConvert.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <NCollection_Array1.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Circ2d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Elips2d.hxx>

namespace bendcore::modeling::detail {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kAngularEps = 1e-12;

Result<Geom2dSegment> degenerate(const std::string& text) {
    return Result<Geom2dSegment>::failure(ErrorCode::InvalidInput, text);
}

double distance(const Point2& a, const Point2& b) { return std::hypot(b.x - a.x, b.y - a.y); }

// Parameter of p on the ellipse/circle measured in the direction of increasing parameter,
// normalised to [base, base + 2*pi).
double normaliseAfter(double value, double base) {
    double v = value;
    while (v < base) v += kTwoPi;
    while (v >= base + kTwoPi) v -= kTwoPi;
    return v;
}

bool isDirect(const gp_Ax22d& axes) {
    const gp_Dir2d& x = axes.XDirection();
    const gp_Dir2d& y = axes.YDirection();
    return x.X() * y.Y() - x.Y() * y.X() > 0.0;
}

// Ellipse frame with a >= b; `shift` is added to parameters given for the input frame.
struct EllipseFrame {
    gp_Ax2d axis;
    double major = 0.0;
    double minor = 0.0;
    double shift = 0.0;
};

Result<EllipseFrame> ellipseFrame(const Ellipse2d& e, double tolerance) {
    const double len = std::hypot(e.majorDirection.x, e.majorDirection.y);
    if (!(len > kAngularEps)) {
        return Result<EllipseFrame>::failure(ErrorCode::InvalidInput,
                                             "ellipse major direction is a zero vector");
    }
    if (!(e.majorRadius > tolerance) || !(e.minorRadius > tolerance)) {
        return Result<EllipseFrame>::failure(ErrorCode::InvalidInput,
                                             "ellipse radius is not greater than the tolerance");
    }
    EllipseFrame frame;
    const gp_Dir2d u(e.majorDirection.x / len, e.majorDirection.y / len);
    if (e.majorRadius >= e.minorRadius) {
        frame.axis = gp_Ax2d(toPnt2d(e.center), u);
        frame.major = e.majorRadius;
        frame.minor = e.minorRadius;
    } else {
        // U' = V, V' = -U, t' = t - pi/2 keeps c + a cos t U + b sin t V.
        frame.axis = gp_Ax2d(toPnt2d(e.center), gp_Dir2d(-u.Y(), u.X()));
        frame.major = e.minorRadius;
        frame.minor = e.majorRadius;
        frame.shift = -std::numbers::pi / 2.0;
    }
    return frame;
}

}  // namespace

std::string formatPoint(const Point2& p) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "(%.6g, %.6g)", p.x, p.y);
    return buffer;
}

Result<Geom2dSegment> toGeom2d(const Curve2d& curve, double tolerance) {
    try {
        if (const auto* line = std::get_if<LineSegment2d>(&curve)) {
            const double length = distance(line->start, line->end);
            if (!(length > tolerance)) {
                return degenerate("line segment of zero length at " + formatPoint(line->start));
            }
            const gp_Dir2d dir((line->end.x - line->start.x) / length,
                               (line->end.y - line->start.y) / length);
            return Geom2dSegment{new Geom2d_Line(toPnt2d(line->start), dir), 0.0, length, false};
        }
        if (const auto* arc = std::get_if<CircularArc2d>(&curve)) {
            const double radius = distance(arc->center, arc->start);
            if (!(radius > tolerance)) {
                return degenerate("arc of zero radius at " + formatPoint(arc->center));
            }
            const double endRadius = distance(arc->center, arc->end);
            if (std::abs(endRadius - radius) > tolerance) {
                char buffer[160];
                std::snprintf(buffer, sizeof(buffer),
                              "arc end point is %.3g mm off its circle (radius %.6g)",
                              std::abs(endRadius - radius), radius);
                return degenerate(buffer);
            }
            if (!(distance(arc->start, arc->end) > tolerance)) {
                return degenerate("arc start and end coincide at " + formatPoint(arc->start) +
                                  "; use a circle for a full turn");
            }
            const gp_Circ2d circ(gp_Ax2d(toPnt2d(arc->center), gp_Dir2d(1.0, 0.0)), radius,
                                 arc->counterClockwise);
            const double first = ElCLib::Parameter(circ, toPnt2d(arc->start));
            const double last = normaliseAfter(ElCLib::Parameter(circ, toPnt2d(arc->end)), first);
            return Geom2dSegment{new Geom2d_Circle(circ), first, last, false};
        }
        if (const auto* circle = std::get_if<Circle2d>(&curve)) {
            if (!(circle->radius > tolerance)) {
                return degenerate("circle of zero radius at " + formatPoint(circle->center));
            }
            const gp_Circ2d circ(gp_Ax2d(toPnt2d(circle->center), gp_Dir2d(1.0, 0.0)),
                                 circle->radius, true);
            return Geom2dSegment{new Geom2d_Circle(circ), 0.0, kTwoPi, true};
        }
        if (const auto* ellipse = std::get_if<Ellipse2d>(&curve)) {
            auto frame = ellipseFrame(*ellipse, tolerance);
            if (!frame.ok()) return frame.error();
            const auto& f = frame.value();
            return Geom2dSegment{new Geom2d_Ellipse(f.axis, f.major, f.minor, true), 0.0, kTwoPi,
                                 true};
        }
        if (const auto* arc = std::get_if<EllipticalArc2d>(&curve)) {
            auto frame = ellipseFrame(arc->ellipse, tolerance);
            if (!frame.ok()) return frame.error();
            const auto& f = frame.value();
            const double span = arc->endParameter - arc->startParameter;
            if (!(span > kAngularEps) || span > kTwoPi + kAngularEps) {
                return degenerate("elliptical arc parameter span must be in (0, 2*pi]");
            }
            occ::handle<Geom2d_Ellipse> basis = new Geom2d_Ellipse(f.axis, f.major, f.minor, true);
            if (span >= kTwoPi - kAngularEps) {
                return Geom2dSegment{basis, 0.0, kTwoPi, true};
            }
            const double first = arc->startParameter + f.shift;
            const double chord = basis->Value(first).Distance(basis->Value(first + span));
            if (!(chord > tolerance)) {
                return degenerate("elliptical arc start and end coincide");
            }
            return Geom2dSegment{basis, first, first + span, false};
        }
        const auto& spline = std::get<BSpline2d>(curve);
        if (spline.poles.size() < 2 || spline.knots.size() < 2 ||
            spline.knots.size() != spline.multiplicities.size() ||
            (!spline.weights.empty() && spline.weights.size() != spline.poles.size())) {
            return degenerate("B-spline needs >= 2 poles, >= 2 knots, one multiplicity per knot "
                              "and one weight per pole (or none)");
        }
        NCollection_Array1<gp_Pnt2d> poles(1, static_cast<int>(spline.poles.size()));
        for (std::size_t i = 0; i < spline.poles.size(); ++i) {
            poles.SetValue(static_cast<int>(i) + 1, toPnt2d(spline.poles[i]));
        }
        NCollection_Array1<double> knots(1, static_cast<int>(spline.knots.size()));
        NCollection_Array1<int> mults(1, static_cast<int>(spline.knots.size()));
        for (std::size_t i = 0; i < spline.knots.size(); ++i) {
            knots.SetValue(static_cast<int>(i) + 1, spline.knots[i]);
            mults.SetValue(static_cast<int>(i) + 1, spline.multiplicities[i]);
        }
        occ::handle<Geom2d_BSplineCurve> bspline;
        if (spline.weights.empty()) {
            bspline = new Geom2d_BSplineCurve(poles, knots, mults, spline.degree, spline.periodic);
        } else {
            NCollection_Array1<double> weights(1, static_cast<int>(spline.weights.size()));
            for (std::size_t i = 0; i < spline.weights.size(); ++i) {
                weights.SetValue(static_cast<int>(i) + 1, spline.weights[i]);
            }
            bspline = new Geom2d_BSplineCurve(poles, weights, knots, mults, spline.degree,
                                              spline.periodic);
        }
        const double first = bspline->FirstParameter();
        const double last = bspline->LastParameter();
        const bool closed =
            spline.periodic || bspline->Value(first).Distance(bspline->Value(last)) <= tolerance;
        // Chord check for open splines: start and end must differ (else closed), and the curve
        // must have extent.
        double extent = 0.0;
        const gp_Pnt2d p0 = bspline->Value(first);
        for (int i = 1; i <= 32; ++i) {
            extent = std::max(extent, p0.Distance(bspline->Value(first + (last - first) * i / 32.0)));
        }
        if (!(extent > tolerance)) {
            return degenerate("B-spline collapses to a point");
        }
        return Geom2dSegment{bspline, first, last, closed};
    } catch (const Standard_Failure& failure) {
        return degenerate("invalid curve data: " + failureText(failure));
    }
}

Result<Curve2d> fromGeom2d(const occ::handle<Geom2d_Curve>& curve, double first, double last,
                           double tolerance) {
    try {
        occ::handle<Geom2d_Curve> basis = curve;
        while (auto trimmed = occ::down_cast<Geom2d_TrimmedCurve>(basis)) {
            basis = trimmed->BasisCurve();
        }
        const Geom2dAdaptor_Curve adaptor(basis);
        const bool full = basis->IsPeriodic() && (last - first) >= kTwoPi - 1e-9;
        const Point2 start = toPoint2(basis->Value(first));
        const Point2 end = toPoint2(basis->Value(last));
        switch (adaptor.GetType()) {
            case GeomAbs_Line:
                return Curve2d{LineSegment2d{start, end}};
            case GeomAbs_Circle: {
                const gp_Circ2d circ = adaptor.Circle();
                const Point2 center = toPoint2(circ.Location());
                if (full) return Curve2d{Circle2d{center, circ.Radius()}};
                return Curve2d{CircularArc2d{center, start, end, isDirect(circ.Axis())}};
            }
            case GeomAbs_Ellipse: {
                const gp_Elips2d elips = adaptor.Ellipse();
                const Point2 center = toPoint2(elips.Location());
                const bool direct = isDirect(elips.Axis());
                if (elips.MajorRadius() - elips.MinorRadius() <= tolerance) {
                    const double radius = 0.5 * (elips.MajorRadius() + elips.MinorRadius());
                    if (full) return Curve2d{Circle2d{center, radius}};
                    return Curve2d{CircularArc2d{center, start, end, direct}};
                }
                const gp_Dir2d u = elips.Axis().XDirection();
                Ellipse2d e{center, Point2{u.X(), u.Y()}, elips.MajorRadius(), elips.MinorRadius()};
                if (full) return Curve2d{e};
                // Indirect frame (U, -V): parameter t there is -t in the direct frame (U, V).
                double a = direct ? first : -last;
                double b = direct ? last : -first;
                const double normalised = normaliseAfter(a, 0.0);
                b += normalised - a;
                a = normalised;
                return Curve2d{EllipticalArc2d{e, a, b}};
            }
            default:
                break;
        }
        occ::handle<Geom2d_BSplineCurve> spline = Geom2dConvert::CurveToBSplineCurve(
            new Geom2d_TrimmedCurve(basis, first, last));
        BSpline2d out;
        out.degree = spline->Degree();
        out.periodic = spline->IsPeriodic();
        for (int i = 1; i <= spline->NbPoles(); ++i) {
            out.poles.push_back(toPoint2(spline->Pole(i)));
            if (spline->IsRational()) out.weights.push_back(spline->Weight(i));
        }
        for (int i = 1; i <= spline->NbKnots(); ++i) {
            out.knots.push_back(spline->Knot(i));
            out.multiplicities.push_back(spline->Multiplicity(i));
        }
        return Curve2d{out};
    } catch (const Standard_Failure& failure) {
        return Result<Curve2d>::failure(ErrorCode::OperationFailed,
                                        "curve conversion failed: " + failureText(failure));
    }
}

}  // namespace bendcore::modeling::detail
