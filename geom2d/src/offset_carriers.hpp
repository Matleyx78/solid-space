// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Offset carriers and their exact meetings (ADR 0011, "Che cosa rende possibili
// decisioni esatte"; ADR 0012, Decisione, point 4). Internal to geom2d: nothing
// here is part of the public contract. Stage 0.2, package P12; P14 (offset of
// a polyline) and P15 (tangent circles) use it without changing it.
//
// The joints of an offset polyline and the centres of tangent circles are
// meetings of offset carriers: the line of a segment moved along its normal,
// the circle of an arc with its radius changed. Their data are no longer
// dyadic: the unit normal of a slanted segment carries the root of its length
// (the line of (0, 0)-(1, 2) moved by d is -2x + y = d * sqrt(5)), and the
// radius of a curved polyline piece is sqrt(rho). Their meetings have nested
// roots, so everything here is written with the numbers of exact/tower.hpp:
// the caller keeps one Tower for all the carriers and points of a decision, so
// that points of different meetings can be compared.
//
// What is here:
// - OffsetCarrier: the carrier of a curve moved by a signed distance, as
//   offset.hpp moves it (positive to the left), with the new radius of a
//   circle compared with zero exactly. A distance of zero gives the carrier of
//   carriers.hpp itself and adjoins no root;
// - TowerPoint: a point whose coordinates are numbers of the tower over a
//   positive dyadic weight; exact points of carriers.hpp (one root) convert to
//   it;
// - meetOffset(): the exact meeting of two offset carriers (or of an offset
//   and an unshifted one): no point, a tangency, two points, the same carrier,
//   with the Contact decided exactly, as meet() of carriers.hpp;
// - for tower points: the side with respect to a carrier (the meaning of
//   side()), the exact order of two points along a carrier from a given point,
//   the exact comparison of their distances from a point, sameness, the domain
//   check and the rounding to doubles.
//
// Running out of roots. A tower holds kMaxTowerRoots roots. A function that
// needs a new one when the tower is full returns nothing: the caller gives a
// diagnosis, never a silent fallback. The decisions of stage 0.2 stay below the
// limit (exact/tower.hpp).
//
// No tolerance enters here, and there is no floating-point filter: that belongs
// to P14 and P15, next to their error bounds. All the computation is in
// offset_carriers.cpp (ADR 0008, point 5); no trigonometry. The functions may
// allocate (exact numbers), so they are not noexcept.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "exact/tower.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/predicates.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace solid_space::geom2d::detail {

using exact::Tower;
using exact::TowerNumber;

/// A point with coordinates (x / weight, y / weight), x and y numbers of a
/// tower. Invariant: weight is positive. The same point has many
/// representations: compare points with samePoint(), never field by field.
struct TowerPoint {
    TowerNumber x;
    TowerNumber y;
    Dyadic weight = Dyadic(1.0);
};

/// The exact value of a point of doubles: weight 1, no root. Precondition:
/// finite coordinates.
[[nodiscard]] TowerPoint towerPoint(Point2 point);

/// The exact point of carriers.hpp as a point of the tower, adjoining the root
/// of its radicand when it has one (and the tower does not have it yet).
/// Nothing when that root does not fit in the tower.
[[nodiscard]] std::optional<TowerPoint> towerPoint(Tower& tower, const ExactPoint& point);

/// An offset line: the points X with cross(direction, X) = constant, run in the
/// direction of `direction`. cross(direction, X) - constant is positive on the
/// left. For the line of a curve from P to Q moved by d, direction = Q - P and
/// constant = cross(Q - P, P) + d * |Q - P|: |Q - P| is a root of the tower,
/// or a dyadic number when |Q - P|^2 is a perfect square.
struct OffsetLine {
    ExactVector direction;
    TowerNumber constant;
};

/// An offset circle: centre (centerX / weight, centerY / weight) with a
/// positive weight, and squared radius radiusSquared / weight^2; run in the
/// direction of `turn`. For the circle of a curve with homogeneous radius
/// sqrt(rho) / weight (curve_meaning.hpp, CircleSupport) moved by d, the
/// radius becomes (sqrt(rho) - d * weight) / weight for a counterclockwise
/// curve and (sqrt(rho) + d * weight) / weight for a clockwise one, as in
/// offset.hpp; radiusSquared is the square of that numerator, and stays the
/// dyadic rho, with no root, when d is zero.
struct OffsetCircle {
    Dyadic centerX;
    Dyadic centerY;
    Dyadic weight;
    TowerNumber radiusSquared;
    Turn turn = Turn::counterclockwise;
};

/// The carrier of a curve moved by a signed distance.
using OffsetCarrier = std::variant<OffsetLine, OffsetCircle>;

/// How offsetCarrier() ended.
enum class OffsetStatus : std::uint8_t {
    /// There is a carrier.
    done,
    /// The new radius of a circle is zero or negative, decided exactly: nothing
    /// is left at that distance (offset.hpp, vanished). The carrier returned is
    /// not to be used.
    vanished,
    /// The root of the length of the segment, or of the radius, does not fit in
    /// the tower. The carrier returned is not to be used.
    towerFull,
};

/// The result of offsetCarrier().
struct OffsetCarrierResult {
    OffsetStatus status = OffsetStatus::done;
    OffsetCarrier carrier;
};

/// The carrier of a curve moved by `distance`, positive to the left of the
/// curve (offset.hpp; for a counterclockwise circle or arc, left is inside).
/// A distance of zero gives the carrier of carriers.hpp, with status done and
/// no root adjoined. Preconditions: the meaning is not degenerate, the
/// distance is finite.
[[nodiscard]] OffsetCarrierResult offsetCarrier(Tower& tower, const CurveMeaning& meaning,
                                                double distance);

/// The sign of the new radius of the circle of an arc moved by `distance`:
/// positive when something is left, zero or negative when the arc vanishes
/// (offset.hpp). Decided exactly; it adjoins no root (the comparison of
/// sqrt(rho) with |d| * weight is made on their squares). Preconditions: the
/// arc is not a single point, the distance is finite.
[[nodiscard]] Sign offsetRadiusSign(const ArcMeaning& arc, double distance);

/// A point where two offset carriers meet, and how they meet there: crossing
/// or touching (never sameCurve, which is about curves, not carriers).
struct OffsetMeetingPoint {
    TowerPoint point;
    Contact contact = Contact::crossing;
};

/// The meeting of two offset carriers, as CarrierMeeting (carriers.hpp):
/// either sameCarrier is true and there are no points (the same line, or the
/// same centre and the same radius), or there are count points (0, 1 or 2) in
/// points[0, count):
/// - two lines: one crossing, or none (distinct parallel lines);
/// - a line and a circle: none, one touching point, or two crossings in the
///   direction of the line, whichever argument it is;
/// - two circles: none (apart, one inside the other, or the same centre with
///   different radii), one touching point, or two crossings, the one on the
///   right of the line from the centre that comes first in the order of the
///   data (centerX, centerY, weight) to the other centre first.
/// With two unshifted carriers these are the points of meet() (carriers.hpp),
/// in the same order.
struct OffsetMeeting {
    bool sameCarrier = false;
    std::size_t count = 0;
    std::array<OffsetMeetingPoint, 2> points{};

    /// The points found, in the order above.
    [[nodiscard]] std::span<const OffsetMeetingPoint> found() const noexcept {
        return {points.data(), count};
    }
};

/// The exact meeting of two offset carriers of the same tower.
///
/// Symmetric: meetOffset(tower, b, a) gives the same points, in the same order
/// and with the same representation, as meetOffset(tower, a, b): the carriers
/// are taken in a fixed order of their data. Every point lies exactly on both
/// carriers (onCarrier()). A point may lie beyond robust::kMaxCoordinate:
/// beyondDomain() says so.
///
/// Two crossings adjoin the root of their discriminant (once: a second call on
/// the same tower finds it). Nothing when that root does not fit in the tower.
/// Precondition: both carriers come from offsetCarrier() with status done.
[[nodiscard]] std::optional<OffsetMeeting> meetOffset(Tower& tower, const OffsetCarrier& first,
                                                      const OffsetCarrier& second);

/// The sign of a point with respect to the equation of a carrier: for a line,
/// cross(direction, X) - constant (positive on the left); for a circle,
/// |X - centre|^2 - radius^2 (negative inside, zero on it, positive outside).
[[nodiscard]] Sign carrierSign(const Tower& tower, const OffsetCarrier& carrier,
                               const TowerPoint& point);

/// True when the point lies exactly on the carrier.
[[nodiscard]] bool onCarrier(const Tower& tower, const OffsetCarrier& carrier,
                             const TowerPoint& point);

/// The side of a point with respect to an offset carrier, seen in its
/// direction, with the meaning of side() (side.hpp): positive on the left,
/// zero on the carrier, negative on the right; for a circle, inside is left
/// when it runs counterclockwise and right when it runs clockwise. With a
/// distance of zero it is sideSign() of carriers.hpp.
[[nodiscard]] Sign sideSign(const Tower& tower, const OffsetCarrier& carrier,
                            const TowerPoint& point);

/// The order of two points along a carrier, measured from `from` in the
/// direction of the carrier, with the definitions of curves.hpp: the sign of
/// position(u) - position(v), where
/// - on a line, the position is the signed distance along the direction (the
///   projection of the point on it), so `from` does not change the order;
/// - on a circle, the position is the angle from the direction of `from` to
///   the direction of the point, both seen from the centre, measured in the
///   direction of the carrier in [0, 2 pi): `from` itself comes first.
/// The points need not lie on the carrier: only their projection, or their
/// direction from the centre, counts. A point of carriers.hpp, with one root,
/// enters through towerPoint(). Precondition: on a circle, from, u and v are
/// not its centre.
[[nodiscard]] Sign compareAlong(const Tower& tower, const OffsetCarrier& carrier,
                                const TowerPoint& from, const TowerPoint& u, const TowerPoint& v);

/// The sign of |u - origin| - |v - origin|: which of two points is nearer to
/// a point (the vertex V of a joint, P14). Decided exactly.
[[nodiscard]] Sign compareDistances(const Tower& tower, const TowerPoint& origin,
                                    const TowerPoint& u, const TowerPoint& v);

/// True when the two points are the same point.
[[nodiscard]] bool samePoint(const Tower& tower, const TowerPoint& a, const TowerPoint& b);

/// True when a coordinate of the point has magnitude beyond
/// robust::kMaxCoordinate (intersection.hpp, omittedBeyondDomain). Decided
/// exactly.
[[nodiscard]] bool beyondDomain(const Tower& tower, const TowerPoint& point);

/// The point rounded to doubles, each coordinate within 2 ulp of its exact
/// value (exact::Tower::quotientToDouble): an error of at most 2^-51 * |p| for
/// a normal coordinate and 2^-1073 for a subnormal one, so within
/// kRelativePointError * max(M, |p|) (curves.hpp) whenever
/// max(M, |p|) >= 2^-1025. A coordinate beyond the largest double becomes
/// +-infinity: check beyondDomain() first.
[[nodiscard]] Point2 roundedPoint(const Tower& tower, const TowerPoint& point);

}  // namespace solid_space::geom2d::detail
