// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Which chosen contours lie inside which (boundary.hpp, nestContours).
//
// The exact path of stage 0.3 (NestSearch::everyPair) classifies every piece
// of each contour against the other with regionParts(), for every pair and in
// both directions. Package FS5 of the 0.3 performance work keeps that path for
// the pairs where it is needed and decides the others with less work, with the
// same answers (region_parts.hpp, NestSearch::boxes):
// - two contours whose boxes (the union of the boxes of their pieces, each
//   containing its piece: winding.hpp, pieceBox()) share no point share no
//   point either, and neither lies inside the other: every point of a contour
//   lies in its box, outside the box of the other, so a half-line from it
//   away from that box meets nothing of the other. regionParts() would give
//   every part of each the class outside, and none on the other: no conflict
//   and no depth, which is what skipping the pair gives. The pairs left are
//   found with overlappingPairs() (arrangement.hpp), sorted as the loop of
//   stage 0.3 visits them, so the conflicts keep their order;
// - two contours whose boxes overlap but whose pieces share no point: a
//   piece of one is compared with a piece of the other only when their boxes
//   overlap (the same reasoning, piece by piece), and then through meet() of
//   their carriers and locate() on both pieces, exactly as addCuts() of
//   region_parts.cpp finds the cuts. When no point is found and no two
//   carriers are the same, the contours are disjoint. A contour is connected
//   (its pieces follow each other end to end, and a piece dropped as a point
//   joins equal ends), so it lies in a single component of the plane without
//   the other contour, where the parity of the even-odd rule is constant:
//   every part regionParts() would give has that class and none lies on the
//   other contour. The exact parity of one point of each contour (the start
//   of its first piece, through pointStatus() of winding.hpp) is then the
//   class of all of it, and countEnclosures() gets the same flags. A point
//   that the intervals of its coordinates put strictly outside the box of
//   the other contour is outside it with no further work (disjointParts());
// - as soon as two pieces share a point, or two carriers are the same, the
//   pair takes the exact path of stage 0.3, unchanged: the conflicts same
//   and crossing, the touching and the shared stretches are decided there.
// No double decides anything: the boxes only skip comparisons whose outcome
// is certain, since they contain their pieces (rounded outward).

#include "arrangement.hpp"
#include "carriers.hpp"
#include "positions.hpp"
#include "region_parts.hpp"
#include "solid_space/geom2d/boundary.hpp"
#include "winding.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d {
namespace detail {
namespace {

struct OffLoopParts {
    bool inside = false;
    bool outside = false;

    [[nodiscard]] bool mixed() const { return inside && outside; }

    [[nodiscard]] bool allOnLoop() const { return !inside && !outside; }
};

OffLoopParts offLoopParts(const LoopPieces& host, const LoopPieces& loop) {
    OffLoopParts offLoop;
    for (const auto& piece : host.pieces) {
        const auto parts = regionParts(piece.meaning, Extension::none, std::span{&loop, 1});
        for (std::size_t i = 0; i < parts.classes.size(); ++i) {
            if (parts.onLoop.at(i)) {
                continue;
            }
            // Points away from the loop have the same parity on both sides.
            offLoop.inside |= parts.classes.at(i) == PartClass::inside;
            offLoop.outside |= parts.classes.at(i) == PartClass::outside;
        }
        if (offLoop.mixed()) {
            break;
        }
    }
    return offLoop;
}

// True when the exact point lies strictly outside the box, by the intervals
// of its coordinates (carriers.hpp, approximationOf()): the exact x lies
// within centre +- radius, and rounding to nearest never decreases, so when
// the rounded centre + radius is below minX the exact sum is too, and so is
// x (the same on the other three sides). False when there are no intervals.
bool outsideBox(const ExactPoint& point, const Box& box) {
    const auto intervals = approximationOf(point);
    if (!intervals.has_value()) {
        return false;
    }
    const auto& [x, y] = *intervals;
    return x.centre + x.radius < box.minX || x.centre - x.radius > box.maxX ||
           y.centre + y.radius < box.minY || y.centre - y.radius > box.maxY;
}

// The flags offLoopParts() gives when the host, whose first piece starts at
// `start`, shares no point with the loop: every part has the parity of any of
// its points (see the top of the file).
// A point strictly outside the box of the loop, which contains the loop, lies
// with all the outside of the box in the unbounded component of the plane
// without the loop, which is outside: then pointStatus() is not needed.
OffLoopParts disjointParts(const ExactPoint& start, const LoopIndex& loop, const Box& loopBox) {
    if (outsideBox(start, loopBox)) {
        return {false, true};
    }
    const auto status = pointStatus(start, loop);
    assert(status.state != PointState::on);
    const bool inside = status.state == PointState::inside;
    return {inside, !inside};
}

void countEnclosures(ContourNesting& nesting, std::size_t first, std::size_t second,
                     const OffLoopParts& aInB, const OffLoopParts& bInA) {
    if (aInB.allOnLoop() && bInA.allOnLoop()) {
        nesting.conflicts.push_back({first, second, ContourConflictKind::same});
    } else if (aInB.mixed() || bInA.mixed() || (aInB.inside && bInA.inside)) {
        nesting.conflicts.push_back({first, second, ContourConflictKind::crossing});
    } else {
        auto& firstDepth = nesting.depth.at(first);
        auto& secondDepth = nesting.depth.at(second);
        firstDepth = firstDepth.value_or(0) + static_cast<std::size_t>(aInB.inside);
        secondDepth = secondDepth.value_or(0) + static_cast<std::size_t>(bInA.inside);
    }
}

// The exact path of stage 0.3 for one pair.
void countEnclosures(ContourNesting& nesting, std::span<const LoopPieces> loops, std::size_t first,
                     std::size_t second) {
    const auto& a = loops[first];
    const auto& b = loops[second];
    countEnclosures(nesting, first, second, offLoopParts(a, b), offLoopParts(b, a));
}

// True when some piece of a shares a point with some piece of b, or the two
// carriers of a piece of each are the same: then the pair takes the exact
// path. Exact; the boxes only skip pairs of pieces that cannot meet.
bool touch(const LoopPieces& a, const LoopIndex& b) {
    for (const auto& piece : a.pieces) {
        for (const std::size_t slot : b.meeting(piece.box)) {
            const auto& other = b.piece(slot);
            if (!overlaps(piece.box, other.box)) {
                continue;
            }
            const auto meeting = meet(piece.carrier, other.carrier);
            if (meeting.sameCarrier) {
                return true;
            }
            for (const auto& contact : meeting.found()) {
                if (takesPart(locate(piece.meaning, contact.point), Extension::none) &&
                    takesPart(locate(other.meaning, contact.point), Extension::none)) {
                    return true;
                }
            }
        }
    }
    return false;
}

Box loopBox(const LoopPieces& loop) {
    Box box = loop.pieces.front().box;
    for (const auto& piece : loop.pieces) {
        box.minX = std::min(box.minX, piece.box.minX);
        box.minY = std::min(box.minY, piece.box.minY);
        box.maxX = std::max(box.maxX, piece.box.maxX);
        box.maxY = std::max(box.maxY, piece.box.maxY);
    }
    return box;
}

void nestByBoxes(ContourNesting& result, std::span<const LoopPieces> loops,
                 std::span<const std::size_t> kept) {
    std::vector<LoopIndex> indices;
    std::vector<Box> boxes;
    std::vector<ExactPoint> starts;
    indices.reserve(kept.size());
    boxes.reserve(kept.size());
    starts.reserve(kept.size());
    for (const std::size_t i : kept) {
        indices.emplace_back(loops.subspan(i, 1));
        boxes.push_back(loopBox(loops[i]));
        starts.push_back(startPoint(loops[i].pieces.front().meaning));
    }
    // The pairs come sorted by (i, j) of kept, which grows with the index of
    // the contour: the order of the loop of stage 0.3.
    for (const auto& [i, j] : overlappingPairs(boxes)) {
        const std::size_t first = kept[i];
        const std::size_t second = kept[j];
        if (touch(loops[first], indices[j])) {
            countEnclosures(result, loops, first, second);
        } else {
            countEnclosures(result, first, second, disjointParts(starts[i], indices[j], boxes[j]),
                            disjointParts(starts[j], indices[i], boxes[i]));
        }
    }
}

}  // namespace

ContourNesting nestContours(std::span<const Loop> contours, NestSearch search) {
    ContourNesting result;
    result.depth.resize(contours.size());
    std::vector<LoopPieces> loops(contours.size());
    std::vector<std::size_t> kept;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        auto checked = checkedLoopPieces(contours[i], i);
        if (checked.hasValue()) {
            loops.at(i) = std::move(checked).value();
            result.depth.at(i) = 0;
            kept.push_back(i);
        } else {
            result.skipped.push_back({i, checked.diagnosis()});
        }
    }
    if (search == NestSearch::boxes) {
        nestByBoxes(result, loops, kept);
        return result;
    }
    for (std::size_t i = 0; i < kept.size(); ++i) {
        for (std::size_t j = i + 1; j < kept.size(); ++j) {
            countEnclosures(result, loops, kept.at(i), kept.at(j));
        }
    }
    return result;
}

}  // namespace detail

ContourNesting nestContours(std::span<const Loop> contours) {
    return detail::nestContours(contours, detail::NestSearch::boxes);
}

}  // namespace solid_space::geom2d
