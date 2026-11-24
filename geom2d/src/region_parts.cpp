// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Splitting and classifying a host against the loops of a region
// (region_parts.hpp).
//
// Which pieces a host meets (package FS1 of the 0.3 performance work). A
// piece is left out of the meetings with the host only when it is certain to
// share no point with the carrier of the host, so addCuts() would add
// nothing for it; the cuts, their order and everything after are those of
// stage 0.3. Two tests, each exact or conservative:
// - for a straight host, the boxes are sorted along a normal n of a
//   direction (keyRange()), and the carrier, within the box of all the
//   pieces, along the same normal (lineKeys()): every point q of a piece lies
//   in its box and in the box of all the pieces, so if q lies on the line its
//   key n . q lies in both intervals, and a piece whose interval misses the
//   one of the line is certain to miss the line. The intervals are computed
//   in doubles, widened by margins that cover their rounding (see there).
//   Then the four corners of the box of every piece left: when
//   robust::orient2d(), which is exact, puts them all strictly on the same
//   side of the line, the box, which is convex, lies strictly on that side,
//   and so does the piece;
// - for a circular host, the box of its circle (arrangement.hpp, boxOf())
//   and the box of the piece must share a point (LoopIndex::meeting()).
// Parallel hosts give intervals of the line a few roundings wide, so each
// finds its pieces in about log n steps: the hatch of ADR 0015, point 9.

#include "region_parts.hpp"

#include "arrangement.hpp"
#include "local_order.hpp"
#include "positions.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The margins of the keys, the same as those of the boxes of arrangement.cpp:
// 2^-40 relative plus 2^-1000 absolute. Every computed double below is a sum
// of at most two products of doubles, or a difference, a quotient or a
// product of two such values: each operation is within 2^-53 of its result
// relative to it, plus 2^-1075 when the result is below the normal doubles.
// A few of these errors stay far below 2^-40 times the magnitude in play and
// 2^-1000; each margin is written next to the value it covers.
constexpr double kRelativeMargin = 0x1p-40;
constexpr double kAbsoluteMargin = 0x1p-1000;
// The magnitudes kept away from the subnormal doubles and from overflow in
// lineKeys(): a host shorter than 2^-500 or longer than 2^500, in the
// largest coordinate difference, is looked at against every piece.
constexpr double kShortestHost = 0x1p-500;
constexpr double kLongestHost = 0x1p500;
// The least slope term in lineKeys(), so that its quotient by the length of
// the host (at most 2^500) stays a normal double, at least 2^-900.
constexpr double kLeastTurn = 0x1p-400;
// A host is nearly parallel to the direction of the sorting when the cosine
// between it and the normal is below about 2^-20: only a choice of when to
// sort again, which never changes an answer.
constexpr double kParallel = 0x1p-20;

double margin(double magnitude) {
    return magnitude * kRelativeMargin + kAbsoluteMargin;
}

bool finite(const Box& box) {
    return std::isfinite(box.minX) && std::isfinite(box.minY) && std::isfinite(box.maxX) &&
           std::isfinite(box.maxY);
}

struct KeyRange {
    double low = -kInfinity;
    double high = kInfinity;
};

// An interval that contains n . q for every point q of the box. The least and
// the largest key are at the corners chosen by the signs of n, as exact sums
// of two products; each is computed with an error of at most 3 * 2^-53 times
// `size` plus 2^-1073 (two products and a sum, each rounded), and the
// subtraction or addition of the margin adds 2^-53 of |key| + margin: all
// within margin(size), since size >= |key| up to its own rounding. An
// infinite box gives the whole line of keys.
KeyRange keyRange(Point2 normal, const Box& box) {
    if (!finite(box)) {
        return {};
    }
    const bool rightward = normal.x >= 0.0;
    const bool upward = normal.y >= 0.0;
    const double low = (normal.x * (rightward ? box.minX : box.maxX)) +
                       (normal.y * (upward ? box.minY : box.maxY));
    const double high = (normal.x * (rightward ? box.maxX : box.minX)) +
                        (normal.y * (upward ? box.maxY : box.minY));
    const double size = (std::fabs(normal.x) * std::max(std::fabs(box.minX), std::fabs(box.maxX))) +
                        (std::fabs(normal.y) * std::max(std::fabs(box.minY), std::fabs(box.maxY)));
    const double slack = margin(size);
    const KeyRange range{low - slack, high + slack};
    if (!std::isfinite(range.low) || !std::isfinite(range.high)) {
        return {};
    }
    return range;
}

// An interval that contains n . q for every point q of the line within the
// box `whole`, or nothing when the doubles cannot bound it. With s and e the
// ends of the line and q = s + t (e - s) on it,
//     n . q = n . s + t * delta,   delta = n . (e - s),
// and |t| * |e - s|_inf = |q - s|_inf <= R, the largest distance of s.x and
// s.y from the sides of `whole`: so n . q lies within
// |delta| * R / |e - s|_inf of n . s. In doubles:
// - dx and dy are e - s rounded (2^-53 relative each); `turn` is delta from
//   them, with an error of at most 3 * 2^-53 of turnSize plus 2^-1073:
//   |turn| + margin(turnSize) >= |delta|, and kLeastTurn above it only
//   widens the interval;
// - `length` is |e - s|_inf within 2^-53, so length - margin(length) is at
//   most |e - s|_inf; `reach` + margin(reach) is at least R;
// - the quotient and the product are within 2 * 2^-53 relative, plus 2^-1075
//   for a product below the normal doubles (the quotient is at least 2^-900):
//   margin(spread) covers both;
// - the key of s has an error within margin(centreSize), as in keyRange(),
//   and the last subtraction and addition are covered by margin(|end|).
std::optional<KeyRange> lineKeys(Point2 normal, const LineMeaning& line, const Box& whole) {
    if (!finite(whole)) {
        return std::nullopt;
    }
    const Point2 s = line.start;
    const double dx = line.end.x - s.x;
    const double dy = line.end.y - s.y;
    const double length = std::max(std::fabs(dx), std::fabs(dy));
    const double shortest = length - margin(length);
    if (!(shortest >= kShortestHost) || !(length <= kLongestHost)) {
        return std::nullopt;
    }
    const double reach =
        std::max(std::max(std::fabs(whole.minX - s.x), std::fabs(whole.maxX - s.x)),
                 std::max(std::fabs(whole.minY - s.y), std::fabs(whole.maxY - s.y)));
    const double turn = (normal.x * dx) + (normal.y * dy);
    const double turnSize = std::fabs(normal.x * dx) + std::fabs(normal.y * dy);
    const double slope = std::max(std::fabs(turn) + margin(turnSize), kLeastTurn);
    const double spread = (slope / shortest) * (reach + margin(reach));
    const double centre = (normal.x * s.x) + (normal.y * s.y);
    const double centreSize = std::fabs(normal.x * s.x) + std::fabs(normal.y * s.y);
    const double slack = spread + margin(spread) + margin(centreSize);
    const double low = centre - slack;
    const double high = centre + slack;
    if (!std::isfinite(low) || !std::isfinite(high)) {
        return std::nullopt;
    }
    return KeyRange{low - margin(std::fabs(low)), high + margin(std::fabs(high))};
}

std::vector<std::size_t> everySlot(std::size_t count) {
    std::vector<std::size_t> all(count);
    std::iota(all.begin(), all.end(), std::size_t{0});
    return all;
}

// True when the box lies strictly on one side of the line: then no point of
// what it contains lies on the line. Exact (robust::orient2d()).
bool apart(const LineMeaning& line, const Box& box) {
    if (!finite(box)) {
        return false;
    }
    const std::array<Point2, 4> corners{
        {{box.minX, box.minY}, {box.maxX, box.minY}, {box.maxX, box.maxY}, {box.minX, box.maxY}}};
    const Sign first = robust::orient2d(line.start, line.end, corners[0]);
    if (first == Sign::zero) {
        return false;
    }
    return std::all_of(corners.begin() + 1, corners.end(), [&line, first](Point2 corner) {
        return robust::orient2d(line.start, line.end, corner) == first;
    });
}

// The normal of a straight host, (-dy, dx) with d = e - s rounded: any finite
// vector that is not zero would do, since every interval above holds for any
// normal; this one makes the hosts parallel to it give narrow intervals.
// Zero only for a degenerate host, since e - s rounds to zero only when e == s.
Point2 normalOf(const LineMeaning& line) {
    return Point2{-(line.end.y - line.start.y), line.end.x - line.start.x};
}

// True when the host is nearly perpendicular to the normal: |n . d| at most
// kParallel times |n|_1 |d|_1, which is at least |n| |d|, so the cosine
// between them is below about kParallel.
bool nearlyAlong(Point2 normal, const LineMeaning& line) {
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    const double turn = std::fabs((normal.x * dx) + (normal.y * dy));
    const double size =
        (std::fabs(normal.x) + std::fabs(normal.y)) * (std::fabs(dx) + std::fabs(dy));
    return turn <= size * kParallel;
}

bool fullCircle(const CurveMeaning& meaning) {
    const auto* arc = std::get_if<ArcMeaning>(&meaning.support);
    return arc != nullptr && (arc->span == ArcSpan::circle || arc->span == ArcSpan::whole);
}

bool circleFits(const CircleSupport& circle) {
    // Each axis needs room for the exact radius beyond the absolute centre.
    const auto limit = Dyadic(robust::kMaxCoordinate) * circle.weight;
    const auto maxCentre = compare(circle.centerX.abs(), circle.centerY.abs()) == Sign::positive
                               ? circle.centerX.abs()
                               : circle.centerY.abs();
    const auto room = limit - maxCentre;
    return room.sign() != Sign::negative &&
           compare(room * room, circle.radiusSquared) != Sign::negative;
}

std::optional<Diagnosis> appendPiece(LoopPieces& loop, const Curve& curve, std::size_t index,
                                     std::size_t input) {
    auto checked = checkedMeaning(curve, input, Degenerate::accept);
    if (!checked.hasValue()) {
        return checked.diagnosis();
    }
    auto meaning = std::move(checked).value();
    if (meaning.degenerate) {
        return std::nullopt;
    }
    auto carrier = carrierOf(meaning);
    if (const auto* circle = std::get_if<CircleSupport>(&carrier);
        circle != nullptr && !circleFits(*circle)) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "the carrying circle exceeds the coordinate domain"};
    }
    loop.pieces.push_back(loopPiece(index, std::move(meaning), std::move(carrier)));
    return std::nullopt;
}

bool validPoint(Point2 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::abs(point.x) <= robust::kMaxCoordinate &&
           std::abs(point.y) <= robust::kMaxCoordinate;
}

void addCuts(std::vector<ExactPoint>& cuts, const CurveMeaning& host, const Carrier& carrier,
             Extension extension, const LoopPiece& piece) {
    const auto meeting = meet(carrier, piece.carrier);
    if (meeting.sameCarrier && !fullCircle(piece.meaning)) {
        for (const auto& end : {startPoint(piece.meaning), endPoint(piece.meaning)}) {
            if (takesPart(locate(host, end), extension)) {
                cuts.push_back(end);
            }
        }
    }
    for (const auto& contact : meeting.found()) {
        if (takesPart(locate(host, contact.point), extension) &&
            takesPart(locate(piece.meaning, contact.point), Extension::none)) {
            cuts.push_back(contact.point);
        }
    }
}

PartClass classAfter(const ExactPoint& cut, const HalfPiece& along, const LoopIndex& loops,
                     std::vector<bool>& onLoop) {
    const std::array references{along};
    const auto sectors = sectorStates(cut, loops, references);
    const auto group = sectors.order.groupOf(sectors.loopHalfPieces());
    const auto previous = (group + sectors.order.groupCount() - 1) % sectors.order.groupCount();
    // A second half piece in the reference group shares its carrier and heading.
    onLoop.push_back(sectors.order.group(group).size() > 1);
    // Sweeping counterclockwise past the forward half piece enters its left
    // side. Each side keeps that parity until the next cut, including when
    // several loop pieces coincide with the host.
    const bool left = sectors.inside.at(group);
    const bool right = sectors.inside.at(previous);
    if (left == right) {
        return left ? PartClass::inside : PartClass::outside;
    }
    return left ? PartClass::boundaryLeft : PartClass::boundaryRight;
}

}  // namespace

Result<LoopPieces> checkedLoopPieces(const Loop& loop, std::size_t input) {
    LoopPieces result;
    if (const auto* circle = std::get_if<Circle>(&loop)) {
        if (auto problem = appendPiece(result, *circle, 0, input)) {
            return std::move(*problem);
        }
    } else {
        auto closed = std::get<Polyline>(loop);
        closed.closed = true;
        const auto segments = pieces(closed);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto& piece = segments[i];
            if (auto problem = appendPiece(result, piece, i, input)) {
                const auto vertex =
                    validPoint(piece.start) && std::isfinite(piece.bulge) && !validPoint(piece.end)
                        ? (i + 1) % closed.vertices.size()
                        : i;
                problem->message = "vertex " + std::to_string(vertex) + ": " + problem->message;
                return std::move(*problem);
            }
        }
    }
    if (result.pieces.empty()) {
        return degenerateDiagnosis(input);
    }
    return result;
}

RegionIndex::RegionIndex(std::span<const LoopPieces> loops, PieceSearch search)
    : m_loops(loops, search) {}

void RegionIndex::sortAlongNormal(Point2 normal) {
    m_normal = normal;
    std::vector<double> lows;
    std::vector<double> highs;
    lows.reserve(m_loops.size());
    highs.reserve(m_loops.size());
    for (std::size_t slot = 0; slot < m_loops.size(); ++slot) {
        const KeyRange range = keyRange(normal, m_loops.piece(slot).box);
        lows.push_back(range.low);
        highs.push_back(range.high);
    }
    m_alongNormal = IntervalIndex(lows, highs);
}

std::vector<std::size_t> RegionIndex::meeting(const CurveMeaning& host, const Carrier& carrier) {
    if (m_loops.search() == PieceSearch::everyPiece) {
        return m_loops.meeting(Box{});
    }
    const auto* line = std::get_if<LineMeaning>(&carrier);
    if (line == nullptr) {
        ArrangementCurve circle;
        circle.meaning = host;
        return m_loops.meeting(boxOf(circle));
    }
    const bool sorted = m_normal.x != 0.0 || m_normal.y != 0.0;
    if (!sorted || !nearlyAlong(m_normal, *line)) {
        sortAlongNormal(normalOf(*line));
    }
    const auto keys = lineKeys(m_normal, *line, m_loops.whole());
    auto found =
        keys.has_value() ? m_alongNormal.meeting(keys->low, keys->high) : everySlot(m_loops.size());
    std::erase_if(found,
                  [this, line](std::size_t slot) { return apart(*line, m_loops.piece(slot).box); });
    return found;
}

RegionParts regionParts(const CurveMeaning& host, Extension extension,
                        std::span<const LoopPieces> loops) {
    RegionIndex index(loops);
    return regionParts(host, extension, index);
}

RegionParts regionParts(const CurveMeaning& host, Extension extension, RegionIndex& loops) {
    RegionParts result;
    const auto* arc = std::get_if<ArcMeaning>(&host.support);
    if (arc != nullptr) {
        extension = Extension::none;
        result.closed = fullCircle(host);
    }
    if (extension == Extension::none) {
        result.cuts.push_back(startPoint(host));
        if (!result.closed) {
            result.cuts.push_back(endPoint(host));
        }
    }
    const auto carrier = carrierOf(host);
    for (const std::size_t slot : loops.meeting(host, carrier)) {
        addCuts(result.cuts, host, carrier, extension, loops.loops().piece(slot));
    }
    std::ranges::sort(result.cuts, [&host](const ExactPoint& a, const ExactPoint& b) {
        return compareAlong(host, a, b) == Sign::negative;
    });
    result.cuts.erase(std::unique(result.cuts.begin(), result.cuts.end(), samePoint),
                      result.cuts.end());
    if (result.closed) {
        result.cuts.push_back(startPoint(host));
    }
    const HalfPiece along{carrier, arc != nullptr && arc->turn == Turn::clockwise
                                       ? Heading::backward
                                       : Heading::forward};
    for (std::size_t i = 1; i < result.cuts.size(); ++i) {
        result.classes.push_back(
            classAfter(result.cuts[i - 1], along, loops.loops(), result.onLoop));
    }
    return result;
}

}  // namespace solid_space::geom2d::detail
