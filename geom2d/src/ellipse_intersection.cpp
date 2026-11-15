// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The two intersect() of ellipse.hpp (stage 0.3, package PS7; ADR 0017,
// Decisione, points 4, 7, 8 and 9), and the meeting of an ellipse with a conic
// declared in ellipse_conic.hpp.
//
// Three ways, as the second input is:
// - a straight curve (Segment, PolylineSegment with bulge 0): the exact
//   meeting of the ellipse and the line of ellipse_meaning.hpp, the same that
//   meetsRectangle() uses on the sides of the rectangle. Its points have the
//   form (a + b sqrt(d)) / w, and their order along the ellipse is decided
//   from their directions on its unit circle;
// - a circle, an arc, a curved piece or another ellipse on a different
//   carrier: the rational form of the first ellipse in the equation of the
//   second carrier, a polynomial of degree at most four (ellipse_conic.hpp).
//   A multiple root is a tangency (touching); the point of the ellipse that
//   the rational form misses is a common point of multiplicity 4 - degree;
// - the same carrier (the equation is the zero polynomial: two ellipses with
//   the same points, or an ellipse of ratio 1 and its circle): the shared
//   stretches, as for two arcs of one circle in intersection.cpp. Every end
//   is a direction on the unit circle of the first ellipse: the carriers have
//   one centre, and the vector from it to an end of either input, mapped to
//   that unit circle, is a dyadic vector, up to a positive factor.
//
// Every decision is exact, and depends only on the exact data: so swapping two
// ellipses gives the same decisions (ellipse.hpp, the second intersect). The
// points are rounded within kRelativePointError (curves.hpp): see
// roundedRootPoint() for the points of the quartic; the others are correctly
// rounded (positions.hpp, nearestPoint).
//
// Floating-point filter (ADR 0017, Decisione, point 9). Two inputs whose boxes
// do not overlap have no common point. The box of an ellipse is
// centre +- (|major.x| + |major.y|) in both coordinates: the extent of the
// ellipse along x is sqrt(major.x^2 + minor.x^2) <= |major.x| + |minor.x|,
// and |minor.x| = ratio * |major.y| <= |major.y| (likewise along y). The box
// of a circle is centre +- radius, that of a segment not extended the box of
// its ends. Every bound is a sum of doubles rounded to nearest, so within half
// an ulp of its exact value, and is moved one ulp outwards (std::nextafter),
// after each rounding, so that it contains the exact box. Lines that are
// extended and curved pieces get no box: they go to the exact path.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "ellipse_conic.hpp"
#include "ellipse_meaning.hpp"
#include "exact/dyadic.hpp"
#include "exact/polynomial.hpp"
#include "exact/quadratic.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/ellipse.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::Dyadic;
using detail::ExactVector;
using exact::Polynomial;

const Dyadic kOne{1.0};

Dyadic cross(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

// a x (x, y) and a . (x, y), with polynomial coordinates.
Polynomial crossOf(const ExactVector& a, const Polynomial& x, const Polynomial& y) {
    return y * a.x - x * a.y;
}

Polynomial dotOf(const ExactVector& a, const Polynomial& x, const Polynomial& y) {
    return x * a.x + y * a.y;
}

}  // namespace

namespace detail {

RationalEllipse rationalForm(const EllipseSupport& support) {
    const auto& center = support.center;
    const auto& major = support.major;
    const auto& minor = support.minor;
    // center * (1 + s^2) + major * (1 - s^2) + minor * 2 s.
    return {
        Polynomial{center.x + major.x, minor.x.timesPowerOfTwo(1), center.x - major.x},
        Polynomial{center.y + major.y, minor.y.timesPowerOfTwo(1), center.y - major.y},
        Polynomial{kOne, Dyadic{}, kOne},
    };
}

ConicTrace traceOfCircle(const RationalEllipse& form, const CircleSupport& circle) {
    // |P - c / w|^2 = rho / w^2, times w^2 weight^2.
    auto directionX = form.x * circle.weight - form.weight * circle.centerX;
    auto directionY = form.y * circle.weight - form.weight * circle.centerY;
    auto equation = directionX * directionX + directionY * directionY -
                    form.weight * form.weight * circle.radiusSquared;
    return {std::move(directionX), std::move(directionY), std::move(equation)};
}

ConicTrace traceOfEllipse(const RationalEllipse& form, const EllipseSupport& other) {
    // With q = P - centre: (q x minor)^2 + (major x q)^2 - determinant^2 = 0
    // (ellipse_meaning.hpp), times weight^2. The two cross products are the
    // direction of P on the unit circle of the other ellipse, times
    // determinant * weight.
    const auto dx = form.x - form.weight * other.center.x;
    const auto dy = form.y - form.weight * other.center.y;
    auto directionX = dx * other.minor.y - dy * other.minor.x;
    auto directionY = dy * other.major.x - dx * other.major.y;
    auto equation = directionX * directionX + directionY * directionY -
                    form.weight * form.weight * (other.determinant * other.determinant);
    return {std::move(directionX), std::move(directionY), std::move(equation)};
}

std::vector<ConicMeetingPoint> meetEllipseConic(const EllipseMeaning& ellipse,
                                                const ConicTrace& trace,
                                                const ArcMeaning& otherArc) {
    assert(!trace.equation.isZero());
    const auto& arc = ellipse.unitArc;
    // w(s) = (1 - s^2, 2 s): the direction of P(s) on the unit circle.
    const Polynomial unitX{kOne, Dyadic{}, -kOne};
    const Polynomial unitY{Dyadic{}, Dyadic(2.0)};
    const auto crossFromStart = crossOf(arc.startDirection, unitX, unitY);
    const auto dotFromStart = dotOf(arc.startDirection, unitX, unitY);
    const auto crossToEnd = -crossOf(arc.endDirection, unitX, unitY);
    const auto otherCrossFromStart =
        crossOf(otherArc.startDirection, trace.directionX, trace.directionY);
    const auto otherDotFromStart =
        dotOf(otherArc.startDirection, trace.directionX, trace.directionY);
    const auto otherCrossToEnd =
        -crossOf(otherArc.endDirection, trace.directionX, trace.directionY);

    // The missing point is the direction (-1, 0), at angle pi. The parameters
    // run counterclockwise from angle -pi to pi, so the order from D0 is: the
    // roots from D0 up to angle pi, the missing point, then the roots from
    // angle -pi up to D0, each part by increasing parameter.
    const ExactVector missing{-kOne, Dyadic{}};
    const auto missingSector = sectorFromStart(arc, missing);
    std::vector<ConicMeetingPoint> beforeMissing;
    std::vector<ConicMeetingPoint> afterMissing;
    for (auto& root : exact::realRoots(trace.equation)) {
        const auto fromStart = sectorOf(oriented(arc.turn, exact::signAt(crossFromStart, root)),
                                        exact::signAt(dotFromStart, root));
        const auto onEllipse =
            locateOnArc(arc, fromStart, oriented(arc.turn, exact::signAt(crossToEnd, root)));
        const auto otherFromStart =
            sectorOf(oriented(otherArc.turn, exact::signAt(otherCrossFromStart, root)),
                     exact::signAt(otherDotFromStart, root));
        const auto onOther =
            locateOnArc(otherArc, otherFromStart,
                        oriented(otherArc.turn, exact::signAt(otherCrossToEnd, root)));
        // w(s) x (-1, 0) = 2 s.
        const auto crossToMissing = oriented(arc.turn, exact::compare(root, Dyadic{}));
        const bool before =
            compareTurns(fromStart, missingSector, crossToMissing) == Sign::negative;
        const auto contact = root.multiplicity() > 1 ? Contact::touching : Contact::crossing;
        auto& part = before ? beforeMissing : afterMissing;
        part.push_back(ConicMeetingPoint{std::move(root), contact, onEllipse, onOther});
    }

    std::vector<ConicMeetingPoint> result = std::move(beforeMissing);
    const auto multiplicityAtInfinity = exact::kMaxDegree - trace.equation.degree();
    if (multiplicityAtInfinity > 0) {
        // The direction of P(s) seen from the other conic tends to that of its
        // coefficients of s^2 (weight has leading coefficient 1).
        const ExactVector otherMissing{trace.directionX.coefficient(2),
                                       trace.directionY.coefficient(2)};
        result.push_back(ConicMeetingPoint{
            std::nullopt, multiplicityAtInfinity > 1 ? Contact::touching : Contact::crossing,
            locateDirection(arc, missing), locateDirection(otherArc, otherMissing)});
    }
    result.insert(result.end(), std::make_move_iterator(afterMissing.begin()),
                  std::make_move_iterator(afterMissing.end()));
    return result;
}

}  // namespace detail

namespace {

using detail::ArcMeaning;
using detail::ArcSpan;
using detail::EllipseMeaning;
using detail::EllipseSupport;
using detail::ExactPoint;
using detail::LineMeaning;
using detail::RationalEllipse;
using exact::RealRoot;
using robust::Sign;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The isolating interval of a root is narrowed to 2^kParameterWidthExponent,
// times s^2 far from zero, before its point is built (roundedRootPoint).
constexpr std::int64_t kParameterWidthExponent = -56;

// Above this the square of the parameter is not used to widen the interval:
// 2^(2 * 1000) is beyond every useful width, and the bound keeps the exponent
// arithmetic small.
constexpr int kLargestParameterExponent = 1000;

// Up to the positive factor determinant, the direction on the unit circle of
// the ellipse of the vector v from its centre (the inverse of F without its
// translation): for v = major * u + minor * t it is determinant * (u, t).
ExactVector toUnitCircle(const EllipseSupport& support, const ExactVector& v) {
    return {cross(v, support.minor), cross(support.major, v)};
}

// --- Filter ----------------------------------------------------------------

struct Box {
    double minX = -kInfinity;
    double minY = -kInfinity;
    double maxX = kInfinity;
    double maxY = kInfinity;
};

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

Box boxOfEllipse(const Ellipse& ellipse) {
    const double reach = above(std::fabs(ellipse.majorAxis.x) + std::fabs(ellipse.majorAxis.y));
    return {below(ellipse.center.x - reach), below(ellipse.center.y - reach),
            above(ellipse.center.x + reach), above(ellipse.center.y + reach)};
}

Box boxOfCircle(Point2 centre, double radius) {
    return {below(centre.x - radius), below(centre.y - radius), above(centre.x + radius),
            above(centre.y + radius)};
}

Box boxOfEnds(Point2 start, Point2 end) {
    return {std::min(start.x, end.x), std::min(start.y, end.y), std::max(start.x, end.x),
            std::max(start.y, end.y)};
}

// No value: the whole plane.
std::optional<Box> boxOf(const Curve& curve, Extension extension) {
    if (const auto* circle = std::get_if<Circle>(&curve)) {
        return boxOfCircle(circle->center, circle->radius);
    }
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        return boxOfCircle(arc->center, arc->radius);
    }
    if (extension == Extension::extended) {
        return std::nullopt;
    }
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return boxOfEnds(segment->start, segment->end);
    }
    const auto& piece = std::get<PolylineSegment>(curve);
    if (piece.bulge == 0.0) {
        return boxOfEnds(piece.start, piece.end);
    }
    return std::nullopt;
}

bool apart(const Box& a, const Box& b) noexcept {
    return a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY;
}

// --- Points along the first ellipse ----------------------------------------

// The direction of an exact point of the carrying ellipse on its unit circle,
// up to a positive factor: rational + radical * sqrt(radicand).
struct UnitDirection {
    ExactVector rational;
    ExactVector radical;
    Dyadic radicand;
};

UnitDirection unitDirectionOf(const EllipseSupport& support, const ExactPoint& point) {
    const ExactVector fromCenter{point.rational.x - support.center.x * point.weight,
                                 point.rational.y - support.center.y * point.weight};
    return {toUnitCircle(support, fromCenter), toUnitCircle(support, point.radical),
            point.radicand};
}

detail::TurnSector sectorFromStart(const ArcMeaning& arc, const UnitDirection& direction) {
    const auto& start = arc.startDirection;
    return detail::sectorOf(
        detail::oriented(arc.turn,
                         exact::signOf(cross(start, direction.rational),
                                       cross(start, direction.radical), direction.radicand)),
        exact::signOf(dot(start, direction.rational), dot(start, direction.radical),
                      direction.radicand));
}

// The order along the ellipse, from D0 counterclockwise, of two exact points
// of its carrier with the same radicand (the two points of a line).
Sign compareAlongEllipse(const EllipseMeaning& ellipse, const ExactPoint& a, const ExactPoint& b) {
    const auto u = unitDirectionOf(ellipse.support, a);
    const auto v = unitDirectionOf(ellipse.support, b);
    assert(compare(u.radicand, v.radicand) == Sign::zero);
    // (ru + qu sqrt(d)) x (rv + qv sqrt(d)).
    const auto uv =
        exact::signOf(cross(u.rational, v.rational) + cross(u.radical, v.radical) * u.radicand,
                      cross(u.rational, v.radical) + cross(u.radical, v.rational), u.radicand);
    const auto& arc = ellipse.unitArc;
    return detail::compareTurns(sectorFromStart(arc, u), sectorFromStart(arc, v),
                                detail::oriented(arc.turn, uv));
}

// --- Points of the quartic -------------------------------------------------

Dyadic parameterWidth(const RealRoot& root) {
    const auto lower = root.lower().sign();
    const auto upper = root.upper().sign();
    if (lower == Sign::zero || upper == Sign::zero || lower != upper) {
        return kOne.timesPowerOfTwo(kParameterWidthExponent);
    }
    // |s| >= m on the whole interval. frexp gives m' = f 2^e with f in
    // [0.5, 1); m' is m rounded to nearest, so m >= 2^(e - 2) even when the
    // rounding crossed a power of two.
    const double m =
        std::min(std::fabs(root.lower().toDouble()), std::fabs(root.upper().toDouble()));
    int exponent = kLargestParameterExponent;
    if (std::isfinite(m)) {
        static_cast<void>(std::frexp(m, &exponent));
    }
    const auto power = std::clamp(exponent - 2, 0, kLargestParameterExponent);
    return kOne.timesPowerOfTwo(kParameterWidthExponent + 2 * static_cast<std::int64_t>(power));
}

// The point P(s) of a root s, within kRelativePointError * max(M, |p|).
//
// Error. On the ellipse x(s) / weight(s) = center.x + major.x cos t +
// minor.x sin t, with t = 2 atan(s), so its derivative in s is
// (-major.x sin t + minor.x cos t) * 2 / (1 + s^2), at most
// 2 (|major.x| + |minor.x|) / (1 + s^2) <= 4 M / (1 + s^2) (likewise for y).
// The point is built at a dyadic s' with |s' - s| <= width / 2, from the
// interval narrowed to width (parameterWidth): 2^-56 near zero, and
// 2^-56 * m^2 when |s| >= m on the whole interval, so that its error is at
// most 4 M 2^-57 = 2^-55 M. The quotient is then correctly rounded, within
// 2^-53 |p|. The total, at most 2^-52 max(M, |p|), is well within 2^-48.
Point2 roundedRootPoint(const RationalEllipse& form, const RealRoot& root) {
    Dyadic parameter = root.lower();
    if (!root.isExact()) {
        const auto narrow = exact::narrowed(root, parameterWidth(root));
        parameter = narrow.isExact() ? narrow.lower()
                                     : (narrow.lower() + narrow.upper()).timesPowerOfTwo(-1);
    }
    const auto weight = form.weight.evaluate(parameter);
    return {quotientToDouble(form.x.evaluate(parameter), weight),
            quotientToDouble(form.y.evaluate(parameter), weight)};
}

// True when a coordinate of P(s) has magnitude beyond robust::kMaxCoordinate:
// |x / weight| > K exactly when x - K weight > 0 or x + K weight < 0.
bool rootBeyondDomain(const RationalEllipse& form, const RealRoot& root) {
    const Dyadic limit{robust::kMaxCoordinate};
    const auto bound = form.weight * limit;
    const auto beyond = [&root, &bound](const exact::Polynomial& coordinate) {
        return exact::signAt(coordinate - bound, root) == Sign::positive ||
               exact::signAt(coordinate + bound, root) == Sign::negative;
    };
    return beyond(form.x) || beyond(form.y);
}

// --- The three ways --------------------------------------------------------

struct Inputs {
    const EllipseMeaning& first;
    Extension firstExtension;
    Extension secondExtension;
};

bool takesPart(const Inputs& inputs, Location onFirst, Location onSecond) {
    return detail::takesPart(onFirst, inputs.firstExtension) &&
           detail::takesPart(onSecond, inputs.secondExtension);
}

void meetLine(const Inputs& inputs, const LineMeaning& line, EllipseIntersections& result) {
    const auto meeting = detail::meetEllipseLine(inputs.first, line);
    std::vector<const detail::EllipseLinePoint*> kept;
    for (const auto& point : meeting.found()) {
        if (!takesPart(inputs, point.onEllipse, point.onSegment)) {
            continue;
        }
        if (detail::beyondDomain(point.point)) {
            ++result.omittedBeyondDomain;
            continue;
        }
        kept.push_back(&point);
    }
    if (kept.size() == 2 &&
        compareAlongEllipse(inputs.first, kept[0]->point, kept[1]->point) == Sign::positive) {
        std::swap(kept[0], kept[1]);
    }
    for (const auto* point : kept) {
        result.points.push_back(EllipseIntersectionPoint{detail::nearestPoint(point->point),
                                                         point->contact, point->onEllipse,
                                                         point->onSegment});
    }
}

void meetConic(const Inputs& inputs, const RationalEllipse& form, const detail::ConicTrace& trace,
               const ArcMeaning& otherArc, EllipseIntersections& result) {
    for (const auto& point : detail::meetEllipseConic(inputs.first, trace, otherArc)) {
        if (!takesPart(inputs, point.onEllipse, point.onOther)) {
            continue;
        }
        Point2 rounded;
        if (point.parameter) {
            if (rootBeyondDomain(form, *point.parameter)) {
                ++result.omittedBeyondDomain;
                continue;
            }
            rounded = roundedRootPoint(form, *point.parameter);
        } else {
            const auto& support = inputs.first.support;
            const ExactPoint missing{
                {support.center.x - support.major.x, support.center.y - support.major.y},
                {},
                {},
                kOne};
            if (detail::beyondDomain(missing)) {
                ++result.omittedBeyondDomain;
                continue;
            }
            rounded = detail::nearestPoint(missing);
        }
        result.points.push_back(
            EllipseIntersectionPoint{rounded, point.contact, point.onEllipse, point.onOther});
    }
}

// The second input on the same carrier as the first ellipse: its ends as
// directions on the unit circle of the first, the direction it runs in, and
// whether it covers the whole carrier.
struct SameCarrier {
    ExactVector start;
    ExactVector end;
    detail::Turn turn = detail::Turn::counterclockwise;
    bool whole = false;
};

struct SharedPoint {
    ExactVector direction;
};

struct SharedStretch {
    bool whole = false;
    ExactVector from;
    ExactVector to;
};

// Two inputs on one carrier, as shareCircle() in intersection.cpp. Angles
// are measured on the unit circle of the first ellipse from its D0,
// counterclockwise: the first is [0, a] or the whole circle, the second
// [b0, b1] (running through 0 when b0 > b1) or the whole circle. locateOnSecond
// gives the Location of an exact point of the carrier on the second input.
template <typename LocateOnSecond>
void shareCarrier(const Inputs& inputs, const SameCarrier& second,
                  const LocateOnSecond& locateOnSecond, EllipseIntersections& result) {
    const auto& arc = inputs.first.unitArc;
    const bool firstWhole =
        inputs.firstExtension == Extension::extended || arc.span != ArcSpan::partial;
    const bool secondWhole = second.whole || inputs.secondExtension == Extension::extended;
    const auto order = [&arc](const ExactVector& u, const ExactVector& v) {
        return detail::compareAlongArc(arc, u, v);
    };
    std::vector<SharedPoint> points;
    std::vector<SharedStretch> stretches;
    const auto addShared = [&](const ExactVector& from, const ExactVector& to) {
        if (order(from, to) == Sign::zero) {
            points.push_back(SharedPoint{from});
        } else {
            stretches.push_back(SharedStretch{false, from, to});
        }
    };
    const auto& zero = arc.startDirection;
    const auto& a = arc.endDirection;
    const bool sameTurn = second.turn == arc.turn;
    const auto& b0 = sameTurn ? second.start : second.end;
    const auto& b1 = sameTurn ? second.end : second.start;
    if (firstWhole && secondWhole) {
        stretches.push_back(SharedStretch{true, zero, zero});
    } else if (secondWhole) {
        addShared(zero, a);
    } else if (firstWhole) {
        addShared(b0, b1);
    } else {
        // The smaller of b1 and a: the end of the part of [.., b1] within [0, a].
        const auto& end = order(b1, a) == Sign::positive ? a : b1;
        if (order(b0, b1) != Sign::positive) {
            // [b0, b1] does not run through 0.
            if (order(b0, a) != Sign::positive) {
                addShared(b0, end);
            }
        } else {
            // [b0, 2 pi) and [0, b1]: the part from 0, then the part from b0.
            addShared(zero, end);
            if (order(b0, a) != Sign::positive) {
                addShared(b0, a);
            }
        }
    }
    // At most two of each: a simple exact sort along the first ellipse.
    std::sort(points.begin(), points.end(), [&order](const SharedPoint& u, const SharedPoint& v) {
        return order(u.direction, v.direction) == Sign::negative;
    });
    std::sort(stretches.begin(), stretches.end(),
              [&order](const SharedStretch& u, const SharedStretch& v) {
                  return order(u.from, v.from) == Sign::negative;
              });

    const auto& support = inputs.first.support;
    for (const auto& point : points) {
        const auto exactPoint = detail::ellipsePoint(support, point.direction);
        const auto onFirst = detail::locateDirection(arc, point.direction);
        const auto onSecond = locateOnSecond(exactPoint);
        if (!takesPart(inputs, onFirst, onSecond)) {
            continue;
        }
        if (detail::beyondDomain(exactPoint)) {
            ++result.omittedBeyondDomain;
            continue;
        }
        result.points.push_back(EllipseIntersectionPoint{detail::nearestPoint(exactPoint),
                                                         Contact::sameCurve, onFirst, onSecond});
    }
    const auto endOf = [&](const ExactVector& direction) {
        const auto exactPoint = detail::ellipsePoint(support, direction);
        return EllipseOverlapEnd{detail::nearestPoint(exactPoint),
                                 detail::locateDirection(arc, direction),
                                 locateOnSecond(exactPoint)};
    };
    for (const auto& stretch : stretches) {
        result.overlaps.push_back(
            EllipseOverlap{stretch.whole, endOf(stretch.from), endOf(stretch.to)});
    }
}

}  // namespace

Result<EllipseIntersections> intersect(const Ellipse& ellipse, Extension ellipseExtension,
                                       const Curve& curve, Extension curveExtension) {
    const auto first = detail::checkedEllipse(ellipse, 0);
    if (!first.hasValue()) {
        return first.diagnosis();
    }
    const auto second = detail::checkedMeaning(curve, 1);
    if (!second.hasValue()) {
        return second.diagnosis();
    }
    EllipseIntersections result;
    if (const auto box = boxOf(curve, curveExtension); box && apart(boxOfEllipse(ellipse), *box)) {
        return result;
    }
    const Inputs inputs{first.value(), ellipseExtension, curveExtension};
    const auto& meaning = second.value();
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        meetLine(inputs, *line, result);
        return result;
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const auto& support = inputs.first.support;
    const auto form = detail::rationalForm(support);
    const auto trace = detail::traceOfCircle(form, arc.circle);
    if (!trace.equation.isZero()) {
        meetConic(inputs, form, trace, arc, result);
        return result;
    }
    // An ellipse of ratio 1 on the circle: the directions of the arc from the
    // common centre, mapped to the unit circle of the ellipse.
    const SameCarrier same{toUnitCircle(support, arc.startDirection),
                           toUnitCircle(support, arc.endDirection), arc.turn,
                           arc.span != ArcSpan::partial};
    shareCarrier(
        inputs, same,
        [&meaning](const ExactPoint& point) { return detail::locate(meaning, point); }, result);
    return result;
}

Result<EllipseIntersections> intersect(const Ellipse& first, Extension firstExtension,
                                       const Ellipse& second, Extension secondExtension) {
    const auto firstMeaning = detail::checkedEllipse(first, 0);
    if (!firstMeaning.hasValue()) {
        return firstMeaning.diagnosis();
    }
    const auto secondMeaning = detail::checkedEllipse(second, 1);
    if (!secondMeaning.hasValue()) {
        return secondMeaning.diagnosis();
    }
    EllipseIntersections result;
    if (apart(boxOfEllipse(first), boxOfEllipse(second))) {
        return result;
    }
    const Inputs inputs{firstMeaning.value(), firstExtension, secondExtension};
    const auto& other = secondMeaning.value();
    const auto& support = inputs.first.support;
    const auto form = detail::rationalForm(support);
    const auto trace = detail::traceOfEllipse(form, other.support);
    if (!trace.equation.isZero()) {
        meetConic(inputs, form, trace, other.unitArc, result);
        return result;
    }
    // The same ellipse: the vector from the common centre to the point of the
    // other in direction D is major * D.x + minor * D.y.
    const auto& otherSupport = other.support;
    const auto fromCenter = [&otherSupport](const ExactVector& direction) {
        return ExactVector{otherSupport.major.x * direction.x + otherSupport.minor.x * direction.y,
                           otherSupport.major.y * direction.x + otherSupport.minor.y * direction.y};
    };
    const auto& otherArc = other.unitArc;
    const SameCarrier same{toUnitCircle(support, fromCenter(otherArc.startDirection)),
                           toUnitCircle(support, fromCenter(otherArc.endDirection)), otherArc.turn,
                           otherArc.span != ArcSpan::partial};
    shareCarrier(
        inputs, same,
        [&other](const ExactPoint& point) { return detail::locateOnEllipse(other, point); },
        result);
    return result;
}

}  // namespace solid_space::geom2d
