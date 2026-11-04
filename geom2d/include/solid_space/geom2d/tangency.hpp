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
// (Location), their order, the direction of a fillet arc and the part of each
// curve that a fillet keeps (KeptPart). Rounded: the
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
/// outOfDomain about the point (input 1). Also outOfDomain (input 1) if a
/// listed point of tangency has an exact value outside the data domain of
/// curves.hpp: the whole call fails, with no listed point silently omitted.
/// With Extension::none, excluded points are not domain-checked; with
/// Extension::extended all points are checked. Returned coordinates obey the
/// boundary rule of curves.hpp.
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
    /// then along the second, then by the exact centre's x coordinate and
    /// finally its y coordinate (exact comparisons). This is a total order;
    /// rounded coordinates must not be used to break these ties.
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
/// an exact constructed centre or point of tangency lies beyond kMaxCoordinate,
/// as for two almost parallel lines, which meet far away. The whole call
/// gives no circles; no candidate is silently omitted. Domain checks concern
/// data values under curves.hpp, never the geometric extent of a circle.
/// All input checks precede construction checks; returned coordinates obey
/// the boundary rule of curves.hpp.
///
/// Swapping the curves gives the same circles: the same infinitelyMany and
/// count, with onFirst and onSecond swapped (their positions decided exactly)
/// and sideOfFirst and sideOfSecond swapped, listed in the order above for the
/// swapped curves. Centres, points and stations agree within their errors,
/// not necessarily in bits (ADR 0013, point 3: the simplest choice, since the
/// difference is below the tolerance of ADR 0009).
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
    /// A pick does not identify a side of the other curve, or does not
    /// identify a unique part to keep under fillet rule 5.
    ambiguousPick,
    /// Fillet only: continuing the arc would require a straight curve to
    /// turn back beyond the end to keep. No fillet is returned.
    radiusTooLarge,
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
/// may choose either: a band of curves.hpp (ADR 0013, point 4). A tie goes to
/// the first circle in the list.
///
/// Diagnoses as tangentCircles; invalidValue or outOfDomain about nearFirst
/// (input 3) or nearSecond (input 4). This includes failure of the whole call
/// on an out-of-domain constructed centre or tangency point (input 2), before
/// choosing among candidates, but after checking every input, including
/// both picks. Domain checks use exact values and returned coordinates obey
/// the boundary rule of curves.hpp.
[[nodiscard]] Result<ChosenCircle> tangentCircleNear(const Curve& first, Point2 nearFirst,
                                                     const Curve& second, Point2 nearSecond,
                                                     double radius);

/// Which part of a curve a fillet keeps, with respect to its exact point of
/// tangency T. Decided exactly (fillet, rule 5). The kept part replaces the
/// curve: the curve is cut at T, or extended to T, so that it ends at the
/// fillet. Parts run in the direction of the curve, on the curve as given and
/// its extension.
///
/// When T is at the kept end the part is a single point. Without a fillet
/// arc to continue, a straight part can also lie entirely on the extension:
/// beforeStart with fromStart, or afterEnd with toEnd. It is the stretch
/// between T and the kept end. A Circle has no ends: the value says only on
/// which side of T, in its counterclockwise direction, it continues the fillet.
enum class KeptPart : std::uint8_t {
    /// From the start of the curve to T: the end moves to T (cut when T is
    /// inside, extended when T is afterEnd; for an arc along the rest of its
    /// circle).
    fromStart,
    /// From T to the end of the curve: the start moves to T (cut when T is
    /// inside, extended when T is beforeStart; for an arc, when T is afterEnd,
    /// back along the rest of its circle).
    toEnd,
};

/// A fillet between two curves.
struct Fillet {
    TangencyOutcome outcome = TangencyOutcome::none;
    // The fields below are meaningful only with TangencyOutcome::found.

    /// Centre of the fillet circle, rounded within kRelativePointError. With
    /// radius zero, the corner.
    Point2 center;
    /// The radius asked for; for a semicircle, half the distance between the
    /// two lines, rounded within kRelativeArcError.
    double radius = 0.0;
    /// The points of tangency. With keepFirst and keepSecond, their positions
    /// tell how to cut or extend each curve so that it ends at the fillet.
    TangencyPoint onFirst;
    TangencyPoint onSecond;
    /// The part of each curve that the fillet keeps (rule 5 of fillet).
    KeptPart keepFirst = KeptPart::fromStart;
    KeptPart keepSecond = KeptPart::fromStart;
    /// True for the semicircle between two parallel straight curves.
    bool semicircle = false;
    /// The fillet arc from onFirst.point to onSecond.point, as Tecnigrafo
    /// stores arcs: the given radius (exactly), start angle and signed sweep
    /// from robust::atan2. Within kRelativeArcError. With radius zero:
    /// {center, 0, 0, 0}.
    Arc arc;
    /// The same arc as the bulge of a PolylineSegment from onFirst.point to
    /// onSecond.point (positive counterclockwise). Within kRelativeArcError.
    /// With radius zero, or when the returned onFirst.point and onSecond.point
    /// coincide: 0. The Arc keeps its given radius and its own rules. Since
    /// point rounding is free within kRelativePointError, coincidence can
    /// depend on a permitted rounding choice: a band of curves.hpp (ADR 0013,
    /// point 4), where neither branch is mandatory. The bulge form may be a
    /// point; use Arc in that case.
    double bulge = 0.0;
};

/// FILLET: the arc of the given radius tangent to both curves, on the sides
/// that the picks show. Each curve is taken with its extension, so the curves
/// need not meet; keepFirst and keepSecond, with the positions of onFirst and
/// onSecond, tell how to cut or extend them.
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
///    is Side::on: ambiguousPick, before constructing any candidate or
///    checking its domain (even if tangentCircles would give outOfDomain).
///    Otherwise, when tangentCircles has infinitelyMany:
///    infinitelyMany. Among the candidates, the one nearest the picks, as
///    tangentCircleNear chooses; none when there is no candidate.
/// 4. The fillet arc is the shorter of the two arcs of the fillet circle
///    between the two points of tangency; its direction is decided exactly.
///    When the two points are opposite ends of a diameter, it is the
///    counterclockwise one (the semicircle of rule 1 excepted); when they are
///    the same exact point, it is that single point (sweep 0, bulge 0).
///    When only the returned points coincide, bulge is 0 while Arc keeps the
///    rules above and the given radius. Coincidence may depend on permitted
///    point rounding (a band, ADR 0013, point 4); a reference case must not
///    require either branch. Both forms still obey kRelativeArcError.
/// 5. The part of each curve to keep (KeptPart), decided exactly, as AutoCAD
///    does. Let t be the direction of a curve at its exact point of tangency
///    (from its start toward its end; a Circle runs counterclockwise).
///    When the two exact points of tangency differ (rules 1 and 3), let a be
///    the direction in which the exact fillet arc leaves the point of
///    tangency of that curve toward the other point. Each curve keeps the
///    part that continues the arc: toEnd when dot(t, a) < 0, fromStart
///    when dot(t, a) > 0 (t and a are parallel, so it is never zero). The
///    path along the kept parts and the arc has no reversal. If a straight
///    curve would turn back at T (beforeStart with fromStart or afterEnd
///    with toEnd), return radiusTooLarge without a fillet. Otherwise extend
///    the appropriate end to reach T when T lies outside the given curve.
///    Without an arc to continue (rule 2 or exactly coincident tangencies),
///    use the pick of each curve relative to the exact corner P (the centre
///    in rule 2, the common point of tangency otherwise):
///    - Straight curves use the CHAMFER rule: toEnd when dot(pick - P, t)
///      is positive, fromStart when it is negative, ambiguousPick at zero.
///      A whole circle uses the same sign with its counterclockwise tangent
///      to identify a local side, since it has no ends.
///    - For an arc or curved polyline segment, consider the directed parts
///      from the start to P and from P to the end on its supporting circle.
///      Compare the radial direction of the pick from the centre with each
///      part's angular interval, including its ends. Keep the only part
///      containing that direction. If P lies outside the given arc and both
///      parts contain the pick direction, keep the one requiring the smaller angular
///      extension of the given arc, with an exact comparison. Equal
///      extensions, a pick at the centre, or no uniquely indicated part
///      in the other cases give ambiguousPick.
///    These checks follow candidate selection and all domain diagnoses.
///    For the same selected geometry, swapping the curves swaps their kept
///    parts and reversing a directed curve swaps its value without changing
///    its kept geometry. Rule 1 can choose another semicircle on swapping
///    the curves or reversing the first curve in an endpoint tie. Rule 4
///    can choose the other semicircle on swapping diametrically opposite
///    tangencies; its counterclockwise tie-break still applies.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the first
/// curve (input 0) or the second (input 1); invalidValue when the radius is
/// negative or not finite, outOfDomain when it is beyond kMaxCoordinate
/// (input 2); invalidValue or outOfDomain about pickFirst (input 3) or
/// pickSecond (input 4). A radius of zero is accepted (rule 2). Input data
/// checks precede the geometric rules, including the radius check for parallel
/// lines. Also outOfDomain (input 2) for a construction outside the data
/// domain of curves.hpp: a centre, constructed radius, point of tangency
/// (including the perpendicular foot in rule 1), reconstructed point m of the
/// bulge form, or a common point of rule 2 beyond kMaxCoordinate. In rule 3
/// the whole-call domain diagnosis of tangentCircles is inherited, without
/// silently dropping candidates. This applies to all three construction
/// rules; such a fillet exists, so it is never answered with none. Checks use
/// exact construction values, except m reconstructed exactly from returned
/// doubles; returned coordinates and radii obey the boundary rule of curves.hpp.
/// Tangency points and bulge remain free within their declared errors. When
/// a point of the exact arc has a coordinate whose magnitude exceeds
/// kMaxCoordinate − kRelativeArcError * max(M, |p|), a permitted choice
/// may give either found or outOfDomain (input 2) for reconstructed m. Farther
/// from the boundary, the reconstructed m is within the domain.
/// A boundary reference case must not require either outcome: with found,
/// points and arc must meet their error bounds and reconstructed m must be
/// within the domain; with outOfDomain, input is 2. This is a band of
/// curves.hpp (ADR 0013, point 4), and adds no requirement to choose an m
/// within the domain.
[[nodiscard]] Result<Fillet> fillet(const Curve& first, Point2 pickFirst, const Curve& second,
                                    Point2 pickSecond, double radius);

}  // namespace solid_space::geom2d
