// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The exact order of the pieces that leave a point (ADR 0014, "Che cosa rende
// la risposta coerente", point 1; ADR 0015, "Conseguenze", the risk of the
// order around a point). Internal to geom2d: nothing here is part of the
// public contract. Stage 0.3, package PS2; PS5 (clipToRegion), PS6 (the
// arrangement), PS9 and PS10 use it without changing it.
//
// What is here:
// - HalfPiece: a stretch of a line or a circle (the Carrier of a curve of
//   stage 0.2) that leaves an exact point P in one of its two headings;
// - compareAround(): the exact order of two half pieces that leave the same
//   point, counterclockwise from the direction +x: first the direction of
//   the tangent in P, then, at equal tangent, the signed curvature (the half
//   piece that turns left sooner comes later), and at equal tangent and
//   curvature the two are equal: the same line or circle in the same heading,
//   a stretch in common that the caller recognises;
// - LocalOrder: the half pieces sorted around P, with the groups of equal
//   ones, and place(): where a reference half piece falls among them, along a
//   group or strictly inside the sector between two consecutive groups.
//
// Why this order is the order of the pieces near P. Two distinct lines or
// circles through P with the same tangent in P meet only there, so near P
// they are apart and the one with the larger signed curvature lies on the
// left of the other: on a small circle around P it comes later
// counterclockwise. Two with the same tangent and the same signed curvature
// in P are the same line or circle, run in the same heading. A line or a
// circle is fixed by its point, tangent and curvature, so no higher order is
// ever needed.
//
// The cyclic order. The keys (angle of the tangent in [0, 2 pi) from +x,
// signed curvature) are sorted lexicographically: a total preorder, and as a
// cyclic order the true order of the pieces near P. A half piece with tangent
// +x and negative curvature, which near P lies just below the +x axis, comes
// first in the list and last around P: the two orders differ only in where
// the cycle is cut. Group 0 is the first group from +x; sector g is the open
// region between group g and group (g + 1) % groupCount(), counterclockwise.
//
// Exactness. P has coordinates (a + b sqrt(d)) / w (carriers.hpp,
// ExactPoint). The tangent of a line is its dyadic direction; the tangent of
// a circle at P is perpendicular to P - centre, whose coordinates are numbers
// a + b sqrt(d) with the radicand of P. So every comparison of two tangents
// is the sign of a number of one quadratic field (exact/quadratic.hpp), and
// every comparison of curvatures is a comparison of squared radii, which are
// dyadic. No tolerance and no rounding enter. All the half pieces of one
// order are computed from the same representation of P: the caller passes
// one ExactPoint, so P may be the meeting of circles with different
// radicands (the other carriers through it only need onCarrier()).
//
// No floating-point filter: every sign is computed with the exact numbers,
// which allocate nothing for the small products of ordinary data
// (exact/dyadic.hpp). A filter, if the bench of PS11 asks for one, goes in
// local_order.cpp with its error bound written next to it.
//
// All the computation is in local_order.cpp (ADR 0008, point 5). The
// functions may allocate (exact numbers, vectors), so they are not noexcept;
// std::bad_alloc is the only exception that may leave them (ADR 0003,
// point 5).

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace solid_space::geom2d::detail {

/// The heading of a half piece along its carrier: forward along a line is
/// from its start toward its end (LineMeaning), forward along a circle is
/// counterclockwise. backward is the other way.
enum class Heading : std::uint8_t {
    forward,
    backward,
};

/// The other heading.
[[nodiscard]] Heading reversed(Heading heading) noexcept;

/// A half piece: the carrier (a whole line or circle) leaving a point in one
/// heading. The point is given apart (compareAround(), LocalOrder), and must
/// lie on the carrier.
struct HalfPiece {
    Carrier carrier;
    Heading heading = Heading::forward;
};

/// What the order reads of a half piece at its point, computed once: the
/// tangent in the point, as numbers a + b sqrt(radicand) with the radicand
/// of the point and scaled by a positive factor, and the signed curvature.
/// Built by departureOf() or rayDeparture().
struct Departure {
    /// The tangent: x = tangent.x + tangentRadical.x * sqrt(radicand), and
    /// the same for y. Never the zero vector.
    ExactVector tangent;
    ExactVector tangentRadical;
    Dyadic radicand;
    /// The sign of the curvature, seen in the heading: positive when the
    /// half piece turns left (a circle run counterclockwise), negative when
    /// it turns right, zero for a line.
    Sign bend = Sign::zero;
    /// For a circle, the squared radius is radiusSquared / weightSquared;
    /// both zero for a line.
    Dyadic radiusSquared;
    Dyadic weightSquared;
};

/// The departure of a half piece from a point. Preconditions (asserted in
/// debug): the carrier is not degenerate and the point lies on it
/// (onCarrier()).
[[nodiscard]] Departure departureOf(const ExactPoint& point, const HalfPiece& piece);

/// The departure of the half-line from a point toward +x: tangent (1, 0),
/// curvature zero. It is equal to the half piece of a line through the point
/// run toward +x, and it is the reference of winding.hpp.
[[nodiscard]] Departure rayDeparture(const ExactPoint& point);

/// The order of two departures from the same point: negative when a comes
/// first counterclockwise from +x (see above), zero when they are equal.
/// Precondition: both were computed from the same ExactPoint (the same
/// radicand; asserted in debug when both tangents use it).
[[nodiscard]] Sign compareDepartures(const Departure& a, const Departure& b);

/// The order of two half pieces leaving the same point: compareDepartures()
/// of their departures. Preconditions: as departureOf(), for both.
[[nodiscard]] Sign compareAround(const ExactPoint& point, const HalfPiece& a, const HalfPiece& b);

/// Where a reference falls among the sorted half pieces of a LocalOrder.
struct Placement {
    /// True when the reference is equal to the half pieces of group `index`
    /// (it runs along them near the point); false when it lies strictly
    /// inside sector `index`, between group index and group
    /// (index + 1) % groupCount(). With no half pieces it is in sector 0,
    /// the whole turn.
    bool along = false;
    std::size_t index = 0;
};

/// Half pieces leaving one point, sorted counterclockwise from +x, with the
/// groups of equal ones.
class LocalOrder {
public:
    /// No half pieces, around the origin.
    LocalOrder() = default;

    /// Sorts the half pieces around the point. Pieces of a group keep the
    /// order of the input. Preconditions: as departureOf(), for each piece.
    LocalOrder(const ExactPoint& point, std::span<const HalfPiece> pieces);

    /// The point the pieces leave.
    [[nodiscard]] const ExactPoint& point() const noexcept { return m_point; }

    /// The number of half pieces.
    [[nodiscard]] std::size_t size() const noexcept { return m_departures.size(); }

    /// The indices of the input half pieces, sorted: group 0 first.
    [[nodiscard]] std::span<const std::size_t> sorted() const noexcept { return m_sorted; }

    /// The number of groups (of sectors, when there is at least one group).
    [[nodiscard]] std::size_t groupCount() const noexcept { return m_groupStart.size(); }

    /// The indices of the input half pieces of group g, in input order.
    /// Precondition (asserted in debug): g < groupCount().
    [[nodiscard]] std::span<const std::size_t> group(std::size_t g) const;

    /// The group of input half piece i. Precondition (asserted in debug):
    /// i < size().
    [[nodiscard]] std::size_t groupOf(std::size_t i) const;

    /// The departure of input half piece i. Precondition (asserted in
    /// debug): i < size().
    [[nodiscard]] const Departure& departure(std::size_t i) const;

    /// Where a reference half piece leaving the same point falls.
    /// Precondition: as departureOf().
    [[nodiscard]] Placement place(const HalfPiece& reference) const;

    /// Where a departure computed from the same ExactPoint falls.
    [[nodiscard]] Placement place(const Departure& reference) const;

private:
    ExactPoint m_point;
    std::vector<Departure> m_departures;
    std::vector<std::size_t> m_sorted;
    // Position in m_sorted of the first piece of each group.
    std::vector<std::size_t> m_groupStart;
    // Group of each input piece.
    std::vector<std::size_t> m_groupOf;
};

}  // namespace solid_space::geom2d::detail
