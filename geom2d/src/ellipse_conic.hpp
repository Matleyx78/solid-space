// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// An ellipse meeting a circle or another ellipse (ADR 0017, "Che cosa rende
// possibili decisioni esatte"). Internal to geom2d: nothing here is part of the
// public contract. Stage 0.3, package PS7; implemented in
// ellipse_intersection.cpp, next to the two intersect() of ellipse.hpp.
//
// The rational form. The point of parameter s of the unit circle is
//     w(s) / (1 + s^2), with w(s) = (1 - s^2, 2 s),
// so the point of the ellipse is P(s) = (x(s), y(s)) / weight(s), with
// x = center.x * weight + major.x * (1 - s^2) + minor.x * 2 s (y likewise)
// and weight = 1 + s^2: polynomials of degree two with dyadic coefficients,
// exact. s runs over the whole circle but one point: as s goes from -infinity
// to +infinity the angle 2 atan(s) of w goes from -pi to pi, counterclockwise,
// so the point of angle pi, P(infinity) = center - major, is missing and is
// handled apart.
//
// The trace of a conic. For a circle with centre c and radius r, and for an
// ellipse with its support (ellipse_meaning.hpp), the point P(s) times its
// weight gives two polynomials (gx, gy) of degree at most two: for a circle
// the vector from the centre to P(s), for an ellipse the direction of P(s) on
// its unit circle, both up to a positive factor. The equation of the conic in
// P(s), times weight^2, is gx^2 + gy^2 - R weight^2, a polynomial of degree at
// most four (R is the squared radius, or the squared determinant of the
// ellipse, scaled alike). It is the zero polynomial exactly when the two
// carriers are the same curve; otherwise its real roots are the common points
// other than P(infinity), and P(infinity) is a common point of multiplicity
// 4 - degree.
//
// Every decision below is exact: roots isolated and compared with
// exact/polynomial.hpp, signs of polynomials of degree at most two at the
// roots for the positions on both curves (Location) and for the order along
// the ellipse. No tolerance: the floating-point filter of exact/polynomial.hpp
// answers a sign only when it is certain (stage 0.3 corrections, FS4).

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "ellipse_meaning.hpp"
#include "exact/polynomial.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <optional>
#include <vector>

namespace solid_space::geom2d::detail {

/// The rational form of an ellipse: P(s) = (x(s), y(s)) / weight(s).
struct RationalEllipse {
    exact::Polynomial x;
    exact::Polynomial y;
    exact::Polynomial weight;
};

/// The rational form of the carrying ellipse of a support.
[[nodiscard]] RationalEllipse rationalForm(const EllipseSupport& support);

/// A conic seen along the rational form of an ellipse (see the top of this
/// file): (directionX, directionY) is the direction of P(s) from the centre
/// of a circle, or on the unit circle of an ellipse, up to a positive factor;
/// equation is zero at the parameters of the common points.
struct ConicTrace {
    exact::Polynomial directionX;
    exact::Polynomial directionY;
    exact::Polynomial equation;
};

/// The trace of a circle (Circle, Arc, curved PolylineSegment).
[[nodiscard]] ConicTrace traceOfCircle(const RationalEllipse& form, const CircleSupport& circle);

/// The trace of the carrying ellipse of another support.
[[nodiscard]] ConicTrace traceOfEllipse(const RationalEllipse& form, const EllipseSupport& other);

/// A common point of the carrying ellipse and a conic.
struct ConicMeetingPoint {
    /// The root of the equation of the trace; nothing for P(infinity), the
    /// point center - major of the ellipse.
    std::optional<exact::RealRoot> parameter;
    /// touching for a multiple root, crossing otherwise.
    Contact contact = Contact::crossing;
    /// On the ellipse (its unit arc) and on the arc of the conic.
    Location onEllipse = Location::inside;
    Location onOther = Location::inside;
};

/// The common points of the carrying ellipse and a conic that is not the same
/// curve, all of them (whatever the extensions and the domain), in the exact
/// order along the ellipse, counterclockwise from its start D0.
/// otherArc is the arc of the conic in the frame of its trace: the ArcMeaning
/// of a circular curve, or unitArc of another ellipse. Precondition: the
/// equation of the trace is not the zero polynomial.
[[nodiscard]] std::vector<ConicMeetingPoint> meetEllipseConic(const EllipseMeaning& ellipse,
                                                              const ConicTrace& trace,
                                                              const ArcMeaning& otherArc);

}  // namespace solid_space::geom2d::detail
