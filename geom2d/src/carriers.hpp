// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The exact meetings of the carriers of two curves: the whole lines and
// circles that carry them (ADR 0010, "Che cosa rende possibili decisioni
// esatte"). Internal to geom2d: nothing here is part of the public contract.
// Stage 0.2, package P9; P10, P12, P14, P15 and P16 build on it, and P10 may
// only add to this header.
//
// What is here:
// - ExactPoint: a point whose coordinates are (a + b * sqrt(d)) / w, with one
//   radicand d for both coordinates. It is the exact value of a point of
//   doubles, of the meeting of two lines (b zero) and of every meeting of a
//   line and a circle or of two circles. Exact points compare with each other
//   (same radicand or different ones, through exact/quadratic.hpp), round to
//   double, and say whether they lie on a carrier;
// - Carrier: the carrying line or circle of a curve, taken from its exact
//   meaning (curve_meaning.hpp), and the exact sign of a point with respect to
//   it. side() (side.hpp) reads the same sign, so a point lies on a carrier
//   exactly when side() gives on: the two answers come from one comparison;
// - meet(): the exact meeting of two carriers: no point, one point of
//   tangency, two points, one crossing of two lines, or the same carrier, with
//   the Contact of every point decided exactly.
//
// No tolerance enters here: every answer is exact about the doubles of the
// curves. There is no floating-point filter yet: every sign is computed with
// the exact numbers, which allocate nothing for the small products of ordinary
// data (exact/dyadic.hpp). A filter, if the bench of P17 asks for one, goes in
// carriers.cpp with its error bound written next to it.
//
// All the computation is in carriers.cpp (ADR 0008, point 5). The functions
// may allocate (exact numbers), so they are not noexcept.

#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/predicates.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <variant>

namespace solid_space::geom2d::detail {

/// A point with exact coordinates
///     x = (rational.x + radical.x * sqrt(radicand)) / weight,
///     y = (rational.y + radical.y * sqrt(radicand)) / weight.
/// Invariants: weight is positive and radicand is not negative. A rational
/// point has a zero radical and a zero radicand. The same point has many
/// representations: compare points with samePoint(), never field by field.
struct ExactPoint {
    ExactVector rational;
    ExactVector radical;
    Dyadic radicand;
    Dyadic weight = Dyadic(1.0);
};

/// The exact value of a point of doubles: weight 1, no radical. Precondition:
/// finite coordinates.
[[nodiscard]] ExactPoint exactPoint(Point2 point);

/// The sign of a.x - b.x, decided exactly (any two radicands).
[[nodiscard]] Sign compareX(const ExactPoint& a, const ExactPoint& b);

/// The sign of a.y - b.y, decided exactly (any two radicands).
[[nodiscard]] Sign compareY(const ExactPoint& a, const ExactPoint& b);

/// True when the two exact points are the same point.
[[nodiscard]] bool samePoint(const ExactPoint& a, const ExactPoint& b);

/// True when a coordinate of the exact point has magnitude beyond
/// robust::kMaxCoordinate (intersection.hpp, omittedBeyondDomain). Decided
/// exactly.
[[nodiscard]] bool beyondDomain(const ExactPoint& point);

/// The point rounded to doubles, each coordinate within one ulp of its exact
/// value and exact when that value is a double (exact::quadraticToDouble). So
/// the error is at most 2^-52 * |p| for a normal coordinate, and at most
/// 2^-1074 for a subnormal one: within kRelativePointError * max(M, |p|)
/// (curves.hpp) whenever max(M, |p|) >= 2^-1026, below which no double can do
/// better than half the smallest subnormal. A coordinate beyond the largest
/// double becomes +-infinity: check beyondDomain() first.
[[nodiscard]] Point2 roundedPoint(const ExactPoint& point);

/// The carrying line or circle of a curve: a LineMeaning (directed from start
/// to end) or a CircleSupport (in homogeneous coordinates), as
/// curve_meaning.hpp computes them.
using Carrier = std::variant<LineMeaning, CircleSupport>;

/// The carrier of a curve. Precondition: the meaning is not degenerate (a
/// line has start != end, a circle a positive radius).
[[nodiscard]] Carrier carrierOf(const CurveMeaning& meaning);

/// The side of an exact point with respect to a directed line: positive on
/// the left of the line from start to end, zero on it, negative on the right.
/// For a point of doubles it is the sign of robust::orient2d(start, end, p).
/// Precondition: start != end.
[[nodiscard]] Sign lineSign(const LineMeaning& line, const ExactPoint& point);

/// The side of an exact point with respect to a circle: the sign of
/// |p - centre|^2 - radius^2, negative inside, zero on the circle, positive
/// outside. Precondition: positive weight.
[[nodiscard]] Sign circleSign(const CircleSupport& circle, const ExactPoint& point);

/// lineSign() or circleSign(), as the carrier is.
[[nodiscard]] Sign carrierSign(const Carrier& carrier, const ExactPoint& point);

/// True when the exact point lies on the carrier: carrierSign() is zero.
[[nodiscard]] bool onCarrier(const Carrier& carrier, const ExactPoint& point);

/// The side of an exact point with respect to the whole line or circle of a
/// curve, seen in the direction of the curve, as side() (side.hpp) defines it:
/// positive on the left, zero on the carrier, negative on the right. For a
/// circular curve, inside is left for a counterclockwise curve and right for a
/// clockwise one. It is zero exactly when onCarrier(carrierOf(meaning), point)
/// is true. Precondition: the meaning is not degenerate.
[[nodiscard]] Sign sideSign(const CurveMeaning& meaning, const ExactPoint& point);

/// A point where two carriers meet, and how they meet there: crossing or
/// touching (never sameCurve, which is about curves, not carriers).
struct CarrierPoint {
    ExactPoint point;
    Contact contact = Contact::crossing;
};

/// The meeting of two carriers. Either sameCarrier is true and there are no
/// points (two equal lines, or two circles with the same centre and the same
/// radius), or there are count points (0, 1 or 2) in points[0, count):
/// - two lines: one crossing, or none (distinct parallel lines);
/// - a line and a circle: none, one touching point (the line is tangent), or
///   two crossings in the direction of the line, whichever argument it is;
/// - two circles: none (apart, one inside the other, or the same centre with
///   different radii), one touching point (tangent inside or outside), or two
///   crossings, the one on the right of the line from the centre of the
///   circle that comes first in the order of the data (see meet()) to the
///   other centre first.
struct CarrierMeeting {
    bool sameCarrier = false;
    std::size_t count = 0;
    std::array<CarrierPoint, 2> points{};

    /// The points found, in the order above.
    [[nodiscard]] std::span<const CarrierPoint> found() const noexcept {
        return {points.data(), count};
    }
};

/// The exact meeting of two carriers.
///
/// Symmetric: meet(b, a) gives the same points, in the same order and with
/// the same representation (the same Dyadic values), as meet(a, b). Two
/// carriers of the same kind are put in a fixed order of their data before
/// computing, and a line is always taken first against a circle, so the
/// rounded points are bit for bit the same too.
///
/// Every point lies exactly on both carriers (onCarrier()). A point may lie
/// beyond robust::kMaxCoordinate (two almost parallel lines): beyondDomain()
/// says so. Preconditions: as carrierOf() (no degenerate carrier).
[[nodiscard]] CarrierMeeting meet(const Carrier& first, const Carrier& second);

}  // namespace solid_space::geom2d::detail
