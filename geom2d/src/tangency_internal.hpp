// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Tangent circles with their exact decisions (tangency.hpp; ADR 0011). Internal
// to geom2d: nothing here is part of the public contract. Stage 0.2, package
// P15; P16 (tangentCircleNear and fillet) chooses among these candidates and
// may only add to this header.
//
// What is here:
// - the positions of the points of a tower (offset_carriers.hpp, TowerPoint)
//   along a curve: their Location, their exact order and their station, with
//   the rules of positions.hpp. positions.hpp handles the points of one root
//   (ExactPoint); the centres and the points of tangency of tangent circles have
//   nested roots, so their positions are computed here, from the same exact
//   tests and with the same rounding rule: the station is the correctly rounded
//   value of the same exact quantity, so a point that both headers can
//   represent gets the same Location and the same bits from both;
// - the correctly rounded value of a point of a tower;
// - the candidates of tangentCircles(): every tangent circle with its exact
//   centre, its exact points of tangency and the rounded TangentCircle that the
//   contract returns, in the order of the contract, so that P16 can decide
//   about them (sides, the direction of an arc) without computing them again.
//   Each candidate keeps the Tower of its own exact numbers: the roots of the
//   two curves and of one discriminant, at most three. One tower for all the
//   candidates would hold up to six roots, and every sign at that depth costs
//   up to 3^6 signs of products (exact/tower.hpp, "Cost").
//
// Floating-point filter. Every sign of a number of a tower is first read from
// a double with a bound of its error, computed with the roots of the tower in
// floating point; the exact sign decides only when the bound does not
// (tangency.cpp). The roundings start from the same doubles.
//
// Decisions are exact, with no tolerance. All the computation is in
// tangency.cpp (ADR 0008, point 5); no trigonometry. The functions may
// allocate (exact numbers), so they are not noexcept.

#include "curve_meaning.hpp"
#include "offset_carriers.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/tangency.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace solid_space::geom2d::detail {

/// The exact location of a point of the tower on a curve and its extension, as
/// locate() of positions.hpp decides it for an ExactPoint. Preconditions: the
/// meaning is not degenerate, the point lies exactly on its carrier and, for a
/// circular curve, is not its centre.
[[nodiscard]] Location locate(const Tower& tower, const CurveMeaning& curve,
                              const TowerPoint& point);

/// The exact order of two points of the tower along a curve, as compareAlong()
/// of positions.hpp: a straight curve along its directed line, a circular one
/// from its start D0 in its direction over the whole circle. Preconditions: as
/// locate(), for both points.
[[nodiscard]] Sign compareAlong(const Tower& tower, const CurveMeaning& curve, const TowerPoint& a,
                                const TowerPoint& b);

/// The station of a point of the tower, with the rule of station() of
/// positions.hpp: a non-decreasing function of the exact position, the same
/// bits for every representation of the same point, and the same bits as
/// station() for a point that is also an ExactPoint. Within
/// kRelativeStationError * max(M, |s|). Preconditions: as locate().
[[nodiscard]] double station(const Tower& tower, const CurveMeaning& curve,
                             const TowerPoint& point);

/// locate() and station() together.
[[nodiscard]] CurvePosition positionOn(const Tower& tower, const CurveMeaning& curve,
                                       const TowerPoint& point);

/// The point rounded to doubles, each coordinate correctly rounded (nearest,
/// ties to even, +0 for an exact zero), as nearestPoint() of positions.hpp:
/// the same bits for every representation of the same point. Precondition:
/// the coordinates are below 2^1000 in magnitude (for example, not
/// beyondDomain()).
[[nodiscard]] Point2 nearestPoint(const Tower& tower, const TowerPoint& point);

/// The diagnoses of a radius (tangency.hpp, input `input`): invalidValue when
/// it is negative or not finite, outOfDomain when it is beyond
/// robust::kMaxCoordinate. A radius of zero passes: tangentCircles() rejects it
/// (degenerateCurve), fillet() accepts it. Nothing when the radius is valid.
[[nodiscard]] std::optional<Diagnosis> checkRadius(double radius, std::size_t input);

/// A tangent circle with its exact data.
struct TangentCandidate {
    /// The exact centre and points of tangency, numbers of `tower`.
    TowerPoint center;
    TowerPoint onFirst;
    TowerPoint onSecond;
    /// The circle as tangentCircles() returns it: rounded points, exact
    /// positions and sides.
    TangentCircle circle;
    /// The roots of the exact numbers above. Numbers of different candidates
    /// belong to different towers and must not be mixed.
    Tower tower;
};

/// The tangent circles of two curves, with their exact data.
struct TangentCandidates {
    /// In the order of tangency.hpp (TangentCircles): along the first curve,
    /// then along the second. Candidates that tie on both (the same points of
    /// tangency) follow the exact order of their centres, by x and then by y.
    std::vector<TangentCandidate> candidates;
    /// As TangentCircles::infinitelyMany; candidates is then empty.
    bool infinitelyMany = false;
};

/// The circles of the given radius tangent to both curves, each taken with its
/// extension (tangentCircles() of tangency.hpp, without the checks of its data).
///
/// Diagnosis outOfDomain about the radius (input 2) when a centre lies beyond
/// robust::kMaxCoordinate (two almost parallel lines): such a circle cannot be
/// returned within the declared error, and it is never left out silently.
/// Preconditions: neither meaning is degenerate, the radius is positive, finite
/// and at most robust::kMaxCoordinate.
[[nodiscard]] Result<TangentCandidates> tangentCandidates(const CurveMeaning& first,
                                                          const CurveMeaning& second,
                                                          double radius);

}  // namespace solid_space::geom2d::detail
