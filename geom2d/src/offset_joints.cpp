// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The exact steps of offsetPolyline (offset_joints.hpp; stage 0.2, package P14).
//
// Notation. A piece P moved by d keeps its exact meaning (curve_meaning.hpp):
// - a straight piece from P to Q, with U = Q - P and L = |U|^2, moves along its
//   left normal N = (-U.y, U.x), and an end X becomes
//       X + d N / sqrt(L) = (X L + d N sqrt(L)) / L,
//   a point of the tower over the weight L, on the line of offsetCarrier();
// - a curved piece with centre C / w and squared radius rho / w^2 moves along
//   the radius, inwards by d when it runs counterclockwise (left is inside) and
//   outwards when it runs clockwise; with s = d (counterclockwise) or -d
//   (clockwise), an end X becomes
//       X - s (X w - C) / sqrt(rho) = (X rho - s (X w - C) sqrt(rho)) / rho,
//   since |X w - C| = sqrt(rho). It lies on the circle of offsetCarrier().
// offsetCurve() rounds its moved ends with roundedPoint() too, so a polyline
// of one piece gives the bits of offsetCurve().
//
// Rounding (package P19). Every vertex is the nearest double to its exact
// value in each coordinate: the roundings of exact/ (within 2 ulp) are moved
// to the nearest double by exact comparisons with the midpoints of the
// neighbouring doubles. The error is at most 2^-53 |p| per coordinate (plus
// 2^-1075 for subnormals), inside kRelativePointError and inside the bound
// 2^-51 max(M, |p|) of the ends of curved pieces (offset.hpp, #177).
//
// The bulge and its representability (representArc(); offset.hpp,
// CurveOffset; #177, #181). The proof of the check follows.
//
// Notation. A is the exact arc: centre C, radius R > 0, exact ends S and E,
// turning as the input, exact sweep theta in (0, 2 pi). A' is the returned
// arc: ends S' and E', distinct, the nearest doubles of S and E; bulge b' of
// the rule of offset.hpp, within one ulp (exact::quadraticToDouble); centre
// C'' and radius R'' of its circle G'', half sweep h = 2 atan |b'|, w the
// unit vector from C'' to the middle of A', so that A' is the set of points
// C'' + R'' u with u a unit vector and u . w >= cos h. delta = C'' - C.
// S(A) is the largest absolute coordinate of A, E = kRelativeOffsetError
// max(M, S(A)), and d_H the Hausdorff distance of A and A'. Every double
// below is computed from exact numbers with a relative error far below
// kMargin = 2^-40, which every bound absorbs.
//
// eta. Each coordinate of S' is within half an ulp of S, so
// |S' - S| <= eta = sqrt(2) (2^-52 max |coordinate of S', E'| + 2^-1074),
// with a factor 2 to spare. Since the ends lie on A, eta <= E / 45 whenever
// M >= 2^-50, where the error bounds apply (curves.hpp).
//
// Lemma 1 (the radial gap, computed, not bounded). Let |delta| < R''. For
// P = C'' + R'' u on A', |P - C| = sqrt(R''^2 + 2 R'' x + |delta|^2) with
// x = delta . u: the gap g(P) = |P - C| - R increases with x. Over A', x is
// extreme at an end or at u = +-delta / |delta| when that unit vector lies on
// A' (+-delta . w >= |delta| cos h). So G = max over A' of |g| is the largest
// of |rho_S| and |rho_E| (rho_X = |X - C| - R at the returned ends) and, for
// the directions that lie on A', of |R'' +- |delta| - R|. These are computed
// from the end X with the larger (smaller) x as
//     g(X) +- 2 R'' (|delta| -+ x) / ((R'' +- |delta|) + |X - C|),
// with |delta| - x = v^2 / (|delta| + x) when x > 0 (v = delta x u), and the
// same for the other sign, so that nothing cancels.
//
// Lemma 2 (the Hausdorff distance). Every point P of A' has
// d(P, A) >= d(P, circle of A) = |g(P)|, so d_H >= G. Conversely, let
// |delta| <= R'' / 2, R >= 64 eta, and let the directions from C of A' not
// wrap around those of A (lemma 3). Every ray from C meets G'' once, and the
// directions of A and A' differ only in the wedges between those of S and S'
// and of E and E', each of angle alpha <= (pi / 2) eta / R <= pi / 128. A
// point of A' outside the directions of A is within |g| + R alpha <=
// G + (pi / 2) eta of S or E; a point of A outside the directions of A' is
// within R alpha of S or E, so within (pi / 2 + 1) eta of S' or E'; every
// other point is within |g| <= G of the point of the other arc on its ray
// from C. So
//     G <= d_H <= G + (pi / 2 + 1) eta.
//
// Lemma 3 (no wrap). Seen from C, A' sweeps an angle Theta in (0, 2 pi) with
// |Theta - 2 h| <= 2 asin(|delta| / R'') <= pi / 3, and
// Theta = theta + (alpha_E - alpha_S) + 2 pi k with k in {-1, 0, 1}. A wrap,
// k != 0, needs theta < 2 alpha or theta > 2 pi - 2 alpha, puts theta and
// Theta on opposite sides of pi (decided exactly: the oriented cross products
// of S - C, E - C and of S' - C, E' - C) and gives |2 h - pi| >= pi - pi / 3
// - pi / 64 > 2.04. Without a wrap and on opposite sides, both are within
// 2 alpha of pi and |2 h - pi| <= pi / 3 + pi / 64 < 1.1. The test rejects
// opposite sides with |b'| outside [0.43, 2.3], that is |2 h - pi| > 1.5:
// every wrap, and no arc without one. A wrap leaves A' near a whole turn and
// A near a point, or the reverse; then one arc has points at distance nearly
// twice its radius from the ends of the other, and the crude test below has
// already failed, so d_H > E / 2: rejecting it is allowed.
//
// The check. With U an upper bound of d_H and S_lo a lower bound of S(A')
// (the ends and the point m of A', and above |b'| = 1/8 the extrema of G''
// that lie on A' by a margin), S(A) >= S_lo - U, so T(U) =
// 2^-46 max(M, S_lo - U) (1 - kMargin) <= E. The arc is representable when
// U <= T(U): sound, never beyond E. The branches, in order:
// - crude: d(P, A) <= |P - S'| + eta for P on A', and d(Q, A') <= |Q - S| +
//   eta for Q on A; |P - S'| is at most the chord for |b'| <= 1 and 2 R''
//   otherwise, |Q - S| at most the exact chord (<= chord' + 2 eta) for theta
//   < pi and 2 R otherwise. U is the largest plus eta;
// - flat, |b'| < 2^-500 (lemma 5);
// - radial, when |delta| <= R'' / 2 and R >= 64 eta: lemma 3, then
//   U = G + (pi / 2 + 1) eta;
// - sampled, for arcs no longer than 64 T(0) (lemma 6); any other arc is
//   rejected.
//
// Lemma 4 (complete: never failed within E / 2). Let d_H <= E / 2. S(A') <=
// S(A) + E / 2, and S(A') <= 1.18 S_lo: below |b'| = 1/8 every point of A' is
// within its sagitta (at most chord / 16, so 0.18 times the largest end
// coordinate) of its chord; above, S_lo has every extremum but those within
// 2^-15 radians of an end. So T >= E / 1.182 > 0.84 E. In the radial branch
// U <= G + 2.6 eta <= E / 2 + 2.6 E / 45 < 0.56 E: accepted. A wrap is
// excluded by lemma 3. When the radial branch does not apply, d_H <= E / 2
// leaves only short arcs (lemma 6). So the check never fails within E / 2.
//
// Lemma 5 (flat and huge arcs). |b'| < 2^-500 means t > 2^498 in the rule, so
// C is farther than 2^497 times the chord from the chord of A': a minor exact
// arc then has a sagitta below 2^-495 times its chord, and d_H <= eta +
// 2^-495 (chord + 2 eta). A major exact arc with such a centre has a radius
// beyond 2^497 times the chord, d_H > E, rejected. Ends beyond 2^500, where
// the doubles below would overflow, are first scaled down by 2^-600: exact,
// and the answer does not change.
//
// Lemma 6 (short arcs). If the crude test fails and the radial branch does not
// apply but d_H <= E / 2, both arcs are short: (a) R < 64 eta < 2 E puts A and
// so A' inside a disk of radius 2.5 E around C; (b) |delta| > R'' / 2 with
// two minor arcs needs a chord below 18 eta (the bisector of S' E' turns by at
// most 2 eta / chord around a point within R of C), where the crude test
// accepts; with A' major, the radial gaps of a half circle of G'' span at
// least R'' / 3, so R'' <= 6 d_H <= 3 E; with A major and A' minor, the same
// for A seen from C'' gives min(|delta|, R) <= 4 d_H, so again a disk of a
// few E. In each case both arcs are shorter than 32 E < 64 T(0). The sampled
// check reads both arcs in coordinates centred on S', where every double is
// within a few ulp of 64 E: A and A' by their centres and radii, or by their
// chords within their sagittas when flatter than 2^-20; 4097 samples of
// each, the distance of every sample to the other arc, plus half the spacing,
// the sagittas and the rounding. That bound is within E / 64 of d_H, so it is
// sound, and it accepts every pair with d_H <= E / 2.
//
// Lemma 7 (the threshold of #177). Let theta <= 2 pi - 1/4 and M >= 2^-50.
// The true end errors are at most e = sqrt(2) 2^-53 S(A), so |rho_S|,
// |rho_E| <= e. Write delta = lambda c + mu w, with c the unit chord of A'.
// The rule puts the exact rule centre on the bisector of S' E', so lambda
// comes from the ends only and mu from the rounding of b' alone:
// |mu| <= R'' 2^-51. In psi, the angle of u from w, g = (R'' - R) + lambda
// sin psi + mu cos psi + q, with 0 <= q <= |delta|^2 / (2 (R'' - |delta|)),
// and the two ends fix the first three terms:
//     g = rho_m + rho_d sin psi / sin h + mu (cos psi - cos h) + (q - its
//         interpolation between the ends),
// rho_m and rho_d the half sum and half difference of rho_E and rho_S. So
//     G <= e max(1, 1 / sin h) + |mu| (1 - cos h) + q (2 + 1 / sin h).
// With 2 h within 2 eta / chord of theta, 1 / sin h <= 1 / sin(1/8) < 8.1;
// R'' (1 - cos h) is at most the chord for |b'| <= 1 and 2 R'' <= 2 sqrt(2)
// S(A') otherwise (an arc beyond half a turn has a coordinate at least
// R'' / sqrt(2)), so |mu| (1 - cos h) <= 2^-50 S(A); for theta >= 1/4,
// |delta| <= eta (1 + cot(1/8)) + |mu| < 10 eta and R'' >= 42 eta in the
// radial branch (R >= 64 eta, |delta| <= R'' / 2), so q (2 + 1 / sin h) <=
// 16 eta < 0.36 E; for theta < 1/4 the radial gap of lemma 1 is reached at an
// end unless mu dominates delta, so G <= e + 3 |mu| (1 - cos h). In every
// case G < 0.09 E + E / 16 + 0.36 E and G + 2.6 eta < 0.57 E < T: accepted.
// When the radial branch does not apply, the arcs are within a few E (lemma
// 6), d_H < 0.57 E, and the sampled bound, within E / 64 of d_H, stays below
// T: accepted. So no
// exact sweep up to 2 pi - 1/4 fails the check, in offsetCurve() and in
// offsetPolyline().

#include "offset_joints.hpp"

#include "exact/dyadic.hpp"
#include "exact/quadratic.hpp"
#include "solid_space/robust/predicates.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

using exact::Tower;
using exact::TowerNumber;

const PolylineSegment& segmentOf(const CurveMeaning& meaning) {
    return std::get<PolylineSegment>(meaning.curve);
}

Point2 endOf(const PolylineSegment& segment, PieceEnd end) {
    return end == PieceEnd::start ? segment.start : segment.end;
}

// A vector with coordinates of the tower.
struct TowerVector {
    TowerNumber x;
    TowerNumber y;
};

// The vector from the centre c / w to the point (x, y) / W, times w W > 0.
TowerVector fromCenter(const OffsetCircle& circle, const TowerPoint& point) {
    return {point.x * circle.weight - TowerNumber(circle.centerX * point.weight),
            point.y * circle.weight - TowerNumber(circle.centerY * point.weight)};
}

// The vector from a point of doubles to the point (x, y) / W, times W > 0.
TowerVector fromVertex(Point2 vertex, const TowerPoint& point) {
    return {point.x - TowerNumber(Dyadic(vertex.x) * point.weight),
            point.y - TowerNumber(Dyadic(vertex.y) * point.weight)};
}

TowerNumber dotOf(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.product(a.x, b.x) + tower.product(a.y, b.y);
}

Sign crossOf(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.compare(tower.product(a.x, b.y), tower.product(a.y, b.x));
}

// The exact direction of travel of a source piece at one of its ends: U for a
// straight piece; for a curved one, the radius X w - C turned by a quarter in
// the direction of the piece.
ExactVector tangentAt(const CurveMeaning& meaning, PieceEnd end) {
    const auto& segment = segmentOf(meaning);
    if (std::holds_alternative<LineMeaning>(meaning.support)) {
        return {Dyadic(segment.end.x) - Dyadic(segment.start.x),
                Dyadic(segment.end.y) - Dyadic(segment.start.y)};
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const Point2 point = endOf(segment, end);
    const ExactVector radial{Dyadic(point.x) * arc.circle.weight - arc.circle.centerX,
                             Dyadic(point.y) * arc.circle.weight - arc.circle.centerY};
    if (arc.turn == Turn::counterclockwise) {
        return {-radial.y, radial.x};
    }
    return {radial.y, -radial.x};
}

// How the direction of `to` lies from the direction of `from`, both seen from
// the centre: the cross product oriented in the direction of the circle, and
// the dot product.
struct Rotation {
    Sign cross = Sign::zero;
    Sign dot = Sign::positive;
};

Rotation rotationOn(const Tower& tower, const OffsetCircle& circle, const TowerPoint& from,
                    const TowerPoint& to) {
    const auto a = fromCenter(circle, from);
    const auto b = fromCenter(circle, to);
    return {oriented(circle.turn, crossOf(tower, a, b)), tower.sign(dotOf(tower, a, b))};
}

bool halfTurn(const Rotation& rotation) {
    return rotation.cross == Sign::zero && rotation.dot == Sign::negative;
}

Joint failedJoint(StepStatus status) {
    Joint joint;
    joint.status = status;
    return joint;
}

// The check of step 2 for an arc piece: its joint point must not lie exactly
// half a turn from its moved end, where the shortest rotation is not defined.
StepStatus checkRotation(OffsetScope& scope, const OffsetSource& source, std::size_t piece,
                         PieceEnd end, const TowerPoint& point) {
    if (std::holds_alternative<LineMeaning>(source.pieces[piece].support)) {
        return StepStatus::done;
    }
    const auto carrier = scope.carrier(piece);
    const auto moved = scope.moved(piece, end);
    if (!carrier || !moved) {
        return StepStatus::towerFull;
    }
    const auto rotation =
        rotationOn(scope.tower(), std::get<OffsetCircle>(*carrier), *moved, point);
    return halfTurn(rotation) ? StepStatus::selfIntersection : StepStatus::done;
}

// Step 2 when the carriers of A and B have isolated common points: the one
// nearest the vertex, a tie to the first along A from its moved start.
// Nothing when they have none, or overlap.
std::optional<Joint> meetingJoint(const OffsetSource& source, const JointRequest& request) {
    OffsetScope scope(source);
    auto& tower = scope.tower();
    const auto meeting = scope.meeting(request.first, request.second);
    if (!meeting) {
        return failedJoint(StepStatus::towerFull);
    }
    if (meeting->sameCarrier || meeting->count == 0) {
        return std::nullopt;
    }
    std::size_t index = 0;
    if (meeting->count == 2) {
        const auto& first = meeting->points[0].point;
        const auto& second = meeting->points[1].point;
        const Sign nearer = compareDistances(tower, towerPoint(request.vertex), first, second);
        if (nearer == Sign::positive) {
            index = 1;
        } else if (nearer == Sign::zero) {
            const auto carrier = scope.carrier(request.first);
            const auto start = scope.moved(request.first, PieceEnd::start);
            if (!carrier || !start) {
                return failedJoint(StepStatus::towerFull);
            }
            if (compareAlong(tower, *carrier, *start, second, first) == Sign::negative) {
                index = 1;
            }
        }
    }
    // The joint point is not checked against the domain here: only the final
    // result is (offset.hpp; #149, #154), after later steps cut or remove it.
    const auto& point = meeting->found()[index].point;
    for (const auto& [piece, end] :
         {std::pair{request.first, PieceEnd::end}, std::pair{request.second, PieceEnd::start}}) {
        const auto status = checkRotation(scope, source, piece, end, point);
        if (status != StepStatus::done) {
            return failedJoint(status);
        }
    }
    Joint joint;
    joint.kind = JointKind::meeting;
    joint.point = PointRecipe::meetingPoint(request.first, request.second, index);
    return joint;
}

// Step 2 for consecutive source pieces whose carriers do not meet: the arc of
// radius |d| centred on V from the end of A to the start of B, turning as the
// input turns at V.
Joint arcJoint(const OffsetSource& source, const JointRequest& request) {
    const auto tangentA = tangentAt(source.pieces[request.first], PieceEnd::end);
    const auto tangentB = tangentAt(source.pieces[request.second], PieceEnd::start);
    Turn turn = Turn::counterclockwise;
    switch (crossSign(tangentA, tangentB)) {
        case Sign::positive:
            break;
        case Sign::negative:
            turn = Turn::clockwise;
            break;
        case Sign::zero:
            // The same tangent would give the same moved ends, which need no
            // joint: no arc turns from one to the other.
            if (dotSign(tangentA, tangentB) != Sign::negative) {
                return failedJoint(StepStatus::openJoint);
            }
            // Opposite tangents: the half circle that continues the pieces,
            // clockwise for a positive distance (offset.hpp).
            turn = source.distance > 0.0 ? Turn::clockwise : Turn::counterclockwise;
            break;
    }
    OffsetScope scope(source);
    auto& tower = scope.tower();
    const auto end = scope.point(request.firstEnd);
    const auto start = scope.point(request.secondStart);
    if (!end || !start) {
        return failedJoint(StepStatus::towerFull);
    }
    const Dyadic distance(source.distance);
    const TowerNumber radiusSquared(distance * distance);
    const auto toEnd = fromVertex(request.vertex, *end);
    const auto toStart = fromVertex(request.vertex, *start);
    // Both ends at |d| from V: |toEnd|^2 = d^2 W^2, and the same for the start.
    if (tower.compare(dotOf(tower, toEnd, toEnd), radiusSquared * (end->weight * end->weight)) !=
            Sign::zero ||
        tower.compare(dotOf(tower, toStart, toStart),
                      radiusSquared * (start->weight * start->weight)) != Sign::zero) {
        return failedJoint(StepStatus::openJoint);
    }
    Joint joint;
    joint.kind = JointKind::arc;
    joint.turn = turn;
    return joint;
}

}  // namespace

std::optional<OffsetCarrier> OffsetScope::carrier(std::size_t piece) {
    for (const auto& [index, carrier] : m_carriers) {
        if (index == piece) {
            return carrier;
        }
    }
    auto result = offsetCarrier(m_tower, m_source->pieces[piece], m_source->distance);
    assert(result.status != OffsetStatus::vanished && "a vanished piece has no carrier");
    if (result.status != OffsetStatus::done) {
        return std::nullopt;
    }
    m_carriers.emplace_back(piece, result.carrier);
    return std::move(result.carrier);
}

std::optional<TowerPoint> OffsetScope::moved(std::size_t piece, PieceEnd end) {
    const auto& meaning = m_source->pieces[piece];
    const auto& segment = segmentOf(meaning);
    const Point2 point = endOf(segment, end);
    const Dyadic distance(m_source->distance);
    const Dyadic x(point.x);
    const Dyadic y(point.y);
    if (std::holds_alternative<LineMeaning>(meaning.support)) {
        // The radicand as offsetCarrier() writes it, so that the root is shared.
        const ExactVector direction{Dyadic(segment.end.x) - Dyadic(segment.start.x),
                                    Dyadic(segment.end.y) - Dyadic(segment.start.y)};
        const Dyadic squaredLength = direction.x * direction.x + direction.y * direction.y;
        const auto length = m_tower.squareRoot(TowerNumber(squaredLength));
        if (!length) {
            return std::nullopt;
        }
        return TowerPoint{TowerNumber(x * squaredLength) + *length * (distance * -direction.y),
                          TowerNumber(y * squaredLength) + *length * (distance * direction.x),
                          squaredLength};
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const auto& circle = arc.circle;
    const auto radius = m_tower.squareRoot(TowerNumber(circle.radiusSquared));
    if (!radius) {
        return std::nullopt;
    }
    const Dyadic inward = arc.turn == Turn::counterclockwise ? distance : -distance;
    const ExactVector radial{x * circle.weight - circle.centerX,
                             y * circle.weight - circle.centerY};
    return TowerPoint{TowerNumber(x * circle.radiusSquared) - *radius * (inward * radial.x),
                      TowerNumber(y * circle.radiusSquared) - *radius * (inward * radial.y),
                      circle.radiusSquared};
}

std::optional<OffsetMeeting> OffsetScope::meeting(std::size_t first, std::size_t second) {
    const auto key = std::pair{first, second};
    for (const auto& [pair, meeting] : m_meetings) {
        if (pair == key) {
            return meeting;
        }
    }
    const auto firstCarrier = carrier(first);
    const auto secondCarrier = carrier(second);
    if (!firstCarrier || !secondCarrier) {
        return std::nullopt;
    }
    auto result = meetOffset(m_tower, *firstCarrier, *secondCarrier);
    if (result) {
        m_meetings.emplace_back(key, *result);
    }
    return result;
}

std::optional<TowerPoint> OffsetScope::point(const PointRecipe& recipe) {
    if (recipe.kind == PointRecipe::Kind::moved) {
        return moved(recipe.piece, recipe.end);
    }
    const auto found = meeting(recipe.piece, recipe.other);
    if (!found) {
        return std::nullopt;
    }
    const auto points = found->found();
    assert(recipe.index < points.size() && "a recipe names a point of its meeting");
    return points[recipe.index].point;
}

Joint join(const OffsetSource& source, const JointRequest& request) {
    {
        OffsetScope scope(source);
        const auto end = scope.point(request.firstEnd);
        const auto start = scope.point(request.secondStart);
        if (!end || !start) {
            return failedJoint(StepStatus::towerFull);
        }
        if (samePoint(scope.tower(), *end, *start)) {
            return Joint{};
        }
    }
    if (const auto joint = meetingJoint(source, request)) {
        return *joint;
    }
    if (!request.consecutive) {
        return failedJoint(StepStatus::openJoint);
    }
    return arcJoint(source, request);
}

// A straight piece runs forward when its end lies after its start along the
// line. A curved one is measured by its unwrapped angles from the direction of
// its moved start S0, in the direction of the piece: the moved end E0 lies at
// alpha in (0, 2 pi). Each new end lies at its position p in [0, 2 pi) on the
// circle (compareAlong from S0) plus a whole number of turns k, fixed by the
// shortest rotation from its moved end:
// - the start, reached from S0 by a rotation in (-pi, pi): k = 0 forwards
//   (or none), k = -1 backwards;
// - the end, reached from E0 = alpha by a rotation in (-pi, pi): forwards,
//   k = 0 if p > alpha, else it passed 2 pi and k = 1; backwards, k = 0 if
//   p < alpha, else it passed 0 and k = -1.
// The new sweep is (p_end - p_start) + 2 pi (k_end - k_start), with
// |p_end - p_start| < 2 pi: negative when k_end - k_start < 0, a full turn or
// more when it is 2, or 1 with p_end >= p_start; otherwise the sign of
// p_end - p_start decides.
PieceCourse courseOf(const OffsetSource& source, std::size_t piece, const PointRecipe& start,
                     const PointRecipe& end) {
    OffsetScope scope(source);
    auto& tower = scope.tower();
    const auto carrier = scope.carrier(piece);
    const auto first = scope.point(start);
    const auto last = scope.point(end);
    if (!carrier || !first || !last) {
        return {StepStatus::towerFull, Course::forward};
    }
    const auto courseFrom = [](Sign order) {
        switch (order) {
            case Sign::negative:
                return Course::backward;
            case Sign::zero:
                return Course::point;
            case Sign::positive:
                break;
        }
        return Course::forward;
    };
    if (std::holds_alternative<OffsetLine>(*carrier)) {
        return {StepStatus::done, courseFrom(compareAlong(tower, *carrier, *first, *last, *first))};
    }
    const auto& circle = std::get<OffsetCircle>(*carrier);
    const auto movedStart = scope.moved(piece, PieceEnd::start);
    const auto movedEnd = scope.moved(piece, PieceEnd::end);
    if (!movedStart || !movedEnd) {
        return {StepStatus::towerFull, Course::forward};
    }
    const auto startRotation = rotationOn(tower, circle, *movedStart, *first);
    const auto endRotation = rotationOn(tower, circle, *movedEnd, *last);
    if (halfTurn(startRotation) || halfTurn(endRotation)) {
        return {StepStatus::selfIntersection, Course::forward};
    }
    const int startTurns = startRotation.cross == Sign::negative ? -1 : 0;
    int endTurns = 0;
    if (endRotation.cross != Sign::zero) {
        const Sign fromMoved = compareAlong(tower, *carrier, *movedStart, *last, *movedEnd);
        if (endRotation.cross == Sign::positive) {
            endTurns = fromMoved == Sign::positive ? 0 : 1;
        } else {
            endTurns = fromMoved == Sign::negative ? 0 : -1;
        }
    }
    const int turns = endTurns - startTurns;
    if (turns < 0) {
        return {StepStatus::done, Course::backward};
    }
    if (turns > 1) {
        return {StepStatus::selfIntersection, Course::forward};
    }
    const Sign order = compareAlong(tower, *carrier, *movedStart, *last, *first);
    if (turns == 1) {
        if (order != Sign::negative) {
            return {StepStatus::selfIntersection, Course::forward};
        }
        return {StepStatus::done, Course::forward};
    }
    return {StepStatus::done, courseFrom(order)};
}

namespace {

// The midpoint of two doubles, exactly.
Dyadic midpointOf(double a, double b) {
    return (Dyadic(a) + Dyadic(b)).timesPowerOfTwo(-1);
}

// More steps than the roundings of exact/ need: they are within 2 ulp.
constexpr int kMaxNearestSteps = 8;

// The double nearest to an exact value, from a double within a few ulp of it.
// against(m) is the sign of value - m for a dyadic m. A tie stays where the
// steps reach it, so the answer is deterministic. An infinite guess (a value
// beyond the doubles) stays as it is.
template <typename Against>
double nearestDouble(double guess, const Against& against) {
    if (!std::isfinite(guess)) {
        return guess;
    }
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    double value = guess;
    for (int step = 0; step < kMaxNearestSteps; ++step) {
        const double up = std::nextafter(value, kInfinity);
        if (std::isfinite(up) && against(midpointOf(value, up)) == Sign::positive) {
            value = up;
            continue;
        }
        const double down = std::nextafter(value, -kInfinity);
        if (std::isfinite(down) && against(midpointOf(down, value)) == Sign::negative) {
            value = down;
            continue;
        }
        break;
    }
    return value;
}

// x / weight to the nearest double, for a positive weight.
double nearestQuotient(const Tower& tower, const TowerNumber& x, const Dyadic& weight) {
    return nearestDouble(tower.quotientToDouble(x, weight),
                         [&](const Dyadic& m) { return tower.sign(x - TowerNumber(m * weight)); });
}

// (a + b sqrt(d)) / c to the nearest double, for a positive c.
double nearestQuadratic(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& c) {
    return nearestDouble(exact::quadraticToDouble(a, b, d, c),
                         [&](const Dyadic& m) { return exact::signOf(a - m * c, b, d); });
}

}  // namespace

RoundedPoint roundedPoint(const OffsetSource& source, const PointRecipe& recipe) {
    OffsetScope scope(source);
    const auto point = scope.point(recipe);
    if (!point) {
        return {StepStatus::towerFull, {}, false};
    }
    RoundedPoint result;
    result.beyondDomain = beyondDomain(scope.tower(), *point);
    if (recipe.kind == PointRecipe::Kind::meeting) {
        result.point = {nearestQuotient(scope.tower(), point->x, point->weight),
                        nearestQuotient(scope.tower(), point->y, point->weight)};
        return result;
    }
    // A moved end, (a + b sqrt(r)) / r for each coordinate, with the
    // arguments of the notation at the top of this file.
    const auto& meaning = source.pieces[recipe.piece];
    const auto& segment = segmentOf(meaning);
    const Point2 end = endOf(segment, recipe.end);
    const Dyadic distance(source.distance);
    const Dyadic x(end.x);
    const Dyadic y(end.y);
    if (std::holds_alternative<LineMeaning>(meaning.support)) {
        const ExactVector normal{Dyadic(segment.start.y) - Dyadic(segment.end.y),
                                 Dyadic(segment.end.x) - Dyadic(segment.start.x)};
        const Dyadic squaredLength = normal.x * normal.x + normal.y * normal.y;
        result.point = {
            nearestQuadratic(x * squaredLength, distance * normal.x, squaredLength, squaredLength),
            nearestQuadratic(y * squaredLength, distance * normal.y, squaredLength, squaredLength)};
        return result;
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const auto& circle = arc.circle;
    const Dyadic outward = arc.turn == Turn::counterclockwise ? -distance : distance;
    const ExactVector radial{x * circle.weight - circle.centerX,
                             y * circle.weight - circle.centerY};
    result.point = {nearestQuadratic(x * circle.radiusSquared, outward * radial.x,
                                     circle.radiusSquared, circle.radiusSquared),
                    nearestQuadratic(y * circle.radiusSquared, outward * radial.y,
                                     circle.radiusSquared, circle.radiusSquared)};
    return result;
}

std::optional<ExactArc> sourceArc(OffsetScope& scope, std::size_t piece, const PointRecipe& start,
                                  const PointRecipe& end) {
    const auto carrier = scope.carrier(piece);
    const auto first = scope.point(start);
    const auto last = scope.point(end);
    if (!carrier || !first || !last) {
        return std::nullopt;
    }
    return ExactArc{std::get<OffsetCircle>(*carrier), *first, *last};
}

std::optional<ExactArc> gapArc(OffsetScope& scope, Point2 vertex, Turn turn,
                               const PointRecipe& start, const PointRecipe& end) {
    const auto first = scope.point(start);
    const auto last = scope.point(end);
    if (!first || !last) {
        return std::nullopt;
    }
    const Dyadic distance(scope.source().distance);
    const OffsetCircle circle{Dyadic(vertex.x), Dyadic(vertex.y), Dyadic(1.0),
                              TowerNumber(distance * distance), turn};
    return ExactArc{circle, *first, *last};
}

namespace {

Dyadic dotOf(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

Dyadic crossOf(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

ExactVector plus(const ExactVector& a, const ExactVector& b) {
    return {a.x + b.x, a.y + b.y};
}

ExactVector minus(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

ExactVector times(const ExactVector& v, const Dyadic& factor) {
    return {v.x * factor, v.y * factor};
}

// Bits of the square roots below: 2^-63 relative, far below the margins.
constexpr std::int64_t kRootBits = 64;

// sqrt(squared) / divisor, for a non-negative squared and a positive divisor:
// within 2^-52 relative (floorSqrt within 2^-63, one correct rounding).
double lengthOver(const Dyadic& squared, const Dyadic& divisor) {
    return exact::quotientToDouble(floorSqrt(squared, kRootBits), divisor);
}

// sqrt(x / weightSquared) for a positive number of the tower, scaled by a
// power of two near `estimate` so that x / weightSquared neither overflows
// nor underflows as a double: within 2^-51 relative.
double towerRoot(const Tower& tower, const TowerNumber& x, const Dyadic& weightSquared,
                 double estimate) {
    int exponent = 0;
    if (estimate > 0.0 && std::isfinite(estimate)) {
        static_cast<void>(std::frexp(estimate, &exponent));
    }
    const double scaled = tower.quotientToDouble(
        x.timesPowerOfTwo(-2 * static_cast<std::int64_t>(exponent)), weightSquared);
    return std::ldexp(std::sqrt(scaled), exponent);
}

// The largest absolute coordinate of a point.
double largestCoordinate(Point2 p) {
    return std::max(std::fabs(p.x), std::fabs(p.y));
}

// Relative margins of the doubles computed below (each a few roundings of
// 2^-53), taken far larger than needed.
constexpr double kMargin = 0x1p-40;
// Beyond this, representArc() scales the arc down first.
constexpr double kHugeCoordinate = 0x1p500;
// The returned bulge underflows only for an arc flatter than any drawing:
// below this, the flat bound of the proof decides.
constexpr double kFlatBulge = 0x1p-500;
// |b'| outside [kNoWrapLow, kNoWrapHigh] means a half sweep 2 h farther than
// 1.5 from pi (lemma 3 at the top of this file).
constexpr double kNoWrapLow = 0.43;
constexpr double kNoWrapHigh = 2.3;
// Sweeps whose ends are this close, as cos h, are read as inside the arc only
// beyond it (lower bound of S(A')).
constexpr double kRangeMargin = 0x1p-30;
// The bulge above which the extrema of the circle count for S(A'): below it
// the sagitta is at most an eighth of the chord.
constexpr double kExtremaBulge = 0.125;
// Arcs up to this many E long are short enough for the sampled check.
constexpr double kShortArc = 64.0;
// Points per arc in the sampled check.
constexpr int kSamples = 4096;
constexpr double kPi = std::numbers::pi;

// The sampled check (lemma 6): an arc in coordinates centred on the returned
// start S', where every point of both arcs is small. An arc flatter than
// kSampledFlat (sweep below 4 kSampledFlat) is read as its chord, within its
// sagitta; any other arc by its centre and radius, which are then within a
// few chords of its ends.
constexpr double kSampledFlat = 0x1p-20;

struct SampledArc {
    Point2 start;
    Point2 end;
    bool flat = false;
    Point2 center;
    double radius = 0.0;
    double startAngle = 0.0;
    double sweep = 0.0;
    double turnSign = 1.0;
    /// The largest distance between the arc and what the samples read.
    double sagitta = 0.0;
    double length = 0.0;
};

double distanceOf(Point2 a, Point2 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

SampledArc sampledArc(Point2 start, Point2 end, Point2 center, double radius, double sweep,
                      double turnSign) {
    SampledArc arc;
    arc.start = start;
    arc.end = end;
    arc.turnSign = turnSign;
    arc.sweep = sweep;
    const double chord = distanceOf(start, end);
    if (sweep < 4.0 * kSampledFlat) {
        // tan(sweep / 4) < kSampledFlat: the sagitta is below chord kSampledFlat.
        arc.flat = true;
        arc.sagitta = chord * kSampledFlat;
        arc.length = chord * (1.0 + kSampledFlat);
        return arc;
    }
    arc.center = center;
    arc.radius = radius;
    arc.startAngle = robust::atan2(start.y - center.y, start.x - center.x);
    arc.length = radius * sweep;
    return arc;
}

Point2 pointOn(const SampledArc& arc, double fraction) {
    if (arc.flat) {
        return {arc.start.x + (arc.end.x - arc.start.x) * fraction,
                arc.start.y + (arc.end.y - arc.start.y) * fraction};
    }
    const auto direction = robust::sinCos(arc.startAngle + arc.turnSign * arc.sweep * fraction);
    return {arc.center.x + arc.radius * direction.cosine,
            arc.center.y + arc.radius * direction.sine};
}

double distanceToSegment(Point2 p, Point2 a, Point2 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double squared = dx * dx + dy * dy;
    const double along = ((p.x - a.x) * dx + (p.y - a.y) * dy) / squared;
    const double fraction = std::clamp(along, 0.0, 1.0);
    return distanceOf(p, {a.x + dx * fraction, a.y + dy * fraction});
}

double distanceToArc(Point2 p, const SampledArc& arc) {
    if (arc.flat) {
        return distanceToSegment(p, arc.start, arc.end);
    }
    const double toEnds = std::min(distanceOf(p, arc.start), distanceOf(p, arc.end));
    const double dx = p.x - arc.center.x;
    const double dy = p.y - arc.center.y;
    double along = arc.turnSign * (robust::atan2(dy, dx) - arc.startAngle);
    while (along < 0.0) {
        along += 2.0 * kPi;
    }
    while (along >= 2.0 * kPi) {
        along -= 2.0 * kPi;
    }
    if (along > arc.sweep) {
        return toEnds;
    }
    return std::min(toEnds, std::fabs(std::sqrt(dx * dx + dy * dy) - arc.radius));
}

// The largest distance from the samples of `from` to `to`.
double sampledDistance(const SampledArc& from, const SampledArc& to) {
    double largest = 0.0;
    for (int k = 0; k <= kSamples; ++k) {
        largest =
            std::max(largest, distanceToArc(pointOn(from, static_cast<double>(k) / kSamples), to));
    }
    return largest;
}

// The vector scaled so that its largest coordinate is 1: products of such
// vectors do not overflow.
Point2 normalized(Point2 v) {
    const double size = largestCoordinate(v);
    return {v.x / size, v.y / size};
}

// The angle from a to b in the direction of the turn, in (0, 2 pi]. Neither is
// zero.
double sweepFrom(Point2 a, Point2 b, double turnSign) {
    a = normalized(a);
    b = normalized(b);
    double sweep = turnSign * robust::atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y);
    if (sweep <= 0.0) {
        sweep += 2.0 * kPi;
    }
    return sweep;
}

}  // namespace

namespace {

// representArc() for coordinates within kHugeCoordinate.
ArcRepresentation representModerate(const Tower& tower, const ExactArc& arc, Point2 start,
                                    Point2 end, double magnitude) {
    const auto& circle = arc.circle;
    const bool counterclockwise = circle.turn == Turn::counterclockwise;
    const double turnSign = counterclockwise ? 1.0 : -1.0;
    const ExactVector center{circle.centerX, circle.centerY};
    const Dyadic& weight = circle.weight;
    const ExactVector first = exactVector(start);
    const ExactVector last = exactVector(end);
    const ExactVector middleTwice = plus(first, last);
    const ExactVector normal{first.y - last.y, last.x - first.x};
    const Dyadic chordSquared = dotOf(normal, normal);

    // The bulge of the rule: t = ((C - m) . n) / (n . n) = tNumerator / tDenominator.
    const Dyadic tNumerator =
        dotOf(minus(times(center, Dyadic(2.0)), times(middleTwice, weight)), normal);
    const Dyadic tDenominator = Dyadic(2.0) * weight * chordSquared;
    double bulge = exact::quadraticToDouble(
        Dyadic(-2.0) * tNumerator, Dyadic(counterclockwise ? 1.0 : -1.0),
        Dyadic(4.0) * tNumerator * tNumerator + tDenominator * tDenominator, tDenominator);
    ArcRepresentation result;
    if (!std::isfinite(bulge)) {
        return result;
    }
    if (bulge == 0.0) {
        bulge = std::copysign(std::numeric_limits<double>::denorm_min(), turnSign);
    }
    result.bulge = bulge;
    const double size = std::fabs(bulge);

    // What every branch needs.
    const double chord = lengthOver(chordSquared, Dyadic(1.0));
    const double ends = std::max(largestCoordinate(start), largestCoordinate(end));
    const double eta = std::numbers::sqrt2 * (0x1p-52 * ends + 0x1p-1074) * (1.0 + kMargin);
    const auto exactFrom = [&](const TowerPoint& p) {
        return std::pair{p.x * weight - TowerNumber(circle.centerX * p.weight),
                         p.y * weight - TowerNumber(circle.centerY * p.weight)};
    };
    const auto [sx, sy] = exactFrom(arc.start);
    const auto [ex, ey] = exactFrom(arc.end);
    const Sign sideExact =
        oriented(circle.turn, tower.compare(tower.product(sx, ey), tower.product(sy, ex)));
    const ExactVector fromCenterStart = minus(times(first, weight), center);
    const ExactVector fromCenterEnd = minus(times(last, weight), center);
    const Sign sideReturned = oriented(circle.turn, crossOf(fromCenterStart, fromCenterEnd).sign());
    const Dyadic weightSquared = weight * weight;
    const double distanceStart = lengthOver(dotOf(fromCenterStart, fromCenterStart), weight);
    const double distanceEnd = lengthOver(dotOf(fromCenterEnd, fromCenterEnd), weight);
    const double radius = towerRoot(tower, circle.radiusSquared, weightSquared, distanceStart);
    const double cosHalf =
        size <= 1.0 ? (1.0 - size * size) / (1.0 + size * size)
                    : ((1.0 / size) * (1.0 / size) - 1.0) / (1.0 + (1.0 / size) * (1.0 / size));

    // A lower bound of S(A'): the ends and the point m are on A'; above
    // kExtremaBulge also the extrema of its circle that lie on it.
    const Dyadic bulgeExact(bulge);
    const ExactVector arcMiddle = times(minus(middleTwice, times(normal, bulgeExact)), Dyadic(0.5));
    double lowerS =
        std::max(ends, largestCoordinate({arcMiddle.x.toDouble(), arcMiddle.y.toDouble()}) *
                           (1.0 - kMargin));
    // The centre C'' = (m2 KD + 2 KN n) / (2 KD), with KN = 1 - b'^2, KD = 4 b'.
    const Dyadic kn = Dyadic(1.0) - bulgeExact * bulgeExact;
    const Dyadic kd = Dyadic(4.0) * bulgeExact;
    const ExactVector centerTwiceKd = plus(times(middleTwice, kd), times(normal, Dyadic(2.0) * kn));
    const Dyadic centerDivisor = Dyadic(2.0) * kd;
    // S' - C'' and E' - C'', times 2 KD.
    const ExactVector toStart = minus(times(first, Dyadic(2.0) * kd), centerTwiceKd);
    const ExactVector toEnd = minus(times(last, Dyadic(2.0) * kd), centerTwiceKd);
    const Dyadic absCenterDivisor = centerDivisor.abs();
    const double radiusReturned = lengthOver(dotOf(toStart, toStart), absCenterDivisor);
    if (size > kExtremaBulge && std::isfinite(radiusReturned)) {
        const double cx = exact::quotientToDouble(centerTwiceKd.x, centerDivisor);
        const double cy = exact::quotientToDouble(centerTwiceKd.y, centerDivisor);
        // The direction w of the middle of A' from C'': -sign(b') n / |n|.
        const double wx = -std::copysign(1.0, bulge) * normal.x.toDouble() / chord;
        const double wy = -std::copysign(1.0, bulge) * normal.y.toDouble() / chord;
        const double threshold = cosHalf + kRangeMargin;
        const auto extremum = [&](double c) {
            return std::fabs(c) + radiusReturned;
        };
        if (wx >= threshold) {
            lowerS = std::max(lowerS, std::fabs(cx + radiusReturned) - kMargin * extremum(cx));
        }
        if (-wx >= threshold) {
            lowerS = std::max(lowerS, std::fabs(cx - radiusReturned) - kMargin * extremum(cx));
        }
        if (wy >= threshold) {
            lowerS = std::max(lowerS, std::fabs(cy + radiusReturned) - kMargin * extremum(cy));
        }
        if (-wy >= threshold) {
            lowerS = std::max(lowerS, std::fabs(cy - radiusReturned) - kMargin * extremum(cy));
        }
    }
    const auto boundOf = [&](double distance) {
        return 0x1p-46 * std::max(magnitude, lowerS - distance) * (1.0 - kMargin);
    };
    const auto accept = [&](double distance) {
        result.representable = std::isfinite(distance) && distance <= boundOf(distance);
        return result;
    };

    // Crude: every point of A' is within its extent of S', every point of A
    // within its extent of S.
    const double extentReturned = size <= 1.0 ? chord : 2.0 * radiusReturned;
    const double extentExact = sideExact == Sign::positive ? chord + 2.0 * eta : 2.0 * radius;
    const double crude = (std::max(extentReturned, extentExact) + eta) * (1.0 + kMargin);
    if (std::isfinite(crude) && crude <= boundOf(crude)) {
        result.representable = true;
        return result;
    }

    // Unreachable after the scaling at the top; kept so that no double below
    // is infinite.
    if (!std::isfinite(chord) || !std::isfinite(ends)) {
        return result;
    }
    // Flat: A' is its chord up to its sagitta, and so is A (lemma 5).
    if (size < kFlatBulge) {
        if (sideExact != Sign::positive) {
            return result;
        }
        return accept(eta + 0x1p-495 * (chord + 2.0 * eta));
    }
    if (!std::isfinite(radiusReturned)) {
        return result;
    }

    // delta = C'' - C = Delta / (2 KD w).
    const ExactVector delta = minus(times(centerTwiceKd, weight), times(center, centerDivisor));
    const Dyadic deltaDivisor = centerDivisor * weight;
    const double deltaLength = lengthOver(dotOf(delta, delta), deltaDivisor.abs());
    const Dyadic absDeltaDivisor = deltaDivisor.abs();

    const bool radial =
        std::isfinite(deltaLength) && deltaLength <= 0.5 * radiusReturned && radius >= 64.0 * eta;
    if (radial) {
        // Lemma 3: a returned arc that runs around the complement of A.
        if (sideExact != Sign::zero && sideReturned != Sign::zero && sideExact != sideReturned &&
            (size < kNoWrapLow || size > kNoWrapHigh)) {
            return result;
        }
        // Lemma 1: the gap is extreme at an end or where u is +-delta / |delta|.
        const auto gapAt = [&](const ExactVector& fromCenter, double distance) {
            const Dyadic power = dotOf(fromCenter, fromCenter);
            return tower.quotientToDouble(TowerNumber(power) - circle.radiusSquared,
                                          weightSquared * Dyadic(distance + radius));
        };
        const double gapStart = gapAt(fromCenterStart, distanceStart);
        const double gapEnd = gapAt(fromCenterEnd, distanceEnd);
        // x = delta . u and v = delta x u at the two ends.
        const auto along = [&](const ExactVector& toPoint) {
            const Dyadic divisor = absDeltaDivisor * floorSqrt(dotOf(toPoint, toPoint), kRootBits);
            return std::pair{exact::quotientToDouble(dotOf(delta, toPoint), divisor),
                             exact::quotientToDouble(crossOf(delta, toPoint), divisor)};
        };
        // delta = Delta / (2 KD w) and u = toX / (2 KD R'') with R'' = |toX| / |2 KD|:
        // the two signs of KD cancel, so x = (Delta . toX) / (|2 KD w| |toX|).
        const auto [alongStart, acrossStart] = along(toStart);
        const auto [alongEnd, acrossEnd] = along(toEnd);
        // mu = delta . w, with w = -sign(b') n / |n|.
        const double mu =
            -std::copysign(1.0, bulge) *
            exact::quotientToDouble(dotOf(delta, normal),
                                    deltaDivisor * floorSqrt(chordSquared, kRootBits));
        double gap = std::max(std::fabs(gapStart), std::fabs(gapEnd));
        const double angleLimit = deltaLength * cosHalf - kMargin * deltaLength;
        const bool startNearer = alongStart >= alongEnd;
        const double alongHigh = startNearer ? alongStart : alongEnd;
        const double acrossHigh = startNearer ? acrossStart : acrossEnd;
        const double gapHigh = startNearer ? gapStart : gapEnd;
        const double distanceHigh = startNearer ? distanceStart : distanceEnd;
        const double alongLow = startNearer ? alongEnd : alongStart;
        const double acrossLow = startNearer ? acrossEnd : acrossStart;
        const double gapLow = startNearer ? gapEnd : gapStart;
        const double distanceLow = startNearer ? distanceEnd : distanceStart;
        if (mu >= angleLimit) {
            // g(|delta|) = g(x) + 2 R'' (|delta| - x) / ((R'' + |delta|) + |X - C|).
            const double rest = alongHigh > 0.0
                                    ? acrossHigh * (acrossHigh / (deltaLength + alongHigh))
                                    : deltaLength - alongHigh;
            gap = std::max(
                gap, std::fabs(gapHigh + 2.0 * rest *
                                             (radiusReturned /
                                              (radiusReturned + deltaLength + distanceHigh))));
        }
        if (-mu >= angleLimit) {
            // g(-|delta|) = g(x) - 2 R'' (|delta| + x) / ((R'' - |delta|) + |X - C|).
            const double rest = alongLow < 0.0 ? acrossLow * (acrossLow / (deltaLength - alongLow))
                                               : deltaLength + alongLow;
            gap = std::max(gap,
                           std::fabs(gapLow - 2.0 * rest *
                                                  (radiusReturned /
                                                   (radiusReturned - deltaLength + distanceLow))));
        }
        const double bound = gap + kMargin * (gap + std::fabs(gapStart) + std::fabs(gapEnd) + eta) +
                             (kPi / 2.0 + 1.0) * eta * (1.0 + kMargin);
        return accept(bound);
    }

    // Lemma 6: short arcs, sampled in coordinates centred on S'.
    const auto local = [&](const TowerNumber& x, const Dyadic& pointWeight, double origin) {
        return tower.quotientToDouble(x - TowerNumber(Dyadic(origin) * pointWeight), pointWeight);
    };
    const Point2 exactStart{local(arc.start.x, arc.start.weight, start.x),
                            local(arc.start.y, arc.start.weight, start.y)};
    const Point2 exactEnd{local(arc.end.x, arc.end.weight, start.x),
                          local(arc.end.y, arc.end.weight, start.y)};
    // C - S' = (c - S' w) / w and C'' - S' = -toStart / (2 KD).
    const ExactVector centerFromStart = minus(center, times(first, weight));
    const Point2 exactCenter{exact::quotientToDouble(centerFromStart.x, weight),
                             exact::quotientToDouble(centerFromStart.y, weight)};
    const Point2 returnedCenter{exact::quotientToDouble(-toStart.x, centerDivisor),
                                exact::quotientToDouble(-toStart.y, centerDivisor)};
    // The exact sweep, from S - C and E - C (times positive weights).
    const Point2 centerToStart{tower.quotientToDouble(sx, weight * arc.start.weight),
                               tower.quotientToDouble(sy, weight * arc.start.weight)};
    const Point2 centerToEnd{tower.quotientToDouble(ex, weight * arc.end.weight),
                             tower.quotientToDouble(ey, weight * arc.end.weight)};
    const SampledArc exactSampled =
        sampledArc(exactStart, exactEnd, exactCenter, radius,
                   sweepFrom(centerToStart, centerToEnd, turnSign), turnSign);
    const SampledArc returnedSampled =
        sampledArc({0.0, 0.0},
                   {exact::quotientToDouble(last.x - first.x, Dyadic(1.0)),
                    exact::quotientToDouble(last.y - first.y, Dyadic(1.0))},
                   returnedCenter, radiusReturned, 4.0 * robust::atan2(size, 1.0), turnSign);
    const double shortLimit = kShortArc * boundOf(0.0);
    const auto tooLong = [&](double length) {
        return std::isnan(length) || length > shortLimit;
    };
    if (tooLong(exactSampled.length) || tooLong(returnedSampled.length)) {
        return result;
    }
    const double sampled = std::max(sampledDistance(returnedSampled, exactSampled),
                                    sampledDistance(exactSampled, returnedSampled));
    // The local doubles are within a few ulp of the largest of these.
    const double scale = std::max(
        {exactSampled.length, returnedSampled.length,
         exactSampled.flat ? 0.0 : distanceOf({0.0, 0.0}, exactCenter) + radius,
         returnedSampled.flat ? 0.0 : distanceOf({0.0, 0.0}, returnedCenter) + radiusReturned});
    const double bound = sampled +
                         std::max(exactSampled.length, returnedSampled.length) / (2.0 * kSamples) +
                         exactSampled.sagitta + returnedSampled.sagitta + 0x1p-44 * scale;
    return accept(bound);
}

}  // namespace

ArcRepresentation representArc(const Tower& tower, const ExactArc& arc, Point2 start, Point2 end,
                               double magnitude) {
    // Coordinates beyond 2^500 would overflow the doubles of the check.
    // Scaling the whole arc, M included, by 2^-600 is exact (no double here
    // underflows), keeps every coordinate the nearest double of its exact
    // value, and changes neither the bulge of the rule nor the answer, which
    // are invariant under scaling.
    if (std::max(largestCoordinate(start), largestCoordinate(end)) <= kHugeCoordinate) {
        return representModerate(tower, arc, start, end, magnitude);
    }
    constexpr std::int64_t kShrink = -600;
    constexpr int kShrinkDouble = -600;
    const auto shrink = [](const TowerPoint& p) {
        return TowerPoint{p.x.timesPowerOfTwo(kShrink), p.y.timesPowerOfTwo(kShrink), p.weight};
    };
    ExactArc scaled = arc;
    scaled.circle.centerX = arc.circle.centerX.timesPowerOfTwo(kShrink);
    scaled.circle.centerY = arc.circle.centerY.timesPowerOfTwo(kShrink);
    scaled.circle.radiusSquared = arc.circle.radiusSquared.timesPowerOfTwo(2 * kShrink);
    scaled.start = shrink(arc.start);
    scaled.end = shrink(arc.end);
    return representModerate(
        tower, scaled, {std::ldexp(start.x, kShrinkDouble), std::ldexp(start.y, kShrinkDouble)},
        {std::ldexp(end.x, kShrinkDouble), std::ldexp(end.y, kShrinkDouble)},
        std::ldexp(magnitude, kShrinkDouble));
}

}  // namespace solid_space::geom2d::detail
