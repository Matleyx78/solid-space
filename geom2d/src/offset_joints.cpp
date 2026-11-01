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
// These are the formulas of offset_curve.cpp, so the rounded moved ends are the
// bits that offsetCurve() returns.

#include "offset_joints.hpp"

#include "exact/dyadic.hpp"
#include "exact/quadratic.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

// A power-of-two scaling large enough to move a number of the tower from
// beyond the doubles (a zero or an infinity after rounding) towards them, and
// smaller than the range of the normal doubles (2^-1022 to 2^1024), so that no
// step jumps over it.
constexpr std::int64_t kScalingStep = 1000;
// More scalings than the exponents of the products of a few dozen doubles can
// need; reached only by numbers that the offset does not build.
constexpr int kMaxScalings = 64;

// numerator / denominator for a positive denominator. The numbers of a tower
// may lie far outside the doubles (products of weights) while their quotient
// does not: both are scaled by the same power of two, which is exact, so that
// the denominator rounds to a double near 1. Each rounding is within 2 ulp
// (exact/tower.hpp), so the quotient is within 5 ulp.
double quotient(const Tower& tower, const TowerNumber& numerator, const TowerNumber& denominator) {
    std::int64_t shift = 0;
    for (int attempt = 0; attempt < kMaxScalings; ++attempt) {
        const double value = tower.toDouble(denominator.timesPowerOfTwo(shift));
        if (value == 0.0) {
            shift += kScalingStep;
            continue;
        }
        if (std::isinf(value)) {
            shift -= kScalingStep;
            continue;
        }
        int exponent = 0;
        static_cast<void>(std::frexp(value, &exponent));
        shift -= exponent;
        break;
    }
    return tower.toDouble(numerator.timesPowerOfTwo(shift)) /
           tower.toDouble(denominator.timesPowerOfTwo(shift));
}

// tan(theta / 4) for the angle theta in (0, 2 pi) from a to b, with
// |a| |b| = whole, a . b = dot and the sign of a x b oriented in the direction
// of the turn: positive below half a turn, zero at half a turn, negative
// beyond. With cos(theta) = dot / whole,
//     sin(theta / 2) = sqrt((whole - dot) / (2 whole)),
//     |cos(theta / 2)| = sqrt((whole + dot) / (2 whole)),
// both from exact differences, so with no cancellation; cos(theta / 2) is
// negative beyond half a turn. Then tan(theta / 4) is
//     sin(theta / 2) / (1 + cos(theta / 2))   up to half a turn,
//     (1 - cos(theta / 2)) / sin(theta / 2)   beyond it,
// the same value written without cancellation in each range. Each is within a
// few ulp of the exact bulge, so the arc through the ends moves by far less
// than kRelativeOffsetError (offset.hpp). Negative for a clockwise turn.
double bulgeFrom(const Tower& tower, const TowerNumber& whole, const TowerNumber& dot,
                 Sign orientedCross, Turn turn) {
    const double sinHalf = std::sqrt(quotient(tower, whole - dot, whole) / 2.0);
    const double cosHalf = std::sqrt(quotient(tower, whole + dot, whole) / 2.0);
    const double magnitude =
        orientedCross == Sign::negative ? (1.0 + cosHalf) / sinHalf : sinHalf / (1.0 + cosHalf);
    return turn == Turn::counterclockwise ? magnitude : -magnitude;
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
    const auto& point = meeting->found()[index].point;
    if (beyondDomain(tower, point)) {
        return failedJoint(StepStatus::beyondDomain);
    }
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
    joint.bulge = bulgeFrom(tower, radiusSquared * (end->weight * start->weight),
                            dotOf(tower, toEnd, toStart),
                            oriented(turn, crossOf(tower, toEnd, toStart)), turn);
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

RoundedPoint roundedPoint(const OffsetSource& source, const PointRecipe& recipe) {
    OffsetScope scope(source);
    const auto point = scope.point(recipe);
    if (!point) {
        return {StepStatus::towerFull, {}};
    }
    if (beyondDomain(scope.tower(), *point)) {
        return {StepStatus::beyondDomain, {}};
    }
    if (recipe.kind == PointRecipe::Kind::meeting) {
        return {StepStatus::done, roundedPoint(scope.tower(), *point)};
    }
    // A moved end, rounded with the arguments of offset_curve.cpp (moveOnNormal):
    // (a + b sqrt(r)) / r for each coordinate.
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
        return {StepStatus::done,
                {exact::quadraticToDouble(x * squaredLength, distance * normal.x, squaredLength,
                                          squaredLength),
                 exact::quadraticToDouble(y * squaredLength, distance * normal.y, squaredLength,
                                          squaredLength)}};
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const auto& circle = arc.circle;
    const Dyadic outward = arc.turn == Turn::counterclockwise ? -distance : distance;
    const ExactVector radial{x * circle.weight - circle.centerX,
                             y * circle.weight - circle.centerY};
    return {StepStatus::done,
            {exact::quadraticToDouble(x * circle.radiusSquared, outward * radial.x,
                                      circle.radiusSquared, circle.radiusSquared),
             exact::quadraticToDouble(y * circle.radiusSquared, outward * radial.y,
                                      circle.radiusSquared, circle.radiusSquared)}};
}

RoundedBulge arcBulge(const OffsetSource& source, std::size_t piece, const PointRecipe& start,
                      const PointRecipe& end) {
    OffsetScope scope(source);
    auto& tower = scope.tower();
    const auto carrier = scope.carrier(piece);
    const auto first = scope.point(start);
    const auto last = scope.point(end);
    if (!carrier || !first || !last) {
        return {StepStatus::towerFull, 0.0};
    }
    const auto& circle = std::get<OffsetCircle>(*carrier);
    const auto a = fromCenter(circle, *first);
    const auto b = fromCenter(circle, *last);
    // |a| = sqrt(radiusSquared) W_a, the same for b: their product is rational
    // in the roots of the tower.
    const TowerNumber whole = circle.radiusSquared * (first->weight * last->weight);
    return {StepStatus::done, bulgeFrom(tower, whole, dotOf(tower, a, b),
                                        oriented(circle.turn, crossOf(tower, a, b)), circle.turn)};
}

}  // namespace solid_space::geom2d::detail
