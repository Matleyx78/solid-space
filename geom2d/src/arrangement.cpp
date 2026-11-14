// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The arrangement of curves of the plane (arrangement.hpp; stage 0.3, package
// PS6).

#include "arrangement.hpp"

#include "positions.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

// The margins of the boxes. Every double that enters a box is within a few
// units of 2^-53 of its exact value, relative to the largest magnitude in
// play (a correctly rounded point or quotient, the square root of a correctly
// rounded value, one more subtraction); below the normal doubles the error is
// at most 2^-1074. The margins are far wider, 2^-40 relative plus 2^-1000
// absolute, so that a box always contains what it stands for. They only let
// more pairs through: no decision reads them.
constexpr double kBoxRelativeMargin = 0x1p-40;
constexpr double kBoxAbsoluteMargin = 0x1p-1000;
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kTinySquaredRadius = 0x1p-900;

double margin(double magnitude) {
    return magnitude * kBoxRelativeMargin + kBoxAbsoluteMargin;
}

Box emptyBox() {
    return Box{kInfinity, kInfinity, -kInfinity, -kInfinity};
}

void include(Box& box, Point2 point) {
    box.minX = std::min(box.minX, point.x - margin(std::fabs(point.x)));
    box.maxX = std::max(box.maxX, point.x + margin(std::fabs(point.x)));
    box.minY = std::min(box.minY, point.y - margin(std::fabs(point.y)));
    box.maxY = std::max(box.maxY, point.y + margin(std::fabs(point.y)));
}

void include(Box& box, const Box& other) {
    box.minX = std::min(box.minX, other.minX);
    box.minY = std::min(box.minY, other.minY);
    box.maxX = std::max(box.maxX, other.maxX);
    box.maxY = std::max(box.maxY, other.maxY);
}

// The box of a whole circle. A centre or a radius beyond the doubles (a
// polyline segment with a tiny bulge) gives the whole plane.
Box circleBox(const CircleSupport& circle) {
    const double cx = quotientToDouble(circle.centerX, circle.weight);
    const double cy = quotientToDouble(circle.centerY, circle.weight);
    double squared = quotientToDouble(circle.radiusSquared, circle.weight * circle.weight);
    // Below 2^-900 the rounded square may have lost all its digits (it may
    // even be zero): its absolute error is at most 2^-1074, so the radius is
    // below 2^-449 and that bound stands for it.
    if (squared < kTinySquaredRadius) {
        squared = 4 * kTinySquaredRadius;
    }
    const double radius = std::sqrt(squared);
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius)) {
        return Box{-kInfinity, -kInfinity, kInfinity, kInfinity};
    }
    const double mx = margin(std::fabs(cx) + radius);
    const double my = margin(std::fabs(cy) + radius);
    return Box{cx - radius - mx, cy - radius - my, cx + radius + mx, cy + radius + my};
}

bool isNegative(Sign sign) noexcept {
    return sign == Sign::negative;
}

bool isPositive(Sign sign) noexcept {
    return sign == Sign::positive;
}

// a < b in the exact order of (x, y).
bool lexicographicallyBefore(const ExactPoint& a, const ExactPoint& b) {
    const Sign x = compareX(a, b);
    if (x != Sign::zero) {
        return isNegative(x);
    }
    return isNegative(compareY(a, b));
}

// The sign of position(a) - position(b) on a circular curve, both measured
// from `origin` in the direction of the curve, in [0, 2 pi). compareAlong()
// measures from D0, the start of the data, which a moved start or a piece
// does not have.
Sign compareFrom(const CurveMeaning& curve, const ExactPoint& origin, const ExactPoint& a,
                 const ExactPoint& b) {
    const bool aWraps = isNegative(compareAlong(curve, a, origin));
    const bool bWraps = isNegative(compareAlong(curve, b, origin));
    if (aWraps != bWraps) {
        return aWraps ? Sign::positive : Sign::negative;
    }
    return compareAlong(curve, a, b);
}

// Points of a circle (c + q sqrt(rho)) / w, with c the centre and rho / w^2 the
// squared radius.
enum class CircleSpot : std::uint8_t {
    centre,  ///< q = 0
    top,     ///< q = (0, 1)
    bottom,  ///< q = (0, -1)
    left,    ///< q = (-1, 0)
};

ExactPoint circlePoint(const CircleSupport& circle, CircleSpot spot) {
    ExactPoint point;
    point.rational = ExactVector{circle.centerX, circle.centerY};
    point.weight = circle.weight;
    if (spot == CircleSpot::centre) {
        return point;
    }
    point.radicand = circle.radiusSquared;
    const Dyadic one = Dyadic::fromInteger(1);
    switch (spot) {
        case CircleSpot::top:
            point.radical.y = one;
            break;
        case CircleSpot::bottom:
            point.radical.y = -one;
            break;
        case CircleSpot::left:
            point.radical.x = -one;
            break;
        case CircleSpot::centre:
            break;
    }
    return point;
}

// A union-find whose roots are the smallest members, so that the result does
// not depend on the order of the unions.
class Partition {
public:
    explicit Partition(std::size_t size) : m_parent(size) {
        std::iota(m_parent.begin(), m_parent.end(), std::size_t{0});
    }

    std::size_t find(std::size_t member) {
        while (m_parent[member] != member) {
            m_parent[member] = m_parent[m_parent[member]];
            member = m_parent[member];
        }
        return member;
    }

    void unite(std::size_t a, std::size_t b) {
        const std::size_t rootA = find(a);
        const std::size_t rootB = find(b);
        m_parent[std::max(rootA, rootB)] = std::min(rootA, rootB);
    }

private:
    std::vector<std::size_t> m_parent;
};

constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

}  // namespace

bool overlaps(const Box& a, const Box& b) noexcept {
    return a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY && b.minY <= a.maxY;
}

std::vector<std::pair<std::size_t, std::size_t>> overlappingPairs(std::span<const Box> boxes) {
    std::vector<std::size_t> order(boxes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return boxes[a].minX < boxes[b].minX; });
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for (std::size_t k = 0; k < order.size(); ++k) {
        const Box& first = boxes[order[k]];
        for (std::size_t l = k + 1; l < order.size() && boxes[order[l]].minX <= first.maxX; ++l) {
            if (overlaps(first, boxes[order[l]])) {
                pairs.emplace_back(std::min(order[k], order[l]), std::max(order[k], order[l]));
            }
        }
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

Box boxOf(const ArrangementCurve& curve) {
    const Carrier carrier = carrierOf(curve.meaning);
    if (const auto* circle = std::get_if<CircleSupport>(&carrier)) {
        return circleBox(*circle);
    }
    Box box = emptyBox();
    include(box, nearestPoint(curve.start.value_or(startPoint(curve.meaning))));
    include(box, nearestPoint(curve.end.value_or(endPoint(curve.meaning))));
    return box;
}

Arrangement::Arrangement(std::span<const ArrangementCurve> curves) {
    split(curves);
    link();
    buildCycles();
    buildGroups();
}

// --- Splitting ---------------------------------------------------------------

Sign Arrangement::compareOn(const CurveData& curve, const ExactPoint& a, const ExactPoint& b) {
    if (!curve.circular || curve.whole) {
        return compareAlong(curve.meaning, a, b);
    }
    return compareFrom(curve.meaning, curve.start, a, b);
}

void Arrangement::split(std::span<const ArrangementCurve> curves) {
    std::vector<Box> boxes;
    m_curves.reserve(curves.size());
    for (const ArrangementCurve& given : curves) {
        assert(!given.meaning.degenerate && "the arrangement needs curves that are not points");
        CurveData curve;
        curve.meaning = given.meaning;
        curve.carrier = carrierOf(given.meaning);
        if (const auto* arc = std::get_if<ArcMeaning>(&given.meaning.support)) {
            curve.circular = true;
            curve.whole = arc->span == ArcSpan::whole || arc->span == ArcSpan::circle;
            curve.heading = arc->turn == Turn::clockwise ? Heading::backward : Heading::forward;
        }
        assert(!(curve.whole && (given.start || given.end)) && "a whole circle has no ends");
        curve.start = given.start.value_or(startPoint(given.meaning));
        curve.end = given.end.value_or(endPoint(given.meaning));
        assert(onCarrier(curve.carrier, curve.start) && onCarrier(curve.carrier, curve.end));
        curve.priority = given.priority;
        curve.box = boxOf(given);
        boxes.push_back(curve.box);
        m_curves.push_back(std::move(curve));
    }

    // Whether a point of the carrier lies on the stretch, its ends included.
    const auto onStretch = [&](const CurveData& curve, const ExactPoint& point) {
        if (curve.whole) {
            return true;
        }
        if (!curve.circular) {
            return !isPositive(compareAlong(curve.meaning, curve.start, point)) &&
                   !isPositive(compareAlong(curve.meaning, point, curve.end));
        }
        return !isPositive(compareFrom(curve.meaning, curve.start, point, curve.end));
    };

    // Every point where a curve meets another or ends, with its curve.
    struct SplitPoint {
        ExactPoint point;
        std::size_t curve = 0;
    };

    std::vector<SplitPoint> points;
    for (std::size_t i = 0; i < m_curves.size(); ++i) {
        if (!m_curves[i].whole) {
            points.push_back({m_curves[i].start, i});
            points.push_back({m_curves[i].end, i});
        }
    }
    const auto addEnds = [&](std::size_t from, std::size_t into) {
        const CurveData& source = m_curves[from];
        if (source.whole) {
            return;
        }
        for (const ExactPoint* end : {&source.start, &source.end}) {
            if (onStretch(m_curves[into], *end)) {
                points.push_back({*end, into});
            }
        }
    };
    for (const auto& [i, j] : overlappingPairs(boxes)) {
        const CarrierMeeting meeting = meet(m_curves[i].carrier, m_curves[j].carrier);
        if (meeting.sameCarrier) {
            addEnds(j, i);
            addEnds(i, j);
            continue;
        }
        for (const CarrierPoint& found : meeting.found()) {
            if (onStretch(m_curves[i], found.point) && onStretch(m_curves[j], found.point)) {
                points.push_back({found.point, i});
                points.push_back({found.point, j});
            }
        }
    }
    // A whole circle that meets nothing gets a vertex at its start. Two equal
    // circles get the same one, and a circle with a point from another curve
    // on the same carrier has that point.
    std::vector<bool> hasPoint(m_curves.size(), false);
    for (const SplitPoint& point : points) {
        hasPoint[point.curve] = true;
    }
    for (std::size_t i = 0; i < m_curves.size(); ++i) {
        if (!hasPoint[i]) {
            points.push_back({startPoint(m_curves[i].meaning), i});
        }
    }

    // The vertices: the distinct points, by increasing (x, y).
    std::vector<std::size_t> order(points.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return lexicographicallyBefore(points[a].point, points[b].point);
    });
    std::vector<std::size_t> vertexOfPoint(points.size());
    for (const std::size_t index : order) {
        const ExactPoint& point = points[index].point;
        if (m_vertices.empty() || !samePoint(m_vertices.back(), point)) {
            m_vertices.push_back(point);
        }
        vertexOfPoint[index] = m_vertices.size() - 1;
    }

    // The vertices of each curve, along it; and the groups.
    std::vector<std::vector<std::size_t>> onCurve(m_curves.size());
    std::vector<std::size_t> firstCurve(m_vertices.size(), kNone);
    Partition groups(m_curves.size());
    for (std::size_t k = 0; k < points.size(); ++k) {
        const std::size_t v = vertexOfPoint[k];
        onCurve[points[k].curve].push_back(v);
        if (firstCurve[v] == kNone) {
            firstCurve[v] = points[k].curve;
        } else {
            groups.unite(firstCurve[v], points[k].curve);
        }
    }
    m_groupOfCurve.assign(m_curves.size(), kNone);
    std::vector<std::size_t> groupOfRoot(m_curves.size(), kNone);
    std::size_t groupCount = 0;
    for (std::size_t i = 0; i < m_curves.size(); ++i) {
        const std::size_t root = groups.find(i);
        if (groupOfRoot[root] == kNone) {
            groupOfRoot[root] = groupCount++;
        }
        m_groupOfCurve[i] = groupOfRoot[root];
    }
    m_holeCycle.assign(groupCount, kNone);

    for (std::size_t i = 0; i < m_curves.size(); ++i) {
        const CurveData& curve = m_curves[i];
        auto& vertices = onCurve[i];
        std::sort(vertices.begin(), vertices.end(), [&](std::size_t a, std::size_t b) {
            const Sign along = compareOn(curve, m_vertices[a], m_vertices[b]);
            return along != Sign::zero ? isNegative(along) : a < b;
        });
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
        if (curve.whole) {
            for (std::size_t k = 0; k < vertices.size(); ++k) {
                m_pieces.push_back({i, vertices[k], vertices[(k + 1) % vertices.size()]});
            }
            continue;
        }
        assert(vertices.size() >= 2 && samePoint(m_vertices[vertices.front()], curve.start) &&
               samePoint(m_vertices[vertices.back()], curve.end) && "a stretch must be simple");
        for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
            m_pieces.push_back({i, vertices[k], vertices[k + 1]});
        }
    }
}

// --- Darts around the vertices ---------------------------------------------------

std::size_t Arrangement::tail(std::size_t dart) const {
    const ArrangementPiece& piece = m_pieces.at(pieceOf(dart));
    return reversed(dart) ? piece.to : piece.from;
}

std::size_t Arrangement::head(std::size_t dart) const {
    const ArrangementPiece& piece = m_pieces.at(pieceOf(dart));
    return reversed(dart) ? piece.from : piece.to;
}

HalfPiece Arrangement::halfPiece(std::size_t dart) const {
    const CurveData& curve = m_curves[m_pieces.at(pieceOf(dart)).curve];
    return HalfPiece{curve.carrier,
                     reversed(dart) ? detail::reversed(curve.heading) : curve.heading};
}

const ExactPoint& Arrangement::vertex(std::size_t v) const {
    return m_vertices.at(v);
}

std::span<const std::size_t> Arrangement::around(std::size_t v) const {
    return m_vertexData.at(v).darts;
}

std::size_t Arrangement::next(std::size_t dart) const {
    const std::span<const std::size_t> darts = around(head(dart));
    const std::size_t back = m_aroundIndex.at(twin(dart));
    return darts[(back + darts.size() - 1) % darts.size()];
}

void Arrangement::link() {
    // Two darts that leave a vertex with the same departure run along the
    // same carrier in the same heading, and the next vertex on both is the
    // same: each curve is split at the ends of the other and at every point
    // where a third curve meets that carrier. So their pieces are one stretch.
    std::vector<std::vector<std::size_t>> leaving(m_vertices.size());
    for (std::size_t dart = 0; dart < dartCount(); ++dart) {
        leaving[tail(dart)].push_back(dart);
    }
    Partition same(m_pieces.size());
    std::vector<LocalOrder> orders;
    orders.reserve(m_vertices.size());
    for (std::size_t v = 0; v < m_vertices.size(); ++v) {
        std::vector<HalfPiece> halves;
        for (const std::size_t dart : leaving[v]) {
            halves.push_back(halfPiece(dart));
        }
        orders.emplace_back(m_vertices[v], halves);
        for (std::size_t g = 0; g < orders.back().groupCount(); ++g) {
            const auto members = orders.back().group(g);
            for (std::size_t k = 1; k < members.size(); ++k) {
                same.unite(pieceOf(leaving[v][members[0]]), pieceOf(leaving[v][members[k]]));
            }
        }
    }

    // The piece kept for each stretch: the curve of lowest priority, then of
    // lowest index.
    std::vector<std::size_t> kept(m_pieces.size(), kNone);
    for (std::size_t p = 0; p < m_pieces.size(); ++p) {
        std::size_t& best = kept[same.find(p)];
        const auto rank = [&](std::size_t piece) {
            return std::pair{m_curves[m_pieces[piece].curve].priority, m_pieces[piece].curve};
        };
        if (best == kNone || rank(p) < rank(best)) {
            best = p;
        }
    }
    std::vector<std::size_t> renumbered(m_pieces.size(), kNone);
    std::vector<ArrangementPiece> pieces;
    for (std::size_t p = 0; p < m_pieces.size(); ++p) {
        if (kept[same.find(p)] == p) {
            renumbered[p] = pieces.size();
            pieces.push_back(m_pieces[p]);
        }
    }

    m_vertexData.resize(m_vertices.size());
    m_aroundIndex.assign(2 * pieces.size(), kNone);
    for (std::size_t v = 0; v < m_vertices.size(); ++v) {
        VertexData& data = m_vertexData[v];
        for (std::size_t g = 0; g < orders[v].groupCount(); ++g) {
            for (const std::size_t member : orders[v].group(g)) {
                const std::size_t dart = leaving[v][member];
                const std::size_t piece = renumbered[pieceOf(dart)];
                if (piece != kNone) {
                    m_aroundIndex[2 * piece + (dart & 1U)] = data.darts.size();
                    data.darts.push_back(2 * piece + (dart & 1U));
                }
            }
            assert(data.darts.size() == g + 1 && "one kept dart per departure");
        }
        data.order = std::move(orders[v]);
    }
    m_pieces = std::move(pieces);
}

// --- Cycles and groups -------------------------------------------------------

std::span<const std::size_t> Arrangement::cycle(std::size_t c) const {
    const std::size_t begin = m_cycleStart.at(c);
    const std::size_t end = c + 1 < m_cycleStart.size() ? m_cycleStart[c + 1] : m_cycleDarts.size();
    return std::span<const std::size_t>(m_cycleDarts).subspan(begin, end - begin);
}

std::size_t Arrangement::cycleOf(std::size_t dart) const {
    return m_cycleOfDart.at(dart);
}

bool Arrangement::isHole(std::size_t c) const {
    return m_holeCycle.at(groupOfCycle(c)) == c;
}

std::size_t Arrangement::groupOfCurve(std::size_t curve) const {
    return m_groupOfCurve.at(curve);
}

std::size_t Arrangement::groupOfCycle(std::size_t c) const {
    return m_groupOfCycle.at(c);
}

std::size_t Arrangement::holeCycle(std::size_t group) const {
    return m_holeCycle.at(group);
}

std::size_t Arrangement::groupVertex(std::size_t group) const {
    return m_groupVertex.at(group);
}

Box Arrangement::pieceBox(std::size_t piece) const {
    const ArrangementPiece& stretch = m_pieces[piece];
    const CurveData& curve = m_curves[stretch.curve];
    if (curve.circular) {
        return curve.box;
    }
    Box box = emptyBox();
    include(box, nearestPoint(m_vertices[stretch.from]));
    include(box, nearestPoint(m_vertices[stretch.to]));
    return box;
}

void Arrangement::buildCycles() {
    m_cycleOfDart.assign(dartCount(), kNone);
    for (std::size_t first = 0; first < dartCount(); ++first) {
        if (m_cycleOfDart[first] != kNone) {
            continue;
        }
        const std::size_t c = m_cycleStart.size();
        m_cycleStart.push_back(m_cycleDarts.size());
        Box box = emptyBox();
        for (std::size_t dart = first; m_cycleOfDart[dart] == kNone; dart = next(dart)) {
            m_cycleOfDart[dart] = c;
            m_cycleDarts.push_back(dart);
            include(box, pieceBox(pieceOf(dart)));
        }
        m_cycleBox.push_back(box);
    }
}

bool Arrangement::strictlyInside(std::size_t piece, const ExactPoint& point) const {
    const ArrangementPiece& stretch = m_pieces[piece];
    const CurveData& curve = m_curves[stretch.curve];
    const ExactPoint& from = m_vertices[stretch.from];
    if (samePoint(point, from)) {
        return false;
    }
    if (stretch.from == stretch.to) {
        return true;
    }
    const ExactPoint& to = m_vertices[stretch.to];
    if (!curve.circular) {
        return isNegative(compareAlong(curve.meaning, from, point)) &&
               isNegative(compareAlong(curve.meaning, point, to));
    }
    return isNegative(compareFrom(curve.meaning, from, point, to));
}

Arrangement::Extreme Arrangement::extremeOf(std::span<const std::size_t> darts) const {
    assert(!darts.empty());
    // The vertices are numbered by increasing (x, y).
    std::size_t smallest = kNone;
    for (const std::size_t dart : darts) {
        smallest = std::min(smallest, tail(dart));
    }
    Extreme extreme{m_vertices[smallest], true, smallest};
    for (const std::size_t dart : darts) {
        const std::size_t piece = pieceOf(dart);
        const auto* circle = std::get_if<CircleSupport>(&m_curves[m_pieces[piece].curve].carrier);
        if (circle == nullptr) {
            continue;
        }
        ExactPoint left = circlePoint(*circle, CircleSpot::left);
        if (lexicographicallyBefore(left, extreme.point) && strictlyInside(piece, left)) {
            extreme = Extreme{std::move(left), false, piece};
        }
    }
    return extreme;
}

std::size_t Arrangement::westLeft(const ExactPoint& point,
                                  std::span<const std::size_t> darts) const {
    std::vector<HalfPiece> halves;
    halves.reserve(darts.size());
    for (const std::size_t dart : darts) {
        halves.push_back(halfPiece(dart));
    }
    const LocalOrder order(point, halves);
    // The half-line toward -x: the reference of rayDeparture(), turned by half
    // a turn (its tangent is (1, 0) with no radical and no curvature).
    Departure west = rayDeparture(point);
    west.tangent.x = -west.tangent.x;
    const Placement placement = order.place(west);
    assert(!placement.along && "nothing runs toward -x from the smallest point");
    const auto members = order.group(placement.index);
    assert(members.size() == 1);
    // Sector g lies counterclockwise after group g: on the left of its dart.
    return darts[members[0]];
}

void Arrangement::buildGroups() {
    m_groupOfCycle.assign(cycleCount(), kNone);
    for (std::size_t c = 0; c < cycleCount(); ++c) {
        m_groupOfCycle[c] = m_groupOfCurve[m_pieces[pieceOf(cycle(c)[0])].curve];
    }
    std::vector<std::vector<std::size_t>> darts(groupCount());
    for (std::size_t dart = 0; dart < dartCount(); ++dart) {
        darts[m_groupOfCurve[m_pieces[pieceOf(dart)].curve]].push_back(dart);
    }
    m_groupVertex.assign(groupCount(), kNone);
    for (std::size_t g = 0; g < groupCount(); ++g) {
        // Every group has a piece: each curve has at least one, and a curve
        // whose pieces were credited elsewhere shares them with its group.
        assert(!darts[g].empty());
        const Extreme extreme = extremeOf(darts[g]);
        for (const std::size_t dart : darts[g]) {
            m_groupVertex[g] = std::min(m_groupVertex[g], tail(dart));
        }
        // Nothing of the group lies west of its smallest point: the sector
        // toward -x there is in its unbounded face.
        std::size_t dart = kNone;
        if (extreme.atVertex) {
            dart = westLeft(extreme.point, around(extreme.index));
        } else {
            const std::array<std::size_t, 2> both{2 * extreme.index, 2 * extreme.index + 1};
            dart = westLeft(extreme.point, both);
        }
        m_holeCycle[g] = cycleOf(dart);
    }
}

// --- Faces, loops and parity ----------------------------------------------------

std::optional<std::size_t> Arrangement::faceOf(const ExactPoint& point) const {
    const Point2 rounded = nearestPoint(point);
    std::optional<std::size_t> best;
    for (std::size_t c = 0; c < cycleCount(); ++c) {
        const Box& box = m_cycleBox[c];
        if (isHole(c) || rounded.x < box.minX || rounded.x > box.maxX || rounded.y < box.minY ||
            rounded.y > box.maxY || !inside(point, cycle(c))) {
            continue;
        }
        // The cycles around the point are nested; of two, the inner one is
        // the one whose group lies inside the other (groups do not cross).
        if (!best) {
            best = c;
            continue;
        }
        assert(groupOfCycle(c) != groupOfCycle(*best) && "faces of a group are disjoint");
        if (inside(m_vertices[groupVertex(groupOfCycle(c))], cycle(*best))) {
            best = c;
        }
    }
    return best;
}

std::vector<std::vector<std::size_t>> Arrangement::simpleLoops(std::size_t c) const {
    // Leaving out a piece with the face on both sides may cut the cycle into
    // several closed walks (a bridge between two contours): each dart left
    // goes on to the first dart left of the cycle met turning clockwise
    // around its head, which is next() once those pieces are gone.
    const auto left = [&](std::size_t dart) {
        return cycleOf(dart) == c && cycleOf(twin(dart)) != c;
    };
    const auto following = [&](std::size_t dart) {
        const std::span<const std::size_t> darts = around(head(dart));
        const std::size_t back = m_aroundIndex[twin(dart)];
        for (std::size_t turn = 1; turn <= darts.size(); ++turn) {
            const std::size_t candidate = darts[(back + darts.size() - turn) % darts.size()];
            if (cycleOf(candidate) != c || cycleOf(twin(candidate)) != c) {
                assert(left(candidate) && "the face goes on along a dart of its cycle");
                return candidate;
            }
        }
        assert(false && "a dart left has a following dart");
        return dart;
    };

    std::vector<std::vector<std::size_t>> loops;
    std::vector<bool> visited(dartCount(), false);
    for (const std::size_t first : cycle(c)) {
        if (!left(first) || visited[first]) {
            continue;
        }
        // One closed walk, split into simple loops at every vertex it passes
        // again: the vertices the open path leaves, with the position of
        // their dart.
        std::vector<std::size_t> path;
        std::map<std::size_t, std::size_t> open;
        for (std::size_t dart = first; !visited[dart]; dart = following(dart)) {
            visited[dart] = true;
            const std::size_t from = tail(dart);
            if (const auto found = open.find(from); found != open.end()) {
                const std::size_t begin = found->second;
                for (std::size_t k = begin; k < path.size(); ++k) {
                    open.erase(tail(path[k]));
                }
                loops.emplace_back(path.begin() + static_cast<std::ptrdiff_t>(begin), path.end());
                path.resize(begin);
            }
            open.emplace(from, path.size());
            path.push_back(dart);
        }
        loops.push_back(std::move(path));
    }
    return loops;
}

bool Arrangement::counterclockwise(std::span<const std::size_t> loop) const {
    const Extreme extreme = extremeOf(loop);
    std::size_t out = kNone;
    std::size_t in = kNone;
    for (const std::size_t dart : loop) {
        if (extreme.atVertex ? tail(dart) == extreme.index : pieceOf(dart) == extreme.index) {
            out = dart;
        }
        if (extreme.atVertex ? head(dart) == extreme.index : pieceOf(dart) == extreme.index) {
            in = dart;
        }
    }
    assert(out != kNone && in != kNone);
    // West of the smallest point is outside: the loop runs clockwise when
    // that sector is on the left of the dart leaving the point.
    const std::array<std::size_t, 2> darts{out, twin(in)};
    return westLeft(extreme.point, darts) != out;
}

bool Arrangement::inside(const ExactPoint& point, std::span<const std::size_t> darts) const {
    // The parity of the crossings of the half-line y = Py + e, x > Px, for an
    // infinitely small e > 0 (winding.hpp, "The method"): a part monotone in y
    // is crossed when exactly one of its ends lies strictly above Py, and the
    // crossing is right of P as decided below.
    const auto above = [&](const ExactPoint& end) {
        return isPositive(compareY(end, point));
    };
    bool odd = false;
    for (const std::size_t dart : darts) {
        const std::size_t piece = pieceOf(dart);
        const CurveData& curve = m_curves[m_pieces[piece].curve];
        const ExactPoint& from = m_vertices[tail(dart)];
        const ExactPoint& to = m_vertices[head(dart)];
        if (const auto* line = std::get_if<LineMeaning>(&curve.carrier)) {
            if (above(from) == above(to)) {
                continue;
            }
            // Crossed right of P when P lies on the left of the line run
            // upward. The data are doubles, so end.y > start.y is exact.
            const Sign side = lineSign(*line, point);
            assert(side != Sign::zero && "the point lies on a dart");
            const bool upward = line->end.y > line->start.y;
            if (upward ? isPositive(side) : isNegative(side)) {
                odd = !odd;
            }
            continue;
        }
        const auto& circle = std::get<CircleSupport>(curve.carrier);
        const ExactPoint centre = circlePoint(circle, CircleSpot::centre);
        // The ends of the parts: the ends of the dart and the top and the
        // bottom of the circle when strictly inside it, in the order of the
        // piece, then turned to the order of the dart.
        std::vector<ExactPoint> ends{m_vertices[m_pieces[piece].from]};
        for (const CircleSpot spot : {CircleSpot::top, CircleSpot::bottom}) {
            ExactPoint extremum = circlePoint(circle, spot);
            if (strictlyInside(piece, extremum)) {
                ends.push_back(std::move(extremum));
            }
        }
        if (ends.size() == 3 && isPositive(compareFrom(curve.meaning, ends[0], ends[1], ends[2]))) {
            std::swap(ends[1], ends[2]);
        }
        ends.push_back(m_vertices[m_pieces[piece].to]);
        if (reversed(dart)) {
            std::reverse(ends.begin(), ends.end());
        }
        const bool turnsLeft = (curve.heading == Heading::forward) != reversed(dart);
        const Sign pointFromCentre = compareX(point, centre);
        const Sign pointFromCircle = circleSign(circle, point);
        for (std::size_t k = 0; k + 1 < ends.size(); ++k) {
            if (above(ends[k]) == above(ends[k + 1])) {
                continue;
            }
            // The part lies on one side of the vertical through the centre;
            // a part from the top to the bottom runs on the left when it
            // turns counterclockwise.
            bool right = false;
            if (const Sign x = compareX(ends[k], centre); x != Sign::zero) {
                right = isPositive(x);
            } else if (const Sign x1 = compareX(ends[k + 1], centre); x1 != Sign::zero) {
                right = isPositive(x1);
            } else {
                right = isPositive(compareY(ends[k], centre)) != turnsLeft;
            }
            // The crossing is Cx + sqrt(Q) on the right, Cx - sqrt(Q) on the
            // left, with Q = R^2 - (Py - Cy)^2 >= 0: right of P when P is
            // left of the centre or inside the circle, and when P is left of
            // the centre and outside the circle, respectively.
            assert((pointFromCircle != Sign::zero || right == isNegative(pointFromCentre)) &&
                   "the point lies on a dart");
            if (right ? isNegative(pointFromCentre) || isNegative(pointFromCircle)
                      : isNegative(pointFromCentre) && isPositive(pointFromCircle)) {
                odd = !odd;
            }
        }
    }
    return odd;
}

bool Arrangement::encloses(std::span<const std::size_t> outer,
                           std::span<const std::size_t> inner) const {
    std::vector<std::size_t> outerVertices;
    for (const std::size_t dart : outer) {
        outerVertices.push_back(tail(dart));
    }
    std::sort(outerVertices.begin(), outerVertices.end());
    // A vertex of inner off outer decides: inner does not cross outer.
    for (const std::size_t dart : inner) {
        if (!std::binary_search(outerVertices.begin(), outerVertices.end(), tail(dart))) {
            return inside(m_vertices[tail(dart)], outer);
        }
    }
    // Every vertex of inner is on outer: decide by the side of outer on
    // which inner leaves its first vertex.
    const std::size_t dart = inner.front();
    const std::size_t v = tail(dart);
    std::size_t out = kNone;
    std::size_t in = kNone;
    for (const std::size_t candidate : outer) {
        if (tail(candidate) == v) {
            out = candidate;
        }
        if (head(candidate) == v) {
            in = candidate;
        }
    }
    assert(out != kNone && in != kNone);
    const std::array<HalfPiece, 3> halves{halfPiece(out), halfPiece(twin(in)), halfPiece(dart)};
    const LocalOrder order(m_vertices[v], halves);
    assert(order.groupCount() == 3 && "the loops share no piece");
    const auto turnsFromOut = [&](std::size_t half) {
        return (order.groupOf(half) + 3 - order.groupOf(0)) % 3;
    };
    const bool onLeftOfOuter = turnsFromOut(2) < turnsFromOut(1);
    return onLeftOfOuter == counterclockwise(outer);
}

}  // namespace solid_space::geom2d::detail
