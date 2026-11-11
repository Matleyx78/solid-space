// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0017, request #217.
//
// The ellipse and the elliptical arc of the plane. They are kept apart from
// Curve (curves.hpp): the functions of the other headers do not take them,
// and nothing in those headers changes.
//
// What is exact and what is rounded follows curves.hpp and intersection.hpp
// (ADR 0010, ADR 0013). Every decision below is exact about the ellipse as
// defined here and about the curves as defined in curves.hpp, for every
// finite double: how many common points there are, whether the curves cross,
// touch or share a stretch, where each point lies on each input (Location),
// the order of the points along the ellipse, whether a rectangle is met, and
// whether the nearest point is an end. Points and distances are rounded within
// the errors declared here and in curves.hpp, and the rule at the top of
// curves.hpp on data of magnitude below 2^-50 holds here too. A rounded point
// does not lie exactly on the curves: decisions are read from the result,
// never by testing a rounded point again.
//
// Checking the data, in the order of curves.hpp ("Checking the data"), input
// by input. Within an ellipse:
// - invalidValue: a value is NaN or infinite, or ratio is not in (0, 1];
// - outOfDomain: a coordinate of center or of majorAxis has magnitude beyond
//   robust::kMaxCoordinate, or startParameter or endParameter has magnitude
//   beyond robust::kMaxTrigArgument;
// - degenerateCurve: the curve is exactly a single point (majorAxis is the
//   zero vector, or the arc is reduced to one direction, see Ellipse).
// A point given with an ellipse is checked after it, for invalidValue and
// outOfDomain only, as side() (side.hpp) checks its point.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <cstddef>
#include <vector>

namespace solid_space::geom2d {

/// Largest error of the point and of the distance returned by nearest(),
/// relative to the magnitude N of the ellipse and of the given point (see
/// nearest()). 2^-44 is about 5.7e-14, four times below the relative
/// tolerance of the rule (robust::kRelativeTolerance, ADR 0009).
inline constexpr double kRelativeNearestError = 0x1p-44;

/// An ellipse or an elliptical arc as the ELLIPSE entity of the DXF stores
/// it: centre (group codes 10 and 20), end of the major axis relative to the
/// centre (11 and 21), ratio of the minor axis to the major axis (40), start
/// and end parameters in radians (41 and 42). The whole ellipse of the DXF
/// has parameters 0 and 2 pi (kFullTurn).
///
/// Exact meaning. Let a = majorAxis and b = ratio * (-a.y, a.x), the
/// products taken exactly (they are never rounded). Let F be the affine map
///     F(x, y) = center + x * a + y * b.
/// The carrying ellipse is the image under F of the unit circle; the point
/// of parameter t is F(cos t, sin t), and the parameter runs
/// counterclockwise, in the plane as on the unit circle.
///
/// Let D0 = robust::sinCos(startParameter) and
/// D1 = robust::sinCos(endParameter), read as direction vectors (cosine,
/// sine) of the unit circle, and alpha in [0, 2 pi) the exact angle from D0
/// to D1, counterclockwise. Let the nominal sweep sigma be
/// endParameter - startParameter plus the integer multiple of kFullTurn
/// that brings it into (0, kFullTurn], all exactly (kFullTurn is the double
/// of curves.hpp, not 2 pi). The curve is the image under F of the part of
/// the unit circle that starts in direction D0 and turns counterclockwise by
/// (the rule of Arc in curves.hpp, with sigma as its sweep):
/// - the whole circle, when sigma == kFullTurn: the two parameters are
///   equal, or differ by an exact multiple of kFullTurn (0 and 2 pi);
/// - the whole circle, when sigma >= kThreeQuarterTurn and alpha <= pi / 2
///   (D1 has passed D0 only through rounding);
/// - nothing, when sigma <= kQuarterTurn and alpha >= 3 pi / 2 (D1 has
///   fallen behind D0 only through rounding): the curve is the single point
///   in direction D0, a degenerate curve;
/// - alpha otherwise (alpha == 0 is again the single point in direction D0).
/// Comparisons with alpha are decided exactly from D0 and D1, without
/// trigonometry. The curve starts at the image of the unit point in direction
/// D0 and ends at that of the unit point in direction D1 (in direction D0 for
/// a whole ellipse). Its extension is the whole carrying ellipse, run
/// counterclockwise from direction D0.
///
/// With majorAxis equal to the zero vector the whole curve is the point
/// center: a degenerate curve. An ellipse of ratio 1 is a circle, with the
/// same points as a Circle of radius |majorAxis|.
struct Ellipse {
    Point2 center;
    Point2 majorAxis;
    double ratio = 1.0;
    double startParameter = 0.0;
    double endParameter = kFullTurn;
};

/// The magnitude M of an ellipse, used by the error bounds and by the
/// tolerance rule (ADR 0009): the largest absolute value among the
/// coordinates of center and of majorAxis (those of b are not larger, since
/// ratio <= 1). With other curves, the largest of their magnitudes.
///
/// Diagnosis invalidValue or outOfDomain (input 0). A degenerate ellipse is
/// accepted.
[[nodiscard]] Result<double> magnitude(const Ellipse& ellipse);

/// The two ends of an ellipse: the images under F of the unit points in
/// directions D0 and D1 (both in direction D0 for a whole ellipse), each
/// within kRelativePointError of the exact point (curves.hpp), even beyond
/// robust::kMaxCoordinate. Tecnigrafo should take the ends of an elliptical
/// arc from here, not from its own cosine and sine, so that they agree with
/// the exact meaning of the arc, as for arcs (ADR 0010, point 4).
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve (input 0).
[[nodiscard]] Result<Endpoints> endpoints(const Ellipse& ellipse);

/// A point where an ellipse meets another curve or another ellipse.
struct EllipseIntersectionPoint {
    /// Rounded within kRelativePointError (curves.hpp), with M the magnitude
    /// of the two inputs.
    Point2 point;
    /// Decided exactly on the whole lines, circles and ellipses that carry
    /// the two inputs, as in intersection.hpp: crossing for different tangent
    /// directions, touching for the same tangent line on different carriers,
    /// sameCurve when the two inputs lie on the same carrier (the same
    /// ellipse, or an ellipse of ratio 1 and its circle) and meet only at
    /// this point.
    Contact contact = Contact::crossing;
    /// Where the point lies on the first input, an ellipse. Decided exactly.
    /// For an ellipse beforeStart is never used, as for an arc; every point
    /// of a whole ellipse but its start is inside.
    Location onFirst = Location::inside;
    /// Where the point lies on the second input, decided exactly, with the
    /// meaning of intersection.hpp for a curve and of onFirst for an ellipse.
    Location onSecond = Location::inside;
};

/// One end of a stretch shared by the two inputs.
struct EllipseOverlapEnd {
    /// Rounded within kRelativePointError, even beyond
    /// robust::kMaxCoordinate.
    Point2 point;
    Location onFirst = Location::inside;
    Location onSecond = Location::inside;
};

/// A stretch shared by two inputs on the same carrier: two ellipses with the
/// same points, or an ellipse of ratio 1 and a Circle, an Arc or a curved
/// PolylineSegment on the same circle (same exact centre, |majorAxis| equal
/// to the exact radius).
struct EllipseOverlap {
    /// True when the shared stretch is the whole carrier (two ellipses, each
    /// whole or extended; an ellipse, whole or extended, and a Circle or an
    /// extended arc); from and to are then the start of the first input.
    bool whole = false;
    /// The ends of the stretch, in the direction of the first input.
    EllipseOverlapEnd from;
    EllipseOverlapEnd to;
};

/// All the common points of an ellipse and another curve or ellipse.
struct EllipseIntersections {
    /// The isolated common points, in the exact order along the first input
    /// (the ellipse), counterclockwise from its start; with
    /// Extension::extended the points after the end of an arc follow those on
    /// it. No two of them are the same exact point.
    std::vector<EllipseIntersectionPoint> points;
    /// The shared stretches, in the order of their start along the first
    /// input. Their ends are not repeated in points.
    std::vector<EllipseOverlap> overlaps;
    /// Isolated common points whose exact coordinates have magnitude beyond
    /// robust::kMaxCoordinate: not listed, only counted, as in
    /// intersection.hpp.
    std::size_t omittedBeyondDomain = 0;
};

/// The common points of an ellipse and a curve of curves.hpp, each taken as
/// given or extended (Extension::extended: the whole ellipse; the whole line
/// or circle of the curve, as in intersection.hpp).
///
/// A polyline is given piece by piece (pieces()): points of different pieces
/// are not merged here. A common vertex of two consecutive pieces is atEnd on
/// one and atStart on the other, decided exactly.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the ellipse
/// (input 0), then about the curve (input 1), as curves.hpp checks it.
[[nodiscard]] Result<EllipseIntersections> intersect(const Ellipse& ellipse,
                                                     Extension ellipseExtension, const Curve& curve,
                                                     Extension curveExtension);

/// The common points of two ellipses, each taken as given or extended.
///
/// Symmetric in the decisions: intersect(b, eb, a, ea) gives the same exact
/// points, contacts and stretches, with onFirst and onSecond swapped and in
/// the order along b. The rounded points are within their error, not
/// necessarily the same bits.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the first
/// ellipse (input 0), then about the second (input 1).
[[nodiscard]] Result<EllipseIntersections> intersect(const Ellipse& first, Extension firstExtension,
                                                     const Ellipse& second,
                                                     Extension secondExtension);

/// The point of an ellipse nearest to a given point.
struct EllipseNearest {
    /// A point of the curve, within the error of nearest(), even beyond
    /// robust::kMaxCoordinate (as endpoints()).
    Point2 point;
    /// Its distance from the given point, within the error of nearest().
    /// Never negative; not a decision (a point exactly on the curve may give
    /// a small positive distance).
    double distance = 0.0;
    /// atStart or atEnd when the chosen candidate is that end of an
    /// elliptical arc: point is then the end as endpoints() gives it, bit for
    /// bit. inside otherwise, and always for a whole ellipse.
    Location location = Location::inside;
};

/// The point of the ellipse as given (an elliptical arc, never its
/// extension) nearest to p, and its distance: the snap Nearest and the choice
/// of an ellipse with a click.
///
/// The candidates are the ends of an arc and the feet of the perpendiculars
/// from p: the points q of the curve where p - q is orthogonal to the
/// tangent of the carrying ellipse (p itself when it lies on the curve). Let
/// d* be the exact distance from p to the curve and N the largest of the
/// magnitude M of the ellipse, |p.x| and |p.y|. The result stands for a
/// candidate q with
/// - |p - q| <= d* + kRelativeNearestError * N;
/// - |point - q| <= kRelativeNearestError * N (point is q itself, bit for
///   bit as endpoints() gives it, when q is an end);
/// - |distance - |p - q|| <= kRelativeNearestError * N.
///
/// Which candidate. When two or more candidates satisfy the first condition
/// (two points at the same distance, as for p on an axis of the ellipse
/// inside it; every point of an ellipse of ratio 1 seen from its centre; an
/// end and a foot at almost the same distance), any of them may be returned:
/// deterministic, but not fixed by the contract (the band of ADR 0013,
/// point 4). Outside that band the candidate, and so the point within its
/// error, is determined.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the ellipse
/// (input 0); invalidValue or outOfDomain about p (input 1).
[[nodiscard]] Result<EllipseNearest> nearest(const Ellipse& ellipse, Point2 p);

/// True when the ellipse as given (an elliptical arc, never its extension)
/// has at least one point in the closed rectangle with sides parallel to the
/// axes and opposite corners corner and oppositeCorner: the crossing window
/// of a selection, borders included. Decided exactly, with no tolerance. The
/// corners may be given in either order; the rectangle may be a segment or a
/// single point. A whole ellipse that surrounds the rectangle without
/// touching it does not meet it.
///
/// It agrees with intersect() and endpoints(): true exactly when the exact
/// start of the curve lies in the rectangle, or when the curve meets one of
/// its sides that is not a single point (a Segment) as intersect() decides
/// with Extension::none, or when the rectangle is a single point of the
/// curve.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the ellipse
/// (input 0); invalidValue or outOfDomain about corner (input 1), then about
/// oppositeCorner (input 2).
[[nodiscard]] Result<bool> meetsRectangle(const Ellipse& ellipse, Point2 corner,
                                          Point2 oppositeCorner);

}  // namespace solid_space::geom2d
