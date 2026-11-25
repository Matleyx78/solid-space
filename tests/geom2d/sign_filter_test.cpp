// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the floating-point filter in front of the exact signs (stage
// 0.3 corrections, package FS2; geom2d/src/exact/interval.hpp). Every filtered
// function is called twice on the same data, with the filter off (the exact
// path) and on, and the two answers must be the same, with no exception:
// - on a few hundred thousand ordinary inputs from a fixed seed, with the
//   generator of the bench (tests/prestazioni/banco.cpp), rational points and
//   points with a radical (meetings of lines and circles, of two circles);
// - on nearly degenerate inputs: points at relative distance 2^-30, 2^-40,
//   2^-52 and 2^-60 from a line, from a circle and from each other, and
//   pieces leaving a point with almost the same tangent;
// - on exact zeros: points on their carriers, the same point written in
//   different ways, the ends of the curves, equal half pieces;
// - at the ends of the domain: coordinates 2^50, 2^-50, 0, subnormals, and
//   points beyond the domain (meetings of almost parallel lines).
// A last test counts how often the filter decides on the ordinary inputs, and
// fails below 90%: a filter that rarely decides does not serve.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/interval.hpp"
#include "local_order.hpp"
#include "positions.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::Location;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::Carrier;
using solid_space::geom2d::detail::CircleSupport;
using solid_space::geom2d::detail::CurveMeaning;
using solid_space::geom2d::detail::Dyadic;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::geom2d::detail::ExactVector;
using solid_space::geom2d::detail::HalfPiece;
using solid_space::geom2d::detail::Heading;
using solid_space::geom2d::detail::LineMeaning;
using solid_space::geom2d::detail::LocalOrder;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;

// The relative distances of the nearly degenerate inputs.
constexpr std::array<int, 4> kNearExponents{30, 40, 52, 60};

// The magnitude of the nearly degenerate data: distances are 2^-k times it.
constexpr double kScale = 16.0;

constexpr double kMaxCoordinate = 0x1p50;
constexpr double kSmallestSubnormal = 0x1p-1074;
constexpr double kSmallestNormal = 0x1p-1022;

// The generator of the bench (tests/prestazioni/banco.cpp): an LCG of 32 bits
// from a fixed seed, with dyadic scaling, so the same data on every system.
class Generator {
public:
    explicit Generator(std::uint32_t seed) : m_state(seed) {}

    std::uint32_t next() {
        m_state = (m_state * 1664525U) + 1013904223U;
        return m_state;
    }

    // A coordinate with 32 bits, in [-32, 32).
    double coordinate() { return (static_cast<double>(next()) * 0x1p-26) - 32.0; }

    // A coordinate multiple of 1/4, in [-32, 32), as in the bench.
    double quarter() { return (static_cast<double>(next() >> 24U) * 0.25) - 32.0; }

    Point2 point() { return {coordinate(), coordinate()}; }

    std::size_t index(std::size_t count) { return next() % count; }

private:
    std::uint32_t m_state;
};

const char* nameOf(Sign sign) {
    switch (sign) {
        case Sign::negative:
            return "negative";
        case Sign::zero:
            return "zero";
        case Sign::positive:
            return "positive";
    }
    return "?";
}

std::string textOf(Sign sign) {
    return nameOf(sign);
}

std::string textOf(bool value) {
    return value ? "true" : "false";
}

std::string textOf(Location location) {
    return std::to_string(static_cast<int>(location));
}

std::string textOf(const std::vector<std::size_t>& values) {
    std::string text;
    for (const auto value : values) {
        text += std::to_string(value) + " ";
    }
    return text;
}

// Calls each function with the filter off and on and counts the answers that
// differ (there must be none) and the calls, per function.
class Checker {
public:
    Checker() = default;
    Checker(const Checker&) = delete;
    Checker& operator=(const Checker&) = delete;
    Checker(Checker&&) = delete;
    Checker& operator=(Checker&&) = delete;

    ~Checker() { exact::enableFilter(true); }

    template <typename Function>
    void same(const std::string& name, const std::string& what, Function function) {
        exact::enableFilter(false);
        const auto exactAnswer = function();
        exact::enableFilter(true);
        const auto filtered = function();
        auto& tally = m_tallies[name];
        ++tally.calls;
        if (!(filtered == exactAnswer)) {
            ++tally.differences;
            if (tally.differences <= 3) {
                ADD_FAILURE() << name << " (" << what << "): filtered " << textOf(filtered)
                              << ", exact " << textOf(exactAnswer);
            }
        }
    }

    void points(const ExactPoint& a, const ExactPoint& b, const std::string& what) {
        same("compareX", what, [&] { return detail::compareX(a, b); });
        same("compareY", what, [&] { return detail::compareY(a, b); });
        same("samePoint", what, [&] { return detail::samePoint(a, b); });
    }

    void point(const ExactPoint& p, const std::string& what) {
        same("beyondDomain", what, [&] { return detail::beyondDomain(p); });
    }

    void carrier(const Carrier& c, const ExactPoint& p, const std::string& what) {
        if (const auto* line = std::get_if<LineMeaning>(&c)) {
            same("lineSign", what, [&] { return detail::lineSign(*line, p); });
        } else {
            same("circleSign", what,
                 [&] { return detail::circleSign(std::get<CircleSupport>(c), p); });
        }
        same("carrierSign", what, [&] { return detail::carrierSign(c, p); });
        same("onCarrier", what, [&] { return detail::onCarrier(c, p); });
    }

    // Points that lie exactly on the carrier of the curve.
    void along(const CurveMeaning& curve, std::span<const ExactPoint> onCurve,
               const std::string& what) {
        for (std::size_t i = 0; i < onCurve.size(); ++i) {
            same("locate", what, [&] { return detail::locate(curve, onCurve[i]); });
            for (std::size_t j = 0; j < onCurve.size(); ++j) {
                same("compareAlong", what,
                     [&] { return detail::compareAlong(curve, onCurve[i], onCurve[j]); });
            }
        }
    }

    // Half pieces that leave the point.
    void around(const ExactPoint& p, std::span<const HalfPiece> pieces, const std::string& what) {
        std::vector<detail::Departure> departures;
        departures.reserve(pieces.size() + 1);
        for (const auto& piece : pieces) {
            departures.push_back(detail::departureOf(p, piece));
        }
        departures.push_back(detail::rayDeparture(p));
        for (const auto& a : departures) {
            for (const auto& b : departures) {
                same("compareDepartures", what, [&] { return detail::compareDepartures(a, b); });
            }
        }
        same("LocalOrder", what, [&] {
            const LocalOrder order(p, pieces);
            std::vector<std::size_t> summary(order.sorted().begin(), order.sorted().end());
            for (std::size_t i = 0; i < order.size(); ++i) {
                summary.push_back(order.groupOf(i));
            }
            for (const auto& departure : departures) {
                const auto placement = order.place(departure);
                summary.push_back(placement.along ? 1 : 0);
                summary.push_back(placement.index);
            }
            return summary;
        });
    }

    // Every function of the list of the plan was checked, with no difference.
    void expectComplete(std::size_t minimumCalls) const {
        for (const char* name : {"compareX", "compareY", "samePoint", "beyondDomain", "lineSign",
                                 "circleSign", "carrierSign", "onCarrier", "locate", "compareAlong",
                                 "compareDepartures", "LocalOrder"}) {
            const auto found = m_tallies.find(name);
            ASSERT_NE(found, m_tallies.end()) << name << " not checked";
            EXPECT_GE(found->second.calls, minimumCalls) << name;
            EXPECT_EQ(found->second.differences, 0U) << name;
        }
    }

    [[nodiscard]] std::size_t totalCalls() const {
        std::size_t total = 0;
        for (const auto& [name, tally] : m_tallies) {
            total += tally.calls;
        }
        return total;
    }

private:
    struct Tally {
        std::size_t calls = 0;
        std::size_t differences = 0;
    };

    std::map<std::string, Tally> m_tallies;
};

std::optional<CurveMeaning> meaningOf(const Curve& curve) {
    if (detail::checkData(curve, 0)) {
        return std::nullopt;
    }
    auto meaning = detail::meaningOf(curve);
    if (meaning.degenerate) {
        return std::nullopt;
    }
    return meaning;
}

Carrier lineThrough(Point2 start, Point2 end) {
    return LineMeaning{start, end};
}

// The carrier of a valid curve that is not degenerate.
Carrier carrierOf(const Curve& curve) {
    EXPECT_FALSE(detail::checkData(curve, 0).has_value());
    return detail::carrierOf(detail::meaningOf(curve));
}

// The points where two carriers meet.
void appendMeeting(const Carrier& a, const Carrier& b, std::vector<ExactPoint>& points) {
    const auto meeting = detail::meet(a, b);
    for (const auto& found : meeting.found()) {
        points.push_back(found.point);
    }
}

// The same point written in another way (a fresh point: its intervals are
// computed again): all the numbers times two, or the radical halved and the
// radicand times four, or the numbers times three.
ExactPoint doubled(const ExactPoint& p) {
    return ExactPoint{ExactVector{p.rational.x.timesPowerOfTwo(1), p.rational.y.timesPowerOfTwo(1)},
                      ExactVector{p.radical.x.timesPowerOfTwo(1), p.radical.y.timesPowerOfTwo(1)},
                      p.radicand, p.weight.timesPowerOfTwo(1)};
}

ExactPoint otherRadicand(const ExactPoint& p) {
    return ExactPoint{p.rational,
                      ExactVector{p.radical.x.timesPowerOfTwo(-1), p.radical.y.timesPowerOfTwo(-1)},
                      p.radicand.timesPowerOfTwo(2), p.weight};
}

ExactPoint tripled(const ExactPoint& p) {
    const Dyadic three = Dyadic::fromInteger(3);
    return ExactPoint{ExactVector{p.rational.x * three, p.rational.y * three},
                      ExactVector{p.radical.x * three, p.radical.y * three}, p.radicand,
                      p.weight * three};
}

// A random curve of the four kinds, valid and not degenerate.
Curve randomCurve(Generator& generator) {
    for (;;) {
        const std::uint32_t kind = generator.next() % 4U;
        Curve curve;
        if (kind == 0) {
            curve = Segment{generator.point(), generator.point()};
        } else if (kind == 1) {
            curve = Circle{generator.point(), 1.0 + std::fabs(generator.coordinate())};
        } else if (kind == 2) {
            const double sweep = generator.coordinate() / 5.5;
            curve = Arc{generator.point(), 1.0 + std::fabs(generator.coordinate()),
                        std::fabs(generator.coordinate()) / 5.5, sweep};
        } else {
            curve = PolylineSegment{generator.point(), generator.point(),
                                    generator.coordinate() / 16.0};
        }
        if (meaningOf(curve)) {
            return curve;
        }
    }
}

// The meaning of a random curve.
CurveMeaning randomMeaning(Generator& generator) {
    return detail::meaningOf(randomCurve(generator));
}

// The ordinary inputs, with the calls of the checker or, for the count of
// the last test, only the filtered calls.
struct OrdinaryData {
    std::vector<CurveMeaning> curves;
    std::vector<Carrier> carriers;
    std::vector<ExactPoint> points;
    // Points on the carrier of curves[i], for the first curves.
    std::vector<std::vector<ExactPoint>> onCurve;
    // Points with half pieces that leave them.
    std::vector<ExactPoint> centres;
    std::vector<std::vector<HalfPiece>> pieces;
};

OrdinaryData ordinaryData() {
    constexpr std::size_t kCurves = 400;
    constexpr std::size_t kRationalPoints = 900;
    constexpr std::size_t kCurvesWithPoints = 120;
    constexpr std::size_t kMeetingsPerCurve = 12;
    constexpr std::size_t kRationalCentres = 150;
    constexpr std::size_t kRadicalCentres = 150;
    constexpr std::size_t kPiecesPerCentre = 10;
    Generator generator(0x51717U);
    OrdinaryData data;
    for (std::size_t i = 0; i < kCurves; ++i) {
        data.curves.push_back(randomMeaning(generator));
        data.carriers.push_back(detail::carrierOf(data.curves.back()));
    }
    for (std::size_t i = 0; i < kRationalPoints; ++i) {
        data.points.push_back(detail::exactPoint(generator.point()));
    }
    // Points with a radical (and some rational ones, meetings of two lines).
    while (data.points.size() < 2 * kRationalPoints) {
        appendMeeting(data.carriers[generator.index(kCurves)],
                      data.carriers[generator.index(kCurves)], data.points);
    }
    data.onCurve.resize(kCurvesWithPoints);
    for (std::size_t i = 0; i < kCurvesWithPoints; ++i) {
        for (std::size_t k = 0; k < kMeetingsPerCurve; ++k) {
            const std::size_t other = generator.index(kCurves);
            if (other != i) {
                appendMeeting(data.carriers[i], data.carriers[other], data.onCurve[i]);
            }
        }
    }
    for (std::size_t i = 0; i < kRationalCentres; ++i) {
        const Point2 centre = generator.point();
        std::vector<HalfPiece> pieces;
        for (std::size_t k = 0; k < kPiecesPerCentre; ++k) {
            const Point2 other = generator.point();
            const Heading heading = (k % 2 == 0) ? Heading::forward : Heading::backward;
            if (k % 3 == 0) {
                const PolylineSegment piece{centre, other, generator.coordinate() / 16.0};
                if (meaningOf(piece)) {
                    pieces.push_back(HalfPiece{carrierOf(piece), heading});
                }
            } else {
                pieces.push_back(HalfPiece{lineThrough(centre, other), heading});
            }
        }
        data.centres.push_back(detail::exactPoint(centre));
        data.pieces.push_back(std::move(pieces));
    }
    while (data.centres.size() < kRationalCentres + kRadicalCentres) {
        const auto& a = data.carriers[generator.index(kCurves)];
        const auto& b = data.carriers[generator.index(kCurves)];
        const auto meeting = detail::meet(a, b);
        for (const auto& found : meeting.found()) {
            data.centres.push_back(found.point);
            data.pieces.push_back({HalfPiece{a, Heading::forward}, HalfPiece{a, Heading::backward},
                                   HalfPiece{b, Heading::forward},
                                   HalfPiece{b, Heading::backward}});
        }
    }
    return data;
}

void checkOrdinary(Checker& checker, const OrdinaryData& data) {
    Generator generator(0x2A2AU);
    const std::size_t pointCount = data.points.size();
    constexpr std::size_t kPairs = 40000;
    for (std::size_t k = 0; k < kPairs; ++k) {
        const std::size_t i = generator.index(pointCount);
        const std::size_t j = generator.index(pointCount);
        if (i != j) {
            checker.points(data.points[i], data.points[j], "ordinary pair");
        }
        checker.carrier(data.carriers[generator.index(data.carriers.size())], data.points[i],
                        "ordinary point and carrier");
    }
    for (const auto& p : data.points) {
        checker.point(p, "ordinary point");
    }
    for (std::size_t i = 0; i < data.onCurve.size(); ++i) {
        checker.along(data.curves[i], data.onCurve[i], "ordinary points on a curve");
    }
    for (std::size_t i = 0; i < data.centres.size(); ++i) {
        checker.around(data.centres[i], data.pieces[i], "ordinary half pieces");
    }
}

TEST(SignFilter, SameAnswersOnOrdinaryData) {
    Checker checker;
    checkOrdinary(checker, ordinaryData());
    checker.expectComplete(100);
    EXPECT_GE(checker.totalCalls(), 200000U);
}

// --- Nearly degenerate inputs ------------------------------------------------

// Points near the line y = 0 and on it, near the diagonal y = x, near the
// circle of centre (R, 0) through the origin, near each other; half pieces
// with almost the same tangent. Near the origin the doubles are dense, so a
// distance of 2^-60 times the scale is a double.
void checkNear(Checker& checker, int exponent) {
    const double delta = std::ldexp(kScale, -exponent);
    const std::string what = "near, 2^-" + std::to_string(exponent);
    Generator generator(0x7E57U + static_cast<std::uint32_t>(exponent));
    const double radius = kScale;

    // A line, and points at distance delta from it: rational ones, and the
    // meetings of circles with the line y = +-delta.
    const Carrier axis = lineThrough({-kScale, 0.0}, {kScale, 0.0});
    const Carrier diagonal = lineThrough({-kScale, -kScale}, {kScale, kScale});
    std::vector<ExactPoint> nearLine;
    std::vector<ExactPoint> nearDiagonal;
    for (int k = 0; k < 40; ++k) {
        const double x = generator.coordinate() / 2.0;
        nearLine.push_back(detail::exactPoint({x, delta}));
        nearLine.push_back(detail::exactPoint({x, -delta}));
        const double small = std::ldexp(generator.coordinate(), -20);
        nearDiagonal.push_back(detail::exactPoint({small, small + delta}));
        nearDiagonal.push_back(detail::exactPoint({small + delta, small}));
        const Carrier circle = carrierOf(Circle{generator.point(), 40.0 + generator.quarter()});
        appendMeeting(lineThrough({-kScale, delta}, {kScale, delta}), circle, nearLine);
        appendMeeting(lineThrough({-kScale, -delta}, {kScale, -delta}), circle, nearLine);
    }
    for (std::size_t i = 0; i < nearLine.size(); ++i) {
        checker.carrier(axis, nearLine[i], what + ", line");
        checker.points(nearLine[i], nearLine[(i + 1) % nearLine.size()], what + ", line");
        checker.point(nearLine[i], what);
    }
    for (std::size_t i = 0; i < nearDiagonal.size(); ++i) {
        checker.carrier(diagonal, nearDiagonal[i], what + ", diagonal");
        checker.points(nearDiagonal[i], nearDiagonal[i ^ 1U], what + ", diagonal");
    }

    // A segment from the origin along y = 0, and points on it near its start:
    // (+-delta, 0) and the meetings of y = 0 with the circle of centre
    // (R, delta) and radius R, at about delta^2 / (2 R) from the origin.
    const Curve segment = Segment{{0.0, 0.0}, {kScale, 0.0}};
    const auto segmentMeaning = detail::meaningOf(segment);
    std::vector<ExactPoint> onSegment{
        detail::exactPoint({delta, 0.0}), detail::exactPoint({-delta, 0.0}),
        detail::exactPoint({0.0, 0.0}), detail::exactPoint({kScale - 0.25, 0.0})};
    appendMeeting(detail::carrierOf(segmentMeaning), carrierOf(Circle{{radius, delta}, radius}),
                  onSegment);
    appendMeeting(detail::carrierOf(segmentMeaning), carrierOf(Circle{{radius, -delta}, radius}),
                  onSegment);
    checker.along(segmentMeaning, onSegment, what + ", segment");

    // The circle of centre (R, 0) through the origin, and points near it:
    // (+-delta, 0), and the meetings of x = delta with the circles of centre
    // (R, +-delta) and radius R.
    const Carrier circle = carrierOf(Circle{{radius, 0.0}, radius});
    std::vector<ExactPoint> nearCircle{detail::exactPoint({delta, 0.0}),
                                       detail::exactPoint({-delta, 0.0})};
    for (int k = 0; k < 20; ++k) {
        const double y = std::ldexp(generator.coordinate(), -12);
        const double x = (y * y) / (2.0 * radius);
        nearCircle.push_back(detail::exactPoint({x + delta, y}));
        nearCircle.push_back(detail::exactPoint({x - delta, y}));
    }
    const Carrier vertical = lineThrough({delta, -1.0}, {delta, 1.0});
    appendMeeting(vertical, carrierOf(Circle{{radius, delta}, radius}), nearCircle);
    appendMeeting(vertical, carrierOf(Circle{{radius, -delta}, radius}), nearCircle);
    for (std::size_t i = 0; i < nearCircle.size(); ++i) {
        checker.carrier(circle, nearCircle[i], what + ", circle");
        checker.points(nearCircle[i], nearCircle[(i + 1) % nearCircle.size()], what + ", circle");
    }

    // A half circle that starts at the origin (bulge 1 from (0, 0) to
    // (2R, 0): centre (R, 0)), and points on it near its start: the meetings of
    // x = delta and x = 2 delta with its circle.
    const auto halfCircle =
        detail::meaningOf(PolylineSegment{{0.0, 0.0}, {2.0 * radius, 0.0}, 1.0});
    std::vector<ExactPoint> onArc{detail::startPoint(halfCircle), detail::endPoint(halfCircle)};
    appendMeeting(vertical, detail::carrierOf(halfCircle), onArc);
    appendMeeting(lineThrough({2.0 * delta, -1.0}, {2.0 * delta, 1.0}),
                  detail::carrierOf(halfCircle), onArc);
    checker.along(halfCircle, onArc, what + ", arc");
    const auto arc = detail::meaningOf(Arc{{radius, 0.0}, radius, std::numbers::pi, -1.0});
    std::vector<ExactPoint> onWholeArc;
    appendMeeting(vertical, detail::carrierOf(arc), onWholeArc);
    appendMeeting(lineThrough({-1.0, delta}, {1.0, delta}), detail::carrierOf(arc), onWholeArc);
    checker.along(arc, onWholeArc, what + ", arc of an Arc");

    // Points near each other: rational, and meetings of x = e and
    // x = e + delta with the same circle.
    for (int k = 0; k < 20; ++k) {
        const double x = std::ldexp(generator.coordinate(), -12);
        const double y = std::ldexp(generator.coordinate(), -12);
        checker.points(detail::exactPoint({x, y}), detail::exactPoint({x + delta, y}), what);
        checker.points(detail::exactPoint({x, y}), detail::exactPoint({x, y - delta}), what);
        const Carrier near = carrierOf(Circle{generator.point(), 40.0});
        std::vector<ExactPoint> meetings;
        appendMeeting(lineThrough({x, -1.0}, {x, 1.0}), near, meetings);
        appendMeeting(lineThrough({x + delta, -1.0}, {x + delta, 1.0}), near, meetings);
        for (std::size_t i = 0; i < meetings.size(); ++i) {
            for (std::size_t j = 0; j < meetings.size(); ++j) {
                checker.points(meetings[i], meetings[j], what + ", radical");
            }
        }
    }

    // Half pieces leaving the origin with almost the same tangent: lines to
    // (R, 0), (R, +-delta), (-R, delta); circles tangent to y = 0 there and
    // almost tangent (bulge 1 to (0, 2R) and to (+-delta, 2R)).
    const Point2 origin{0.0, 0.0};
    std::vector<HalfPiece> pieces;
    for (const Point2 to : {Point2{radius, 0.0}, Point2{radius, delta}, Point2{radius, -delta},
                            Point2{-radius, delta}, Point2{delta, radius}}) {
        pieces.push_back(HalfPiece{lineThrough(origin, to), Heading::forward});
        pieces.push_back(HalfPiece{lineThrough(origin, to), Heading::backward});
    }
    for (const Point2 to : {Point2{0.0, 2.0 * radius}, Point2{delta, 2.0 * radius},
                            Point2{-delta, 2.0 * radius}, Point2{0.0, -2.0 * radius}}) {
        pieces.push_back(HalfPiece{carrierOf(PolylineSegment{origin, to, 1.0}), Heading::forward});
        pieces.push_back(HalfPiece{carrierOf(PolylineSegment{origin, to, 1.0}), Heading::backward});
    }
    checker.around(detail::exactPoint(origin), pieces, what + ", half pieces");

    // Two circles that almost touch, and the half pieces of both at their
    // meetings.
    const Carrier first = carrierOf(Circle{{0.0, 0.0}, radius});
    const Carrier second = carrierOf(Circle{{2.0 * radius - delta, 0.0}, radius});
    const auto meeting = detail::meet(first, second);
    for (const auto& found : meeting.found()) {
        const std::vector<HalfPiece> touching{
            HalfPiece{first, Heading::forward}, HalfPiece{first, Heading::backward},
            HalfPiece{second, Heading::forward}, HalfPiece{second, Heading::backward}};
        checker.around(found.point, touching, what + ", almost touching circles");
        checker.carrier(first, found.point, what + ", almost touching circles");
    }
}

TEST(SignFilter, SameAnswersOnNearlyDegenerateData) {
    Checker checker;
    for (const int exponent : kNearExponents) {
        checkNear(checker, exponent);
    }
    checker.expectComplete(4);
}

// --- Exact zeros ---------------------------------------------------------------

TEST(SignFilter, SameAnswersOnExactZeros) {
    Checker checker;
    Generator generator(0x2E80U);
    std::vector<CurveMeaning> curves;
    std::vector<Carrier> carriers;
    for (int i = 0; i < 60; ++i) {
        curves.push_back(randomMeaning(generator));
        carriers.push_back(detail::carrierOf(curves.back()));
    }
    for (std::size_t i = 0; i < carriers.size(); ++i) {
        const std::size_t j = (i + 1) % carriers.size();
        const auto meeting = detail::meet(carriers[i], carriers[j]);
        for (const auto& found : meeting.found()) {
            const ExactPoint& p = found.point;
            // On both carriers.
            checker.carrier(carriers[i], p, "meeting on its carrier");
            checker.carrier(carriers[j], p, "meeting on its carrier");
            // The same point in other ways.
            for (const auto& other : {doubled(p), otherRadicand(p), tripled(p)}) {
                checker.points(p, other, "same point");
                checker.carrier(carriers[i], other, "same point on its carrier");
                checker.point(other, "same point");
                const std::vector<ExactPoint> pair{p, other};
                checker.along(curves[i], pair, "same point along a curve");
            }
            // The half pieces of both carriers, each twice.
            const std::vector<HalfPiece> pieces{HalfPiece{carriers[i], Heading::forward},
                                                HalfPiece{carriers[i], Heading::forward},
                                                HalfPiece{carriers[j], Heading::backward},
                                                HalfPiece{carriers[j], Heading::backward},
                                                HalfPiece{carriers[i], Heading::backward}};
            checker.around(p, pieces, "equal half pieces");
        }
        // The ends of the curve: atStart and atEnd, and equal positions.
        const std::vector<ExactPoint> ends{detail::startPoint(curves[i]),
                                           detail::endPoint(curves[i]),
                                           detail::startPoint(curves[i])};
        checker.along(curves[i], ends, "ends of a curve");
        for (const auto& end : ends) {
            checker.carrier(carriers[i], end, "end on its carrier");
        }
    }
    // A rational point and the meeting of two lines through it.
    for (int k = 0; k < 40; ++k) {
        const Point2 p = generator.point();
        const Carrier a = lineThrough(p, generator.point());
        const Carrier b = lineThrough(generator.point(), p);
        std::vector<ExactPoint> meeting;
        appendMeeting(a, b, meeting);
        for (const auto& q : meeting) {
            checker.points(detail::exactPoint(p), q, "meeting of lines through a point");
            checker.carrier(a, detail::exactPoint(p), "point on its line");
        }
    }
    checker.expectComplete(10);
}

// --- The ends of the domain -----------------------------------------------------

// A rational point (x / weight, y / weight), with its intervals still to be
// computed.
ExactPoint rationalPoint(const Dyadic& x, const Dyadic& y, const Dyadic& weight) {
    return ExactPoint{ExactVector{x, y}, ExactVector{}, Dyadic(), weight};
}

// Points whose intervals have a centre that fits the doubles while the
// interval does not (a review of FS2, round 1): the exact coordinate, or a
// value of its interval, beyond the domain or nonzero below the smallest
// normal double. None of them may have intervals.
std::vector<ExactPoint> pointsAtTheEdgeOfTheDoubles() {
    const Dyadic one(1.0);
    const Dyadic three = Dyadic::fromInteger(3);
    const Dyadic limit(kMaxCoordinate);
    // Just beyond and just within the domain: K + 2^-4 and K - 2^-5 both
    // round to K (the doubles next to K are K - 2^-3 and K + 2^-2), so the
    // centre is K and the radius positive.
    const Dyadic justBeyond = limit + one.timesPowerOfTwo(-4);
    const Dyadic justWithin = limit - one.timesPowerOfTwo(-5);
    // The smallest normal double, and twice it, written with weight 3: the
    // centre is normal, the radius (at least 2^-1020 / 3) reaches below the
    // smallest normal double and past zero.
    const Dyadic normal = Dyadic(kSmallestNormal) * three;
    const Dyadic twiceNormal = Dyadic(2.0 * kSmallestNormal) * three;
    // A normal number just above the smallest normal double, beyond the
    // precision of a double: centre the smallest normal double.
    const Dyadic aboveNormal = Dyadic(kSmallestNormal) + one.timesPowerOfTwo(-1080);
    return {rationalPoint(justBeyond, Dyadic(1.0), one),
            rationalPoint(-justBeyond, Dyadic(1.0), one),
            rationalPoint(Dyadic(1.0), justBeyond, one),
            rationalPoint(justWithin, Dyadic(2.0), one),
            rationalPoint(Dyadic(2.0), -justWithin, one),
            rationalPoint(justBeyond * three, Dyadic(3.0), three),
            rationalPoint(normal, Dyadic(3.0), three),
            rationalPoint(Dyadic(3.0), -normal, three),
            rationalPoint(twiceNormal, twiceNormal, three),
            rationalPoint(aboveNormal, Dyadic(1.0), one),
            rationalPoint(Dyadic(1.0), -aboveNormal, one)};
}

TEST(SignFilter, NoIntervalsWhereTheIntervalMayLeaveTheDoubles) {
    for (const auto& p : pointsAtTheEdgeOfTheDoubles()) {
        const auto rounded = detail::roundedPoint(p);
        EXPECT_FALSE(detail::approximationOf(p).has_value())
            << "(" << rounded.x << ", " << rounded.y << ")";
    }
    // The beyond ones are beyond, the others are not, with the filter on and
    // off (the filter never answers true).
    const auto points = pointsAtTheEdgeOfTheDoubles();
    for (const bool enabled : {false, true}) {
        exact::enableFilter(enabled);
        EXPECT_TRUE(detail::beyondDomain(points[0]));
        EXPECT_TRUE(detail::beyondDomain(points[1]));
        EXPECT_TRUE(detail::beyondDomain(points[2]));
        EXPECT_FALSE(detail::beyondDomain(points[3]));
        EXPECT_FALSE(detail::beyondDomain(points[4]));
        EXPECT_TRUE(detail::beyondDomain(points[5]));
        for (std::size_t k = 6; k < points.size(); ++k) {
            EXPECT_FALSE(detail::beyondDomain(points[k])) << k;
        }
    }
    exact::enableFilter(true);

    // Exact values that fit keep their intervals: the doubles at the ends of
    // the domain, and a meeting with a coordinate exactly zero.
    for (const double value :
         {0.0, kSmallestNormal, -kSmallestNormal, kMaxCoordinate, -kMaxCoordinate}) {
        EXPECT_TRUE(detail::approximationOf(detail::exactPoint({value, 1.0})).has_value()) << value;
    }
    EXPECT_FALSE(
        detail::approximationOf(detail::exactPoint({kSmallestNormal / 2.0, 1.0})).has_value());
    const auto meeting =
        detail::meet(lineThrough({-1.0, -3.0}, {1.0, 5.0}), lineThrough({0.0, 1.0}, {3.0, 2.0}));
    ASSERT_EQ(meeting.count, 1U);
    const auto crossing = detail::roundedPoint(meeting.points[0].point);
    EXPECT_EQ(crossing.x, 0.0);
    EXPECT_EQ(crossing.y, 1.0);
    EXPECT_TRUE(detail::approximationOf(meeting.points[0].point).has_value());
}

TEST(SignFilter, SameAnswersAtTheEndsOfTheDomain) {
    Checker checker;
    const std::vector<double> values{0.0,
                                     kSmallestSubnormal,
                                     -kSmallestSubnormal,
                                     0x1p-1060,
                                     kSmallestNormal / 2.0,
                                     kSmallestNormal,
                                     0x1p-50,
                                     -0x1p-50,
                                     1.0,
                                     0x1p49,
                                     kMaxCoordinate,
                                     -kMaxCoordinate,
                                     kMaxCoordinate - 0.5};
    std::vector<ExactPoint> points;
    std::vector<Point2> doubles;
    for (const double x : values) {
        for (const double y : values) {
            doubles.push_back({x, y});
            points.push_back(detail::exactPoint({x, y}));
        }
    }
    // Points beyond the domain: meetings of almost parallel lines, the line
    // y = 0 and lines that drop by 2^-60 or 2^-40 over 2^40 from 2^-30.
    const Carrier axis = lineThrough({0.0, 0.0}, {1.0, 0.0});
    for (const double drop : {0x1p-60, 0x1p-40, -0x1p-60}) {
        appendMeeting(axis, lineThrough({0.0, 0x1p-30}, {0x1p40, 0x1p-30 - drop}), points);
    }
    // Meetings of tiny and huge circles with lines.
    appendMeeting(carrierOf(Circle{{0.0, 0.0}, 0x1p-50}), axis, points);
    appendMeeting(carrierOf(Circle{{0x1p-50, 0x1p-50}, 0x1p-50}),
                  lineThrough({0.0, 0.0}, {0x1p-50, 0x1p-49}), points);
    appendMeeting(carrierOf(Circle{{0.0, 0.0}, 0x1p49}), lineThrough({0.0, 0.0}, {1.0, 1.0}),
                  points);
    appendMeeting(carrierOf(Circle{{0.0, 0.0}, kSmallestNormal}), axis, points);
    // Points whose centres fit the doubles while their intervals do not.
    for (const auto& p : pointsAtTheEdgeOfTheDoubles()) {
        points.push_back(p);
    }

    const std::vector<Carrier> carriers{
        axis,
        lineThrough({-kMaxCoordinate, -kMaxCoordinate}, {kMaxCoordinate, kMaxCoordinate}),
        lineThrough({0.0, 0.0}, {kSmallestSubnormal, kSmallestSubnormal}),
        lineThrough({0x1p-50, 0.0}, {0.0, 0x1p-50}),
        carrierOf(Circle{{0.0, 0.0}, 0x1p-50}),
        carrierOf(Circle{{0.0, 0.0}, 0x1p49}),
        carrierOf(Circle{{kSmallestSubnormal, 0.0}, kSmallestNormal}),
        carrierOf(PolylineSegment{{0.0, 0.0}, {0x1p-50, 0.0}, 1.0})};
    for (const auto& p : points) {
        checker.point(p, "end of the domain");
        for (const auto& q : points) {
            checker.points(p, q, "ends of the domain");
        }
        for (const auto& c : carriers) {
            checker.carrier(c, p, "end of the domain and a carrier");
        }
    }

    // Curves through the ends of the domain and points on them.
    for (const Curve& curve :
         {Curve{Segment{{-kMaxCoordinate, -kMaxCoordinate}, {kMaxCoordinate, kMaxCoordinate}}},
          Curve{Segment{{0.0, 0.0}, {kSmallestSubnormal, 0.0}}},
          Curve{Segment{{0x1p-50, 0x1p-50}, {-0x1p-50, 0.0}}}, Curve{Circle{{0.0, 0.0}, 0x1p-50}},
          Curve{Arc{{0.0, 0.0}, 0x1p-50, 0.5, 2.0}}, Curve{Arc{{0.0, 0.0}, 0x1p49, 0.5, -2.0}},
          Curve{PolylineSegment{{0.0, 0.0}, {0x1p-50, 0x1p-50}, -0.5}}}) {
        const auto meaning = meaningOf(curve);
        ASSERT_TRUE(meaning.has_value());
        const Carrier carrier = detail::carrierOf(*meaning);
        std::vector<ExactPoint> on{detail::startPoint(*meaning), detail::endPoint(*meaning)};
        for (const auto& other : carriers) {
            appendMeeting(carrier, other, on);
        }
        appendMeeting(carrier, lineThrough({0.0, 0.0}, {1.0, 0.0}), on);
        appendMeeting(carrier, lineThrough({0.0, 0.0}, {0.0, 1.0}), on);
        checker.along(*meaning, on, "curve at the end of the domain");
    }

    // Half pieces leaving points at the ends of the domain.
    for (const Point2 centre :
         {Point2{0.0, 0.0}, Point2{kSmallestSubnormal, 0.0},
          Point2{kMaxCoordinate, -kMaxCoordinate}, Point2{0x1p-50, 0x1p-50}}) {
        std::vector<HalfPiece> pieces;
        for (const Point2 to : doubles) {
            if (to.x != centre.x || to.y != centre.y) {
                pieces.push_back(HalfPiece{lineThrough(centre, to), Heading::forward});
            }
            if (pieces.size() >= 24) {
                break;
            }
        }
        const PolylineSegment bulge{centre, {0.0, 0x1p-50}, 0.75};
        if (meaningOf(bulge)) {
            pieces.push_back(HalfPiece{carrierOf(bulge), Heading::backward});
        }
        checker.around(detail::exactPoint(centre), pieces, "half pieces at the end of the domain");
    }
    checker.expectComplete(1);
}

// --- How often the filter decides ---------------------------------------------

// The share of filtered calls that the intervals decide, on the ordinary data
// of the first test, per function; at least 90%.
TEST(SignFilter, DecidesMostOrdinaryData) {
    const auto data = ordinaryData();
    exact::enableFilter(true);
    std::map<std::string, exact::FilterCounts> counts;
    const auto count = [&](const std::string& name, const auto& calls) {
        exact::startFilterCount();
        calls();
        counts[name] = exact::stopFilterCount();
    };
    Generator generator(0x2A2AU);
    const std::size_t pointCount = data.points.size();
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    std::vector<std::size_t> carrierIndex;
    for (std::size_t k = 0; k < 40000; ++k) {
        const std::size_t i = generator.index(pointCount);
        const std::size_t j = generator.index(pointCount);
        if (i != j) {
            pairs.emplace_back(i, j);
        }
        carrierIndex.push_back(generator.index(data.carriers.size()));
    }
    std::size_t sink = 0;
    const auto keep = [&sink](auto value) {
        sink += static_cast<std::size_t>(value);
    };
    count("compareX", [&] {
        for (const auto& [i, j] : pairs) {
            keep(static_cast<int>(detail::compareX(data.points[i], data.points[j])) + 1);
        }
    });
    count("compareY", [&] {
        for (const auto& [i, j] : pairs) {
            keep(static_cast<int>(detail::compareY(data.points[i], data.points[j])) + 1);
        }
    });
    count("samePoint", [&] {
        for (const auto& [i, j] : pairs) {
            keep(detail::samePoint(data.points[i], data.points[j]));
        }
    });
    count("beyondDomain", [&] {
        for (const auto& p : data.points) {
            keep(detail::beyondDomain(p));
        }
    });
    count("carrierSign", [&] {
        for (std::size_t k = 0; k < carrierIndex.size(); ++k) {
            keep(static_cast<int>(detail::carrierSign(data.carriers[carrierIndex[k]],
                                                      data.points[k % pointCount])) +
                 1);
        }
    });
    count("locate", [&] {
        for (std::size_t i = 0; i < data.onCurve.size(); ++i) {
            for (const auto& p : data.onCurve[i]) {
                keep(static_cast<int>(detail::locate(data.curves[i], p)));
            }
        }
    });
    count("compareAlong", [&] {
        for (std::size_t i = 0; i < data.onCurve.size(); ++i) {
            for (const auto& a : data.onCurve[i]) {
                for (const auto& b : data.onCurve[i]) {
                    if (&a != &b) {
                        keep(static_cast<int>(detail::compareAlong(data.curves[i], a, b)) + 1);
                    }
                }
            }
        }
    });
    // The departures before counting: departureOf() asserts onCarrier() in
    // debug builds, an exact zero that the count must not see.
    std::vector<std::vector<detail::Departure>> allDepartures;
    for (std::size_t i = 0; i < data.centres.size(); ++i) {
        auto& departures = allDepartures.emplace_back();
        for (const auto& piece : data.pieces[i]) {
            departures.push_back(detail::departureOf(data.centres[i], piece));
        }
    }
    count("compareDepartures", [&] {
        for (const auto& departures : allDepartures) {
            for (std::size_t a = 0; a < departures.size(); ++a) {
                for (std::size_t b = 0; b < departures.size(); ++b) {
                    // The two headings of one carrier are never equal; the
                    // same half piece always is, and only the exact path
                    // says zero.
                    if (a != b) {
                        keep(static_cast<int>(
                                 detail::compareDepartures(departures[a], departures[b])) +
                             1);
                    }
                }
            }
        }
    });
    EXPECT_GT(sink, 0U);
    for (const auto& [name, tally] : counts) {
        const auto calls = tally.decided + tally.undecided;
        ASSERT_GT(calls, 0U) << name;
        const double share = static_cast<double>(tally.decided) / static_cast<double>(calls);
        EXPECT_GE(share, 0.9) << name << ": " << tally.decided << " of " << calls;
    }
}

}  // namespace
