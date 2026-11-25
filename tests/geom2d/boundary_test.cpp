// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// detectBoundary (boundary.hpp): the properties of ADR 0014 ("Verifica") on
// drawings generated deterministically, and the conversion of the arcs of
// step 1 (boundary_rule.hpp). The reference cases are in tests/corpus/.

#include "solid_space/geom2d/boundary.hpp"

#include "boundary_rule.hpp"
#include "curve_meaning.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using robust::TolerancePolicy;

constexpr double kPi = std::numbers::pi;

// A small deterministic generator (a linear congruential one).
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    // A double in [0, 1) with 30 random bits.
    double uniform() {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(m_state >> 34U) * 0x1p-30;
    }

private:
    std::uint64_t m_state;
};

double magnitudeOf(std::span<const Curve> curves) {
    double largest = 0.0;
    for (const Curve& curve : curves) {
        largest = std::max(largest, magnitude(curve).value());
    }
    return largest;
}

double largestCoordinate(Point2 point) {
    return std::max(std::fabs(point.x), std::fabs(point.y));
}

// The carrying circle of a circular input curve, or nothing for a straight one.
struct TestCircle {
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
};

std::optional<TestCircle> circleOf(const Curve& curve) {
    if (const auto* circle = std::get_if<Circle>(&curve)) {
        return TestCircle{circle->center.x, circle->center.y, circle->radius};
    }
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        return TestCircle{arc->center.x, arc->center.y, arc->radius};
    }
    const auto* piece = std::get_if<PolylineSegment>(&curve);
    if (piece == nullptr || piece->bulge == 0.0) {
        return std::nullopt;
    }
    // Centre = mid + (b^2 - 1) / (4 b) (dy, -dx) (curve_meaning.hpp).
    const long double dx = static_cast<long double>(piece->end.x) - piece->start.x;
    const long double dy = static_cast<long double>(piece->end.y) - piece->start.y;
    const long double b = piece->bulge;
    const long double k = (b * b - 1) / (4 * b);
    const long double cx = (static_cast<long double>(piece->start.x) + piece->end.x) / 2 + k * dy;
    const long double cy = (static_cast<long double>(piece->start.y) + piece->end.y) / 2 - k * dx;
    const long double r = std::hypot(cx - piece->start.x, cy - piece->start.y);
    return TestCircle{static_cast<double>(cx), static_cast<double>(cy), static_cast<double>(r)};
}

// The distance from a point to the carrying line or circle of an input curve.
double distanceToCarrier(const Curve& curve, Point2 point) {
    if (const auto circle = circleOf(curve)) {
        const long double distance = std::hypot(static_cast<long double>(point.x) - circle->x,
                                                static_cast<long double>(point.y) - circle->y);
        return static_cast<double>(std::fabs(distance - circle->radius));
    }
    Point2 a;
    Point2 b;
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        a = segment->start;
        b = segment->end;
    } else {
        const auto& piece = std::get<PolylineSegment>(curve);
        a = piece.start;
        b = piece.end;
    }
    const long double dx = static_cast<long double>(b.x) - a.x;
    const long double dy = static_cast<long double>(b.y) - a.y;
    const long double cross = dx * (static_cast<long double>(point.y) - a.y) -
                              dy * (static_cast<long double>(point.x) - a.x);
    return static_cast<double>(std::fabs(cross) / std::hypot(dx, dy));
}

// The point m of a polyline segment (curves.hpp) and the bulge of its halves.
Point2 middleOf(Point2 from, Point2 to, double bulge) {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    return {(from.x + to.x) / 2 + bulge / 2 * dy, (from.y + to.y) / 2 - bulge / 2 * dx};
}

double halfBulge(double bulge) {
    return bulge / (1 + std::sqrt(1 + bulge * bulge));
}

// Three points of the arc from `from` to `to` with the given bulge: its
// middle and the middles of its halves.
std::vector<Point2> samplesOf(Point2 from, Point2 to, double bulge) {
    const Point2 middle = middleOf(from, to, bulge);
    const double half = halfBulge(bulge);
    return {middleOf(from, middle, half), middle, middleOf(middle, to, half)};
}

// Checks the errors of a returned loop (region.hpp, boundary.hpp): every
// vertex within kRelativePointError of the curves of its pieces, every
// curved piece within kRelativeLoopError of its curve.
void expectWithinErrors(const RegionLoop& loop, std::span<const Curve> curves, double scale,
                        const std::string& where) {
    const auto* polyline = std::get_if<Polyline>(&loop.loop);
    if (polyline == nullptr) {
        return;
    }
    // The test computes in long double or double: a few units of 2^-53 more.
    constexpr double kTestRounding = 0x1p-50;
    const std::size_t count = polyline->vertices.size();
    ASSERT_EQ(loop.origins.size(), count) << where;
    for (std::size_t k = 0; k < count; ++k) {
        const LoopPieceOrigin& origin = loop.origins[k];
        if (origin.source == LoopPieceSource::link) {
            continue;
        }
        const Curve& curve = curves[origin.curve];
        const Point2 from = polyline->vertices[k].position;
        const Point2 to = polyline->vertices[(k + 1) % count].position;
        const double bulge = polyline->vertices[k].bulge;
        // A converted Arc gives pieces within kRelativeLoopError / 2 of it.
        const double pointError = std::holds_alternative<Arc>(curve)
                                      ? kRelativeLoopError
                                      : kRelativePointError + kTestRounding;
        for (const Point2 vertex : {from, to}) {
            EXPECT_LE(distanceToCarrier(curve, vertex),
                      pointError * std::max(scale, largestCoordinate(vertex)))
                << where << ", vertex of piece " << k;
        }
        if (bulge != 0.0) {
            ASSERT_TRUE(circleOf(curve).has_value()) << where << ", piece " << k;
            for (const Point2 sample : samplesOf(from, to, bulge)) {
                EXPECT_LE(distanceToCarrier(curve, sample),
                          (kRelativeLoopError + kTestRounding) *
                              std::max(scale, largestCoordinate(sample)))
                    << where << ", piece " << k;
            }
        } else {
            EXPECT_FALSE(circleOf(curve).has_value()) << where << ", piece " << k;
        }
    }
}

// --- Drawings ----------------------------------------------------------------------

void addRectangle(std::vector<Curve>& curves, double x0, double y0, double x1, double y1) {
    curves.emplace_back(Segment{{x0, y0}, {x1, y0}});
    curves.emplace_back(Segment{{x1, y0}, {x1, y1}});
    curves.emplace_back(Segment{{x1, y1}, {x0, y1}});
    curves.emplace_back(Segment{{x0, y1}, {x0, y0}});
}

// A rectangle with its corners rounded by quarter arcs: the ends of the arcs
// are rounded, so the rule links them to the sides.
void addRoundedRectangle(std::vector<Curve>& curves, double x0, double y0, double x1, double y1,
                         double radius) {
    curves.emplace_back(Segment{{x0 + radius, y0}, {x1 - radius, y0}});
    curves.emplace_back(Arc{{x1 - radius, y0 + radius}, radius, -kQuarterTurn, kQuarterTurn});
    curves.emplace_back(Segment{{x1, y0 + radius}, {x1, y1 - radius}});
    curves.emplace_back(Arc{{x1 - radius, y1 - radius}, radius, 0.0, kQuarterTurn});
    curves.emplace_back(Segment{{x1 - radius, y1}, {x0 + radius, y1}});
    curves.emplace_back(Arc{{x0 + radius, y1 - radius}, radius, kQuarterTurn, kQuarterTurn});
    curves.emplace_back(Segment{{x0, y1 - radius}, {x0, y0 + radius}});
    curves.emplace_back(Arc{{x0 + radius, y0 + radius}, radius, kPi, kQuarterTurn});
}

// A drawing with islands that do not touch: an outer rectangle, a grid of
// cells with a circle, a rectangle with a circle inside, a rounded rectangle,
// or a polygon of polyline pieces with a bulge. Coordinates are offset and
// scaled, with sizes that are not dyadic.
struct Drawing {
    std::vector<Curve> curves;
    Point2 picked;
    double unitInMeters = 1.0;
};

Drawing generatedDrawing(std::uint64_t seed, double offset, double scale) {
    Generator random(seed);
    Drawing drawing;
    drawing.unitInMeters = scale > 100 ? 1000.0 : 0.001;
    const auto at = [&](double value) {
        return offset + scale * value;
    };
    addRectangle(drawing.curves, at(0), at(0), at(30.1), at(30.3));
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const double x = 3 + 9 * column + random.uniform();
            const double y = 3 + 9 * row + random.uniform();
            const double size = 5.1 + random.uniform();
            switch (static_cast<int>(random.uniform() * 4)) {
                case 0:
                    drawing.curves.emplace_back(
                        Circle{{at(x + size / 2), at(y + size / 2)}, scale * size / 2});
                    break;
                case 1:
                    addRectangle(drawing.curves, at(x), at(y), at(x + size), at(y + size));
                    drawing.curves.emplace_back(
                        Circle{{at(x + size / 2), at(y + size / 2)}, scale * size / 4});
                    break;
                case 2:
                    addRoundedRectangle(drawing.curves, at(x), at(y), at(x + size), at(y + size),
                                        scale * 0.7);
                    break;
                default: {
                    const double bulge = 0.2 + random.uniform() / 2;
                    const std::vector<Point2> corners{{at(x), at(y)},
                                                      {at(x + size), at(y)},
                                                      {at(x + size), at(y + size)},
                                                      {at(x), at(y + size)}};
                    for (std::size_t k = 0; k < corners.size(); ++k) {
                        drawing.curves.emplace_back(
                            PolylineSegment{corners[k], corners[(k + 1) % corners.size()],
                                            k % 2 == 0 ? bulge : 0.0});
                    }
                    break;
                }
            }
        }
    }
    drawing.picked = {at(1.3), at(1.7)};
    return drawing;
}

// The loops of two results describe the same drawing: same kinds, depths and
// counts, vertices within the errors, origins related by `inputOf`.
template <typename Map>
void expectSameLoops(const DetectedBoundary& expected, const DetectedBoundary& actual,
                     double tolerance, Map inputOf, const std::string& where) {
    ASSERT_EQ(actual.outcome, expected.outcome) << where;
    ASSERT_EQ(actual.loops.size(), expected.loops.size()) << where;
    for (std::size_t l = 0; l < expected.loops.size(); ++l) {
        const RegionLoop& a = expected.loops[l];
        const RegionLoop& b = actual.loops[l];
        EXPECT_EQ(b.depth, a.depth) << where << ", loop " << l;
        ASSERT_EQ(b.loop.index(), a.loop.index()) << where << ", loop " << l;
        ASSERT_EQ(b.origins.size(), a.origins.size()) << where << ", loop " << l;
        for (std::size_t k = 0; k < a.origins.size(); ++k) {
            const std::size_t first = inputOf(a.origins[k].curve);
            const std::size_t second = inputOf(a.origins[k].otherCurve);
            EXPECT_EQ(b.origins[k].source, a.origins[k].source) << where;
            EXPECT_EQ(b.origins[k].curve, std::min(first, second)) << where;
            EXPECT_EQ(b.origins[k].otherCurve, std::max(first, second)) << where;
            EXPECT_EQ(b.origins[k].reversed, a.origins[k].reversed) << where;
        }
        if (const auto* circle = std::get_if<Circle>(&a.loop)) {
            const auto& other = std::get<Circle>(b.loop);
            EXPECT_EQ(other.center.x, circle->center.x) << where;
            EXPECT_EQ(other.center.y, circle->center.y) << where;
            EXPECT_EQ(other.radius, circle->radius) << where;
            continue;
        }
        const auto& first = std::get<Polyline>(a.loop);
        const auto& second = std::get<Polyline>(b.loop);
        ASSERT_EQ(second.vertices.size(), first.vertices.size()) << where;
        for (std::size_t k = 0; k < first.vertices.size(); ++k) {
            EXPECT_NEAR(second.vertices[k].position.x, first.vertices[k].position.x, tolerance)
                << where;
            EXPECT_NEAR(second.vertices[k].position.y, first.vertices[k].position.y, tolerance)
                << where;
            EXPECT_NEAR(second.vertices[k].bulge, first.vertices[k].bulge, 0x1p-40) << where;
        }
    }
}

struct Scale {
    double offset;
    double scale;
};

constexpr std::array<Scale, 2> kScales{{{0.0, 1.0}, {1e5, 1000.0}}};

TEST(DetectBoundary, GeneratedDrawingsKeepTheirErrorsAndDepths) {
    for (const Scale& scale : kScales) {
        for (std::uint64_t seed = 1; seed <= 6; ++seed) {
            const Drawing drawing = generatedDrawing(seed, scale.offset, scale.scale);
            const auto policy = TolerancePolicy::forUnit(drawing.unitInMeters).value();
            const std::string where =
                "seed " + std::to_string(seed) + ", scale " + std::to_string(scale.scale);
            const auto result = detectBoundary(drawing.picked, drawing.curves, policy);
            ASSERT_TRUE(result.hasValue()) << where;
            const DetectedBoundary& boundary = result.value();
            ASSERT_EQ(boundary.outcome, BoundaryOutcome::enclosed) << where;
            EXPECT_TRUE(boundary.skipped.empty()) << where;
            // The contour and one island for each cell, at least.
            ASSERT_GE(boundary.loops.size(), 10U) << where;
            EXPECT_EQ(boundary.loops.front().depth, 0U) << where;

            const double magnitudeM = magnitudeOf(drawing.curves);
            std::vector<Loop> loops;
            for (const RegionLoop& loop : boundary.loops) {
                expectWithinErrors(loop, drawing.curves, magnitudeM, where);
                loops.push_back(loop.loop);
            }
            // The loops do not touch: nestContours finds the same depths.
            const ContourNesting nesting = nestContours(loops);
            EXPECT_TRUE(nesting.conflicts.empty()) << where;
            for (std::size_t l = 0; l < loops.size(); ++l) {
                EXPECT_EQ(nesting.depth[l], boundary.loops[l].depth) << where << ", loop " << l;
            }
            // The order: by depth, the contour first.
            for (std::size_t l = 1; l < loops.size(); ++l) {
                EXPECT_LE(boundary.loops[l - 1].depth, boundary.loops[l].depth) << where;
            }
        }
    }
}

TEST(DetectBoundary, PermutingTheCurvesPermutesTheOrigins) {
    for (const Scale& scale : kScales) {
        for (std::uint64_t seed = 11; seed <= 14; ++seed) {
            const Drawing drawing = generatedDrawing(seed, scale.offset, scale.scale);
            const auto policy = TolerancePolicy::forUnit(drawing.unitInMeters).value();
            const std::string where = "seed " + std::to_string(seed);
            const auto expected = detectBoundary(drawing.picked, drawing.curves, policy);
            ASSERT_TRUE(expected.hasValue()) << where;

            // A permutation: curve i goes to position place[i].
            const std::size_t count = drawing.curves.size();
            std::vector<std::size_t> place(count);
            std::iota(place.begin(), place.end(), std::size_t{0});
            Generator random(seed);
            for (std::size_t i = count; i > 1; --i) {
                const auto j = static_cast<std::size_t>(random.uniform() * static_cast<double>(i));
                std::swap(place[i - 1], place[j]);
            }
            std::vector<Curve> permuted(count);
            for (std::size_t i = 0; i < count; ++i) {
                permuted[place[i]] = drawing.curves[i];
            }
            const auto actual = detectBoundary(drawing.picked, permuted, policy);
            ASSERT_TRUE(actual.hasValue()) << where;
            const double tolerance =
                2 * kRelativePointError * (std::fabs(scale.offset) + 31 * scale.scale);
            expectSameLoops(
                expected.value(), actual.value(), tolerance,
                [&](std::size_t input) { return place[input]; }, where);
        }
    }
}

TEST(DetectBoundary, CurvesOutsideTheContourChangeNothing) {
    for (const Scale& scale : kScales) {
        for (std::uint64_t seed = 21; seed <= 24; ++seed) {
            Drawing drawing = generatedDrawing(seed, scale.offset, scale.scale);
            const auto policy = TolerancePolicy::forUnit(drawing.unitInMeters).value();
            const std::string where = "seed " + std::to_string(seed);
            const auto expected = detectBoundary(drawing.picked, drawing.curves, policy);
            ASSERT_TRUE(expected.hasValue()) << where;
            // Curves beyond the outer rectangle, touching none of the others,
            // and an open one with free ends.
            const auto at = [&](double value) {
                return scale.offset + scale.scale * value;
            };
            drawing.curves.emplace_back(Circle{{at(40), at(15)}, scale.scale * 3});
            addRectangle(drawing.curves, at(-20), at(-20), at(-10), at(-5));
            drawing.curves.emplace_back(Segment{{at(-3), at(35)}, {at(33), at(37)}});
            drawing.curves.emplace_back(Arc{{at(15), at(-10)}, scale.scale * 4, 0.3, 2.0});
            const auto actual = detectBoundary(drawing.picked, drawing.curves, policy);
            ASSERT_TRUE(actual.hasValue()) << where;
            expectSameLoops(
                expected.value(), actual.value(), 0.0, [](std::size_t input) { return input; },
                where);
        }
    }
}

// --- Conversion of the arcs (step 1) ---------------------------------------------

TEST(DetectBoundary, ConvertedArcsStayWithinHalfTheLoopError) {
    Generator random(7);
    for (int k = 0; k < 400; ++k) {
        const double scale = k % 2 == 0 ? 1.0 : 1e5;
        const Arc arc{{scale * (random.uniform() - 0.5), scale * (random.uniform() - 0.5)},
                      scale * (0.001 + random.uniform()),
                      7 * (random.uniform() - 0.5),
                      (random.uniform() - 0.5) * 2 * 6.2};
        const auto meaning = detail::meaningOf(arc);
        const auto& arcMeaning = std::get<detail::ArcMeaning>(meaning.support);
        if (meaning.degenerate || arcMeaning.span != detail::ArcSpan::partial) {
            continue;
        }
        const auto pieces = detail::convertedArc(arc, arcMeaning);
        const bool aboveHalf = arcMeaning.endSector > detail::TurnSector::half;
        ASSERT_EQ(pieces.size(), aboveHalf ? 2U : 1U) << "arc " << k;
        const Endpoints ends = endpoints(Curve{arc}).value();
        EXPECT_EQ(pieces.front().start.x, ends.start.x);
        EXPECT_EQ(pieces.front().start.y, ends.start.y);
        EXPECT_EQ(pieces.back().end.x, ends.end.x);
        EXPECT_EQ(pieces.back().end.y, ends.end.y);
        const double magnitudeM = magnitude(Curve{arc}).value();
        for (const PolylineSegment& piece : pieces) {
            // Each piece turns by at most half a turn, in the direction of the arc.
            EXPECT_LE(std::fabs(piece.bulge), 1 + 0x1p-40) << "arc " << k;
            EXPECT_EQ(piece.bulge > 0, arc.sweep > 0) << "arc " << k;
            std::vector<Point2> points = samplesOf(piece.start, piece.end, piece.bulge);
            points.push_back(piece.start);
            points.push_back(piece.end);
            for (const Point2 point : points) {
                EXPECT_LE(distanceToCarrier(Curve{arc}, point),
                          (kRelativeLoopError / 2) * std::max(magnitudeM, largestCoordinate(point)))
                    << "arc " << k;
            }
        }
    }
}

// --- Single drawings ---------------------------------------------------------------

TEST(DetectBoundary, StretchAboveHalfATurnStartsAtItsMiddle) {
    // The circle cut by x = 6: the face of the point is the larger part, an
    // arc from (6, 8) to (6, -8) through (-10, 0), split there, and that
    // middle is the smallest vertex.
    const std::vector<Curve> curves{Circle{{0, 0}, 10}, Segment{{6, -20}, {6, 20}}};
    const auto policy = TolerancePolicy::forUnit(1e-3).value();
    const auto result = detectBoundary({-5, 1}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    const DetectedBoundary& boundary = result.value();
    ASSERT_EQ(boundary.outcome, BoundaryOutcome::enclosed);
    ASSERT_EQ(boundary.loops.size(), 1U);
    const auto& loop = std::get<Polyline>(boundary.loops[0].loop);
    ASSERT_EQ(loop.vertices.size(), 3U);
    EXPECT_TRUE(loop.closed);
    EXPECT_NEAR(loop.vertices[0].position.x, -10, 10 * kRelativePointError);
    EXPECT_NEAR(loop.vertices[0].position.y, 0, 10 * kRelativePointError);
    EXPECT_EQ(loop.vertices[1].position.x, 6);
    EXPECT_EQ(loop.vertices[1].position.y, -8);
    EXPECT_EQ(loop.vertices[2].position.x, 6);
    EXPECT_EQ(loop.vertices[2].position.y, 8);
    // Two halves of equal angle, pi - atan(8 / 6) each.
    const double half = kPi - std::atan2(8.0, 6.0);
    EXPECT_NEAR(loop.vertices[0].bulge, std::tan(half / 4), 0x1p-44);
    EXPECT_EQ(loop.vertices[1].bulge, 0.0);
    EXPECT_NEAR(loop.vertices[2].bulge, std::tan(half / 4), 0x1p-44);
    const auto& origins = boundary.loops[0].origins;
    ASSERT_EQ(origins.size(), 3U);
    EXPECT_EQ(origins[0].curve, 0U);
    EXPECT_FALSE(origins[0].reversed);
    EXPECT_EQ(origins[1].curve, 1U);
    EXPECT_FALSE(origins[1].reversed);
    EXPECT_EQ(origins[2].curve, 0U);
    expectWithinErrors(boundary.loops[0], curves, 20, "split stretch");
}

TEST(DetectBoundary, ClockwiseWholeArcIsAReversedCircle) {
    const std::vector<Curve> curves{Arc{{1, 2}, 5, 0.25, -kFullTurn}};
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    const auto result = detectBoundary({1, 2}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    const DetectedBoundary& boundary = result.value();
    ASSERT_EQ(boundary.outcome, BoundaryOutcome::enclosed);
    ASSERT_EQ(boundary.loops.size(), 1U);
    const auto& circle = std::get<Circle>(boundary.loops[0].loop);
    EXPECT_EQ(circle.center.x, 1);
    EXPECT_EQ(circle.center.y, 2);
    EXPECT_EQ(circle.radius, 5);
    ASSERT_EQ(boundary.loops[0].origins.size(), 1U);
    EXPECT_EQ(boundary.loops[0].origins[0].source, LoopPieceSource::curve);
    EXPECT_TRUE(boundary.loops[0].origins[0].reversed);
}

TEST(DetectBoundary, PieceExtendedAtBothEndsCoversItsCircle) {
    // A curved piece of almost a whole turn, with a gap of 2^-29 around
    // (1, 0), wider than its tolerance (1e-9 m). The x axis crosses the gap
    // and reaches both ends within its own tolerance (2^-42 * 1e5): both
    // extensions stop at the same point, and the piece covers its circle.
    const double gap = 0x1p-30;
    const double bulge = 2 / gap;
    const std::vector<Curve> curves{PolylineSegment{{1, gap}, {1, -gap}, bulge},
                                    Segment{{-1e5, 0}, {1e5, 0}}};
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    const auto result = detectBoundary({0, 0.5}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    const DetectedBoundary& boundary = result.value();
    ASSERT_EQ(boundary.outcome, BoundaryOutcome::enclosed);
    ASSERT_EQ(boundary.loops.size(), 1U);
    const auto& loop = std::get<Polyline>(boundary.loops[0].loop);
    ASSERT_EQ(loop.vertices.size(), 2U);
    // From the left end of the diameter along the axis, then back along the
    // upper half of the circle.
    EXPECT_NEAR(loop.vertices[0].position.x, -1, 0x1p-40);
    EXPECT_EQ(loop.vertices[0].position.y, 0);
    EXPECT_NEAR(loop.vertices[1].position.x, 1, 0x1p-40);
    EXPECT_EQ(loop.vertices[1].position.y, 0);
    EXPECT_EQ(loop.vertices[0].bulge, 0.0);
    EXPECT_NEAR(loop.vertices[1].bulge, 1.0, 0x1p-40);
    const auto& origins = boundary.loops[0].origins;
    ASSERT_EQ(origins.size(), 2U);
    EXPECT_EQ(origins[0].curve, 1U);
    EXPECT_FALSE(origins[0].reversed);
    EXPECT_EQ(origins[1].curve, 0U);
    EXPECT_FALSE(origins[1].reversed);
}

TEST(DetectBoundary, PointNearALinkOnlyIsOnCurve) {
    // Curves 1 and 2 end 2^-30 apart (below 1e-9 m): a link joins them. The
    // point is 0.95e-9 from the link and more than 1e-9 from both curves.
    const double gap = 0x1p-30;
    const std::vector<Curve> curves{Circle{{50, 50}, 1}, Segment{{-1, 0}, {0, 0}},
                                    Segment{{0, gap}, {-1, gap}}};
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    const auto result = detectBoundary({0.95e-9, gap / 2}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().outcome, BoundaryOutcome::onCurve);
    EXPECT_EQ(result.value().nearCurve, 1U);
    // A little farther, it is outside the thin loop closed by the two links.
    const auto farther = detectBoundary({1.1e-9, gap / 2}, curves, policy);
    ASSERT_TRUE(farther.hasValue());
    EXPECT_EQ(farther.value().outcome, BoundaryOutcome::notEnclosed);
    EXPECT_TRUE(farther.value().openEnds.empty());
}

TEST(DetectBoundary, StretchSharedByLinksGoesToTheLowestPair) {
    // Three collinear free ends e = (0, 0), f = (0, 2^-31), g = (0, 2^-30)
    // of curves 0, 1 and 2, within 1e-9 m of each other: the links (0, 1)
    // and (0, 2) share the stretch from e to f, credited to (0, 1).
    const double step = 0x1p-31;
    const std::vector<Curve> curves{Segment{{0, -1}, {0, 0}}, Segment{{-1, step}, {0, step}},
                                    Segment{{0, 2 * step}, {1, 2 * step}},
                                    Segment{{-1, -1}, {0, -1}}, Segment{{-1, step}, {-1, -1}}};
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    const auto result = detectBoundary({-0.5, -0.5}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    const DetectedBoundary& boundary = result.value();
    ASSERT_EQ(boundary.outcome, BoundaryOutcome::enclosed);
    ASSERT_EQ(boundary.loops.size(), 1U);
    const auto& loop = std::get<Polyline>(boundary.loops[0].loop);
    ASSERT_EQ(loop.vertices.size(), 5U);
    EXPECT_EQ(loop.vertices[0].position.x, -1);
    EXPECT_EQ(loop.vertices[0].position.y, -1);
    const auto& origins = boundary.loops[0].origins;
    ASSERT_EQ(origins.size(), 5U);
    EXPECT_EQ(origins[0].curve, 3U);
    EXPECT_EQ(origins[1].curve, 0U);
    EXPECT_EQ(origins[2].source, LoopPieceSource::link);
    EXPECT_EQ(origins[2].curve, 0U);
    EXPECT_EQ(origins[2].otherCurve, 1U);
    EXPECT_FALSE(origins[2].reversed);
    EXPECT_EQ(origins[3].curve, 1U);
    EXPECT_TRUE(origins[3].reversed);
    EXPECT_EQ(origins[4].curve, 4U);
}

TEST(DetectBoundary, NoCurveLeavesThePointNotEnclosed) {
    const std::vector<Curve> curves{Segment{{1, 1}, {1, 1}}, Circle{{0, 0}, -1}};
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    const auto result = detectBoundary({0, 0}, curves, policy);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().outcome, BoundaryOutcome::notEnclosed);
    EXPECT_TRUE(result.value().openEnds.empty());
    ASSERT_EQ(result.value().skipped.size(), 2U);
    EXPECT_EQ(result.value().skipped[0].diagnosis.code, DiagnosisCode::degenerateCurve);
    EXPECT_EQ(result.value().skipped[0].diagnosis.input, 1U);
    EXPECT_EQ(result.value().skipped[1].diagnosis.code, DiagnosisCode::invalidValue);
    EXPECT_EQ(result.value().skipped[1].diagnosis.input, 2U);
}

// --- Conversion of the arcs, bit for bit (0.3-FS3) ---------------------------------

// The pieces of convertedArc() for arcs of every kind: small sweeps and sweeps
// of almost a whole turn, radii 2^-20 and 2^40, counterclockwise and
// clockwise. The doubles were taken from the code before the speed-up of
// package FS3 (commit 685da52): the conversion must give the same bits.
struct ExpectedPiece {
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
    double bulge = 0.0;
};

struct ArcConversion {
    Arc arc;
    std::vector<ExpectedPiece> pieces;
};

std::vector<Arc> conversionArcs() {
    constexpr double kTurn = 2 * kPi;
    std::vector<Arc> arcs;
    const std::array<double, 4> radii{1.0, 0x1p-20, 0x1p40, 5.0};
    const std::array<Point2, 4> centres{
        {{0.0, 0.0}, {0.375, -0.125}, {0x1p40, -0x1p39}, {12345.25, -678.5}}};
    const std::array<double, 4> starts{0.0, 0.3, -kPi / 2, 2.5};
    const std::array<double, 9> sweeps{1e-7, 1e-3, 0.7,          kPi / 2,     kPi,
                                       3.5,  5.0,  kTurn - 1e-3, kTurn - 1e-9};
    for (std::size_t r = 0; r < radii.size(); ++r) {
        for (std::size_t s = 0; s < sweeps.size(); ++s) {
            const double start = starts[(r + s) % starts.size()];
            for (const double sign : {1.0, -1.0}) {
                arcs.push_back(Arc{centres[r], radii[r], start, sign * sweeps[s]});
            }
        }
    }
    return arcs;
}

TEST(DetectBoundary, ConvertedArcsKeepTheirBits) {
    const std::vector<Arc> arcs = conversionArcs();
    const std::vector<std::vector<ExpectedPiece>> expected = {
        {{0x1p+0, 0x0p+0, 0x1.fffffffffffd3p-1, 0x1.ad7f29abcaf3bp-24, 0x1.ad7f29abcaf48p-26}},
        {{0x1p+0, 0x0p+0, 0x1.fffffffffffd3p-1, -0x1.ad7f29abcaf3bp-24, -0x1.ad7f29abcaf48p-26}},
        {{0x1.e921dd42f09bap-1, 0x1.2e9cd95baba33p-2, 0x1.e8fb1138fa7e2p-1, 0x1.2f973efa588b8p-2,
          0x1.0624dd8abae31p-12}},
        {{0x1.e921dd42f09bap-1, 0x1.2e9cd95baba33p-2, 0x1.e948893e9fbdfp-1, 0x1.2da25fe7ff398p-2,
          -0x1.0624dd8abadefp-12}},
        {{0x1.1a62633145c07p-54, -0x1p+0, 0x1.49d6e694619b8p-1, -0x1.87996529f9d92p-1,
          0x1.6a1aa2e1bef6ep-3}},
        {{0x1.1a62633145c07p-54, -0x1p+0, -0x1.49d6e694619b9p-1, -0x1.87996529f9d92p-1,
          -0x1.6a1aa2e1bef71p-3}},
        {{-0x1.9a2f7ef858b7dp-1, 0x1.326af0dcfcab1p-1, -0x1.326af0dcfcab1p-1, -0x1.9a2f7ef858b7dp-1,
          0x1.a827999fcef33p-2}},
        {{-0x1.9a2f7ef858b7dp-1, 0x1.326af0dcfcab1p-1, 0x1.326af0dcfcabp-1, 0x1.9a2f7ef858b7dp-1,
          -0x1.a827999fcef32p-2}},
        {{0x1p+0, 0x0p+0, -0x1p+0, 0x1.1a62633145c07p-53, 0x1p+0}},
        {{0x1p+0, 0x0p+0, -0x1p+0, -0x1.1a62633145c07p-53, -0x1p+0}},
        {{0x1.e921dd42f09bap-1, 0x1.2e9cd95baba33p-2, -0x1.d823708a2aaeep-2, 0x1.c6545c41f67fdp-1,
          0x1.def49eaab37ap-2},
         {-0x1.d823708a2aaeep-2, 0x1.c6545c41f67fdp-1, -0x1.94f9b84dba8ebp-1, -0x1.39456ffecc09dp-1,
          0x1.def49eaab37a1p-2}},
        {{0x1.e921dd42f09bap-1, 0x1.2e9cd95baba33p-2, 0x1.ed944fd82a10ap-4, -0x1.fc44e08da43dcp-1,
          -0x1.def49eaab37ap-2},
         {0x1.ed944fd82a10ap-4, -0x1.fc44e08da43dcp-1, -0x1.ff207e2b9cbb6p-1, 0x1.de33739e82d32p-5,
          -0x1.def49eaab37a3p-2}},
        {{0x1.1a62633145c07p-54, -0x1p+0, 0x1.326af0dcfcabp-1, 0x1.9a2f7ef858b7dp-1,
          0x1.7166689d41efp-1},
         {0x1.326af0dcfcabp-1, 0x1.9a2f7ef858b7dp-1, -0x1.eaf81f5e09933p-1, -0x1.22785706b4adap-2,
          0x1.7166689d41ef1p-1}},
        {{0x1.1a62633145c07p-54, -0x1p+0, -0x1.326af0dcfcab1p-1, 0x1.9a2f7ef858b7dp-1,
          -0x1.7166689d41efp-1},
         {-0x1.326af0dcfcab1p-1, 0x1.9a2f7ef858b7dp-1, 0x1.eaf81f5e09933p-1, -0x1.22785706b4ad8p-2,
          -0x1.7166689d41eefp-1}},
        {{-0x1.9a2f7ef858b7dp-1, 0x1.326af0dcfcab1p-1, 0x1.9a0842e9f21dfp-1, -0x1.329f6f4fde032p-1,
          0x1.ffdf3c70be23p-1},
         {0x1.9a0842e9f21dfp-1, -0x1.329f6f4fde032p-1, -0x1.99e10023beb9fp-1, 0x1.32d3e8bcada59p-1,
          0x1.ffdf3c70be231p-1}},
        {{-0x1.9a2f7ef858b7dp-1, 0x1.326af0dcfcab1p-1, 0x1.9a56b44e4df7ap-1, -0x1.32366d64e5ca9p-1,
          -0x1.ffdf3c70be232p-1},
         {0x1.9a56b44e4df7ap-1, -0x1.32366d64e5ca9p-1, -0x1.9a7de2eb2d69fp-1, 0x1.3201e4e875a36p-1,
          -0x1.ffdf3c70be231p-1}},
        {{0x1p+0, 0x0p+0, -0x1p+0, 0x1.12e0c4673b40fp-31, 0x1.fffffffdda3e8p-1},
         {-0x1p+0, 0x1.12e0c4673b40fp-31, 0x1p+0, -0x1.12e0c469898ccp-30, 0x1.fffffffdda3e8p-1}},
        {{0x1p+0, 0x0p+0, -0x1p+0, -0x1.12e0c4673b40fp-31, -0x1.fffffffdda3e8p-1},
         {-0x1p+0, -0x1.12e0c4673b40fp-31, 0x1p+0, 0x1.12e0c469898ccp-30, -0x1.fffffffdda3e8p-1}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.80003d243b88ap-2, -0x1.ffffb458c80ecp-4,
          0x1.ad847fd3d04cbp-26}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.80003d243bc82p-2, -0x1.ffffb458cb436p-4,
          -0x1.ad847fd3c4c83p-26}},
        {{0x1.8p-2, -0x1.00008p-3, 0x1.80000010624dap-2, -0x1.00007ffffbce4p-3,
          0x1.0624dd38a57a5p-12}},
        {{0x1.8p-2, -0x1.00008p-3, 0x1.7fffffef9db26p-2, -0x1.00007ffffbce4p-3,
          -0x1.0624dd38a57a5p-12}},
        {{0x1.7fffccba1020fp-2, -0x1.ffff66ca87918p-4, 0x1.7fffc01bf03a9p-2, -0x1.00000778cdce8p-3,
          0x1.6a1aa2e1c4dafp-3}},
        {{0x1.7fffccba1020fp-2, -0x1.ffff66ca87918p-4, 0x1.7ffff17585587p-2, -0x1.ffff06b1ebf21p-4,
          -0x1.6a1aa2e1ef2ccp-3}},
        {{0x1.80004p-2, -0x1p-3, 0x1.8p-2, -0x1.ffffp-4, 0x1.a827999fcef33p-2}},
        {{0x1.80004p-2, -0x1p-3, 0x1.8p-2, -0x1.00008p-3, -0x1.a827999fcef33p-2}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.7fffc2dbc457ap-2, -0x1.000025d39b2b7p-3,
          0x1.00000000067e1p+0}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.7fffc2dbc457ap-2, -0x1.000025d39b2b7p-3,
          -0x1.00000000067e1p+0}},
        {{0x1.8p-2, -0x1.00008p-3, 0x1.80003ef9a0316p-2, -0x1.ffffd25e776c6p-4,
          0x1.def49eaa9974ep-2},
         {0x1.80003ef9a0316p-2, -0x1.ffffd25e776c6p-4, 0x1.7fffe98cc4814p-2, -0x1.ffff10445fe1fp-4,
          0x1.def49eaac9a39p-2}},
        {{0x1.8p-2, -0x1.00008p-3, 0x1.7fffc1065fceap-2, -0x1.ffffd25e776c6p-4,
          -0x1.def49eaa9974ep-2},
         {0x1.7fffc1065fceap-2, -0x1.ffffd25e776c6p-4, 0x1.800016733b7ecp-2, -0x1.ffff10445fe1fp-4,
          -0x1.def49eaac9a39p-2}},
        {{0x1.7fffccba1020fp-2, -0x1.ffff66ca87918p-4, 0x1.8000122785707p-2, -0x1.00007abe07d78p-3,
          0x1.7166689d525cbp-1},
         {0x1.8000122785707p-2, -0x1.00007abe07d78p-3, 0x1.8000162f45e67p-2, -0x1.ffff0fdf3bc82p-4,
          0x1.7166689d2e97ep-1}},
        {{0x1.7fffccba1020fp-2, -0x1.ffff66ca87918p-4, 0x1.80004p-2, -0x1p-3,
          -0x1.7166689d47f6dp-1},
         {0x1.80004p-2, -0x1p-3, 0x1.7fffccba1020fp-2, -0x1.00004c9abc374p-3,
          -0x1.7166689d47f6dp-1}},
        {{0x1.80004p-2, -0x1p-3, 0x1.7fffc00000863p-2, -0x1.ffffffdf3b647p-4, 0x1.ffdf3c70ec0bcp-1},
         {0x1.7fffc00000863p-2, -0x1.ffffffdf3b647p-4, 0x1.80003ffffde72p-2, -0x1.00000020c49b5p-3,
          0x1.ffdf3c70de52ep-1}},
        {{0x1.80004p-2, -0x1p-3, 0x1.7fffc00000863p-2, -0x1.00000010624dcp-3,
          -0x1.ffdf3c70ec0acp-1},
         {0x1.7fffc00000863p-2, -0x1.00000010624dcp-3, 0x1.80003ffffde72p-2, -0x1.ffffffbe76c97p-4,
          -0x1.ffdf3c70de4ccp-1}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.7fffc2dbc4578p-2, -0x1.000025d39b2a7p-3,
          0x1.fffffffde0346p-1},
         {0x1.7fffc2dbc4578p-2, -0x1.000025d39b2a7p-3, 0x1.80003d243ba8bp-2, -0x1.ffffb458c9ad3p-4,
          0x1.fffffffdad2a9p-1}},
        {{0x1.80003d243ba86p-2, -0x1.ffffb458c9a91p-4, 0x1.7fffc2dbc457dp-2, -0x1.000025d39b2c8p-3,
          -0x1.fffffffdb9e3cp-1},
         {0x1.7fffc2dbc457dp-2, -0x1.000025d39b2c8p-3, 0x1.80003d243ba81p-2, -0x1.ffffb458c9a5p-4,
          -0x1.fffffffde833p-1}},
        {{0x1p+40, -0x1.8p+40, 0x1.000001ad7f29bp+40, -0x1.7ffffffffffe9p+40,
          0x1.ad7f29b00001p-26}},
        {{0x1p+40, -0x1.8p+40, 0x1.fffffca501acbp+39, -0x1.7ffffffffffe9p+40,
          -0x1.ad7f29a80001p-26}},
        {{0x1.9742041e9d20dp+37, 0x1.935786e7e5587p+36, 0x1.960874534a595p+37,
          0x1.900f2743ad1d6p+36, 0x1.0624dd8abac7cp-12}},
        {{0x1.9742041e9d20dp+37, 0x1.935786e7e5587p+36, 0x1.987bff710516p+37, 0x1.969f45e56d268p+36,
          -0x1.0624dd8abaa24p-12}},
        {{0x1p+41, -0x1p+39, 0x1.c3ccb294fcec9p+40, 0x1.275b9a51866dfp+37, 0x1.6a1aa2e1bef7p-3}},
        {{0x1p+41, -0x1p+39, 0x1.c3ccb294fcec9p+40, -0x1.24eb734a30cdcp+40, -0x1.6a1aa2e1bef7p-3}},
        {{0x1.f490eea1784ddp+40, -0x1.a2c64d48a8b9ap+37, 0x1.68b193522a2e6p+39,
          0x1.d243ba85e1375p+38, 0x1.a827999fcef32p-2}},
        {{0x1.f490eea1784ddp+40, -0x1.a2c64d48a8b9ap+37, 0x1.4ba73656eae8dp+40,
          -0x1.7490eea1784ddp+40, -0x1.a827999fcef32p-2}},
        {{0x1p+40, -0x1.8p+40, 0x1p+40, 0x1p+39, 0x1p+0}},
        {{0x1p+40, -0x1.8p+40, 0x1.ffffffffffffep+39, 0x1p+39, -0x1p+0}},
        {{0x1.9742041e9d20dp+37, 0x1.935786e7e5587p+36, 0x1.1b9a6ba754c06p+39,
          -0x1.651e05c7dc1b8p+40, 0x1.def49eaab37a1p-2},
         {0x1.1b9a6ba754c06p+39, -0x1.651e05c7dc1b8p+40, 0x1.f5cdb84bc117bp+40,
          -0x1.8f0f8c558516p+39, 0x1.def49eaab37a3p-2}},
        {{0x1.9742041e9d20dp+37, 0x1.935786e7e5587p+36, 0x1.bb4ff632a908fp+40,
          0x1.73ff05afe3c35p+37, -0x1.def49eaab379fp-2},
         {0x1.bb4ff632a908fp+40, 0x1.73ff05afe3c35p+37, 0x1.8a51407da8346p+40,
          -0x1.576aa47848677p+40, -0x1.def49eaab37a3p-2}},
        {{0x1p+41, -0x1p+39, 0x1.9742041e9d20cp+37, 0x1.935786e7e5584p+36, 0x1.7166689d41efp-1},
         {0x1.9742041e9d20cp+37, 0x1.935786e7e5584p+36, 0x1.489e15c1ad2b6p+40,
          -0x1.757c0faf04c9ap+40, 0x1.7166689d41eeep-1}},
        {{0x1p+41, -0x1p+39, 0x1.9742041e9d20cp+37, -0x1.1935786e7e558p+40, -0x1.7166689d41efp-1},
         {0x1.9742041e9d20cp+37, -0x1.1935786e7e558p+40, 0x1.489e15c1ad2b6p+40,
          0x1.d5f03ebc13266p+38, -0x1.7166689d41eefp-1}},
        {{0x1.f490eea1784ddp+40, -0x1.a2c64d48a8b9ap+37, 0x1.6cac8bd465a45p+35,
          -0x1.970fcf8db760ep+39, 0x1.ffdf3c70be231p-1},
         {0x1.6cac8bd465a45p+35, -0x1.970fcf8db760ep+39, 0x1.f4a4449f4fdfp+40,
          -0x1.a4bb4030018ebp+37, 0x1.ffdf3c70be232p-1}},
        {{0x1.f490eea1784ddp+40, -0x1.a2c64d48a8b9ap+37, 0x1.6f184c06a359fp+35,
          -0x1.978d0753544f8p+39, -0x1.ffdf3c70be232p-1},
         {0x1.6f184c06a359fp+35, -0x1.978d0753544f8p+39, 0x1.f47d889c7d3f1p+40,
          -0x1.a0d1820b4ee82p+37, -0x1.ffdf3c70be233p-1}},
        {{0x1p+40, -0x1.8p+40, 0x1.0000000225c18p+40, 0x1p+39, 0x1.fffffffdda3e8p-1},
         {0x1.0000000225c18p+40, 0x1p+39, 0x1.fffffff768f9ep+39, -0x1.8p+40, 0x1.fffffffdda3e8p-1}},
        {{0x1p+40, -0x1.8p+40, 0x1.fffffffbb47cep+39, 0x1p+39, -0x1.fffffffdda3e8p-1},
         {0x1.fffffffbb47cep+39, 0x1p+39, 0x1.000000044b831p+40, -0x1.8p+40,
          -0x1.fffffffdda3e8p-1}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81a9f449ec6f6p+13,
          -0x1.51c0fa5647fcep+9, 0x1.ad7f385826b01p-26}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81a9f44a3cc2cp+13,
          -0x1.51c0fa4f8f8b8p+9, -0x1.ad7f3858267ddp-26}},
        {{0x1.81f2p+13, -0x1.534p+9, 0x1.81f1fffeb074bp+13, -0x1.533f5c28f78cbp+9,
          0x1.0624dd8abb97ap-12}},
        {{0x1.81f2p+13, -0x1.534p+9, 0x1.81f1fffeb074bp+13, -0x1.5340a3d708735p+9,
          -0x1.0624dd8abb97ap-12}},
        {{0x1.81f036a5493adp+13, -0x1.5282ddf826b4cp+9, 0x1.81df9cb213a25p+13, -0x1.51257564d34bp+9,
          0x1.6a1aa2e1bec48p-3}},
        {{0x1.81f036a5493adp+13, -0x1.5282ddf826b4cp+9, 0x1.81eed7aa21d29p+13,
          -0x1.54393a4d1bb21p+9, -0x1.6a1aa2e1bee39p-3}},
        {{0x1.81cap+13, -0x1.55cp+9, 0x1.81f2p+13, -0x1.534p+9, 0x1.a827999fcef32p-2}},
        {{0x1.81cap+13, -0x1.55cp+9, 0x1.81a2p+13, -0x1.534p+9, -0x1.a827999fcef32p-2}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81ea0bb5eb66fp+13,
          -0x1.54bf05ad143bdp+9, 0x1.0000000000071p+0}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81ea0bb5eb66fp+13,
          -0x1.54bf05ad143bdp+9, -0x1.0000000000071p+0}},
        {{0x1.81f2p+13, -0x1.534p+9, 0x1.81c2dec2a8edcp+13, -0x1.50ca3fbe1221dp+9,
          0x1.def49eaab3b21p-2},
         {0x1.81c2dec2a8edcp+13, -0x1.50ca3fbe1221dp+9, 0x1.81a48aaefb4eap+13,
          -0x1.54208052f347dp+9, 0x1.def49eaab3462p-2}},
        {{0x1.81f2p+13, -0x1.534p+9, 0x1.81c2dec2a8edcp+13, -0x1.55b5c041edde3p+9,
          -0x1.def49eaab3b21p-2},
         {0x1.81c2dec2a8edcp+13, -0x1.55b5c041edde3p+9, 0x1.81a48aaefb4eap+13,
          -0x1.525f7fad0cb83p+9, -0x1.def49eaab3462p-2}},
        {{0x1.81f036a5493adp+13, -0x1.5282ddf826b4cp+9, 0x1.81a44fa4aad2bp+13, -0x1.52699b8a9e55p+9,
          0x1.7166689d42134p-1},
         {0x1.81a44fa4aad2bp+13, -0x1.52699b8a9e55p+9, 0x1.81e02ccb0f514p+13, -0x1.5554a6b29eb17p+9,
          0x1.7166689d4219fp-1}},
        {{0x1.81f036a5493adp+13, -0x1.5282ddf826b4cp+9, 0x1.81b275bfa1952p+13,
          -0x1.5545700d0158dp+9, -0x1.7166689d42019p-1},
         {0x1.81b275bfa1952p+13, -0x1.5545700d0158dp+9, 0x1.81c98123dcaf7p+13,
          -0x1.50c00c92cfa19p+9, -0x1.7166689d41b5dp-1}},
        {{0x1.81cap+13, -0x1.55cp+9, 0x1.81ca051eb84e5p+13, -0x1.50c000053e2d6p+9,
          0x1.ffdf3c70beb82p-1},
         {0x1.81ca051eb84e5p+13, -0x1.50c000053e2d6p+9, 0x1.81c9f5c28f78dp+13,
          -0x1.55bfffeb074a9p+9, 0x1.ffdf3c70be12bp-1}},
        {{0x1.81cap+13, -0x1.55cp+9, 0x1.81c9fae147b1bp+13, -0x1.50c000053e2d6p+9,
          -0x1.ffdf3c70beb82p-1},
         {0x1.81c9fae147b1bp+13, -0x1.50c000053e2d6p+9, 0x1.81ca0a3d70873p+13,
          -0x1.55bfffeb074a9p+9, -0x1.ffdf3c70be12bp-1}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81ea0bb5eb338p+13,
          -0x1.54bf05ad1888fp+9, 0x1.fffffffdd9cbap-1},
         {0x1.81ea0bb5eb338p+13, -0x1.54bf05ad1888fp+9, 0x1.81a9f44a14ffep+13, -0x1.51c0fa52e32ap+9,
          0x1.fffffffdda575p-1}},
        {{0x1.81a9f44a14991p+13, -0x1.51c0fa52ebc43p+9, 0x1.81ea0bb5eb9a5p+13,
          -0x1.54bf05ad0feecp+9, -0x1.fffffffdda49ap-1},
         {0x1.81ea0bb5eb9a5p+13, -0x1.54bf05ad0feecp+9, 0x1.81a9f44a14324p+13,
          -0x1.51c0fa52f45e5p+9, -0x1.fffffffdd9cbep-1}},
    };
    ASSERT_EQ(expected.size(), arcs.size());
    for (std::size_t k = 0; k < arcs.size(); ++k) {
        const Arc& arc = arcs[k];
        auto checked = detail::checkedMeaning(Curve{arc}, 1);
        ASSERT_TRUE(checked.hasValue()) << "arc " << k;
        const auto& meaning = std::get<detail::ArcMeaning>(checked.value().support);
        ASSERT_EQ(meaning.span, detail::ArcSpan::partial) << "arc " << k;
        const auto pieces = detail::convertedArc(arc, meaning);
        ASSERT_EQ(pieces.size(), expected[k].size()) << "arc " << k;
        for (std::size_t p = 0; p < pieces.size(); ++p) {
            const ExpectedPiece& want = expected[k][p];
            // Bit for bit: equal doubles of the same sign.
            const auto same = [](double a, double b) {
                return a == b && std::signbit(a) == std::signbit(b);
            };
            EXPECT_TRUE(same(pieces[p].start.x, want.startX)) << "arc " << k << " piece " << p;
            EXPECT_TRUE(same(pieces[p].start.y, want.startY)) << "arc " << k << " piece " << p;
            EXPECT_TRUE(same(pieces[p].end.x, want.endX)) << "arc " << k << " piece " << p;
            EXPECT_TRUE(same(pieces[p].end.y, want.endY)) << "arc " << k << " piece " << p;
            EXPECT_TRUE(same(pieces[p].bulge, want.bulge)) << "arc " << k << " piece " << p;
        }
    }
}

}  // namespace
}  // namespace solid_space::geom2d
