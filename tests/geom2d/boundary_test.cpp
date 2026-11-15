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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using robust::TolerancePolicy;

constexpr double kPi = 3.141592653589793;

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

constexpr Scale kScales[] = {{0.0, 1.0}, {1e5, 1000.0}};

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

}  // namespace
}  // namespace solid_space::geom2d
