// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/geom2d/tangency.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <algorithm>
#include <array>
#include <chrono>
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

[[nodiscard]] Observation observe(const geom::Result<geom::PairIntersections>& result) {
    if (!result.hasValue()) {
        return {};
    }
    const auto& value = result.value();
    double checksum = static_cast<double>(value.points.size() + value.overlaps.size() +
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
                data.emplace_back(curves(x, y, false)[first], curves(x, y, true)[second]);
            }
            const std::string name =
                "intersect " + std::string(kNames[first]) + "/" + std::string(kNames[second]);
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
    constexpr double kDelta = 0x1p-40;
    const geom::Curve circle = geom::Circle{{0.0, 0.0}, 5.0};
    const std::array<std::pair<std::string_view, Pair>, 6> data{{
        {"near tangent line/circle inside",
         {geom::Segment{{-8.0, 5.0 - kDelta}, {8.0, 5.0 - kDelta}}, circle}},
        {"near tangent line/circle outside",
         {geom::Segment{{-8.0, 5.0 + kDelta}, {8.0, 5.0 + kDelta}}, circle}},
        {"near tangent circle/circle inside", {circle, geom::Circle{{10.0 - kDelta, 0.0}, 5.0}}},
        {"near tangent circle/circle outside", {circle, geom::Circle{{10.0 + kDelta, 0.0}, 5.0}}},
        {"near coincident circles", {circle, geom::Circle{{kDelta, 0.0}, 5.0}}},
        {"near coincident lines",
         {geom::Segment{{-8.0, 0.0}, {8.0, 0.0}}, geom::Segment{{-8.0, -kDelta}, {8.0, kDelta}}}},
    }};
    for (const auto& [name, pair] : data) {
        const auto operation = [&pair](std::size_t) {
            constexpr auto kExtension = geom::Extension::none;
            const auto result = geom::intersect(pair.first, kExtension, pair.second, kExtension);
            return observe(result);
        };
        measure(name, "ADR 0010.8: bounded exact path (no numeric limit)", 1, kPairRepetitions,
                operation, report);
    }
}

void measureSnap(Generator& generator, Report& report) {
    std::vector<Pair> data;
    for (std::size_t index = 0; index < kSnapPairs; ++index) {
        const double x = generator.coordinate();
        const double y = generator.coordinate();
        data.emplace_back(curves(x, y, false)[index % 5], curves(x, y, true)[(index + 1) % 5]);
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
            const double x = static_cast<double>(index);
            cutters.emplace_back(geom::Segment{{x, -1.0}, {x, 1.0}});
        }
        const auto operation = [&host, &cutters](std::size_t) {
            const auto result =
                geom::intersectAlong(host, geom::Extension::none, cutters, geom::Extension::none);
            if (!result.hasValue()) {
                return Observation{};
            }
            const auto& value = result.value();
            double checksum = static_cast<double>(value.cuts.size());
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
        const auto& curve = data[kCircular[index]];
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
        measure("tangentsFrom " + std::string(kNames[index]),
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

}  // namespace

int main() {
    Generator generator;
    Report report;
    std::cout << "Performance bench; fixed seed " << kSeed
              << "; steady_clock; times in microseconds.\n\n"
              << "| Workload | Calls | Mean us/call | Worst us/call | ADR objective | Data |\n"
              << "|---|---:|---:|---:|---|---|\n"
              << std::fixed << std::setprecision(3);
    measurePairs(generator, report);
    measureNearDegeneracies(report);
    measureSnap(generator, report);
    measureAlong(report);
    measureTangents(generator, report);
    measureOffsets(report);
    std::cout << "\nChecksum: " << report.checksum << "\nSnap objective: "
              << (report.snapGoalMet ? "met" : "EXCEEDED (report; no automatic failure)") << '\n';
    if (!report.valid) {
        std::cerr << "Invalid workload result: "
                  << "do not interpret its timings as successful geometry.\n";
        return 1;
    }
    return 0;
}
