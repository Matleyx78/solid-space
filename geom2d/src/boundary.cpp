// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// detectBoundary (boundary.hpp; ADR 0014; stage 0.3, package PS9): steps 5-7
// on the arrangement (arrangement.hpp) of the data of steps 1-3
// (boundary_rule.hpp), and the construction of the returned loops.
//
// Every decision is exact: the face of the point, the loops, the depths, the
// order of the loops and of their pieces, the first vertex of a polyline. The
// middle of a stretch split in two is a point with nested radicals; it is
// compared with the other vertices in a tower of square roots
// (exact/tower.hpp). Only the returned doubles are rounded:
// - a vertex that is an end of the data of step 1 is copied; any other vertex
//   is rounded correctly (positions.hpp, nearestPoint), or for the middle of a
//   split stretch built from the rounded ends (boundary_rule.hpp,
//   stretchMiddle), within a few units of 2^-53 * max(|p|, |centre|, r);
// - a bulge comes from the rounded ends and the rounded centre and radius
//   (boundary_rule.hpp, arcBulge): the segment moves by a few units of
//   2^-53 * max(|p|, |centre|, r) from the exact stretch.
// The centre and radius of a stretch are within 3 M of the origin (curves.hpp,
// magnitude: a circle of a stretch above half a turn has a radius below the
// distance from its middle to the chord), so both errors are far below
// kRelativePointError = 2^-48 and kRelativeLoopError = 2^-44 relative to
// max(M, |p|). A converted Arc adds its own kRelativeLoopError / 2.

#include "solid_space/geom2d/boundary.hpp"

#include "arrangement.hpp"
#include "boundary_rule.hpp"
#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/tower.hpp"
#include "positions.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::Arrangement;
using detail::ArrangementCurve;
using detail::BoundaryRule;
using detail::Box;
using detail::CircleSupport;
using detail::Dyadic;
using detail::ExactPoint;
using detail::Sign;
using exact::Tower;
using exact::TowerNumber;

constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool isNegative(Sign sign) noexcept {
    return sign == Sign::negative;
}

bool isPositive(Sign sign) noexcept {
    return sign == Sign::positive;
}

// --- Picked point ------------------------------------------------------------

std::optional<Diagnosis> checkPicked(Point2 picked) {
    if (!std::isfinite(picked.x) || !std::isfinite(picked.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, 0, "the picked point is not finite"};
    }
    if (std::fabs(picked.x) > robust::kMaxCoordinate ||
        std::fabs(picked.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, 0,
                         "the picked point is beyond the coordinate domain"};
    }
    return std::nullopt;
}

// --- Boxes of loops ------------------------------------------------------------

// As arrangement.cpp: far wider than any rounding, so that a box always
// contains what it stands for. Boxes only skip pairs; no decision reads them.
constexpr double kBoxRelativeMargin = 0x1p-40;
constexpr double kBoxAbsoluteMargin = 0x1p-1000;

void include(Box& box, Point2 point) {
    const double mx = std::fabs(point.x) * kBoxRelativeMargin + kBoxAbsoluteMargin;
    const double my = std::fabs(point.y) * kBoxRelativeMargin + kBoxAbsoluteMargin;
    box.minX = std::min(box.minX, point.x - mx);
    box.maxX = std::max(box.maxX, point.x + mx);
    box.minY = std::min(box.minY, point.y - my);
    box.maxY = std::max(box.maxY, point.y + my);
}

void include(Box& box, const Box& other) {
    box.minX = std::min(box.minX, other.minX);
    box.minY = std::min(box.minY, other.minY);
    box.maxX = std::max(box.maxX, other.maxX);
    box.maxY = std::max(box.maxY, other.maxY);
}

bool contains(const Box& box, Point2 point) {
    return box.minX <= point.x && point.x <= box.maxX && box.minY <= point.y && point.y <= box.maxY;
}

// --- Vertices of the returned loops ------------------------------------------

// A vertex of a returned loop: a vertex of the arrangement, the single point
// of a Circle for the order of the loops, or the middle of a stretch split in
// two (from `point` to `to` on `circle`, turning by more than half a turn).
struct LoopVertex {
    ExactPoint point;
    std::size_t vertex = kNone;
    bool middle = false;
    CircleSupport circle;
    ExactPoint to;
};

// A point of a tower: (x / weight, y / weight), with a positive weight.
struct TowerPoint {
    TowerNumber x;
    TowerNumber y;
    TowerNumber weight;
};

TowerNumber rootOf(Tower& tower, const TowerNumber& radicand) {
    // At most three roots for each of the two points compared: within
    // exact::kMaxTowerRoots, so a root is always available.
    const auto root = tower.squareRoot(radicand);
    assert(root.has_value() && "two vertices need at most six roots");
    return root.value_or(TowerNumber());
}

// The numerators (a + b sqrt(d)) of the coordinates of an exact point.
std::pair<TowerNumber, TowerNumber> numerators(Tower& tower, const ExactPoint& point) {
    const TowerNumber root = rootOf(tower, TowerNumber(point.radicand));
    return {TowerNumber(point.rational.x) + root * point.radical.x,
            TowerNumber(point.rational.y) + root * point.radical.y};
}

// The middle of the stretch from F to T of a circle with centre c / w and
// squared radius rho / w^2, turning by more than half a turn: opposite to
// S = F + T - 2 C from the centre, C - r S / |S|. With S = S' / D, D the
// product of the weights, and N = |S'|^2:
//     C - r S / |S| = (c N - S' sqrt(rho N)) / (w N).
TowerPoint inTower(Tower& tower, const LoopVertex& vertex) {
    const auto [px, py] = numerators(tower, vertex.point);
    if (!vertex.middle) {
        return {px, py, TowerNumber(vertex.point.weight)};
    }
    const auto [qx, qy] = numerators(tower, vertex.to);
    const CircleSupport& circle = vertex.circle;
    const Dyadic& wf = vertex.point.weight;
    const Dyadic& wt = vertex.to.weight;
    const Dyadic& w = circle.weight;
    const Dyadic both = (wf * wt).timesPowerOfTwo(1);
    const TowerNumber sx = px * (wt * w) + qx * (wf * w) - TowerNumber(circle.centerX * both);
    const TowerNumber sy = py * (wt * w) + qy * (wf * w) - TowerNumber(circle.centerY * both);
    const TowerNumber norm = tower.product(sx, sx) + tower.product(sy, sy);
    const TowerNumber root = rootOf(tower, norm * circle.radiusSquared);
    return {circle.centerX * norm - tower.product(sx, root),
            circle.centerY * norm - tower.product(sy, root), norm * w};
}

// The exact lexicographic order of (x, y): the sign of a - b.
Sign compareVertices(const LoopVertex& a, const LoopVertex& b) {
    if (!a.middle && !b.middle) {
        if (a.vertex != kNone && b.vertex != kNone) {
            // The vertices of the arrangement are numbered in this order.
            if (a.vertex == b.vertex) {
                return Sign::zero;
            }
            return a.vertex < b.vertex ? Sign::negative : Sign::positive;
        }
        const Sign x = detail::compareX(a.point, b.point);
        return x != Sign::zero ? x : detail::compareY(a.point, b.point);
    }
    Tower tower;
    const TowerPoint pa = inTower(tower, a);
    const TowerPoint pb = inTower(tower, b);
    const Sign x = tower.sign(tower.product(pa.x, pb.weight) - tower.product(pb.x, pa.weight));
    if (x != Sign::zero) {
        return x;
    }
    return tower.sign(tower.product(pa.y, pb.weight) - tower.product(pb.y, pa.weight));
}

// The antipode 2 C - F of an exact point F of a circle: the same radicand.
ExactPoint antipode(const CircleSupport& circle, const ExactPoint& point) {
    ExactPoint result;
    const Dyadic& w = circle.weight;
    const Dyadic twice = point.weight.timesPowerOfTwo(1);
    result.rational = detail::ExactVector{circle.centerX * twice - w * point.rational.x,
                                          circle.centerY * twice - w * point.rational.y};
    result.radical = detail::ExactVector{-(w * point.radical.x), -(w * point.radical.y)};
    result.radicand = point.radicand;
    result.weight = w * point.weight;
    return result;
}

// --- Building the loops --------------------------------------------------------

// A loop being returned, with what orders it among the others.
struct BuiltLoop {
    RegionLoop region;
    std::vector<LoopVertex> sequence;
    std::vector<std::size_t> darts;
    Box box;
};

class LoopBuilder {
public:
    LoopBuilder(const Arrangement& arrangement, const BoundaryRule& rule,
                std::span<const ArrangementCurve> curves)
        : m_arrangement(arrangement),
          m_rule(rule),
          m_curves(curves),
          m_rounded(arrangement.vertexCount()) {
        // The ends of the data of step 1, by their doubles, for the vertices
        // returned with the doubles of the data (+0 and -0 are one key).
        for (const detail::RuleCurve& curve : rule.curves) {
            if (!curve.whole) {
                m_dataEnds.emplace(key(curve.start), curve.start);
                m_dataEnds.emplace(key(curve.end), curve.end);
            }
        }
    }

    // A simple loop of darts that runs counterclockwise.
    [[nodiscard]] BuiltLoop build(std::vector<std::size_t> darts) const;

private:
    static std::pair<double, double> key(Point2 point) {
        // Adding +0 turns -0 into +0 and changes nothing else.
        return {point.x + 0.0, point.y + 0.0};
    }

    [[nodiscard]] Point2 rounded(std::size_t vertex) const;
    [[nodiscard]] LoopPieceOrigin originOf(std::size_t dart) const;
    [[nodiscard]] Box boxOf(std::span<const std::size_t> darts) const;

    const Arrangement& m_arrangement;
    const BoundaryRule& m_rule;
    std::span<const ArrangementCurve> m_curves;
    std::map<std::pair<double, double>, Point2> m_dataEnds;
    mutable std::vector<std::optional<Point2>> m_rounded;
};

Point2 LoopBuilder::rounded(std::size_t vertex) const {
    auto& cached = m_rounded[vertex];
    if (!cached) {
        const ExactPoint& exactVertex = m_arrangement.vertex(vertex);
        Point2 point = detail::nearestPoint(exactVertex);
        // A vertex that is an end of the data is a double: nearestPoint()
        // gives it exactly, and the data give the sign of its zeros.
        if (const auto found = m_dataEnds.find(key(point));
            found != m_dataEnds.end() &&
            detail::samePoint(exactVertex, detail::exactPoint(found->second))) {
            point = found->second;
        }
        cached = point;
    }
    return *cached;
}

LoopPieceOrigin LoopBuilder::originOf(std::size_t dart) const {
    const std::size_t curve = m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
    if (curve < m_rule.curves.size()) {
        const detail::RuleCurve& given = m_rule.curves[curve];
        return LoopPieceOrigin{LoopPieceSource::curve, given.input, given.input,
                               Arrangement::reversed(dart) != given.againstInput};
    }
    const detail::RuleLink& link = m_rule.links[curve - m_rule.curves.size()];
    return LoopPieceOrigin{LoopPieceSource::link, link.lowCurve, link.highCurve, false};
}

Box LoopBuilder::boxOf(std::span<const std::size_t> darts) const {
    Box box{kInfinity, kInfinity, -kInfinity, -kInfinity};
    for (const std::size_t dart : darts) {
        const std::size_t curve = m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
        if (std::holds_alternative<detail::ArcMeaning>(m_curves[curve].meaning.support)) {
            include(box, detail::boxOf(m_curves[curve]));
        } else {
            include(box, detail::nearestPoint(m_arrangement.vertex(m_arrangement.tail(dart))));
            include(box, detail::nearestPoint(m_arrangement.vertex(m_arrangement.head(dart))));
        }
    }
    return box;
}

BuiltLoop LoopBuilder::build(std::vector<std::size_t> darts) const {
    BuiltLoop built;
    built.box = boxOf(darts);
    const auto curveOf = [&](std::size_t dart) {
        return m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
    };

    // A whole input circle, credited with every piece of the loop.
    const std::size_t first = curveOf(darts.front());
    const bool single = std::all_of(darts.begin(), darts.end(),
                                    [&](std::size_t dart) { return curveOf(dart) == first; });
    if (single && first < m_rule.curves.size() && m_rule.curves[first].whole) {
        const detail::RuleCurve& given = m_rule.curves[first];
        built.region.loop = given.circle;
        built.region.origins.push_back(originOf(darts.front()));
        LoopVertex vertex;
        vertex.point.rational =
            detail::ExactVector{Dyadic(given.circle.center.x) - Dyadic(given.circle.radius),
                                Dyadic(given.circle.center.y)};
        built.sequence.push_back(vertex);
        built.darts = std::move(darts);
        return built;
    }

    // The segments, from the tail of each dart; a curved stretch above half
    // a turn as two segments of equal angle.
    std::vector<PolylineVertex> vertices;
    std::vector<LoopPieceOrigin> origins;
    std::vector<LoopVertex> sequence;
    for (const std::size_t dart : darts) {
        const std::size_t tail = m_arrangement.tail(dart);
        const std::size_t head = m_arrangement.head(dart);
        const LoopPieceOrigin origin = originOf(dart);
        LoopVertex start;
        start.point = m_arrangement.vertex(tail);
        start.vertex = tail;
        const ArrangementCurve& curve = m_curves[curveOf(dart)];
        const auto* arc = std::get_if<detail::ArcMeaning>(&curve.meaning.support);
        if (arc == nullptr) {
            vertices.push_back({rounded(tail), 0.0});
            origins.push_back(origin);
            sequence.push_back(std::move(start));
            continue;
        }
        const CircleSupport& circle = arc->circle;
        const bool counterclockwise =
            (arc->turn == detail::Turn::counterclockwise) != Arrangement::reversed(dart);
        const Point2 centre{quotientToDouble(circle.centerX, circle.weight),
                            quotientToDouble(circle.centerY, circle.weight)};
        const double radius =
            std::sqrt(quotientToDouble(circle.radiusSquared, circle.weight * circle.weight));
        const Point2 from = rounded(tail);
        const Point2 to = rounded(head);
        // Above half a turn when the antipode of the start of the piece lies
        // strictly inside it (a piece from a vertex back to it is a whole turn).
        const detail::ArrangementPiece& piece = m_arrangement.pieces()[Arrangement::pieceOf(dart)];
        const ExactPoint& pieceFrom = m_arrangement.vertex(piece.from);
        const bool split =
            piece.from == piece.to ||
            isNegative(detail::compareFrom(curve.meaning, pieceFrom, antipode(circle, pieceFrom),
                                           m_arrangement.vertex(piece.to)));
        if (!split) {
            vertices.push_back(
                {from, detail::arcBulge(from, to, centre, radius, counterclockwise)});
            origins.push_back(origin);
            sequence.push_back(std::move(start));
            continue;
        }
        const Point2 middle = detail::stretchMiddle(circle, from, to, counterclockwise);
        vertices.push_back(
            {from, detail::arcBulge(from, middle, centre, radius, counterclockwise)});
        vertices.push_back(
            {middle, detail::arcBulge(middle, to, centre, radius, counterclockwise)});
        origins.push_back(origin);
        origins.push_back(origin);
        LoopVertex halfway;
        halfway.point = start.point;
        halfway.middle = true;
        halfway.circle = circle;
        halfway.to = m_arrangement.vertex(head);
        sequence.push_back(std::move(start));
        sequence.push_back(std::move(halfway));
    }

    // Start at the exactly smallest vertex.
    std::size_t smallest = 0;
    for (std::size_t k = 1; k < sequence.size(); ++k) {
        if (isNegative(compareVertices(sequence[k], sequence[smallest]))) {
            smallest = k;
        }
    }
    const auto offset = static_cast<std::ptrdiff_t>(smallest);
    std::rotate(vertices.begin(), vertices.begin() + offset, vertices.end());
    std::rotate(origins.begin(), origins.begin() + offset, origins.end());
    std::rotate(sequence.begin(), sequence.begin() + offset, sequence.end());
    built.region.loop = Polyline{std::move(vertices), true};
    built.region.origins = std::move(origins);
    built.sequence = std::move(sequence);
    built.darts = std::move(darts);
    return built;
}

// The darts of a simple loop, run counterclockwise.
std::vector<std::size_t> counterclockwiseDarts(const Arrangement& arrangement,
                                               std::vector<std::size_t> loop) {
    if (!arrangement.counterclockwise(loop)) {
        std::reverse(loop.begin(), loop.end());
        for (std::size_t& dart : loop) {
            dart = Arrangement::twin(dart);
        }
    }
    return loop;
}

// The order of the islands at equal depth: their vertex sequences, a
// shorter one first when it is the start of the other.
bool sequenceBefore(const BuiltLoop& a, const BuiltLoop& b) {
    const std::size_t count = std::min(a.sequence.size(), b.sequence.size());
    for (std::size_t k = 0; k < count; ++k) {
        const Sign order = compareVertices(a.sequence[k], b.sequence[k]);
        if (order != Sign::zero) {
            return isNegative(order);
        }
    }
    if (a.sequence.size() != b.sequence.size()) {
        return a.sequence.size() < b.sequence.size();
    }
    // Two loops with the same vertices in the same order: the order of their
    // first darts, so that the result stays deterministic.
    return a.darts.front() < b.darts.front();
}

// --- Steps 5-7 -------------------------------------------------------------------

// Step 5 with an unbounded face: the free ends of the first connected group
// met by the half-line from the picked point toward +x that has free ends.
std::vector<OpenEnd> openEnds(const Arrangement& arrangement, const BoundaryRule& rule,
                              std::span<const ArrangementCurve> curves, Point2 picked) {
    const ExactPoint origin = detail::exactPoint(picked);
    // The line y = picked.y, directed toward +x.
    const double ahead = picked.x + std::max(1.0, std::fabs(picked.x));
    const detail::Carrier ray = detail::LineMeaning{picked, Point2{ahead, picked.y}};
    const Box rayBox{picked.x, picked.y, kInfinity, picked.y};

    // The first point of each group on the half-line.
    std::vector<std::optional<ExactPoint>> firstMeeting(arrangement.groupCount());
    const auto consider = [&](std::size_t group, const ExactPoint& point) {
        if (!isPositive(detail::compareX(point, origin))) {
            return;
        }
        auto& best = firstMeeting[group];
        if (!best || isNegative(detail::compareX(point, *best))) {
            best = point;
        }
    };
    for (std::size_t a = 0; a < curves.size(); ++a) {
        const ArrangementCurve& curve = curves[a];
        if (!detail::overlaps(detail::boxOf(curve), rayBox)) {
            continue;
        }
        const std::size_t group = arrangement.groupOfCurve(a);
        const detail::CarrierMeeting meeting = detail::meet(detail::carrierOf(curve.meaning), ray);
        if (meeting.sameCarrier) {
            // A straight stretch along the line: its first point is an end.
            consider(group, curve.start.value_or(detail::startPoint(curve.meaning)));
            consider(group, curve.end.value_or(detail::endPoint(curve.meaning)));
            continue;
        }
        for (const detail::CarrierPoint& found : meeting.found()) {
            if (detail::onStretch(curve, found.point)) {
                consider(group, found.point);
            }
        }
    }

    // The free ends left by steps 2 and 3, by group.
    std::vector<std::vector<OpenEnd>> endsOfGroup(arrangement.groupCount());
    for (std::size_t end = 0; end < rule.freeEnd.size(); ++end) {
        if (rule.leftFree(end)) {
            const detail::RuleCurve& curve = rule.curves[end / 2];
            const bool atStart = end % 2 == 0;
            endsOfGroup[arrangement.groupOfCurve(end / 2)].push_back(
                OpenEnd{curve.input, atStart, atStart ? curve.start : curve.end});
        }
    }

    std::vector<std::size_t> met;
    for (std::size_t g = 0; g < arrangement.groupCount(); ++g) {
        if (firstMeeting[g]) {
            met.push_back(g);
        }
    }
    // Two groups share no point: their first meetings differ.
    std::sort(met.begin(), met.end(), [&](std::size_t a, std::size_t b) {
        return isNegative(detail::compareX(*firstMeeting[a], *firstMeeting[b]));
    });
    for (const std::size_t g : met) {
        if (!endsOfGroup[g].empty()) {
            std::vector<OpenEnd> ends = std::move(endsOfGroup[g]);
            std::sort(ends.begin(), ends.end(), [](const OpenEnd& a, const OpenEnd& b) {
                return std::pair{a.curve, !a.atStart} < std::pair{b.curve, !b.atStart};
            });
            return ends;
        }
    }
    return {};
}

// Steps 6 and 7 with the bounded face whose outer cycle is `face`: the
// contour, the islands and their depths, in the returned order.
std::vector<RegionLoop> enclosedLoops(const Arrangement& arrangement, const BoundaryRule& rule,
                                      std::span<const ArrangementCurve> curves, std::size_t face) {
    const LoopBuilder builder(arrangement, rule, curves);
    std::vector<BuiltLoop> loops;
    std::vector<BuiltLoop> islands;
    for (auto& loop : arrangement.simpleLoops(face)) {
        if (loops.empty() && arrangement.counterclockwise(loop)) {
            // The loop of the face that encloses it: the only one that runs
            // counterclockwise with the face on its left.
            loops.push_back(builder.build(std::move(loop)));
        } else {
            islands.push_back(builder.build(counterclockwiseDarts(arrangement, std::move(loop))));
        }
    }
    assert(loops.size() == 1 && "a bounded face has one outer loop");
    const BuiltLoop& contour = loops.front();

    // The outer boundary of every other group inside the contour.
    const std::size_t contourGroup = arrangement.groupOfCycle(face);
    for (std::size_t g = 0; g < arrangement.groupCount(); ++g) {
        if (g == contourGroup) {
            continue;
        }
        const ExactPoint& vertex = arrangement.vertex(arrangement.groupVertex(g));
        if (!contains(contour.box, detail::nearestPoint(vertex)) ||
            !arrangement.inside(vertex, contour.darts)) {
            continue;
        }
        for (auto& loop : arrangement.simpleLoops(arrangement.holeCycle(g))) {
            islands.push_back(builder.build(counterclockwiseDarts(arrangement, std::move(loop))));
        }
    }
    for (auto& island : islands) {
        loops.push_back(std::move(island));
    }

    // Step 7: the depth of a loop is the number of the others that enclose
    // it. A loop inside another lies in its box.
    for (std::size_t inner = 0; inner < loops.size(); ++inner) {
        std::size_t depth = 0;
        for (std::size_t outer = 0; outer < loops.size(); ++outer) {
            if (outer != inner && detail::overlaps(loops[outer].box, loops[inner].box) &&
                arrangement.encloses(loops[outer].darts, loops[inner].darts)) {
                ++depth;
            }
        }
        loops[inner].region.depth = depth;
    }
    assert(loops.front().region.depth == 0 && "nothing encloses the contour");
    std::stable_sort(loops.begin() + 1, loops.end(), [](const BuiltLoop& a, const BuiltLoop& b) {
        if (a.region.depth != b.region.depth) {
            return a.region.depth < b.region.depth;
        }
        return sequenceBefore(a, b);
    });

    std::vector<RegionLoop> result;
    result.reserve(loops.size());
    for (auto& loop : loops) {
        result.push_back(std::move(loop.region));
    }
    return result;
}

}  // namespace

Result<DetectedBoundary> detectBoundary(Point2 picked, std::span<const Curve> curves,
                                        const robust::TolerancePolicy& policy) {
    if (auto problem = checkPicked(picked)) {
        return std::move(*problem);
    }
    // Steps 1-3.
    const BoundaryRule rule = detail::applyRule(curves, policy);
    const std::vector<ArrangementCurve> given = detail::arrangementCurves(rule, curves.size());

    DetectedBoundary result;
    result.skipped = rule.skipped;
    // Step 4.
    if (const auto nearest = detail::nearestCurve(rule, given, picked, policy)) {
        result.outcome = BoundaryOutcome::onCurve;
        result.nearCurve = *nearest;
        return result;
    }
    if (given.empty()) {
        result.outcome = BoundaryOutcome::notEnclosed;
        return result;
    }
    // Step 5: the point lies on no piece, after step 4.
    const Arrangement arrangement(given);
    const std::optional<std::size_t> face = arrangement.faceOf(detail::exactPoint(picked));
    if (!face) {
        result.outcome = BoundaryOutcome::notEnclosed;
        result.openEnds = openEnds(arrangement, rule, given, picked);
        return result;
    }
    // Steps 6 and 7.
    result.outcome = BoundaryOutcome::enclosed;
    result.loops = enclosedLoops(arrangement, rule, given, *face);
    return result;
}

}  // namespace solid_space::geom2d
