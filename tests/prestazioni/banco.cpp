// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/boundary.hpp"
#include "solid_space/geom2d/clipping.hpp"
#include "solid_space/geom2d/ellipse.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/geom2d/tangency.hpp"
#include "solid_space/robust/elementary.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace geom = solid_space::geom2d;
using Clock = std::chrono::steady_clock;
using Pair = std::pair<geom::Curve, geom::Curve>;

constexpr std::uint32_t kSeed = 0x51717U;
constexpr std::size_t kSamples = 8;
constexpr std::size_t kPairRepetitions = 16;
constexpr std::size_t kLargeRepetitions = 3;
constexpr std::size_t kSnapPairs = 32;
constexpr double kSnapGoalMicroseconds = 1000.0;

class Generator {
public:
    // Fixed unsigned arithmetic and dyadic scaling avoid implementation-defined
    // distributions and library trigonometry in the input data.
    [[nodiscard]] double coordinate() {
        m_state = m_state * 1664525U + 1013904223U;
        return static_cast<double>(m_state >> 24U) * 0.25;
    }

private:
    std::uint32_t m_state = kSeed;
};

struct Observation {
    bool valid = false;
    double checksum = 0.0;
};

struct Report {
    bool valid = true;
    bool snapGoalMet = true;
    double checksum = 0.0;
};

template <typename Intersections>
[[nodiscard]] Observation observe(const geom::Result<Intersections>& result) {
    if (!result.hasValue()) {
        return {};
    }
    const auto& value = result.value();
    auto checksum = static_cast<double>(value.points.size() + value.overlaps.size() +
                                        value.omittedBeyondDomain);
    for (const auto& point : value.points) {
        checksum += point.point.x + point.point.y;
    }
    return {true, checksum};
}

template <typename Operation>
void measure(std::string_view name, std::string_view goal, std::size_t samples,
             std::size_t repetitions, Operation operation, Report& report, bool snap = false) {
    // Warm each input before measurement; data generation and table output are
    // excluded. Result inspection/destruction and clock overhead are included.
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto observation = operation(sample);
        report.valid = report.valid && observation.valid;
        report.checksum += observation.checksum;
    }
    double total = 0.0;
    double worst = 0.0;
    bool valid = true;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        for (std::size_t sample = 0; sample < samples; ++sample) {
            const auto start = Clock::now();
            const auto observation = operation(sample);
            const auto end = Clock::now();
            const double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
            total += elapsed;
            worst = std::max(worst, elapsed);
            valid = valid && observation.valid;
            report.checksum += observation.checksum;
        }
    }
    report.valid = report.valid && valid;
    report.snapGoalMet = report.snapGoalMet && (!snap || worst < kSnapGoalMicroseconds);
    const auto calls = samples * repetitions;
    std::cout << "| " << name << " | " << calls << " | " << total / static_cast<double>(calls)
              << " | " << worst << " | " << goal << " | " << (valid ? "OK" : "INVALID") << " |\n";
}

[[nodiscard]] std::array<geom::Curve, 5> curves(double x, double y, bool second) {
    const double shift = second ? 3.0 : 0.0;
    const geom::Point2 center{x + shift, y};
    const geom::Segment segment{{x - 8.0, y - 3.0 + shift}, {x + 8.0, y + 3.0 - shift}};
    const geom::Arc arc{center, 5.0, 0.0, geom::kThreeQuarterTurn};
    const geom::Circle circle{center, 5.0};
    const geom::PolylineSegment curved{{x - 5.0 + shift, y}, {x + 5.0 + shift, y}, 1.0};
    const geom::PolylineSegment straight{
        {x - 8.0, y + 4.0 - shift}, {x + 8.0, y - 4.0 + shift}, 0.0};
    return {segment, arc, circle, curved, straight};
}

void measurePairs(Generator& generator, Report& report) {
    constexpr std::array<std::string_view, 5> kNames{"segment", "arc", "circle", "bulge",
                                                     "straight-bulge"};
    for (std::size_t first = 0; first < kNames.size(); ++first) {
        for (std::size_t second = first; second < kNames.size(); ++second) {
            std::vector<Pair> data;
            for (std::size_t sample = 0; sample < kSamples; ++sample) {
                const double x = generator.coordinate();
                const double y = generator.coordinate();
                data.emplace_back(curves(x, y, false).at(first), curves(x, y, true).at(second));
            }
            const std::string name =
                "intersect " + std::string(kNames.at(first)) + "/" + std::string(kNames.at(second));
            const auto operation = [&data](std::size_t sample) {
                const auto& [a, b] = data[sample];
                return observe(geom::intersect(a, geom::Extension::none, b, geom::Extension::none));
            };
            measure(name, "ADR 0010.8: a few us", data.size(), kPairRepetitions, operation, report);
        }
    }
}

void measureNearDegeneracies(Report& report) {
    // 2^-40 stays representable at this scale. Each row straddles the exact
    // degeneracy; these are workloads, not geometric reference assertions.
    // Diagonal tangencies keep the bounding boxes overlapping on both sides.
    constexpr double kDelta = 0x1p-40;
    const geom::Curve circle = geom::Circle{{0.0, 0.0}, 5.0};
    const std::array<std::pair<std::string_view, Pair>, 6> data{{
        {"near tangent line/circle inside",
         {geom::Segment{{11.0 - 3.0 * kDelta, -2.0 - 4.0 * kDelta},
                        {-5.0 - 3.0 * kDelta, 10.0 - 4.0 * kDelta}},
          circle}},
        {"near tangent line/circle outside",
         {geom::Segment{{11.0 + 3.0 * kDelta, -2.0 + 4.0 * kDelta},
                        {-5.0 + 3.0 * kDelta, 10.0 + 4.0 * kDelta}},
          circle}},
        {"near tangent circle/circle inside",
         {circle, geom::Circle{{6.0 - 3.0 * kDelta, 8.0 - 4.0 * kDelta}, 5.0}}},
        {"near tangent circle/circle outside",
         {circle, geom::Circle{{6.0 + 3.0 * kDelta, 8.0 + 4.0 * kDelta}, 5.0}}},
        {"near coincident circles", {circle, geom::Circle{{kDelta, 0.0}, 5.0}}},
        {"near coincident lines",
         {geom::Segment{{-8.0, 0.0}, {8.0, 0.0}}, geom::Segment{{-8.0, -kDelta}, {8.0, kDelta}}}},
    }};
    for (const auto& entry : data) {
        const auto operation = [&entry](std::size_t) {
            constexpr auto kExtension = geom::Extension::none;
            const auto& [first, second] = entry.second;
            return observe(geom::intersect(first, kExtension, second, kExtension));
        };
        measure(entry.first, "ADR 0010.8: bounded exact path (no numeric limit)", 1,
                kPairRepetitions, operation, report);
    }
}

void measureSnap(Generator& generator, Report& report) {
    std::vector<Pair> data;
    for (std::size_t index = 0; index < kSnapPairs; ++index) {
        const double x = generator.coordinate();
        const double y = generator.coordinate();
        data.emplace_back(curves(x, y, false).at(index % 5),
                          curves(x, y, true).at((index + 1) % 5));
    }
    const auto operation = [&data](std::size_t) {
        Observation batch{true, 0.0};
        for (const auto& [a, b] : data) {
            const auto observation =
                observe(geom::intersect(a, geom::Extension::none, b, geom::Extension::none));
            batch.valid = batch.valid && observation.valid;
            batch.checksum += observation.checksum;
        }
        return batch;
    };
    measure("snap: 32 pairs (whole batch)", "ADR 0010.8: < 1000 us/batch", 1, kPairRepetitions,
            operation, report, true);
}

void measureAlong(Report& report) {
    constexpr std::array<std::size_t, 3> kSizes{10, 100, 1000};
    const geom::Curve host = geom::Segment{{0.0, 0.0}, {1001.0, 0.0}};
    for (const auto size : kSizes) {
        std::vector<geom::Curve> cutters;
        // Reverse stations exercise ordering instead of supplying pre-sorted cuts.
        for (std::size_t index = size; index > 0; --index) {
            const auto x = static_cast<double>(index);
            cutters.emplace_back(geom::Segment{{x, -1.0}, {x, 1.0}});
        }
        const auto operation = [&host, &cutters](std::size_t) {
            const auto result =
                geom::intersectAlong(host, geom::Extension::none, cutters, geom::Extension::none);
            if (!result.hasValue()) {
                return Observation{};
            }
            const auto& value = result.value();
            auto checksum = static_cast<double>(value.cuts.size());
            for (const auto& cut : value.cuts) {
                checksum += cut.point.x + cut.onHost.station;
            }
            return Observation{value.skipped.empty() && value.cuts.size() == cutters.size(),
                               checksum};
        };
        measure("intersectAlong " + std::to_string(size), "ADR 0010.8: report scaling", 1,
                kLargeRepetitions, operation, report);
    }
}

void measureTangents(Generator& generator, Report& report) {
    const double x = generator.coordinate();
    const double y = generator.coordinate();
    const auto data = curves(x, y, false);
    constexpr std::array<std::size_t, 3> kCircular{1, 2, 3};
    constexpr std::array<std::string_view, 3> kNames{"arc", "circle", "bulge"};
    for (std::size_t index = 0; index < kCircular.size(); ++index) {
        const auto& curve = data.at(kCircular.at(index));
        const auto operation = [&curve](std::size_t sample) {
            const geom::Point2 externalPoint{100.0 + static_cast<double>(sample), 100.0};
            const auto result = geom::tangentsFrom(externalPoint, curve, geom::Extension::extended);
            if (!result.hasValue()) {
                return Observation{};
            }
            double checksum = 0.0;
            for (const auto& point : result.value().points) {
                checksum += point.point.x + point.point.y;
            }
            return Observation{result.value().points.size() == 2, checksum};
        };
        measure("tangentsFrom " + std::string(kNames.at(index)),
                "ADR 0011.10: common path + square root (no numeric limit)", kSamples,
                kPairRepetitions, operation, report);
    }
}

void measureOffsets(Report& report) {
    const auto policy = solid_space::robust::TolerancePolicy::forUnit(0.001);
    if (!policy.hasValue()) {
        report.valid = false;
        return;
    }
    constexpr std::array<std::size_t, 3> kSizes{10, 100, 500};
    for (const auto size : kSizes) {
        geom::Polyline polyline;
        // An open, x-monotone orthogonal staircase: unit outward offset,
        // long edges, no vanished pieces or intersections with the input.
        geom::Point2 point{0.0, 0.0};
        polyline.vertices.push_back({point, 0.0});
        for (std::size_t index = 0; index < size; ++index) {
            if (index % 2 == 0) {
                point.x += 20.0;
            } else {
                point.y += 20.0;
            }
            polyline.vertices.push_back({point, 0.0});
        }
        const auto operation = [&polyline, &policy, size](std::size_t) {
            const auto result = geom::offsetPolyline(polyline, 1.0, policy.value());
            if (!result.hasValue()) {
                return Observation{};
            }
            const auto& value = result.value();
            double checksum = 0.0;
            for (const auto& vertex : value.polyline.vertices) {
                checksum += vertex.position.x + vertex.position.y + vertex.bulge;
            }
            return Observation{value.outcome == geom::OffsetOutcome::done && value.simple &&
                                   value.origins.size() == size,
                               checksum};
        };
        measure("offsetPolyline " + std::to_string(size),
                "ADR 0012.11: command time (no numeric limit)", 1, kLargeRepetitions, operation,
                report);
    }
}

void measureBoundary(Generator& generator, Report& report) {
    const auto policy = solid_space::robust::TolerancePolicy::forUnit(0.001);
    if (!policy.hasValue()) {
        report.valid = false;
        return;
    }
    std::vector<geom::Curve> drawing;
    const double originX = generator.coordinate();
    const double originY = generator.coordinate();
    // 24 x 20 rounded slots, each with an inner circular hole: 2400 curves.
    // Horizontal segments join semicircular ends tangentially. Arc endpoint
    // rounding is within the public tolerance, as in a technical drawing.
    for (std::size_t row = 0; row < 20; ++row) {
        for (std::size_t column = 0; column < 24; ++column) {
            const double x = originX + 40.0 * static_cast<double>(column);
            const double y = originY + 40.0 * static_cast<double>(row);
            drawing.emplace_back(geom::Segment{{x, y - 5.0}, {x + 10.0, y - 5.0}});
            drawing.emplace_back(
                geom::Arc{{x + 10.0, y}, 5.0, -geom::kQuarterTurn, 2.0 * geom::kQuarterTurn});
            drawing.emplace_back(geom::Segment{{x + 10.0, y + 5.0}, {x, y + 5.0}});
            drawing.emplace_back(
                geom::Arc{{x, y}, 5.0, geom::kQuarterTurn, 2.0 * geom::kQuarterTurn});
            drawing.emplace_back(geom::Circle{{x + 5.0, y}, 1.0});
        }
    }
    const std::array<geom::Point2, 2> picks{{{originX + 2.0, originY}, {originX - 20.0, originY}}};
    constexpr std::array<std::string_view, 2> kNames{"enclosed", "notEnclosed"};
    for (std::size_t index = 0; index < picks.size(); ++index) {
        const auto operation = [&drawing, &policy, &picks, index](std::size_t) {
            const auto result = geom::detectBoundary(picks.at(index), drawing, policy.value());
            if (!result.hasValue()) {
                return Observation{};
            }
            const auto& value = result.value();
            const auto expected =
                index == 0 ? geom::BoundaryOutcome::enclosed : geom::BoundaryOutcome::notEnclosed;
            auto checksum = static_cast<double>(value.openEnds.size());
            for (const auto& loop : value.loops) {
                checksum += static_cast<double>(loop.origins.size() + loop.depth);
            }
            return Observation{value.skipped.empty() && value.outcome == expected &&
                                   (index != 0 || !value.loops.empty()),
                               checksum};
        };
        measure("detectBoundary 2400 " + std::string(kNames.at(index)),
                "ADR 0014.9: a few tens of ms", 1, kLargeRepetitions, operation, report);
    }
}

[[nodiscard]] geom::Polyline square(double x, double y, double side, std::size_t divisions) {
    geom::Polyline loop;
    loop.closed = true;
    for (std::size_t edge = 0; edge < 4; ++edge) {
        for (std::size_t index = 0; index < divisions; ++index) {
            const double step = side * static_cast<double>(index) / static_cast<double>(divisions);
            const std::array<geom::Point2, 4> points{{{x + step, y},
                                                      {x + side, y + step},
                                                      {x + side - step, y + side},
                                                      {x, y + side - step}}};
            loop.vertices.push_back({points.at(edge), 0.0});
        }
    }
    return loop;
}

void measureClipping(Generator& generator, Report& report) {
    const double x = generator.coordinate();
    const double y = generator.coordinate();
    std::vector<geom::Loop> region{square(x, y, 256.0, 64)};
    // 256 outer pieces and 16 circular islands; hosts are whole parallel lines.
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            region.emplace_back(geom::Circle{{x + 32.0 + 64.0 * static_cast<double>(column),
                                              y + 32.0 + 64.0 * static_cast<double>(row)},
                                             12.0});
        }
    }
    std::vector<geom::Curve> hosts;
    for (std::size_t index = 0; index < 256; ++index) {
        const double height = y + static_cast<double>(index) + 0.5;
        hosts.emplace_back(geom::Segment{{x - 1.0, height}, {x + 1.0, height}});
    }
    const auto operation = [&hosts, &region](std::size_t) {
        const auto value = geom::clipToRegion(hosts, geom::Extension::extended, region);
        bool valid = value.skippedHosts.empty() && value.skippedLoops.empty() &&
                     value.hosts.size() == hosts.size();
        double checksum = 0.0;
        for (const auto& host : value.hosts) {
            valid = valid && !host.inside.empty();
            checksum += static_cast<double>(host.inside.size() + host.boundary.size());
            for (const auto& stretch : host.inside) {
                checksum += stretch.from.point.x + stretch.to.point.x;
            }
        }
        return Observation{valid, checksum};
    };
    measure("clipToRegion 256 lines / 272 pieces", "ADR 0015.9: a few ms/batch", 1,
            kLargeRepetitions, operation, report);
}

void measureNesting(Generator& generator, Report& report) {
    const geom::Point2 center{generator.coordinate(), generator.coordinate()};
    constexpr std::array<std::size_t, 3> kSizes{100, 200, 300};
    for (const auto size : kSizes) {
        std::vector<geom::Loop> contours;
        for (std::size_t index = 0; index < size; ++index) {
            contours.emplace_back(geom::Circle{center, 2.0 + static_cast<double>(index)});
        }
        const auto operation = [&contours](std::size_t) {
            const auto value = geom::nestContours(contours);
            bool valid = value.skipped.empty() && value.conflicts.empty() &&
                         value.depth.size() == contours.size();
            double checksum = 0.0;
            for (const auto& depth : value.depth) {
                valid = valid && depth.has_value();
                checksum += static_cast<double>(depth.value_or(0));
            }
            return Observation{valid, checksum};
        };
        measure("nestContours " + std::to_string(size), "report scaling (no numeric limit)", 1,
                kLargeRepetitions, operation, report);
    }
}

[[nodiscard]] Observation observeNearest(const geom::Result<geom::EllipseNearest>& result) {
    if (!result.hasValue()) {
        return {};
    }
    const auto& value = result.value();
    return {std::isfinite(value.distance) && value.distance >= 0.0,
            value.point.x + value.point.y + value.distance};
}

void measureEllipses(Generator& generator, Report& report) {
    constexpr std::array<std::string_view, 5> kNames{"segment", "arc", "circle", "bulge",
                                                     "straight-bulge"};
    const double x = generator.coordinate();
    const double y = generator.coordinate();
    const geom::Ellipse ellipse{{x, y}, {10.0, 0.0}, 0.5};
    const auto ordinary = curves(x, y, true);
    constexpr double kDelta = 0x1p-40;
    // The ellipse touches y=5 and the circle centred at (0,10), radius 5.
    // Move each carrier inward by a dyadic amount, retaining two nearby cuts.
    const double height = y + 5.0 - kDelta;
    const double centerY = y + 10.0 - kDelta;
    const std::array<geom::Curve, 5> near{
        {geom::Segment{{x - 20.0, height}, {x + 20.0, height}},
         geom::Arc{{x, centerY}, 5.0, 0.0, geom::kThreeQuarterTurn},
         geom::Circle{{x, centerY}, 5.0},
         geom::PolylineSegment{{x - 5.0, centerY}, {x + 5.0, centerY}, 1.0},
         geom::PolylineSegment{{x - 20.0, height}, {x + 20.0, height}, 0.0}}};
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        for (const bool nearTangent : {false, true}) {
            const auto& curve = (nearTangent ? near : ordinary).at(index);
            const auto operation = [&ellipse, &curve](std::size_t) {
                return observe(
                    geom::intersect(ellipse, geom::Extension::none, curve, geom::Extension::none));
            };
            measure("ellipse/" + std::string(kNames.at(index)) +
                        (nearTangent ? " near tangent" : " ordinary"),
                    nearTangent ? "ADR 0017.9: exact path (no numeric limit)"
                                : "ADR 0017.9: a few tens of us",
                    1, kPairRepetitions, operation, report);
        }
    }
    for (const bool nearTangent : {false, true}) {
        const geom::Ellipse other = nearTangent ? geom::Ellipse{{x, centerY}, {10.0, 0.0}, 0.5}
                                                : geom::Ellipse{{x, y}, {0.0, 10.0}, 0.5};
        const auto operation = [&ellipse, &other](std::size_t) {
            return observe(
                geom::intersect(ellipse, geom::Extension::none, other, geom::Extension::none));
        };
        measure(nearTangent ? "ellipse/ellipse near tangent" : "ellipse/ellipse ordinary",
                nearTangent ? "ADR 0017.9: exact path (no numeric limit)"
                            : "ADR 0017.9: a few tens of us",
                1, kPairRepetitions, operation, report);
    }
    // For axes (10,5), the cusp of the evolute on +x is (7.5,0).
    for (const bool nearEvolute : {false, true}) {
        const geom::Point2 point =
            nearEvolute ? geom::Point2{x + 7.5, y + kDelta} : geom::Point2{x + 20.0, y + 12.0};
        const auto operation = [&ellipse, point](std::size_t) {
            return observeNearest(geom::nearest(ellipse, point));
        };
        measure(nearEvolute ? "ellipse nearest near evolute" : "ellipse nearest far",
                "ADR 0017.9: a few tens of us (nondegenerate)", 1, kPairRepetitions, operation,
                report);
    }
    std::vector<geom::Ellipse> entities;
    for (std::size_t index = 0; index < kSnapPairs; ++index) {
        entities.push_back({{generator.coordinate(), generator.coordinate()}, {10.0, 0.0}, 0.5});
    }
    const auto snap = [&entities](std::size_t) {
        Observation batch{true, 0.0};
        const geom::Curve line = geom::Segment{{-20.0, 24.0}, {90.0, 24.0}};
        for (const auto& entity : entities) {
            const auto nearest = observeNearest(geom::nearest(entity, {30.0, 25.0}));
            const auto intersection = observe(
                geom::intersect(entity, geom::Extension::none, line, geom::Extension::none));
            batch.valid = batch.valid && nearest.valid && intersection.valid;
            batch.checksum += nearest.checksum + intersection.checksum;
        }
        return batch;
    };
    measure("ellipse snap: 32 entities (nearest + intersect)", "ADR 0017.9: < 1000 us/batch", 1,
            kPairRepetitions, snap, report, true);
}

void measureElementary(Generator& generator, Report& report) {
    namespace robust = solid_space::robust;
    std::array<double, kSamples> data{};
    for (auto& value : data) {
        value = generator.coordinate() / 128.0;
    }
    const auto tangent = [&data](std::size_t sample) {
        const double value = robust::tan(data.at(sample));
        return Observation{std::isfinite(value), value};
    };
    const auto arccosine = [&data](std::size_t sample) {
        const double value = robust::acos(data.at(sample));
        return Observation{std::isfinite(value), value};
    };
    const auto length = [&data](std::size_t sample) {
        const double value = robust::hypot(data.at(sample), 0.75);
        return Observation{std::isfinite(value), value};
    };
    measure("tan", "report only (no numeric limit)", data.size(), kPairRepetitions, tangent,
            report);
    measure("acos", "report only (no numeric limit)", data.size(), kPairRepetitions, arccosine,
            report);
    measure("hypot", "report only (no numeric limit)", data.size(), kPairRepetitions, length,
            report);
}

int run() {
    Generator generator;
    Report report;
    std::cout << "Performance bench; fixed seed " << std::showbase << std::hex << kSeed << std::dec
              << std::noshowbase << "; steady_clock; times in microseconds.\n\n"
              << "| Workload | Calls | Mean us/call | Worst us/call | ADR objective | Data |\n"
              << "|---|---:|---:|---:|---|---|\n"
              << std::fixed << std::setprecision(3);
    measurePairs(generator, report);
    measureNearDegeneracies(report);
    measureSnap(generator, report);
    measureAlong(report);
    measureTangents(generator, report);
    measureOffsets(report);
    measureBoundary(generator, report);
    measureClipping(generator, report);
    measureNesting(generator, report);
    measureEllipses(generator, report);
    measureElementary(generator, report);
    std::cout << "\nChecksum: " << report.checksum << "\nSnap objective: "
              << (report.snapGoalMet ? "met" : "EXCEEDED (report; no automatic failure)") << '\n';
    if (!report.valid) {
        std::cerr << "Invalid workload result: "
                  << "do not interpret its timings as successful geometry.\n";
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    try {
        return run();
    } catch (...) {
        return 2;
    }
}
