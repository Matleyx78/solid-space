// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0011, request #21.
// Declarations only: nothing here may be used before the package that
// implements it is merged.
//
// Tangent lines from a point, circles tangent to two curves, and fillets
// between two curves of the plane: the constructions behind FILLET, the
// tangent snap and CIRCLE tangent-tangent-radius.
//
// What is exact and what is rounded, as in intersection.hpp. Exact, about the
// curves of curves.hpp and the given doubles: whether a tangent circle or a
// tangent point exists and how many there are, on which side of each curve a
// tangent circle lies, where each point of tangency lies on each curve
// (Location), their order, and the direction of a fillet arc. Rounded: the
// centres and points (within kRelativePointError), the stations (within
// kRelativeStationError) and the fillet arcs (within kRelativeArcError). Every
// curve is taken with its extension (the whole line or circle that carries
// it); the positions say where the points lie on the curve as given.
//
// The magnitude M of an operation is the largest among the magnitudes of its
// curves (curves.hpp), the absolute coordinates of a given point and the
// radius.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/side.hpp"

#include <cstdint>
#include <vector>

namespace solid_space::geom2d {

/// Largest distance of a returned fillet arc from the exact one, relative to
/// the magnitude: every point of the returned Arc (as curves.hpp defines it)
/// and of the returned bulge form lies within
/// kRelativeArcError * max(M, |p|) of the exact fillet arc, and the reverse;
/// their ends lie within the same distance of the exact points of tangency.
/// 2^-46 is about 1.4e-14, 16 times below the relative tolerance of ADR 0009.
inline constexpr double kRelativeArcError = 0x1p-46;

/// Where a point lies with respect to the circle that carries a curve.
/// Decided exactly.
enum class Placement : std::uint8_t {
    outside,
    on,
    inside,
};

/// A point of tangency on a curve.
struct TangencyPoint {
    /// Rounded within kRelativePointError. When the exact point is a given
    /// point (tangentsFrom with Placement::on), it is that point, unrounded.
    Point2 point;
    /// Where the exact point lies on the curve as given, and how far along it
    /// (intersection.hpp).
    CurvePosition position;
};

/// The tangents from a point to a circular curve.
struct TangentsFromPoint {
    Placement placement = Placement::outside;
    /// outside: the points where the two lines through the point touch the
    /// circle; on: the point itself; inside: none. Only the points on the
    /// curve as given are listed, unless the call extends the curve. In the
    /// exact order along the curve; stations never decrease.
    std::vector<TangencyPoint> points;
};

/// The points of tangency of the lines through a point to the circle that
/// carries an Arc, a Circle or a curved PolylineSegment. With
/// Extension::none only the points that lie on the curve as given (atStart,
/// inside, atEnd) are listed; with Extension::extended, all.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the curve
/// (input 0); notCircular when it is straight (input 0); invalidValue or
/// outOfDomain about the point (input 1).
[[nodiscard]] Result<TangentsFromPoint> tangentsFrom(Point2 point, const Curve& curve,
                                                     Extension extension);

/// A circle tangent to two curves.
struct TangentCircle {
    /// Rounded within kRelativePointError.
    Point2 center;
    TangencyPoint onFirst;
    TangencyPoint onSecond;
    /// The side of each curve (side.hpp) on which the circle lies near its
    /// point of tangency with that curve. Never Side::on. For a straight curve
    /// it is the side of the centre; for a circular curve, left or right of
    /// the curve in its direction (a circle inside a counterclockwise arc is on
    /// its left; a circle that touches it from outside, or encloses its
    /// circle, is on its right).
    Side sideOfFirst = Side::left;
    Side sideOfSecond = Side::left;
};

/// All the circles of a radius tangent to two curves.
struct TangentCircles {
    /// In the exact order of their point of tangency along the first curve,
    /// then along the second.
    std::vector<TangentCircle> circles;
    /// True when there are infinitely many (the two curves lie on the same
    /// line or circle; two parallel lines at distance exactly 2 * radius; two
    /// concentric circles whose radii differ, or add up, to exactly
    /// 2 * radius). The list is then empty.
    bool infinitelyMany = false;
};

/// The circles of the given radius tangent to both curves, each taken with
/// its extension. There are at most eight. A circle that coincides with a
/// circular curve (the radius equals the radius of that curve and the centre
/// is its centre) touches it everywhere, not at a point of tangency: it is not
/// a tangent circle and is not listed.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the first
/// curve (input 0) or the second (input 1); invalidValue when the radius is
/// negative or not finite, outOfDomain when it is beyond kMaxCoordinate,
/// degenerateCurve when it is zero (input 2). Also outOfDomain (input 2) when
/// the centre of a tangent circle lies beyond kMaxCoordinate, as for two
/// almost parallel lines, which meet far away: such a circle is never left out
/// silently, and the call gives no circles.
[[nodiscard]] Result<TangentCircles> tangentCircles(const Curve& first, const Curve& second,
                                                    double radius);

/// How a construction that chooses among tangent circles ended.
enum class TangencyOutcome : std::uint8_t {
    /// The construction exists; the other fields hold it.
    found,
    /// No circle of that radius fits, on the sides asked for.
    none,
    /// The curves lie on the same line or circle, or more generally the
    /// choice is among infinitely many circles: nothing is chosen.
    infinitelyMany,
    /// A pick lies exactly on the line or circle of the other curve, so it
    /// does not tell a side (fillet only).
    ambiguousPick,
};

/// A tangent circle chosen near two picks.
struct ChosenCircle {
    TangencyOutcome outcome = TangencyOutcome::none;
    /// Meaningful only with TangencyOutcome::found.
    TangentCircle circle;
};

/// CIRCLE tangent-tangent-radius: among tangentCircles(first, second,
/// radius), the circle whose points of tangency are nearest the two picks.
///
/// "Nearest" is the smallest |onFirst.point - nearFirst|^2 +
/// |onSecond.point - nearSecond|^2, evaluated in double, in that order, on
/// the rounded points: the choice is deterministic (the same bits everywhere,
/// ADR 0008, point 5) but not exact, so picks almost equally near two circles
/// may choose either. A tie goes to the first circle in the list.
///
/// Diagnoses as tangentCircles; invalidValue or outOfDomain about nearFirst
/// (input 3) or nearSecond (input 4).
[[nodiscard]] Result<ChosenCircle> tangentCircleNear(const Curve& first, Point2 nearFirst,
                                                     const Curve& second, Point2 nearSecond,
                                                     double radius);

/// A fillet between two curves.
struct Fillet {
    TangencyOutcome outcome = TangencyOutcome::none;
    // The fields below are meaningful only with TangencyOutcome::found.

    /// Centre of the fillet circle, rounded within kRelativePointError. With
    /// radius zero, the corner.
    Point2 center;
    /// The radius asked for; for a semicircle, half the distance between the
    /// two lines (rounded to double).
    double radius = 0.0;
    /// The points of tangency. Their positions tell how to cut or extend each
    /// curve so that it ends at the fillet.
    TangencyPoint onFirst;
    TangencyPoint onSecond;
    /// True for the semicircle between two parallel straight curves.
    bool semicircle = false;
    /// The fillet arc from onFirst.point to onSecond.point, as Tecnigrafo
    /// stores arcs: the given radius (exactly), start angle and signed sweep
    /// from robust::atan2. Within kRelativeArcError. With radius zero:
    /// {center, 0, 0, 0}.
    Arc arc;
    /// The same arc as the bulge of a PolylineSegment from onFirst.point to
    /// onSecond.point (positive counterclockwise). Within kRelativeArcError.
    /// With radius zero: 0.
    double bulge = 0.0;
};

/// FILLET: the arc of the given radius tangent to both curves, on the sides
/// that the picks show. Each curve is taken with its extension, so the curves
/// need not meet; the positions of onFirst and onSecond tell how to cut or
/// extend them.
///
/// The rules, in this order:
/// 1. Both curves straight, on two distinct parallel lines (decided exactly):
///    the semicircle, whatever the radius. It touches the first curve at the
///    end of the first curve nearer pickFirst (squared distances compared
///    exactly; a tie goes to its end), and the second line at the foot of the
///    perpendicular from that end; it is the half of the circle beyond that
///    end, away from the first curve. On the same line: infinitelyMany.
/// 2. Radius zero: the corner. The candidates are the common points of the two
///    extended curves (intersect() with Extension::extended); one of them is
///    chosen as tangentCircleNear chooses. A shared stretch gives
///    infinitelyMany; no common point, none.
/// 3. Otherwise the candidates are tangentCircles(first, second, radius) with
///    sideOfFirst == side(first, pickSecond) and
///    sideOfSecond == side(second, pickFirst): the fillet lies in the corner
///    where each pick shows the part of its curve to keep. When either side()
///    is Side::on: ambiguousPick. When tangentCircles has infinitelyMany:
///    infinitelyMany. Among the candidates, the one nearest the picks, as
///    tangentCircleNear chooses; none when there is no candidate.
/// 4. The fillet arc is the shorter of the two arcs of the fillet circle
///    between the two points of tangency; its direction is decided exactly.
///    When the two points are opposite ends of a diameter, it is the
///    counterclockwise one (the semicircle of rule 1 excepted); when they are
///    the same exact point, it is that single point (sweep 0, bulge 0).
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the first
/// curve (input 0) or the second (input 1); invalidValue when the radius is
/// negative or not finite, outOfDomain when it is beyond kMaxCoordinate
/// (input 2); invalidValue or outOfDomain about pickFirst (input 3) or
/// pickSecond (input 4). A radius of zero is accepted (rule 2). Also
/// outOfDomain (input 2) when rule 3 meets a tangent circle whose centre lies
/// beyond kMaxCoordinate, as tangentCircles does, and when a common point of
/// rule 2 lies beyond kMaxCoordinate (two almost parallel lines meet far away):
/// such a fillet exists, so it is never answered with none.
[[nodiscard]] Result<Fillet> fillet(const Curve& first, Point2 pickFirst, const Curve& second,
                                    Point2 pickSecond, double radius);

}  // namespace solid_space::geom2d
