// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The draft contract of robust (ADR 0007, point 15; ADR 0009 and ADR 0010):
// the public headers compile, and the declared functions have the declared
// signatures. Nothing declared is called, because nothing is implemented yet;
// Result, the only definition, is tested at run time.

#include "solid_space/robust/diagnosis.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/predicates.hpp"
#include "solid_space/robust/result.hpp"
#include "solid_space/robust/tolerance.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace robust = solid_space::robust;

namespace {

// Signatures of the declared functions, checked without odr-using them.
static_assert(
    std::is_same_v<decltype(&robust::orient2d),
                   robust::Sign (*)(robust::Point2, robust::Point2, robust::Point2) noexcept>);
static_assert(std::is_same_v<decltype(&robust::sinCos), robust::SinCos (*)(double) noexcept>);
static_assert(std::is_same_v<decltype(&robust::atan2), double (*)(double, double) noexcept>);
static_assert(
    std::is_same_v<decltype(&robust::TolerancePolicy::forUnit),
                   robust::Result<robust::TolerancePolicy, robust::Diagnosis> (*)(double)>);
static_assert(std::is_same_v<decltype(&robust::TolerancePolicy::epsilon),
                             double (robust::TolerancePolicy::*)(double) const noexcept>);
static_assert(std::is_same_v<decltype(&robust::TolerancePolicy::coincident),
                             bool (robust::TolerancePolicy::*)(robust::Point2, robust::Point2)
                                 const noexcept>);
static_assert(std::is_same_v<decltype(&robust::TolerancePolicy::negligible),
                             bool (robust::TolerancePolicy::*)(double, double) const noexcept>);

// The rule exists only through forUnit, which checks the unit.
static_assert(!std::is_default_constructible_v<robust::TolerancePolicy>);

// Plain data and stable codes.
static_assert(std::is_trivially_copyable_v<robust::Point2>);
static_assert(std::is_aggregate_v<robust::Point2>);
static_assert(static_cast<std::int8_t>(robust::Sign::negative) == -1);
static_assert(static_cast<std::int8_t>(robust::Sign::zero) == 0);
static_assert(static_cast<std::int8_t>(robust::Sign::positive) == 1);

// The constants of the contract.
static_assert(robust::kMaxCoordinate == 1125899906842624.0);  // 2^50
static_assert(robust::kMaxTrigArgument == 1048576.0);         // 2^20
static_assert(robust::kRelativeTolerance == 0x1p-42);
static_assert(robust::kPhysicalFloorMeters == 1e-9);

using IntResult = robust::Result<int, robust::Diagnosis>;

}  // namespace

TEST(RobustContract, ResultHoldsAValue) {
    const IntResult result(7);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 7);
}

TEST(RobustContract, ResultHoldsADiagnosis) {
    const IntResult result(robust::Diagnosis{robust::DiagnosisCode::invalidValue, "unit is zero"});
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, robust::DiagnosisCode::invalidValue);
    EXPECT_EQ(result.diagnosis().message, "unit is zero");
}

TEST(RobustContract, ResultValueMovesOut) {
    robust::Result<std::string, robust::Diagnosis> result(std::string("moved"));
    const std::string value = std::move(result).value();
    EXPECT_EQ(value, "moved");
}
