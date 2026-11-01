// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0010, which fixes the exact
// meaning of the curves, and ADR 0009. Declarations only: nothing here may be
// used before the package that implements it is merged.
//
// Every curve has an exact meaning, fixed below from its doubles. All the
// decisions of geom2d (intersections, positions, order, side) are exact
// answers about that meaning. Constructions (points, stations, lengths) are
// rounded, within the errors declared here. Every construction error bound in
// geom2d, including bounds declared in other headers, uses a scale of at least
// 2^-1022 (the smallest normal double): max(M, |p|, 2^-1022) for points and
// max(M, |s|, 2^-1022) for stations and lengths. This floor changes no exact
// decision and does not restrict the input domain.

#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/robust/point.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace solid_space::geom2d {

using Point2 = robust::Point2;

/// Angle constants, each the double nearest to the exact value. They are
/// compared with the doubles of an Arc exactly, so the comparisons are the
/// same everywhere.
inline constexpr double kQuarterTurn = 1.5707963267948966;     // pi / 2
inline constexpr double kThreeQuarterTurn = 4.71238898038469;  // 3 pi / 2
inline constexpr double kFullTurn = 6.283185307179586;         // 2 pi

/// Largest error of a constructed point, relative to the magnitude: a point p
/// returned by geom2d differs from the exact point by at most
/// kRelativePointError * max(M, |p|, 2^-1022), where |p| is the largest absolute
/// coordinate of the exact point and M the magnitude of the inputs (see
/// magnitude()). 2^-48 is about 3.6e-15.
inline constexpr double kRelativePointError = 0x1p-48;

/// Largest error of a station or a length (see CurvePosition), relative in
/// the same way: at most kRelativeStationError * max(M, |s|, 2^-1022), with s the exact
/// value. 2^-46 is about 1.4e-14.
inline constexpr double kRelativeStationError = 0x1p-46;

/// The closed straight segment from start to end. Its extension is the whole
/// line through start and end, directed from start to end.
struct Segment {
    Point2 start;
    Point2 end;
};

/// The whole circle of the given centre and radius, run counterclockwise
/// from its point at angle zero, centre + (radius, 0). Its extension is
/// itself.
struct Circle {
    Point2 center;
    double radius = 0.0;
};

/// An arc as Tecnigrafo stores it: centre, radius, start angle and signed
/// sweep, in radians (counterclockwise from +x; a positive sweep runs
/// counterclockwise).
///
/// Exact meaning. Let D0 = robust::sinCos(startAngle) and
/// D1 = robust::sinCos(startAngle + sweep), the sum rounded to double, read
/// as direction vectors (cosine, sine) from the centre. Let alpha in
/// [0, 2 pi) be the exact angle from D0 to D1 measured in the direction of the
/// sweep. The arc is the part of the circle (center, radius) that starts in
/// direction D0 and turns by:
/// - the whole circle, when |sweep| >= kFullTurn;
/// - the whole circle, when |sweep| >= kThreeQuarterTurn and
///   alpha <= pi / 2 (D1 has passed D0 only through rounding);
/// - nothing, when |sweep| <= kQuarterTurn and alpha >= 3 pi / 2 (D1 has
///   fallen behind D0 only through rounding): the arc is the single point in
///   direction D0, a degenerate curve;
/// - alpha otherwise (alpha == 0 is again the single point in direction D0).
/// Comparisons with alpha are decided exactly from D0 and D1, without
/// trigonometry. The arc ends in direction D1 (in direction D0 for a whole
/// circle). Its extension is the whole circle, run in the direction of the
/// sweep starting from D0.
struct Arc {
    Point2 center;
    double radius = 0.0;
    double startAngle = 0.0;
    double sweep = 0.0;
};

/// A segment of a polyline as the DXF writes it: from start to end, with the
/// bulge of the DXF, tan(theta / 4) where theta is the included angle,
/// positive for a counterclockwise arc.
///
/// Exact meaning. With bulge == 0 it is the straight Segment{start, end}.
/// Otherwise, with (dx, dy) = end - start, let
///     m = (start + end) / 2 + (bulge / 2) * (dy, -dx),
/// computed exactly (it is a rational function of the doubles, never
/// rounded): the curve is the arc of the circle through start, m and end that
/// contains m, from start to end. If start and end are the same point it is
/// that single point, a degenerate curve. Its extension is the whole line
/// (straight) or the whole circle, run from start in the direction of the
/// arc.
struct PolylineSegment {
    Point2 start;
    Point2 end;
    double bulge = 0.0;
};

/// Any curve of the plane that geom2d decides about.
using Curve = std::variant<Segment, Arc, Circle, PolylineSegment>;

/// A vertex of a polyline: its position and the bulge of the segment that
/// starts from it (as in the LWPOLYLINE of the DXF).
struct PolylineVertex {
    Point2 position;
    double bulge = 0.0;
};

/// An open or closed polyline. A closed polyline has a last segment from the
/// last vertex back to the first, with the bulge of the last vertex.
struct Polyline {
    std::vector<PolylineVertex> vertices;
    bool closed = false;
};

/// The segments of a polyline, in order: n - 1 for n vertices, n if closed,
/// none with fewer than two vertices. Segment i starts at vertex i, so a
/// position on a polyline is a segment index and a position on that segment.
/// The pieces are copied as they are: they are checked when used.
[[nodiscard]] std::vector<PolylineSegment> pieces(const Polyline& polyline);

// Checking the data. Every function of geom2d checks inputs in increasing
// Diagnosis::input order: all of input 0, then input 1, and so on. The first
// applicable diagnosis wins. Within each curve, the order is:
// - invalidValue: a value is NaN or infinite, or a radius is negative;
// - outOfDomain: a coordinate, a radius or the point m of a curved
//   PolylineSegment has magnitude beyond robust::kMaxCoordinate, or
//   startAngle or startAngle + sweep of an Arc has magnitude beyond
//   robust::kMaxTrigArgument;
// - degenerateCurve: the curve is exactly a single point (see above; a
//   radius of zero counts too). magnitude() accepts degenerate curves.
// A polyline is checked piece by piece in pieces() order, with the curve
// order above within each piece. The first piece with invalid data decides; the message
// names its vertex. A piece exactly equal to a point is dropped without a
// degenerateCurve diagnosis on its vertex. After these drops, no piece
// left gives degenerateCurve on input 0, before checking later inputs. Values
// belonging to no piece (the last bulge of an open polyline) are not checked.
// An input-specific diagnosis about its data (e.g. notCircular) follows that
// input's data checks and precedes checks of later inputs. All input data
// checks finish before constructions begin: a construction-domain diagnosis
// follows the checks of every input, even when it names an earlier input.
//
// Every construction that geom2d returns keeps its coordinates, radii and
// the point m of curved PolylineSegments within robust::kMaxCoordinate. This
// checks the data values, never the geometric extent of the curve. Domain
// decisions use the exact construction values, even when rounding would
// bring an out-of-domain value back within the domain. An exact coordinate
// or radius within kMaxCoordinate is never returned beyond it: if rounding
// would exceed either boundary, return that boundary (+/-kMaxCoordinate).
// The point m of a returned curved PolylineSegment is reconstructed exactly
// from the returned doubles and checked that way, not from an exact precursor.
// A construction outside the domain is never returned or silently omitted;
// each operation specifies its diagnosis or failure. Degenerate results are
// governed by the individual operation, not by this domain rule.

/// The magnitude M of a curve, used by the error bounds above and by the
/// tolerance rule (ADR 0009): the largest absolute value among
/// - Segment: the coordinates of start and end;
/// - Circle and Arc: the coordinates of the centre and the radius;
/// - PolylineSegment: the coordinates of start, end and, if curved, of the
///   point m above (rounded to double).
/// The magnitude of several curves is the largest of theirs.
///
/// Diagnosis invalidValue or outOfDomain for invalid data (input 0).
[[nodiscard]] Result<double> magnitude(const Curve& curve);

/// The two ends of a curve. A circle starts and ends at its point at angle
/// zero; a whole-circle arc starts and ends in direction D0. Each point is
/// within the error of kRelativePointError.
struct Endpoints {
    Point2 start;
    Point2 end;
};

/// The ends of a curve, computed as declared above: Tecnigrafo should take
/// the ends of an arc from here, not from its own cosine and sine, so that
/// they agree with the exact meaning of the arc.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve (input 0).
[[nodiscard]] Result<Endpoints> endpoints(const Curve& curve);

/// The length of a curve, within the error of kRelativeStationError.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve (input 0).
[[nodiscard]] Result<double> length(const Curve& curve);

}  // namespace solid_space::geom2d
