#pragma once

// Private helpers of bendcore_modeling_sketch: conversion between Curve2d and Geom2d curves,
// OCCT exception text.

#include <bendcore/modeling/sketch_profile.hpp>

#include <string>

#include <Geom2d_Curve.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pnt2d.hxx>

namespace bendcore::modeling::detail {

inline std::string failureText(const Standard_Failure& failure) {
    std::string text = failure.ExceptionType();
    const char* message = failure.what();
    if (message != nullptr && *message != '\0') {
        text += ": ";
        text += message;
    }
    return text;
}

inline gp_Pnt2d toPnt2d(const Point2& p) { return gp_Pnt2d(p.x, p.y); }
inline Point2 toPoint2(const gp_Pnt2d& p) { return Point2{p.X(), p.Y()}; }

std::string formatPoint(const Point2& p);

// A Curve2d as an OCCT curve on its parameter range [first, last].
struct Geom2dSegment {
    occ::handle<Geom2d_Curve> basis;  // untrimmed curve
    double first = 0.0;
    double last = 0.0;
    bool closed = false;  // start and end coincide (full circle, ellipse, closed B-spline)
};

// Error (InvalidInput) describes why the curve is degenerate.
Result<Geom2dSegment> toGeom2d(const Curve2d& curve, double tolerance);

// Inverse conversion of a curve piece; lines, circles and ellipses keep their analytic kind,
// everything else becomes a B-spline. Circles are recognised among ellipses with equal radii.
Result<Curve2d> fromGeom2d(const occ::handle<Geom2d_Curve>& curve, double first, double last,
                           double tolerance);

}  // namespace bendcore::modeling::detail
