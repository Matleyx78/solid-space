// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The rule of detectBoundary (boundary.hpp, steps 1-4; ADR 0014, Decisione,
// points 2 and 3): what the user meant, applied to the data before the exact
// decisions of the arrangement (ADR 0009, point 1). Internal to geom2d:
// nothing here is part of the public contract. Stage 0.3, package PS9.
//
// What is here:
// - step 1: the curves checked, the curves left out, the arcs converted to
//   pieces of polyline (or to their circle), and the free ends;
// - step 2: the links between ends within the tolerance of their curves;
// - step 3: the extensions of the free ends without a link;
// - step 4: whether the picked point lies within the tolerance of a curve or
//   of a link;
// - the curves of steps 1-3 as the arrangement (arrangement.hpp) takes them,
//   with the priorities of step 5.
//
// Exactness. The tolerance (robust/tolerance.hpp) gives the threshold; every
// comparison with it is exact, on the doubles of the data of step 1 and on
// the exact meetings of stage 0.2 (carriers.hpp, positions.hpp). The only
// rounded values are the data of step 1 themselves: the ends, middle points
// and bulges of the converted arcs, each within its declared error. The
// bounding boxes of arrangement.hpp only skip pairs that cannot matter.
//
// Determinism (ADR 0008, point 5): all the computation is in boundary.cpp,
// with no trigonometric function; every order is exact or by index. The
// functions may allocate, so they are not noexcept.

#include "arrangement.hpp"
#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/region.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace solid_space::geom2d::detail {

/// A curve of step 1: a Segment, a PolylineSegment (given, or made from an
/// Arc) or a Circle (given, or a whole-circle Arc).
struct RuleCurve {
    /// The index of the input curve in the span given to detectBoundary.
    std::size_t input = 0;
    /// The exact meaning of the curve of step 1.
    CurveMeaning meaning;
    Carrier carrier;
    /// The magnitude M of the input curve (for a converted Arc, of the Arc):
    /// the scale of steps 2, 3 and 4 (boundary.hpp, #232).
    double magnitude = 0.0;
    /// A whole circle: an input Circle or whole-circle Arc. It has no ends.
    bool whole = false;
    /// True for a whole-circle Arc with a negative sweep: its Circle runs
    /// counterclockwise, against the input curve (region.hpp, reversed).
    bool againstInput = false;
    /// For a whole circle, the Circle of the input, with its doubles.
    Circle circle;
    /// The doubles of the ends; for a whole circle, its start.
    Point2 start;
    Point2 end;
    /// A box that contains the curve (the whole circle of a circular one).
    Box box;
};

/// A link of step 2: the Segment between two ends.
struct RuleLink {
    /// The ends, by number (see BoundaryRule::ends), from < to.
    std::size_t fromEnd = 0;
    std::size_t toEnd = 0;
    Point2 from;
    Point2 to;
    /// The input indices of the two curves, lower first.
    std::size_t lowCurve = 0;
    std::size_t highCurve = 0;
    /// The larger magnitude of the two input curves: the scale of the link in
    /// step 4, as in step 2.
    double magnitude = 0.0;
};

/// The data after steps 1-3.
struct BoundaryRule {
    std::vector<RuleCurve> curves;
    std::vector<SkippedInput> skipped;
    /// The links, by (lowCurve, highCurve) and then by their ends: the order
    /// of their priorities in the arrangement.
    std::vector<RuleLink> links;
    // The ends are numbered 2k for the start of curve k and 2k + 1 for its
    // end; the entries of a whole circle are never set.
    /// Free after step 1: not a common point with another curve.
    std::vector<bool> freeEnd;
    /// A link of step 2 ends there.
    std::vector<bool> linked;
    /// Where step 3 moved the end, on the extension of its curve.
    std::vector<std::optional<ExactPoint>> moved;
    /// The largest magnitude of the curves kept.
    double largestMagnitude = 0.0;

    /// True when an end is still free after steps 2 and 3.
    [[nodiscard]] bool leftFree(std::size_t end) const {
        return freeEnd[end] && !linked[end] && !moved[end].has_value();
    }
};

/// Steps 1, 2 and 3 of detectBoundary.
[[nodiscard]] BoundaryRule applyRule(std::span<const Curve> curves,
                                     const robust::TolerancePolicy& policy);

/// The curves of steps 1-3 as the arrangement takes them: first the curves,
/// in the order of rule.curves, with their moved ends and the input index as
/// priority; then the links, in the order of rule.links, with priorities
/// above every input index. A curved piece whose two ends were moved to the
/// same point covers its whole circle, with that point on it.
[[nodiscard]] std::vector<ArrangementCurve> arrangementCurves(const BoundaryRule& rule,
                                                              std::size_t inputCount);

/// boxOf() (arrangement.hpp) of each curve of arrangementCurves(rule, ...),
/// taken from the boxes of step 1 where they are the same.
[[nodiscard]] std::vector<Box> arrangementBoxes(const BoundaryRule& rule,
                                                std::span<const ArrangementCurve> curves);

/// Step 4: the lowest input index of the curves (as extended in step 3) and
/// links within the tolerance of the picked point (for a link, the lower
/// index of its curves), or nothing. M counts the coordinates of the point
/// and the magnitudes of the input curves (of both curves for a link).
/// `curves` is arrangementCurves(rule, ...) and `boxes` arrangementBoxes() of
/// them. Precondition: the point is finite and within the domain.
[[nodiscard]] std::optional<std::size_t> nearestCurve(const BoundaryRule& rule,
                                                      std::span<const ArrangementCurve> curves,
                                                      std::span<const Box> boxes, Point2 picked,
                                                      const robust::TolerancePolicy& policy);

/// The pieces of polyline that stand for an Arc that is not a whole circle
/// (step 1): from its rounded start to its rounded end (endpoints(),
/// curves.hpp), split at a rounded middle point when the arc turns by more
/// than half a turn, each within kRelativeLoopError / 2 of the arc. None when
/// the rounded ends of the arc coincide; a piece whose ends coincide (only
/// below the bounds of curves.hpp, M < 2^-50) is left out too.
/// Precondition: the arc is checked, not degenerate and not a whole circle.
[[nodiscard]] std::vector<PolylineSegment> convertedArc(const Arc& arc, const ArcMeaning& meaning);

/// The bulge of the arc of a circle from `from` to `to`, turning by at most
/// half a turn: positive when it turns counterclockwise. From half the chord
/// c and the distance d from the centre to the chord, tan(theta / 4) =
/// c / (r + d): no trigonometry and no cancellation. Within a few units
/// of 2^-53 * max(|p|, |centre|, r) / c of the exact one, so the arc it
/// gives moves by a few units of 2^-53 * max(|p|, |centre|, r) at most.
[[nodiscard]] double arcBulge(Point2 from, Point2 to, Point2 centre, double radius,
                              bool counterclockwise);

/// The rounded middle point of the stretch of a circle from `from` to `to`,
/// turning by more than half a turn in the given direction: the point of
/// the circle in a direction computed from the rounded ends, so within a
/// few units of 2^-53 * max(|p|, |centre|, r) of the exact middle.
[[nodiscard]] Point2 stretchMiddle(const CircleSupport& circle, Point2 from, Point2 to,
                                   bool counterclockwise);

/// The sign of position(a) - position(b) on a circular curve, both measured
/// from `origin` in the direction of the curve, in [0, 2 pi). Preconditions:
/// the three points lie on the carrier of the curve.
[[nodiscard]] Sign compareFrom(const CurveMeaning& curve, const ExactPoint& origin,
                               const ExactPoint& a, const ExactPoint& b);

/// True when a point of the carrier of an arrangement curve lies on its
/// stretch, ends included.
[[nodiscard]] bool onStretch(const ArrangementCurve& curve, const ExactPoint& point);

}  // namespace solid_space::geom2d::detail
