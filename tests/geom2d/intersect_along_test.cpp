// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of intersectAlong (stage 0.2, package P11): the properties of ADR
// 0010, "Verifica", on data generated in a fixed way, also almost degenerate
// (data on a small grid moved by one ulp, pieces of one line and of one
// circle, bad cutters among good ones): each cutter alone gives the pair call
// bit for bit, permuting the cutters changes only the indices, stations never
// decrease along the cuts. A few configurations are worked out by hand in the
// comments. The cases of [0.2-R11] are in tests/corpus/.

#include "curve_meaning.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Contact;
using solid_space::geom2d::Curve;
using solid_space::geom2d::CurvePosition;
using solid_space::geom2d::Cut;
using solid_space::geom2d::CutterOverlap;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::Extension;
using solid_space::geom2d::HostIntersections;
using solid_space::geom2d::intersect;
using solid_space::geom2d::intersectAlong;
using solid_space::geom2d::Location;
using solid_space::geom2d::Overlap;
using solid_space::geom2d::OverlapEnd;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
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

bool sameBits(const OverlapEnd& a, const OverlapEnd& b) {
    return sameBits(a.point, b.point) && sameBits(a.onFirst, b.onFirst) &&
           sameBits(a.onSecond, b.onSecond);
}

bool sameBits(const Overlap& a, const Overlap& b) {
    return a.whole == b.whole && sameBits(a.from, b.from) && sameBits(a.to, b.to);
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

    // A small integer in [-limit, limit]: grids give many exact meetings, and
    // many points shared by different cutters.
    double integer(int limit) {
        const auto span = (2 * static_cast<std::uint64_t>(limit)) + 1;
        return static_cast<double>(static_cast<int>(bits() % span) - limit);
    }

    // A value of the grid, sometimes moved by one ulp.
    double nearInteger(int limit) {
        const double value = integer(limit);
        switch (bits() % 6U) {
            case 0:
                return next(value);
            case 1:
                return previous(value);
            default:
                return value;
        }
    }

    bool chance(unsigned outOf) { return bits() % outOf == 0; }

    std::size_t below(std::size_t limit) { return static_cast<std::size_t>(bits() % limit); }

private:
    std::uint64_t m_state;
};

// A curve of any type with data near a small grid, never degenerate.
Curve gridCurve(Sequence& sequence) {
    for (;;) {
        const Point2 a{sequence.nearInteger(3), sequence.nearInteger(3)};
        const Point2 b{sequence.nearInteger(3), sequence.nearInteger(3)};
        const double radius = std::fabs(sequence.nearInteger(3));
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

// The end of a polyline segment of the circle of radius 25 about the origin
// that starts on an axis with the bulge t: the start turned by 4 atan(t), whose
// cosine and sine (c^2 - s^2, 2 s c with c = (1 - t^2) / (1 + t^2) and
// s = 2 t / (1 + t^2)) are multiples of 1/25 for the bulges used here (1/2,
// 1, 2 and 3, and their opposites, all exact doubles), so the end has integer
// coordinates and the segment lies exactly on that circle.
Point2 turnedBy(Point2 start, double bulge) {
    const double t2 = bulge * bulge;
    const double c = (1.0 - t2) / (1.0 + t2);
    const double s = 2.0 * bulge / (1.0 + t2);
    const double cosine = (c * c) - (s * s);
    const double sine = 2.0 * s * c;
    return Point2{std::round((start.x * cosine) - (start.y * sine)),
                  std::round((start.x * sine) + (start.y * cosine))};
}

// A piece of the circle of radius 25 about the origin: the circle, an arc
// given by angles, or a polyline segment on it.
Curve circlePiece(Sequence& sequence) {
    constexpr std::array<Point2, 4> kAxes{Point2{25.0, 0.0}, Point2{0.0, 25.0}, Point2{-25.0, 0.0},
                                          Point2{0.0, -25.0}};
    constexpr std::array<double, 8> kBulges{0.5, 1.0, 2.0, 3.0, -0.5, -1.0, -2.0, -3.0};
    for (;;) {
        switch (sequence.bits() % 4U) {
            case 0:
                return Circle{{0.0, 0.0}, 25.0};
            case 1: {
                const Curve arc = Arc{{0.0, 0.0},
                                      25.0,
                                      sequence.integer(8) * std::numbers::pi / 4.0,
                                      sequence.integer(8) * std::numbers::pi / 4.0};
                if (!detail::check(arc, 0)) {
                    return arc;
                }
                break;
            }
            default: {
                const auto start = kAxes.at(sequence.below(kAxes.size()));
                const double bulge = kBulges.at(sequence.below(kBulges.size()));
                const auto end = turnedBy(start, bulge);
                if (sequence.chance(2)) {
                    return PolylineSegment{start, end, bulge};
                }
                // The same arc backwards.
                return PolylineSegment{end, start, -bulge};
            }
        }
    }
}

// A piece of the line y = 2 x, or the whole of it as a segment.
Curve linePiece(Sequence& sequence) {
    for (;;) {
        const double a = sequence.integer(4);
        const double b = sequence.integer(4);
        if (a == b) {
            continue;
        }
        if (sequence.chance(2)) {
            return Segment{{a, 2.0 * a}, {b, 2.0 * b}};
        }
        return PolylineSegment{{a, 2.0 * a}, {b, 2.0 * b}, 0.0};
    }
}

// A cutter that intersectAlong must skip.
Curve badCurve(Sequence& sequence) {
    switch (sequence.bits() % 3U) {
        case 0:
            return Segment{{kNaN, 0.0}, {1.0, 1.0}};
        case 1:
            return Circle{{0x1p60, 0.0}, 1.0};
        default:
            return Segment{{1.0, 1.0}, {1.0, 1.0}};
    }
}

enum class Family : std::uint8_t { grid, line, circle };

Curve curveOf(Family family, Sequence& sequence) {
    switch (family) {
        case Family::line:
            return sequence.chance(3) ? gridCurve(sequence) : linePiece(sequence);
        case Family::circle:
            return sequence.chance(3) ? gridCurve(sequence) : circlePiece(sequence);
        case Family::grid:
            break;
    }
    return gridCurve(sequence);
}

struct Configuration {
    Curve host;
    std::vector<Curve> cutters;
};

Configuration configurationFrom(Sequence& sequence) {
    const auto family = static_cast<Family>(sequence.bits() % 3U);
    Configuration configuration{curveOf(family, sequence), {}};
    const auto count = sequence.below(6);
    for (std::size_t i = 0; i < count; ++i) {
        configuration.cutters.push_back(sequence.chance(8) ? badCurve(sequence)
                                                           : curveOf(family, sequence));
    }
    return configuration;
}

// --- Property checks -----------------------------------------------------------

// Each cutter alone gives the pair call: the same points with the same bits,
// in the same order, the same stretches, skipped exactly when the pair call
// fails about it; and the omitted points are the sum of the pair calls.
void expectEachCutterAlone(const Configuration& data, Extension hostPart, Extension cutterPart,
                           const HostIntersections& result, const std::string& where) {
    std::size_t omitted = 0;
    for (std::size_t cutter = 0; cutter < data.cutters.size(); ++cutter) {
        const auto pair = intersect(data.host, hostPart, data.cutters[cutter], cutterPart);
        const auto skipped =
            std::find_if(result.skipped.begin(), result.skipped.end(),
                         [cutter](const auto& entry) { return entry.cutter == cutter; });
        if (!pair.hasValue()) {
            ASSERT_NE(skipped, result.skipped.end()) << where << ", cutter " << cutter;
            EXPECT_EQ(skipped->diagnosis.code, pair.diagnosis().code) << where;
            EXPECT_EQ(skipped->diagnosis.input, cutter + 1) << where;
            continue;
        }
        EXPECT_EQ(skipped, result.skipped.end()) << where << ", cutter " << cutter;
        omitted += pair.value().omittedBeyondDomain;
        std::size_t k = 0;
        const auto& points = pair.value().points;
        for (const auto& cut : result.cuts) {
            for (const auto& contact : cut.cutters) {
                if (contact.cutter != cutter) {
                    continue;
                }
                ASSERT_LT(k, points.size()) << where << ", cutter " << cutter;
                const auto& expected = points[k];
                EXPECT_TRUE(sameBits(cut.point, expected.point)) << where << ", point " << k;
                EXPECT_TRUE(sameBits(cut.onHost, expected.onFirst)) << where << ", point " << k;
                EXPECT_EQ(contact.contact, expected.contact) << where << ", point " << k;
                EXPECT_TRUE(sameBits(contact.onCutter, expected.onSecond))
                    << where << ", point " << k;
                ++k;
            }
        }
        EXPECT_EQ(k, points.size()) << where << ", cutter " << cutter;
        std::size_t s = 0;
        const auto& overlaps = pair.value().overlaps;
        for (const auto& overlap : result.overlaps) {
            if (overlap.cutter != cutter) {
                continue;
            }
            ASSERT_LT(s, overlaps.size()) << where << ", cutter " << cutter;
            EXPECT_TRUE(sameBits(overlap.overlap, overlaps[s])) << where << ", overlap " << s;
            ++s;
        }
        EXPECT_EQ(s, overlaps.size()) << where << ", cutter " << cutter;
    }
    EXPECT_EQ(result.omittedBeyondDomain, omitted) << where;
}

// Stations never decrease along the cuts nor along the starts of the
// stretches; every cut has its cutters by strictly increasing index.
void expectSorted(const HostIntersections& result, const std::string& where) {
    for (std::size_t k = 0; k < result.cuts.size(); ++k) {
        const auto& cut = result.cuts[k];
        ASSERT_FALSE(cut.cutters.empty()) << where;
        for (std::size_t c = 1; c < cut.cutters.size(); ++c) {
            EXPECT_LT(cut.cutters[c - 1].cutter, cut.cutters[c].cutter) << where;
        }
        if (k > 0) {
            EXPECT_LE(result.cuts[k - 1].onHost.station, cut.onHost.station) << where;
        }
    }
    for (std::size_t k = 1; k < result.overlaps.size(); ++k) {
        EXPECT_LE(result.overlaps[k - 1].overlap.from.onFirst.station,
                  result.overlaps[k].overlap.from.onFirst.station)
            << where;
    }
    for (std::size_t k = 1; k < result.skipped.size(); ++k) {
        EXPECT_LT(result.skipped[k - 1].cutter, result.skipped[k].cutter) << where;
    }
}

HostIntersections run(const Configuration& data, Extension hostPart, Extension cutterPart) {
    auto result = intersectAlong(data.host, hostPart, data.cutters, cutterPart);
    EXPECT_TRUE(result.hasValue());
    if (!result.hasValue()) {
        return {};
    }
    return std::move(result).value();
}

TEST(IntersectAlong, EachCutterAloneOnGeneratedData) {
    Sequence sequence(20);
    for (int i = 0; i < 700; ++i) {
        const auto data = configurationFrom(sequence);
        for (const auto hostPart : kExtensions) {
            for (const auto cutterPart : kExtensions) {
                const auto where = "configuration " + std::to_string(i);
                const auto result = run(data, hostPart, cutterPart);
                expectEachCutterAlone(data, hostPart, cutterPart, result, where);
                expectSorted(result, where);
            }
        }
    }
}

// The result of a permutation of the cutters, its indices mapped back.
HostIntersections mappedBack(HostIntersections result, const std::vector<std::size_t>& original) {
    for (auto& cut : result.cuts) {
        for (auto& contact : cut.cutters) {
            contact.cutter = original.at(contact.cutter);
        }
        std::sort(cut.cutters.begin(), cut.cutters.end(),
                  [](const auto& a, const auto& b) { return a.cutter < b.cutter; });
    }
    for (auto& overlap : result.overlaps) {
        overlap.cutter = original.at(overlap.cutter);
    }
    for (auto& skipped : result.skipped) {
        skipped.cutter = original.at(skipped.cutter);
    }
    std::sort(result.skipped.begin(), result.skipped.end(),
              [](const auto& a, const auto& b) { return a.cutter < b.cutter; });
    return result;
}

// The stretches with the same start are by index, which the permutation
// changes: both lists are put in the order of (start station, index), which
// keeps the exact order of different starts (their stations never decrease)
// and fixes the order of equal ones.
std::vector<CutterOverlap> canonical(std::vector<CutterOverlap> overlaps) {
    std::stable_sort(overlaps.begin(), overlaps.end(), [](const auto& a, const auto& b) {
        if (a.overlap.from.onFirst.station != b.overlap.from.onFirst.station) {
            return a.overlap.from.onFirst.station < b.overlap.from.onFirst.station;
        }
        return a.cutter < b.cutter;
    });
    return overlaps;
}

void expectSameButIndices(const HostIntersections& a, const HostIntersections& b,
                          const std::string& where) {
    ASSERT_EQ(a.cuts.size(), b.cuts.size()) << where;
    for (std::size_t k = 0; k < a.cuts.size(); ++k) {
        const auto& x = a.cuts[k];
        const auto& y = b.cuts[k];
        EXPECT_TRUE(sameBits(x.point, y.point)) << where << ", cut " << k;
        EXPECT_TRUE(sameBits(x.onHost, y.onHost)) << where << ", cut " << k;
        ASSERT_EQ(x.cutters.size(), y.cutters.size()) << where << ", cut " << k;
        for (std::size_t c = 0; c < x.cutters.size(); ++c) {
            EXPECT_EQ(x.cutters[c].cutter, y.cutters[c].cutter) << where << ", cut " << k;
            EXPECT_EQ(x.cutters[c].contact, y.cutters[c].contact) << where << ", cut " << k;
            EXPECT_TRUE(sameBits(x.cutters[c].onCutter, y.cutters[c].onCutter))
                << where << ", cut " << k;
        }
    }
    const auto overlapsA = canonical(a.overlaps);
    const auto overlapsB = canonical(b.overlaps);
    ASSERT_EQ(overlapsA.size(), overlapsB.size()) << where;
    for (std::size_t k = 0; k < overlapsA.size(); ++k) {
        EXPECT_EQ(overlapsA[k].cutter, overlapsB[k].cutter) << where << ", overlap " << k;
        EXPECT_TRUE(sameBits(overlapsA[k].overlap, overlapsB[k].overlap))
            << where << ", overlap " << k;
    }
    ASSERT_EQ(a.skipped.size(), b.skipped.size()) << where;
    for (std::size_t k = 0; k < a.skipped.size(); ++k) {
        EXPECT_EQ(a.skipped[k].cutter, b.skipped[k].cutter) << where;
        EXPECT_EQ(a.skipped[k].diagnosis.code, b.skipped[k].diagnosis.code) << where;
    }
    EXPECT_EQ(a.omittedBeyondDomain, b.omittedBeyondDomain) << where;
}

TEST(IntersectAlong, PermutingTheCuttersChangesOnlyTheIndices) {
    Sequence sequence(21);
    for (int i = 0; i < 400; ++i) {
        const auto data = configurationFrom(sequence);
        // A permutation drawn from the sequence (Fisher-Yates).
        std::vector<std::size_t> original(data.cutters.size());
        for (std::size_t k = 0; k < original.size(); ++k) {
            original[k] = k;
        }
        for (std::size_t k = original.size(); k > 1; --k) {
            std::swap(original[k - 1], original[sequence.below(k)]);
        }
        Configuration permuted{data.host, {}};
        for (const auto index : original) {
            permuted.cutters.push_back(data.cutters[index]);
        }
        for (const auto hostPart : kExtensions) {
            for (const auto cutterPart : kExtensions) {
                const auto where = "configuration " + std::to_string(i);
                const auto direct = run(data, hostPart, cutterPart);
                const auto other = mappedBack(run(permuted, hostPart, cutterPart), original);
                expectSameButIndices(direct, other, where);
            }
        }
    }
}

// --- Configurations by hand ---------------------------------------------------

HostIntersections runOn(const Curve& host, const std::vector<Curve>& cutters,
                        Extension hostPart = Extension::none,
                        Extension cutterPart = Extension::none) {
    return run(Configuration{host, cutters}, hostPart, cutterPart);
}

std::vector<std::size_t> indicesOf(const Cut& cut) {
    std::vector<std::size_t> indices;
    indices.reserve(cut.cutters.size());
    for (const auto& contact : cut.cutters) {
        indices.push_back(contact.cutter);
    }
    return indices;
}

TEST(IntersectAlong, PointsOfDifferentCuttersMerged) {
    // The host y = 0 from x = -4 to 4; cutters 0, 2 and 3 pass through (1, 0):
    // the vertical x = 1, the line y = x - 1 and the circle of centre (1, 1)
    // and radius 1, which touches y = 0 there. Cutter 1, x = -1, crosses
    // alone, and cutter 4 ends on the host at (1, 0) from above.
    const Curve host = Segment{{-4.0, 0.0}, {4.0, 0.0}};
    const std::vector<Curve> cutters{
        Segment{{1.0, -2.0}, {1.0, 2.0}}, Segment{{-1.0, -2.0}, {-1.0, 2.0}},
        Segment{{0.0, -1.0}, {2.0, 1.0}}, Circle{{1.0, 1.0}, 1.0}, Segment{{1.0, 3.0}, {1.0, 0.0}}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.cuts.size(), 2U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{1}));
    EXPECT_EQ(indicesOf(result.cuts[1]), (std::vector<std::size_t>{0, 2, 3, 4}));
    const auto& merged = result.cuts[1];
    EXPECT_EQ(merged.onHost.location, Location::inside);
    EXPECT_EQ(merged.cutters[0].contact, Contact::crossing);
    EXPECT_EQ(merged.cutters[1].contact, Contact::crossing);
    EXPECT_EQ(merged.cutters[2].contact, Contact::touching);
    EXPECT_EQ(merged.cutters[3].contact, Contact::crossing);
    EXPECT_EQ(merged.cutters[3].onCutter.location, Location::atEnd);
}

TEST(IntersectAlong, DistinctRootsCloserThanAnUlp) {
    // The host y = 0 from -2 to 2 and two circles of centre (0, 2^-30) with
    // radii r0 = 1 and r1 = next(1). They cross the host at -+sqrt(r^2 -
    // 2^-60): r1^2 - r0^2 = 2^-51 + 2^-104 > 0, so the points of cutter 1 are
    // the outer ones, by less than 2^-52 (sqrt(a + e) - sqrt(a) < e / (2 sqrt(a))
    // with a near 1): the order is 1, 0, 0, 1, four distinct cuts, which the
    // rounded stations may not tell apart.
    const Curve host = Segment{{-2.0, 0.0}, {2.0, 0.0}};
    const std::vector<Curve> cutters{Circle{{0.0, 0x1p-30}, 1.0},
                                     Circle{{0.0, 0x1p-30}, next(1.0)}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.cuts.size(), 4U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{1}));
    EXPECT_EQ(indicesOf(result.cuts[1]), (std::vector<std::size_t>{0}));
    EXPECT_EQ(indicesOf(result.cuts[2]), (std::vector<std::size_t>{0}));
    EXPECT_EQ(indicesOf(result.cuts[3]), (std::vector<std::size_t>{1}));
    // The same with the two radicands swapped between the cutters.
    const auto swapped = runOn(host, {cutters[1], cutters[0]});
    ASSERT_EQ(swapped.cuts.size(), 4U);
    EXPECT_EQ(indicesOf(swapped.cuts[0]), (std::vector<std::size_t>{0}));
    EXPECT_EQ(indicesOf(swapped.cuts[1]), (std::vector<std::size_t>{1}));
    EXPECT_EQ(indicesOf(swapped.cuts[2]), (std::vector<std::size_t>{1}));
    EXPECT_EQ(indicesOf(swapped.cuts[3]), (std::vector<std::size_t>{0}));
}

TEST(IntersectAlong, AlongACircleFromItsStart) {
    // The circle of radius 2 about the origin starts at (2, 0) and runs
    // counterclockwise. The line x = 0 (cutter 0) meets it at (0, 2) and
    // (0, -2), the line y = 0 (cutter 1) at (2, 0) and (-2, 0), and the line
    // y = x - 2 (cutter 2) at (2, 0) and (0, -2): along the circle (2, 0)
    // with 1 and 2, (0, 2) with 0, (-2, 0) with 1, (0, -2) with 0 and 2.
    const Curve host = Circle{{0.0, 0.0}, 2.0};
    const std::vector<Curve> cutters{Segment{{0.0, -3.0}, {0.0, 3.0}},
                                     Segment{{3.0, 0.0}, {-3.0, 0.0}},
                                     Segment{{3.0, 1.0}, {-1.0, -3.0}}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.cuts.size(), 4U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{1, 2}));
    EXPECT_EQ(indicesOf(result.cuts[1]), (std::vector<std::size_t>{0}));
    EXPECT_EQ(indicesOf(result.cuts[2]), (std::vector<std::size_t>{1}));
    EXPECT_EQ(indicesOf(result.cuts[3]), (std::vector<std::size_t>{0, 2}));
    EXPECT_TRUE(sameBits(result.cuts[0].onHost.station, 0.0));
    EXPECT_TRUE(sameBits(result.cuts[0].point, Point2{2.0, 0.0}));
    EXPECT_TRUE(sameBits(result.cuts[3].point, Point2{0.0, -2.0}));
}

TEST(IntersectAlong, StretchesByStartThenIndex) {
    // The host y = 0 from -2 to 2. Cutters 0 to 2 lie on it: [0, 1],
    // [-1, 0] and [-1, 1]; cutter 3 follows on from its end, [2, 3], and
    // meets it at (2, 0) only; cutter 4, x = 0, crosses it at (0, 0).
    const Curve host = Segment{{-2.0, 0.0}, {2.0, 0.0}};
    const std::vector<Curve> cutters{
        Segment{{0.0, 0.0}, {1.0, 0.0}}, Segment{{0.0, 0.0}, {-1.0, 0.0}},
        Segment{{-1.0, 0.0}, {1.0, 0.0}}, Segment{{3.0, 0.0}, {2.0, 0.0}},
        Segment{{0.0, -1.0}, {0.0, 1.0}}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.overlaps.size(), 3U);
    EXPECT_EQ(result.overlaps[0].cutter, 1U);
    EXPECT_EQ(result.overlaps[1].cutter, 2U);
    EXPECT_EQ(result.overlaps[2].cutter, 0U);
    // Cutter 1 runs backwards: its stretch starts at its end.
    EXPECT_EQ(result.overlaps[0].overlap.from.onSecond.location, Location::atEnd);
    ASSERT_EQ(result.cuts.size(), 2U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{4}));
    EXPECT_EQ(indicesOf(result.cuts[1]), (std::vector<std::size_t>{3}));
    EXPECT_EQ(result.cuts[1].onHost.location, Location::atEnd);
    EXPECT_EQ(result.cuts[1].cutters[0].contact, Contact::sameCurve);
    EXPECT_EQ(result.cuts[1].cutters[0].onCutter.location, Location::atEnd);

    // Extended, every cutter shares the whole line from the start of the host.
    const auto whole = runOn(host, cutters, Extension::none, Extension::extended);
    ASSERT_EQ(whole.overlaps.size(), 4U);
    for (std::size_t k = 0; k < 4; ++k) {
        EXPECT_EQ(whole.overlaps[k].cutter, k);
        EXPECT_EQ(whole.overlaps[k].overlap.from.onFirst.location, Location::atStart);
    }
}

TEST(IntersectAlong, StretchesOfArcsOnTheHostCircle) {
    // The host is the circle of radius 25 about the origin, from (25, 0)
    // counterclockwise. Cutter 0 is the half circle from (0, 25) through
    // (-25, 0) to (0, -25) (bulge 1); cutter 1 the half circle from (0, -25)
    // through (25, 0) to (0, 25): on a whole host its stretch is one, from
    // (0, -25), even if it runs through the start of the host. Cutter 2 runs
    // clockwise from (-7, 24) to (25, 0) (bulge -1/2, see turnedBy()): its
    // stretch starts at its end, the start of the host. By start: 2, 0, 1.
    const Curve host = Circle{{0.0, 0.0}, 25.0};
    const std::vector<Curve> cutters{PolylineSegment{{0.0, 25.0}, {0.0, -25.0}, 1.0},
                                     PolylineSegment{{0.0, -25.0}, {0.0, 25.0}, 1.0},
                                     PolylineSegment{{-7.0, 24.0}, {25.0, 0.0}, -0.5}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.overlaps.size(), 3U);
    EXPECT_EQ(result.overlaps[0].cutter, 2U);
    EXPECT_EQ(result.overlaps[1].cutter, 0U);
    EXPECT_EQ(result.overlaps[2].cutter, 1U);
    EXPECT_TRUE(sameBits(result.overlaps[0].overlap.from.point, Point2{25.0, 0.0}));
    EXPECT_TRUE(sameBits(result.overlaps[0].overlap.from.onFirst.station, 0.0));
    EXPECT_EQ(result.overlaps[0].overlap.from.onSecond.location, Location::atEnd);
    EXPECT_TRUE(sameBits(result.overlaps[2].overlap.from.point, Point2{0.0, -25.0}));
    EXPECT_TRUE(result.cuts.empty());

    // The host is the left half circle, from (0, 25) through (-25, 0) to
    // (0, -25). Cutter 0 runs counterclockwise from (-25, 0) to (-7, 24)
    // (bulge 3, a turn of 4 atan(3), about 286 degrees): it leaves the host at
    // (0, -25) and comes back at its start, so it shares two stretches, from
    // the start of the host and from (-25, 0). Cutter 1, the whole circle,
    // shares the whole host, from its start: after cutter 0, by index.
    const Curve half = PolylineSegment{{0.0, 25.0}, {0.0, -25.0}, 1.0};
    const std::vector<Curve> arcs{PolylineSegment{{-25.0, 0.0}, {-7.0, 24.0}, 3.0},
                                  Circle{{0.0, 0.0}, 25.0}};
    const auto split = runOn(half, arcs);
    ASSERT_EQ(split.overlaps.size(), 3U);
    EXPECT_EQ(split.overlaps[0].cutter, 0U);
    EXPECT_EQ(split.overlaps[1].cutter, 1U);
    EXPECT_EQ(split.overlaps[2].cutter, 0U);
    EXPECT_EQ(split.overlaps[0].overlap.from.onFirst.location, Location::atStart);
    EXPECT_EQ(split.overlaps[1].overlap.from.onFirst.location, Location::atStart);
    EXPECT_FALSE(split.overlaps[1].overlap.whole);
    EXPECT_TRUE(sameBits(split.overlaps[2].overlap.from.point, Point2{-25.0, 0.0}));
    EXPECT_EQ(split.overlaps[2].overlap.from.onSecond.location, Location::atStart);
    EXPECT_TRUE(split.cuts.empty());
}

TEST(IntersectAlong, SkippedCuttersAndBadHost) {
    const Curve host = Segment{{-2.0, 0.0}, {2.0, 0.0}};
    const std::vector<Curve> cutters{Segment{{1.0, 1.0}, {1.0, 1.0}},
                                     Segment{{0.0, -1.0}, {0.0, 1.0}}, Circle{{0.0, 0.0}, kNaN},
                                     Circle{{0.0, 0.0}, -1.0}, Segment{{0x1p60, 0.0}, {0.0, 1.0}}};
    const auto result = runOn(host, cutters);
    ASSERT_EQ(result.skipped.size(), 4U);
    EXPECT_EQ(result.skipped[0].cutter, 0U);
    EXPECT_EQ(result.skipped[0].diagnosis.code, DiagnosisCode::degenerateCurve);
    EXPECT_EQ(result.skipped[0].diagnosis.input, 1U);
    EXPECT_EQ(result.skipped[1].cutter, 2U);
    EXPECT_EQ(result.skipped[1].diagnosis.code, DiagnosisCode::invalidValue);
    EXPECT_EQ(result.skipped[1].diagnosis.input, 3U);
    EXPECT_EQ(result.skipped[2].cutter, 3U);
    EXPECT_EQ(result.skipped[2].diagnosis.code, DiagnosisCode::invalidValue);
    EXPECT_EQ(result.skipped[3].cutter, 4U);
    EXPECT_EQ(result.skipped[3].diagnosis.code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.skipped[3].diagnosis.input, 5U);
    ASSERT_EQ(result.cuts.size(), 1U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{1}));

    for (const Curve& bad : {Curve{Segment{{0.0, 0.0}, {0.0, 0.0}}},
                             Curve{Circle{{kNaN, 0.0}, 1.0}}, Curve{Circle{{0.0, 0x1p60}, 1.0}}}) {
        const auto failed = intersectAlong(bad, Extension::none, cutters, Extension::none);
        ASSERT_FALSE(failed.hasValue());
        EXPECT_EQ(failed.diagnosis().input, 0U);
    }
    const auto none = intersectAlong(host, Extension::none, {}, Extension::none);
    ASSERT_TRUE(none.hasValue());
    EXPECT_TRUE(none.value().cuts.empty());
    EXPECT_TRUE(none.value().overlaps.empty());
    EXPECT_TRUE(none.value().skipped.empty());
}

TEST(IntersectAlong, OmittedSummedOverCutters) {
    // The extended host y = 0 and the extended lines through (0, 1) and
    // (1, 1 - 2^-53), and through (0, 2) and (1, 2 - 2^-52): both meet y = 0 at
    // x = 2^53, beyond the domain (2^50). The third crosses at (1, 0).
    const Curve host = Segment{{0.0, 0.0}, {1.0, 0.0}};
    const std::vector<Curve> cutters{Segment{{0.0, 1.0}, {1.0, previous(1.0)}},
                                     Segment{{0.0, 2.0}, {1.0, previous(2.0)}},
                                     Segment{{0.0, 1.0}, {1.0, 0.0}}};
    const auto result = runOn(host, cutters, Extension::extended, Extension::extended);
    EXPECT_EQ(result.omittedBeyondDomain, 2U);
    ASSERT_EQ(result.cuts.size(), 1U);
    EXPECT_EQ(indicesOf(result.cuts[0]), (std::vector<std::size_t>{2}));
}

}  // namespace
