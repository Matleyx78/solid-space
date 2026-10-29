// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0009.

#include <cstdint>
#include <string>

namespace solid_space::robust {

/// Why an operation of robust gave no value. The values are stable: new codes
/// are added at the end, existing ones never change meaning.
enum class DiagnosisCode : std::uint8_t {
    invalidValue,  ///< a value is NaN, infinite or outside its allowed range
};

/// The diagnosis of robust (ADR 0003, point 5): a stable code for programs and
/// a message for people.
struct Diagnosis {
    DiagnosisCode code = DiagnosisCode::invalidValue;
    std::string message;
};

}  // namespace solid_space::robust
