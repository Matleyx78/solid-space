// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// intersect of intersection.hpp (stage 0.2, package P10; ADR 0010).
//
// The common points of two curves come from the exact meeting of their
// carriers (carriers.hpp): each point is kept when it lies on the part of both
// curves that takes part (positions.hpp, locate and takesPart), counted when it
// lies beyond robust::kMaxCoordinate, and listed otherwise. Curves on the same
// line or circle share stretches instead: their exact intervals along the
// first curve are intersected, and an intersection reduced to one point is a
// point with Contact::sameCurve. Every decision is exact.
//
// Symmetry. Every output is a function of exact values: the positions of a
// point depend only on the point and the curve (positions.hpp), the points are
// correctly rounded (nearestPoint), and the meeting of the carriers is the
// same whatever their order (carriers.hpp, meet). So intersect(b, a) gives
// the same bits as intersect(a, b), with the roles swapped and sorted along b.
//
// Floating-point filter (ADR 0010, point 8). Most pairs given to intersect do
// not meet (the snap tries every curve near the cursor). Two tests reject them
// before any exact arithmetic, and they never reject a pair that meets:
// - boxes: a straight curve that is not extended lies in the box of its ends,
//   which is exact; a circular curve (extended or not) lies in the box of its
//   circle, and a curved polyline segment of at most half a turn (|bulge| <= 1)
//   in the box of the disc on its chord, since such an arc sees its chord
//   under an angle of at least a right angle. The bounds of these boxes are
//   sums of doubles rounded to nearest, so each is within half an ulp of its
//   exact value; moving it one more ulp outwards (std::nextafter) makes the box
//   contain the exact one. Two boxes that do not overlap leave no common point;
// - two straight curves: the side of the ends of each one with respect to the
//   line of the other, from robust::orient2d (exact, with its own filter).
//   When a curve that is not extended has both ends strictly on the same side
//   of the other line, they have no common point.

#include "solid_space/geom2d/intersection.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "positions.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::ArcMeaning;
using detail::ArcSpan;
using detail::CircleSupport;
using detail::CurveMeaning;
using detail::Dyadic;
using detail::ExactPoint;
using detail::ExactVector;
using detail::LineMeaning;
using robust::Sign;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// --- Filter ----------------------------------------------------------------

// A closed box that contains a curve; no value means the whole plane.
struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

Box boxOfEnds(Point2 start, Point2 end) {
    return Box{std::min(start.x, end.x), std::min(start.y, end.y), std::max(start.x, end.x),
               std::max(start.y, end.y)};
}

// The box of the circle of an Arc or a Circle: centre -+ radius, each bound
// rounded and moved one ulp outwards.
Box boxOfCircle(Point2 centre, double radius) {
    return Box{below(centre.x - radius), below(centre.y - radius), above(centre.x + radius),
               above(centre.y + radius)};
}

// The box of the disc on the chord of a curved polyline segment of at most
// half a turn: within the box of its ends widened by half the chord's extent
// in the other coordinate (the disc has centre (start + end) / 2 and radius
// |chord| / 2 <= (|dx| + |dy|) / 2). |dx| and |dy| are rounded upwards first.
Box boxOfChordDisc(const PolylineSegment& piece) {
    const double halfX = above(std::fabs(piece.end.x - piece.start.x)) * 0.5;
    const double halfY = above(std::fabs(piece.end.y - piece.start.y)) * 0.5;
    const auto ends = boxOfEnds(piece.start, piece.end);
    return Box{below(ends.minX - halfY), below(ends.minY - halfX), above(ends.maxX + halfY),
               above(ends.maxY + halfX)};
}

std::optional<Box> boxOf(const CurveMeaning& meaning, Extension extension) {
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        if (extension == Extension::extended) {
            return std::nullopt;
        }
        return boxOfEnds(line->start, line->end);
    }
    if (const auto* arc = std::get_if<Arc>(&meaning.curve)) {
        return boxOfCircle(arc->center, arc->radius);
    }
    if (const auto* circle = std::get_if<Circle>(&meaning.curve)) {
        return boxOfCircle(circle->center, circle->radius);
    }
    const auto& piece = std::get<PolylineSegment>(meaning.curve);
    if (extension == Extension::extended || std::fabs(piece.bulge) > 1.0) {
        return std::nullopt;
    }
    return boxOfChordDisc(piece);
}

bool apart(const Box& a, const Box& b) noexcept {
    return a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY;
}

bool sameStrictSide(Sign a, Sign b) noexcept {
    return a != Sign::zero && a == b;
}

// True when the curve from start to end, not extended, has both ends
// strictly on the same side of the line of the other curve.
bool beside(const LineMeaning& curve, const LineMeaning& other) noexcept {
    return sameStrictSide(robust::orient2d(other.start, other.end, curve.start),
                          robust::orient2d(other.start, other.end, curve.end));
}

// True when the filter proves that the two curves have no common point.
bool cannotMeet(const CurveMeaning& first, Extension firstExtension, const CurveMeaning& second,
                Extension secondExtension) {
    const auto firstBox = boxOf(first, firstExtension);
    const auto secondBox = boxOf(second, secondExtension);
    if (firstBox && secondBox && apart(*firstBox, *secondBox)) {
        return true;
    }
    const auto* firstLine = std::get_if<LineMeaning>(&first.support);
    const auto* secondLine = std::get_if<LineMeaning>(&second.support);
    if (firstLine == nullptr || secondLine == nullptr) {
        return false;
    }
    return (firstExtension == Extension::none && beside(*firstLine, *secondLine)) ||
           (secondExtension == Extension::none && beside(*secondLine, *firstLine));
}

// --- Results ---------------------------------------------------------------

// A common point found exactly, before rounding.
struct Found {
    ExactPoint point;
    Contact contact = Contact::crossing;
    Location onFirst = Location::inside;
    Location onSecond = Location::inside;
};

// A shared stretch found exactly, from its start to its end in the direction
// of the first curve; whole when it is the whole carrier.
struct Stretch {
    bool whole = false;
    ExactPoint from;
    ExactPoint to;
};

struct Pair {
    const CurveMeaning& first;
    Extension firstExtension;
    const CurveMeaning& second;
    Extension secondExtension;
};

// Adds a point that lies exactly on both carriers, if it is on the part of
// both curves that takes part: listed, or counted when beyond the domain.
void addPoint(const Pair& pair, ExactPoint point, Contact contact, std::vector<Found>& found,
              std::size_t& omitted) {
    const auto onFirst = detail::locate(pair.first, point);
    if (!detail::takesPart(onFirst, pair.firstExtension)) {
        return;
    }
    const auto onSecond = detail::locate(pair.second, point);
    if (!detail::takesPart(onSecond, pair.secondExtension)) {
        return;
    }
    if (detail::beyondDomain(point)) {
        ++omitted;
        return;
    }
    found.push_back(Found{std::move(point), contact, onFirst, onSecond});
}

// The part of a common carrier between two ends, in the direction of the first
// curve: a stretch, or a single point when the two ends are the same point.
void addShared(const Pair& pair, ExactPoint from, ExactPoint to, std::vector<Stretch>& stretches,
               std::vector<Found>& found, std::size_t& omitted) {
    if (detail::compareAlong(pair.first, from, to) == Sign::zero) {
        addPoint(pair, std::move(from), Contact::sameCurve, found, omitted);
        return;
    }
    stretches.push_back(Stretch{false, std::move(from), std::move(to)});
}

// Two straight curves on the same line, ordered along the first by
// dot(X, U1): the first is [start, end] or the whole line, the second
// [low, high] in that order or the whole line.
struct LineEnd {
    Point2 point;
    Dyadic key;
};

void shareLine(const Pair& pair, std::vector<Stretch>& stretches, std::vector<Found>& found,
               std::size_t& omitted) {
    const auto& line1 = std::get<LineMeaning>(pair.first.support);
    const auto& line2 = std::get<LineMeaning>(pair.second.support);
    const auto start1 = detail::exactVector(line1.start);
    const ExactVector direction{Dyadic(line1.end.x) - start1.x, Dyadic(line1.end.y) - start1.y};
    const auto keyOf = [&direction](Point2 point) {
        const auto value = detail::exactVector(point);
        return LineEnd{point, value.x * direction.x + value.y * direction.y};
    };
    const bool firstWhole = pair.firstExtension == Extension::extended;
    const bool secondWhole = pair.secondExtension == Extension::extended;
    if (firstWhole && secondWhole) {
        stretches.push_back(
            Stretch{true, detail::exactPoint(line1.start), detail::exactPoint(line1.start)});
        return;
    }
    std::optional<LineEnd> low;
    std::optional<LineEnd> high;
    if (!firstWhole) {
        low = keyOf(line1.start);
        high = keyOf(line1.end);
    }
    if (!secondWhole) {
        auto a = keyOf(line2.start);
        auto b = keyOf(line2.end);
        if (compare(a.key, b.key) == Sign::positive) {
            std::swap(a, b);
        }
        if (!low || compare(a.key, low->key) == Sign::positive) {
            low = std::move(a);
        }
        if (!high || compare(b.key, high->key) == Sign::negative) {
            high = std::move(b);
        }
    }
    if (compare(low->key, high->key) == Sign::positive) {
        return;
    }
    addShared(pair, detail::exactPoint(low->point), detail::exactPoint(high->point), stretches,
              found, omitted);
}

// An end of a circular interval: a direction from the centre, and the circle
// (of one of the two curves) that it is taken on.
struct CircleEnd {
    const CircleSupport* circle = nullptr;
    ExactVector direction;
};

ExactPoint pointOf(const CircleEnd& end) {
    return detail::pointInDirection(*end.circle, end.direction);
}

bool coversCircle(const ArcMeaning& arc, Extension extension) noexcept {
    return extension == Extension::extended || arc.span == ArcSpan::whole ||
           arc.span == ArcSpan::circle;
}

// Two circular curves on the same circle. Angles are measured from D0 of the
// first curve in its direction (curve_meaning.hpp, compareAlongArc): the
// first curve is [0, a] or the whole circle, the second [b0, b1] (running
// through 0 when b0 > b1) or the whole circle. Directions of either curve are
// compared as vectors from the common centre: each is a positive multiple of
// the true direction.
void shareCircle(const Pair& pair, std::vector<Stretch>& stretches, std::vector<Found>& found,
                 std::size_t& omitted) {
    const auto& arc1 = std::get<ArcMeaning>(pair.first.support);
    const auto& arc2 = std::get<ArcMeaning>(pair.second.support);
    const bool firstWhole = coversCircle(arc1, pair.firstExtension);
    const bool secondWhole = coversCircle(arc2, pair.secondExtension);
    if (firstWhole && secondWhole) {
        stretches.push_back(
            Stretch{true, detail::startPoint(pair.first), detail::startPoint(pair.first)});
        return;
    }
    const CircleEnd zero{&arc1.circle, arc1.startDirection};
    const CircleEnd a{&arc1.circle, arc1.endDirection};
    if (secondWhole) {
        addShared(pair, pointOf(zero), pointOf(a), stretches, found, omitted);
        return;
    }
    const bool sameTurn = arc1.turn == arc2.turn;
    const CircleEnd b0{&arc2.circle, sameTurn ? arc2.startDirection : arc2.endDirection};
    const CircleEnd b1{&arc2.circle, sameTurn ? arc2.endDirection : arc2.startDirection};
    if (firstWhole) {
        addShared(pair, pointOf(b0), pointOf(b1), stretches, found, omitted);
        return;
    }
    const auto order = [&arc1](const CircleEnd& u, const CircleEnd& v) {
        return detail::compareAlongArc(arc1, u.direction, v.direction);
    };
    // The smaller of b1 and a: the end of the part of [.., b1] within [0, a].
    const CircleEnd& end = order(b1, a) == Sign::positive ? a : b1;
    if (order(b0, b1) != Sign::positive) {
        // [b0, b1] does not run through 0.
        if (order(b0, a) != Sign::positive) {
            addShared(pair, pointOf(b0), pointOf(end), stretches, found, omitted);
        }
        return;
    }
    // [b0, 2 pi) and [0, b1]: the part from 0, then the part from b0.
    addShared(pair, pointOf(zero), pointOf(end), stretches, found, omitted);
    if (order(b0, a) != Sign::positive) {
        addShared(pair, pointOf(b0), pointOf(a), stretches, found, omitted);
    }
}

OverlapEnd overlapEnd(const Pair& pair, const ExactPoint& point) {
    return OverlapEnd{detail::nearestPoint(point), detail::positionOn(pair.first, point),
                      detail::positionOn(pair.second, point)};
}

PairIntersections intersectMeanings(const Pair& pair) {
    PairIntersections result;
    if (cannotMeet(pair.first, pair.firstExtension, pair.second, pair.secondExtension)) {
        return result;
    }
    std::vector<Found> found;
    std::vector<Stretch> stretches;
    const auto meeting =
        detail::meet(detail::carrierOf(pair.first), detail::carrierOf(pair.second));
    if (meeting.sameCarrier) {
        if (std::holds_alternative<LineMeaning>(pair.first.support)) {
            shareLine(pair, stretches, found, result.omittedBeyondDomain);
        } else {
            shareCircle(pair, stretches, found, result.omittedBeyondDomain);
        }
    } else {
        for (const auto& point : meeting.found()) {
            addPoint(pair, point.point, point.contact, found, result.omittedBeyondDomain);
        }
    }
    // At most two of each: a simple exact sort along the first curve.
    std::sort(found.begin(), found.end(), [&pair](const Found& a, const Found& b) {
        return detail::compareAlong(pair.first, a.point, b.point) == Sign::negative;
    });
    std::sort(stretches.begin(), stretches.end(), [&pair](const Stretch& a, const Stretch& b) {
        return detail::compareAlong(pair.first, a.from, b.from) == Sign::negative;
    });
    result.points.reserve(found.size());
    for (const auto& point : found) {
        result.points.push_back(IntersectionPoint{
            detail::nearestPoint(point.point), point.contact,
            CurvePosition{point.onFirst, detail::station(pair.first, point.point)},
            CurvePosition{point.onSecond, detail::station(pair.second, point.point)}});
    }
    result.overlaps.reserve(stretches.size());
    for (const auto& stretch : stretches) {
        result.overlaps.push_back(
            Overlap{stretch.whole, overlapEnd(pair, stretch.from), overlapEnd(pair, stretch.to)});
    }
    return result;
}

}  // namespace

Result<PairIntersections> intersect(const Curve& first, Extension firstExtension,
                                    const Curve& second, Extension secondExtension) {
    auto firstMeaning = detail::checkedMeaning(first, 0);
    if (!firstMeaning.hasValue()) {
        return firstMeaning.diagnosis();
    }
    auto secondMeaning = detail::checkedMeaning(second, 1);
    if (!secondMeaning.hasValue()) {
        return secondMeaning.diagnosis();
    }
    return intersectMeanings(
        Pair{firstMeaning.value(), firstExtension, secondMeaning.value(), secondExtension});
}

}  // namespace solid_space::geom2d
