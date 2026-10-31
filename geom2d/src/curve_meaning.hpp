// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The exact meaning of the curves of geom2d (curves.hpp; ADR 0010, point 1 and
// "Che cosa rende possibili decisioni esatte"). Internal to geom2d: nothing
// here is part of the public contract. Stage 0.2, package P7; the packages that
// decide about curves (P8-P16) build on it and extend this header only when
// their own plan lists it.
//
// What is here:
// - the checking of the data, in the order of curves.hpp ("Checking the
//   data"), with and without the degenerate curves;
// - the exact meaning of every curve, computed once from its doubles with the
//   exact numbers of geom2d/src/exact/: the carrying line or circle, the
//   direction of travel and, for an arc, the directions D0 and D1 of its ends
//   with the four rules of Arc decided exactly;
// - the basic questions that the decisions ask again and again: where a
//   direction from the centre lies on an arc, which of two directions comes
//   first along it, and the magnitude M of several curves.
//
// A direction is compared with D0 and D1 only through the signs of exact cross
// and dot products, so the same functions serve directions with radicals (the
// intersection points of later packages): the caller computes the signs with
// its own numbers and passes them in. The overloads that take an ExactVector
// do it for dyadic directions.
//
// All the computation is in curve_meaning.cpp (ADR 0008, point 5). The
// functions may allocate (exact numbers, see exact/dyadic.hpp), so they are not
// noexcept unless they only look at signs.

#include "exact/dyadic.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace solid_space::geom2d::detail {

using exact::Dyadic;
using robust::Sign;

/// A point or a vector with exact dyadic coordinates.
struct ExactVector {
    Dyadic x;
    Dyadic y;
};

/// The exact value of a point of doubles. Precondition: finite coordinates.
[[nodiscard]] ExactVector exactVector(Point2 point);

/// The sign of the cross product a.x * b.y - a.y * b.x: positive when b turns
/// counterclockwise from a by less than half a turn.
[[nodiscard]] Sign crossSign(const ExactVector& a, const ExactVector& b);

/// The sign of the dot product a.x * b.x + a.y * b.y.
[[nodiscard]] Sign dotSign(const ExactVector& a, const ExactVector& b);

/// The direction in which a circular curve is run.
enum class Turn : std::uint8_t {
    counterclockwise,
    clockwise,
};

/// A sign seen in the direction of a turn: unchanged for counterclockwise,
/// reversed for clockwise. A cross product oriented this way is positive when
/// the second vector comes after the first, by less than half a turn, in the
/// direction of travel.
[[nodiscard]] Sign oriented(Turn turn, Sign sign) noexcept;

/// The carrying line of a straight curve (Segment, PolylineSegment with
/// bulge 0): the line through start and end, directed from start to end. The
/// doubles are the exact data. start == end only for a degenerate curve.
struct LineMeaning {
    Point2 start;
    Point2 end;
};

/// The carrying circle of a circular curve, in homogeneous coordinates: the
/// centre is (centerX / weight, centerY / weight) and the squared radius is
/// radiusSquared / weight^2. weight is positive: 1 for Circle and Arc, whose
/// data are the centre and the radius. For a curved PolylineSegment with
/// bulge b, chord (dx, dy) = end - start and midpoint mid = (start + end) / 2,
/// the centre is mid + (b^2 - 1) / (4 b) * (dy, -dx), so weight = 4 |b| and
/// radiusSquared = (dx^2 + dy^2) * (1 + b^2)^2: the circle through start, m
/// and end (curves.hpp), with no division and no rounding.
struct CircleSupport {
    Dyadic centerX;
    Dyadic centerY;
    Dyadic weight;
    Dyadic radiusSquared;
};

/// Which part of its circle a circular curve is.
enum class ArcSpan : std::uint8_t {
    /// A single point, in direction D0: a degenerate curve.
    point,
    /// The turn from D0 to D1, strictly between zero and a whole turn.
    partial,
    /// A whole-circle Arc: it starts and ends in direction D0, and only its
    /// start is atStart.
    whole,
    /// A Circle: run counterclockwise from direction (1, 0), with every point
    /// inside (intersection.hpp, Location).
    circle,
};

/// Where a direction lies, seen from a reference direction and measured in
/// the direction of a turn, as an angle in [0, 2 pi): on one of the four
/// half-axes of the reference (zero, quarter, half, threeQuarters) or
/// strictly inside one of the four quarters between them (first, second,
/// third, fourth). The order of the enumerators is the order of the angles.
enum class TurnSector : std::uint8_t {
    zero,           ///< the same direction as the reference
    first,          ///< strictly between 0 and pi / 2
    quarter,        ///< exactly pi / 2
    second,         ///< strictly between pi / 2 and pi
    half,           ///< exactly pi: the opposite direction
    third,          ///< strictly between pi and 3 pi / 2
    threeQuarters,  ///< exactly 3 pi / 2
    fourth,         ///< strictly between 3 pi / 2 and 2 pi
};

/// The sector of a direction v from a reference r, given the sign of the
/// cross product r x v oriented in the direction of the turn (see oriented())
/// and the sign of the dot product r . v. Precondition (asserted in debug):
/// the two signs are not both zero, that is v and r are not zero vectors.
[[nodiscard]] TurnSector sectorOf(Sign orientedCross, Sign dot) noexcept;

/// The sign of angle(u) - angle(v), both measured from the same reference in
/// the direction of the turn and lying in sectors su and sv. When they lie in
/// the same quarter, the order is decided by orientedCrossUV, the sign of
/// u x v oriented in the direction of the turn; otherwise it is not read.
[[nodiscard]] Sign compareTurns(TurnSector su, TurnSector sv, Sign orientedCrossUV) noexcept;

/// The exact meaning of an arc of a circle (Arc, Circle, curved
/// PolylineSegment).
struct ArcMeaning {
    CircleSupport circle;
    Turn turn = Turn::counterclockwise;
    ArcSpan span = ArcSpan::point;
    /// D0: the direction of the start, from the centre. For an Arc, the
    /// doubles of robust::sinCos(startAngle); for a Circle, (1, 0); for a
    /// curved PolylineSegment, start * weight - (centerX, centerY), which is
    /// the direction of start - centre scaled by weight. It is the zero vector
    /// only for a PolylineSegment whose start equals its end, and a radius of
    /// zero makes the whole circle a single point anyway.
    ExactVector startDirection;
    /// D1: the direction of the end, from the centre, built in the same way
    /// (robust::sinCos(startAngle + sweep) for an Arc, the sum rounded). For
    /// a whole circle it is D0, where the curve ends.
    ExactVector endDirection;
    /// The sector of D1 from D0 in the direction of the turn: alpha as
    /// curves.hpp defines it. zero when span is not partial.
    TurnSector endSector = TurnSector::zero;
};

/// The exact meaning of a curve, computed once and passed to the questions
/// below. It keeps the curve it was made from.
struct CurveMeaning {
    Curve curve;
    /// The magnitude M of the curve (curves.hpp, magnitude()).
    double magnitude = 0.0;
    /// True when the curve is exactly a single point (degenerateCurve).
    bool degenerate = false;
    std::variant<LineMeaning, ArcMeaning> support;
};

/// The diagnoses invalidValue and outOfDomain of curves.hpp, in that order,
/// about the given input: the checks that also degenerate curves must pass
/// (magnitude(), coincidence.hpp). Nothing when the data are valid.
[[nodiscard]] std::optional<Diagnosis> checkData(const Curve& curve, std::size_t input);

/// The first diagnosis of curves.hpp that applies, in order: invalidValue,
/// outOfDomain, degenerateCurve. Nothing when the curve is valid.
[[nodiscard]] std::optional<Diagnosis> check(const Curve& curve, std::size_t input);

/// The diagnosis degenerateCurve about the given input.
[[nodiscard]] Diagnosis degenerateDiagnosis(std::size_t input);

/// The point m of a curved PolylineSegment, exactly:
/// (start + end) / 2 + (bulge / 2) * (dy, -dx). Precondition: finite data.
[[nodiscard]] ExactVector arcMidpoint(const PolylineSegment& piece);

/// The magnitude M of a curve (curves.hpp). Precondition: checkData() gives
/// nothing (degenerate curves are accepted).
[[nodiscard]] double magnitudeOf(const Curve& curve);

/// The largest magnitude of several curves (M of the inputs of an operation).
/// Zero for no curves.
[[nodiscard]] double magnitudeOf(std::span<const CurveMeaning> curves) noexcept;

/// What the rules of Arc decide: the direction of travel, the span, and the
/// sector of D1 from D0 (zero unless the span is partial).
struct ArcRule {
    Turn turn = Turn::counterclockwise;
    ArcSpan span = ArcSpan::point;
    TurnSector endSector = TurnSector::zero;
};

/// The rules of an Arc with the given signed sweep and the directions D0 and
/// D1 of its ends: the four rules of curves.hpp, decided exactly from the
/// doubles of D0 and D1 (no trigonometry). A positive sweep runs
/// counterclockwise, a zero or negative one clockwise (a zero sweep is a
/// point anyway). Exposed so that the rules can be checked with directions
/// built by hand. Precondition: finite values, d0 and d1 not zero vectors.
[[nodiscard]] ArcRule arcRule(double sweep, Point2 d0, Point2 d1);

/// The exact meaning of a curve. Precondition: checkData() gives nothing.
/// Degenerate curves get a meaning too, with degenerate set.
[[nodiscard]] CurveMeaning meaningOf(const Curve& curve);

/// What checkedMeaning() does with a degenerate curve.
enum class Degenerate : std::uint8_t {
    reject,  ///< diagnosis degenerateCurve
    accept,  ///< a meaning, with degenerate set
};

/// check() (or checkData() when degenerate curves are accepted) and
/// meaningOf() together, computing the meaning only once.
[[nodiscard]] Result<CurveMeaning> checkedMeaning(const Curve& curve, std::size_t input,
                                                  Degenerate degenerate = Degenerate::reject);

/// The sector of a direction from the start D0 of an arc, in the direction of
/// the arc. Precondition: span is not point and direction is not zero.
[[nodiscard]] TurnSector sectorFromStart(const ArcMeaning& arc, const ExactVector& direction);

/// Where a point of the circle lies on the arc, given the sector of its
/// direction from D0 (sectorFromStart()) and the sign of direction x D1
/// oriented in the direction of the arc (read only for a partial arc):
/// atStart in direction D0, inside strictly between D0 and D1, atEnd in
/// direction D1, afterEnd on the rest of the circle (intersection.hpp,
/// Location). A whole-circle Arc has atStart and inside only, a Circle only
/// inside. Precondition: span is not point.
[[nodiscard]] Location locateOnArc(const ArcMeaning& arc, TurnSector fromStart,
                                   Sign orientedCrossToEnd) noexcept;

/// locateOnArc() for a dyadic direction from the centre. Precondition: span
/// is not point and direction is not zero.
[[nodiscard]] Location locateDirection(const ArcMeaning& arc, const ExactVector& direction);

/// The order of two directions from the centre along the arc and its circle,
/// from D0 in the direction of the arc: the sign of turn(u) - turn(v), each
/// measured in [0, 2 pi). Precondition: span is not point, u and v are not
/// zero.
[[nodiscard]] Sign compareAlongArc(const ArcMeaning& arc, const ExactVector& u,
                                   const ExactVector& v);

}  // namespace solid_space::geom2d::detail
