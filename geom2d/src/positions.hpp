// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Positions of exact points along the curves (ADR 0010, points 2 and 3;
// intersection.hpp, Location and CurvePosition). Internal to geom2d: nothing
// here is part of the public contract. Stage 0.2, package P10; P11, P14, P15
// and P16 build on it, and P11 may only add to this header.
//
// What is here, for an exact point (carriers.hpp, ExactPoint) that lies
// exactly on the carrier of a curve:
// - its Location on the curve and on its extension (before the start, at the
//   start, inside, at the end, past the end), with the rules of the whole
//   arcs and circles of curves.hpp; decided exactly;
// - the exact order of two exact points along the curve, with the same
//   radicand or with different ones: for a straight curve along its directed
//   line, for a circular one from its start D0 in its direction, over the whole
//   circle (an arc and its extension are one closed curve);
// - its station, rounded: a function of the exact point and of the curve only,
//   which never decreases along the curve;
// - the correctly rounded value of an exact number or point.
//
// Why the station is a function of the exact point only (stage plan, section
// 4, "Stazioni e simmetria"). The contract wants stations that never decrease
// along a sorted list, and the same bits when the arguments of intersect are
// swapped or when a cutter of intersectAlong is called alone. Both hold if the
// station depends on nothing but the exact point and the curve and is a
// non-decreasing function of the exact position: then equal points get equal
// bits whatever list they are in, and a list sorted exactly has non-decreasing
// stations. Every rounding below is a composition of non-decreasing steps:
// - the correctly rounded value (round to nearest, ties to even) of an exact
//   quantity that grows with the position: the signed distance from the start
//   along a line; the sine or the cosine of the angle within an eighth of a
//   turn from D0, whichever grows;
// - for an arc, a series of arcsine with positive coefficients evaluated in
//   Horner's form on a non-negative argument: every addition and product of
//   non-negative doubles rounded to nearest is non-decreasing in each operand;
// - additions of constants, products by a positive constant (the radius), and
//   clamps to the bounds of each eighth of a turn, all non-decreasing.
// Correct rounding also makes the result independent of how the exact point is
// written (ExactPoint has many representations): the bits depend on the value.
//
// Decisions are exact, with no tolerance. All the computation is in
// positions.cpp (ADR 0008, point 5). The functions may allocate (exact
// numbers), so they are not noexcept unless they only look at enumerators.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/predicates.hpp"

namespace solid_space::geom2d::detail {

/// True when a point at this location is on the part of the curve that takes
/// part in an operation: the curve itself (atStart, inside, atEnd) with
/// Extension::none, the curve and its extension with Extension::extended.
[[nodiscard]] bool takesPart(Location location, Extension extension) noexcept;

/// The exact location of a point on a curve and its extension (intersection.hpp,
/// Location). For a straight curve the point is compared with the start and the
/// end along the directed line; for a circular one, its direction from the
/// centre with D0 and D1 (curve_meaning.hpp, locateOnArc): a Circle has every
/// point inside, a whole-circle Arc only its start atStart. Preconditions: the
/// meaning is not degenerate and the point lies exactly on its carrier
/// (onCarrier()).
[[nodiscard]] Location locate(const CurveMeaning& curve, const ExactPoint& point);

/// The exact order of two points along a curve: the sign of position(a) -
/// position(b). A straight curve is ordered along its directed line, before the
/// start included; a circular one from its start D0 in its direction, over the
/// whole circle, so that the points past the end of an arc come after its end
/// and before its start. Any two radicands. Preconditions: as locate(), for
/// both points.
[[nodiscard]] robust::Sign compareAlong(const CurveMeaning& curve, const ExactPoint& a,
                                        const ExactPoint& b);

/// The station of a point: its arc length from the start of the curve, in the
/// direction of the curve (intersection.hpp, CurvePosition), within
/// kRelativeStationError * max(M, |s|) of the exact value, with M the magnitude
/// of the curve. Negative before the start of a straight curve; in
/// [0, 2 pi r] on a circular one, from D0 in its direction (a Circle from its
/// point at angle zero).
///
/// A function of the exact point and of the curve only: the same bits for every
/// representation of the same point. Non-decreasing along the curve:
/// compareAlong(curve, a, b) < 0 implies station(curve, a) <=
/// station(curve, b). Exactly zero (+0) at the start. Preconditions: as
/// locate().
[[nodiscard]] double station(const CurveMeaning& curve, const ExactPoint& point);

/// locate() and station() together.
[[nodiscard]] CurvePosition positionOn(const CurveMeaning& curve, const ExactPoint& point);

/// The value (a + b sqrt(d)) / (w sqrt(k)) correctly rounded to double: the
/// nearest double, ties to even, and +0 for an exact zero. A non-decreasing
/// function of the exact value, whatever a, b, d, w and k represent it.
/// Preconditions (asserted in debug): d >= 0, w > 0, k > 0, and the value is
/// well inside the range of the doubles (below 2^1000 in magnitude).
[[nodiscard]] double correctlyRounded(const exact::Dyadic& a, const exact::Dyadic& b,
                                      const exact::Dyadic& d, const exact::Dyadic& w,
                                      const exact::Dyadic& k);

/// The exact point rounded to doubles, each coordinate correctly rounded (so
/// within 2^-53 |p| for a normal coordinate, and exact when it is a double).
/// The same bits for every representation of the same point, unlike
/// roundedPoint() of carriers.hpp. Precondition: the coordinates are below
/// 2^1000 in magnitude, as for every point of a curve within the domain (below
/// 2^52) and every point that beyondDomain() lets through.
[[nodiscard]] Point2 nearestPoint(const ExactPoint& point);

/// The exact point of a circle in a direction from its centre:
/// centre + radius * direction / |direction|. Its coordinates are
/// (c |D|^2 + D sqrt(rho |D|^2)) / (w |D|^2), with c / w the centre, rho / w^2
/// the squared radius and D the direction, so they need one radicand. Used for
/// the ends of arcs (D0 and D1, which are exact dyadic vectors). Precondition:
/// the direction is not the zero vector.
[[nodiscard]] ExactPoint pointInDirection(const CircleSupport& circle,
                                          const ExactVector& direction);

/// The exact start of a curve: the start of a straight curve, the point in
/// direction D0 of a circular one (a Circle starts at its point at angle zero).
/// Precondition: the meaning is not degenerate.
[[nodiscard]] ExactPoint startPoint(const CurveMeaning& curve);

/// The exact end of a curve: the end of a straight curve, the point in
/// direction D1 of a circular one (D0 for a whole circle). Precondition: the
/// meaning is not degenerate.
[[nodiscard]] ExactPoint endPoint(const CurveMeaning& curve);

}  // namespace solid_space::geom2d::detail
