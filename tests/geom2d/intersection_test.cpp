// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of intersect (stage 0.2, package P10): the properties of ADR
// 0010, "Verifica", on data generated in a fixed way, also almost degenerate
// (points shared by both curves, tangencies and near tangencies of one ulp,
// curves on the same line or circle), and a few configurations worked out by
// hand in the comments. The cases of [0.2-R10a] and [0.2-R10b] are in
// tests/corpus/.

#include "solid_space/geom2d/intersection.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/side.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <variant>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Contact;
using solid_space::geom2d::Curve;
using solid_space::geom2d::CurvePosition;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::Extension;
using solid_space::geom2d::intersect;
using solid_space::geom2d::Location;
using solid_space::geom2d::OverlapEnd;
using solid_space::geom2d::PairIntersections;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::Side;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr std::array<Extension, 2> kExtensions{Extension::none, Extension::extended};

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool sameBits(Point2 a, Point2 b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool sameBits(const CurvePosition& a, const CurvePosition& b) {
    return a.location == b.location && sameBits(a.station, b.station);
}

double next(double value) {
    return std::nextafter(value, kInfinity);
}

double previous(double value) {
    return std::nextafter(value, -kInfinity);
}

class Sequence {
public:
    explicit Sequence(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t bits() {
        m_state += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }

    // A small integer in [-limit, limit]: grids give many exact meetings.
    double integer(int limit) {
        const auto span = (2 * static_cast<std::uint64_t>(limit)) + 1;
        return static_cast<double>(static_cast<int>(bits() % span) - limit);
    }

    // A value of the grid, sometimes moved by one ulp.
    double nearInteger(int limit) {
        const double value = integer(limit);
        switch (bits() % 4U) {
            case 0:
                return next(value);
            case 1:
                return previous(value);
            default:
                return value;
        }
    }

    bool chance(unsigned outOf) { return bits() % outOf == 0; }

private:
    std::uint64_t m_state;
};

// A curve of any type with data near a small grid, never degenerate.
Curve curveFrom(Sequence& sequence) {
    for (;;) {
        const Point2 a{sequence.nearInteger(4), sequence.nearInteger(4)};
        const Point2 b{sequence.nearInteger(4), sequence.nearInteger(4)};
        const double radius = std::fabs(sequence.nearInteger(4));
        Curve curve;
        switch (sequence.bits() % 5U) {
            case 0:
                curve = Segment{a, b};
                break;
            case 1:
                curve = Circle{a, radius};
                break;
            case 2:
                curve = Arc{a, radius, sequence.integer(6) * 0.5, sequence.integer(14) * 0.5};
                break;
            case 3:
                curve = PolylineSegment{a, b, sequence.integer(4) * 0.5};
                break;
            default:
                curve = PolylineSegment{a, b, 0.0};
                break;
        }
        if (!detail::check(curve, 0)) {
            return curve;
        }
    }
}

// --- Property checks -----------------------------------------------------------

void expectSymmetric(const PairIntersections& ab, const PairIntersections& ba,
                     const std::string& where) {
    ASSERT_EQ(ab.points.size(), ba.points.size()) << where;
    ASSERT_EQ(ab.overlaps.size(), ba.overlaps.size()) << where;
    EXPECT_EQ(ab.omittedBeyondDomain, ba.omittedBeyondDomain) << where;
    // Two exact points may round to the same double: a point matches when one
    // point of the other result has its bits, contact and positions.
    for (const auto& p : ab.points) {
        bool matched = false;
        for (const auto& q : ba.points) {
            matched =
                matched || (sameBits(p.point, q.point) && p.contact == q.contact &&
                            sameBits(p.onFirst, q.onSecond) && sameBits(p.onSecond, q.onFirst));
        }
        EXPECT_TRUE(matched) << where;
    }
    const auto sameEnd = [](const OverlapEnd& e, const OverlapEnd& f) {
        return sameBits(e.point, f.point) && sameBits(e.onFirst, f.onSecond) &&
               sameBits(e.onSecond, f.onFirst);
    };
    for (const auto& o : ab.overlaps) {
        bool matched = false;
        for (const auto& r : ba.overlaps) {
            if (o.whole || r.whole) {
                matched = matched || (o.whole && r.whole);
                continue;
            }
            matched = matched || (sameEnd(o.from, r.from) && sameEnd(o.to, r.to)) ||
                      (sameEnd(o.from, r.to) && sameEnd(o.to, r.from));
        }
        EXPECT_TRUE(matched) << where;
    }
}

void expectSorted(const PairIntersections& result, const std::string& where) {
    for (std::size_t i = 1; i < result.points.size(); ++i) {
        EXPECT_LE(result.points[i - 1].onFirst.station, result.points[i].onFirst.station) << where;
    }
    for (std::size_t i = 1; i < result.overlaps.size(); ++i) {
        EXPECT_LE(result.overlaps[i - 1].from.onFirst.station,
                  result.overlaps[i].from.onFirst.station)
            << where;
    }
    // The ends of the stretches are not repeated among the points.
    for (const auto& overlap : result.overlaps) {
        for (const auto& point : result.points) {
            EXPECT_FALSE(sameBits(point.point, overlap.from.point)) << where;
            EXPECT_FALSE(sameBits(point.point, overlap.to.point)) << where;
        }
    }
}

bool onPart(const CurvePosition& position, Extension extension) {
    return detail::takesPart(position.location, extension);
}

// When the exact point of an intersection is a double, the rounded point is
// that point, and side() must find it on both curves: the same exact
// comparison decides both (carriers.hpp). The exact points are found again
// here from the carriers, so that a point is tested only when it is a double.
void expectOnBothWhenExact(const Curve& first, const Curve& second, const PairIntersections& result,
                           const std::string& where) {
    const auto a = detail::meaningOf(first);
    const auto b = detail::meaningOf(second);
    const auto meeting = detail::meet(detail::carrierOf(a), detail::carrierOf(b));
    for (const auto& point : result.points) {
        const auto exact = detail::exactPoint(point.point);
        bool isExact = false;
        for (const auto& found : meeting.found()) {
            isExact = isExact || detail::samePoint(found.point, exact);
        }
        if (meeting.sameCarrier) {
            isExact = detail::onCarrier(detail::carrierOf(a), exact);
        }
        if (!isExact) {
            continue;
        }
        EXPECT_EQ(solid_space::geom2d::side(first, point.point).value(), Side::on) << where;
        EXPECT_EQ(solid_space::geom2d::side(second, point.point).value(), Side::on) << where;
    }
}

Point2 startOf(const Curve& curve) {
    return solid_space::geom2d::endpoints(curve).value().start;
}

Point2 endOf(const Curve& curve) {
    return solid_space::geom2d::endpoints(curve).value().end;
}

bool opposite(Side a, Side b) {
    return (a == Side::left && b == Side::right) || (a == Side::right && b == Side::left);
}

// No contradiction with side(): two segments crossing inside both have the
// ends of the second strictly on opposite sides of the first; at a tangency
// the ends of the second are never on opposite sides of the first.
void expectNoContradiction(const Curve& first, const Curve& second, const PairIntersections& result,
                           const std::string& where) {
    const bool straight =
        std::holds_alternative<Segment>(first) && std::holds_alternative<Segment>(second);
    // The ends of an Arc and of a Circle are rounded: only the others are exact.
    const bool exactEnds =
        !std::holds_alternative<Arc>(second) && !std::holds_alternative<Circle>(second);
    for (const auto& point : result.points) {
        const auto start = solid_space::geom2d::side(first, startOf(second)).value();
        const auto end = solid_space::geom2d::side(first, endOf(second)).value();
        if (straight && point.contact == Contact::crossing &&
            point.onFirst.location == Location::inside &&
            point.onSecond.location == Location::inside) {
            EXPECT_TRUE(opposite(start, end)) << where;
        }
        if (exactEnds && point.contact == Contact::touching) {
            EXPECT_FALSE(opposite(start, end)) << where;
        }
    }
}

// The curves are named apart from the parameters of intersect, which the
// swapped call crosses on purpose.
void checkPair(const Curve& left, Extension leftPart, const Curve& right, Extension rightPart,
               const std::string& where) {
    const auto ab = intersect(left, leftPart, right, rightPart);
    const auto ba = intersect(right, rightPart, left, leftPart);
    ASSERT_TRUE(ab.hasValue()) << where;
    ASSERT_TRUE(ba.hasValue()) << where;
    expectSymmetric(ab.value(), ba.value(), where);
    expectSorted(ab.value(), where);
    expectSorted(ba.value(), where);
    for (const auto& point : ab.value().points) {
        EXPECT_TRUE(onPart(point.onFirst, leftPart)) << where;
        EXPECT_TRUE(onPart(point.onSecond, rightPart)) << where;
    }
    expectOnBothWhenExact(left, right, ab.value(), where);
    expectNoContradiction(left, right, ab.value(), where);
}

TEST(Intersect, PropertiesOnGeneratedPairs) {
    Sequence sequence(10);
    for (int i = 0; i < 3000; ++i) {
        const auto first = curveFrom(sequence);
        const auto second = curveFrom(sequence);
        for (const auto e1 : kExtensions) {
            for (const auto e2 : kExtensions) {
                checkPair(first, e1, second, e2, "pair " + std::to_string(i));
            }
        }
    }
}

// The end of a polyline segment of the circle of radius 25 about the origin
// that starts on an axis with the bulge t: the start turned by 4 atan(t), whose
// cosine and sine (c^2 - s^2, 2 s c with c = (1 - t^2) / (1 + t^2) and
// s = 2 t / (1 + t^2)) are multiples of 1/25 for the bulges used here, so the
// end has integer coordinates and the segment lies exactly on that circle.
Point2 turnedBy(Point2 start, double bulge) {
    const double t2 = bulge * bulge;
    const double c = (1.0 - t2) / (1.0 + t2);
    const double s = 2.0 * bulge / (1.0 + t2);
    const double cosine = (c * c) - (s * s);
    const double sine = 2.0 * s * c;
    return Point2{std::round((start.x * cosine) - (start.y * sine)),
                  std::round((start.x * sine) + (start.y * cosine))};
}

TEST(Intersect, PropertiesOnTheSameCarrier) {
    // Pieces of one line and of one circle: overlaps, shared ends, wraps.
    constexpr std::array<Point2, 4> kAxes{Point2{25.0, 0.0}, Point2{0.0, 25.0}, Point2{-25.0, 0.0},
                                          Point2{0.0, -25.0}};
    constexpr std::array<double, 10> kBulges{0.5,  1.0,  2.0,  1.0 / 3.0,  3.0,
                                             -0.5, -1.0, -2.0, -1.0 / 3.0, -3.0};
    Sequence sequence(11);
    for (int i = 0; i < 600; ++i) {
        const double a = sequence.integer(8);
        const double b = sequence.integer(8);
        const double c = sequence.integer(8);
        const double d = sequence.integer(8);
        std::vector<Curve> curves;
        if (a != b) {
            curves.emplace_back(Segment{{a, 2.0 * a}, {b, 2.0 * b}});
        }
        if (c != d) {
            curves.emplace_back(PolylineSegment{{c, 2.0 * c}, {d, 2.0 * d}, 0.0});
        }
        for (int k = 0; k < 2; ++k) {
            const auto start = kAxes.at(sequence.bits() % kAxes.size());
            const double bulge = kBulges.at(sequence.bits() % kBulges.size());
            const auto end = turnedBy(start, bulge);
            if (sequence.chance(2)) {
                curves.emplace_back(PolylineSegment{start, end, bulge});
            } else {
                // The same arc backwards.
                curves.emplace_back(PolylineSegment{end, start, -bulge});
            }
        }
        // Arcs of the same circle given by angles, and the circle itself.
        curves.emplace_back(Arc{{0.0, 0.0},
                                25.0,
                                a * std::numbers::pi / 4.0,
                                c * std::numbers::pi / 4.0 + (sequence.chance(2) ? 0.0 : 0.5)});
        curves.emplace_back(Circle{{0.0, 0.0}, 25.0});
        for (std::size_t x = 0; x < curves.size(); ++x) {
            for (std::size_t y = 0; y < curves.size(); ++y) {
                if (detail::check(curves[x], 0) || detail::check(curves[y], 0)) {
                    continue;
                }
                for (const auto e1 : kExtensions) {
                    for (const auto e2 : kExtensions) {
                        checkPair(curves[x], e1, curves[y], e2,
                                  "same carrier " + std::to_string(i));
                    }
                }
            }
        }
    }
}

// --- Configurations by hand ---------------------------------------------------

PairIntersections run(const Curve& first, const Curve& second, Extension e1 = Extension::none,
                      Extension e2 = Extension::none) {
    const auto result = intersect(first, e1, second, e2);
    EXPECT_TRUE(result.hasValue());
    return result.hasValue() ? result.value() : PairIntersections{};
}

TEST(Intersect, CollinearSegments) {
    const Segment a{{0.0, 0.0}, {4.0, 0.0}};
    // Overlap [2, 4], in the direction of the first.
    auto result = run(a, Segment{{6.0, 0.0}, {2.0, 0.0}});
    ASSERT_EQ(result.overlaps.size(), 1U);
    EXPECT_TRUE(result.points.empty());
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{2.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].to.point, Point2{4.0, 0.0}));
    EXPECT_EQ(result.overlaps[0].to.onFirst.location, Location::atEnd);
    EXPECT_EQ(result.overlaps[0].from.onSecond.location, Location::atEnd);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.onSecond.station, 4.0));
    // Following on: one point, sameCurve.
    result = run(a, Segment{{4.0, 0.0}, {9.0, 0.0}});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_TRUE(result.overlaps.empty());
    EXPECT_EQ(result.points[0].contact, Contact::sameCurve);
    EXPECT_EQ(result.points[0].onFirst.location, Location::atEnd);
    EXPECT_EQ(result.points[0].onSecond.location, Location::atStart);
    // Apart, and apart by one ulp.
    EXPECT_TRUE(run(a, Segment{{next(4.0), 0.0}, {9.0, 0.0}}).points.empty());
    // Both extended: the whole line, from the start of the first.
    result = run(a, Segment{{9.0, 0.0}, {7.0, 0.0}}, Extension::extended, Extension::extended);
    ASSERT_EQ(result.overlaps.size(), 1U);
    EXPECT_TRUE(result.overlaps[0].whole);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{0.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].from.onFirst.station, 0.0));
    EXPECT_TRUE(sameBits(result.overlaps[0].from.onSecond.station, 9.0));
    EXPECT_EQ(result.overlaps[0].from.onSecond.location, Location::afterEnd);
}

TEST(Intersect, ArcsOnOneCircle) {
    // The unit circle: half circles as polyline segments, exact ends.
    const PolylineSegment upper{{1.0, 0.0}, {-1.0, 0.0}, 1.0};  // through (0, 1)
    const PolylineSegment lower{{-1.0, 0.0}, {1.0, 0.0}, 1.0};  // through (0, -1)
    // Upper and lower meet at both ends only.
    auto result = run(upper, lower);
    ASSERT_EQ(result.points.size(), 2U);
    EXPECT_TRUE(result.overlaps.empty());
    EXPECT_TRUE(sameBits(result.points[0].point, Point2{1.0, 0.0}));
    EXPECT_TRUE(sameBits(result.points[1].point, Point2{-1.0, 0.0}));
    EXPECT_EQ(result.points[0].contact, Contact::sameCurve);
    EXPECT_EQ(result.points[0].onFirst.location, Location::atStart);
    EXPECT_EQ(result.points[0].onSecond.location, Location::atEnd);
    // Upper against the lower extended: the whole upper half is shared.
    result = run(upper, lower, Extension::none, Extension::extended);
    ASSERT_EQ(result.overlaps.size(), 1U);
    EXPECT_FALSE(result.overlaps[0].whole);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{1.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].to.point, Point2{-1.0, 0.0}));
    EXPECT_EQ(result.overlaps[0].from.onSecond.location, Location::atEnd);
    // On the circle of radius 25: the first from (25, 0) counterclockwise by
    // 4 atan(2) to (-7, -24), the second from (-25, 0) by the same turn to
    // (7, 24). They share [(25, 0), (7, 24)] and [(-25, 0), (-7, -24)], in
    // the order of the first.
    const PolylineSegment a{{25.0, 0.0}, {-7.0, -24.0}, 2.0};
    const PolylineSegment b{{-25.0, 0.0}, {7.0, 24.0}, 2.0};
    result = run(a, b);
    EXPECT_TRUE(result.points.empty());
    ASSERT_EQ(result.overlaps.size(), 2U);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{25.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].to.point, Point2{7.0, 24.0}));
    EXPECT_TRUE(sameBits(result.overlaps[1].from.point, Point2{-25.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[1].to.point, Point2{-7.0, -24.0}));
    EXPECT_EQ(result.overlaps[0].from.onFirst.location, Location::atStart);
    EXPECT_EQ(result.overlaps[0].to.onSecond.location, Location::atEnd);
    EXPECT_EQ(result.overlaps[1].from.onSecond.location, Location::atStart);
    EXPECT_EQ(result.overlaps[1].to.onFirst.location, Location::atEnd);
    // Two equal circles: whole, from the start of the first.
    result = run(Circle{{0.0, 0.0}, 1.0}, Arc{{0.0, 0.0}, 1.0, 1.0, -7.0});
    ASSERT_EQ(result.overlaps.size(), 1U);
    EXPECT_TRUE(result.overlaps[0].whole);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{1.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].to.point, Point2{1.0, 0.0}));
}

TEST(Intersect, TangenciesAndNearTangencies) {
    // The unit circle and y = 1: touching at (0, 1). One ulp lower: two
    // crossings; one ulp higher: nothing.
    const Circle circle{{0.0, 0.0}, 1.0};
    auto result = run(circle, Segment{{-2.0, 1.0}, {2.0, 1.0}});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].contact, Contact::touching);
    EXPECT_TRUE(sameBits(result.points[0].point, Point2{0.0, 1.0}));
    result = run(circle, Segment{{-2.0, previous(1.0)}, {2.0, previous(1.0)}});
    ASSERT_EQ(result.points.size(), 2U);
    EXPECT_EQ(result.points[0].contact, Contact::crossing);
    // Along the circle from (1, 0): the right crossing first.
    EXPECT_GT(result.points[0].point.x, 0.0);
    EXPECT_LT(result.points[1].point.x, 0.0);
    result = run(circle, Segment{{-2.0, next(1.0)}, {2.0, next(1.0)}});
    EXPECT_TRUE(result.points.empty());
    // Two circles touching outside at (1, 0), and inside at (1, 0).
    result = run(circle, Circle{{2.0, 0.0}, 1.0});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].contact, Contact::touching);
    EXPECT_TRUE(sameBits(result.points[0].point, Point2{1.0, 0.0}));
    EXPECT_TRUE(sameBits(result.points[0].onFirst.station, 0.0));
    EXPECT_NEAR(result.points[0].onSecond.station, std::numbers::pi, 1e-14);
    result = run(circle, Circle{{0.5, 0.0}, 0.5});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].contact, Contact::touching);
}

TEST(Intersect, ExtensionsAndLocations) {
    // y = 0 from (0, 0) to (1, 0) against x = 3 from (3, 1) to (3, 2): the
    // crossing (3, 0) is past the end of both.
    const Segment a{{0.0, 0.0}, {1.0, 0.0}};
    const Segment b{{3.0, 1.0}, {3.0, 2.0}};
    EXPECT_TRUE(run(a, b).points.empty());
    EXPECT_TRUE(run(a, b, Extension::extended, Extension::none).points.empty());
    auto result = run(a, b, Extension::extended, Extension::extended);
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].onFirst.location, Location::afterEnd);
    EXPECT_EQ(result.points[0].onSecond.location, Location::beforeStart);
    EXPECT_TRUE(sameBits(result.points[0].onFirst.station, 3.0));
    EXPECT_TRUE(sameBits(result.points[0].onSecond.station, -1.0));
    // An arc and its circle: a quarter from (1, 0) to (0, 1) against x = -0.5
    // only meets it when extended, past its end.
    const PolylineSegment quarter{{1.0, 0.0}, {0.0, 1.0}, std::numbers::sqrt2 - 1.0};
    const Segment vertical{{-0.5, -2.0}, {-0.5, 2.0}};
    EXPECT_TRUE(run(quarter, vertical).points.empty());
    result = run(quarter, vertical, Extension::extended, Extension::none);
    ASSERT_EQ(result.points.size(), 2U);
    for (const auto& point : result.points) {
        EXPECT_EQ(point.onFirst.location, Location::afterEnd);
    }
    EXPECT_LT(result.points[0].onFirst.station, result.points[1].onFirst.station);
}

TEST(Intersect, OmittedBeyondTheDomain) {
    // y = 0 and y = 1 - 2^-52 x meet at x = 2^52, beyond 2^50.
    const Segment a{{0.0, 0.0}, {1.0, 0.0}};
    const Segment b{{0.0, 1.0}, {1.0, previous(1.0)}};
    auto result = run(a, b, Extension::extended, Extension::extended);
    EXPECT_TRUE(result.points.empty());
    EXPECT_EQ(result.omittedBeyondDomain, 1U);
    // A circle reaching 2^51 against the x axis: (2^51, 0) is beyond, (0, 0)
    // is not.
    result = run(Circle{{0x1p50, 0.0}, 0x1p50}, a, Extension::none, Extension::extended);
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_TRUE(sameBits(result.points[0].point, Point2{0.0, 0.0}));
    EXPECT_EQ(result.omittedBeyondDomain, 1U);
    // The ends of a shared stretch are listed even beyond 2^50: two equal
    // circles reaching 2^51 share the whole circle from (2^51, 0).
    result = run(Circle{{0x1p50, 0.0}, 0x1p50}, Arc{{0x1p50, 0.0}, 0x1p50, 0.0, 7.0});
    ASSERT_EQ(result.overlaps.size(), 1U);
    EXPECT_TRUE(result.overlaps[0].whole);
    EXPECT_TRUE(sameBits(result.overlaps[0].from.point, Point2{0x1p51, 0.0}));
    EXPECT_EQ(result.omittedBeyondDomain, 0U);
}

TEST(Intersect, FilterKeepsTouchingBoxes) {
    // Boxes that only touch must not be rejected.
    auto result = run(Circle{{0.0, 0.0}, 1.0}, Circle{{2.0, 0.0}, 1.0});
    EXPECT_EQ(result.points.size(), 1U);
    result = run(Segment{{0.0, 0.0}, {1.0, 1.0}}, Segment{{1.0, 1.0}, {2.0, 0.0}});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].onFirst.location, Location::atEnd);
    // A half circle (bulge 1) touching a line at the top of its arc.
    result = run(PolylineSegment{{-1.0, 0.0}, {1.0, 0.0}, -1.0}, Segment{{-1.0, 1.0}, {1.0, 1.0}});
    ASSERT_EQ(result.points.size(), 1U);
    EXPECT_EQ(result.points[0].contact, Contact::touching);
    // Far apart: nothing.
    EXPECT_TRUE(run(Circle{{0.0, 0.0}, 1.0}, Segment{{5.0, 5.0}, {6.0, 7.0}}).points.empty());
}

TEST(Intersect, Diagnoses) {
    const Segment good{{0.0, 0.0}, {1.0, 0.0}};
    const Segment point{{1.0, 1.0}, {1.0, 1.0}};
    const Circle negative{{0.0, 0.0}, -1.0};
    auto result = intersect(good, Extension::none, point, Extension::none);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::degenerateCurve);
    EXPECT_EQ(result.diagnosis().input, 1U);
    result = intersect(negative, Extension::none, good, Extension::none);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::invalidValue);
    EXPECT_EQ(result.diagnosis().input, 0U);
    result = intersect(good, Extension::none, Segment{{0.0, 0.0}, {0x1p51, 0.0}}, Extension::none);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 1U);
}

}  // namespace
