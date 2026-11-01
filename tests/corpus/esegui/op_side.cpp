// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of side() (stage 0.2, package P9; references [0.2-R9]).

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/side.hpp"

namespace solid_space::corpus {
namespace {

Value sideOperation(const Value& input) {
    const auto result =
        geom2d::side(readCurve(field(input, "curva")), readPoint(field(input, "punto")));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({{"lato", writeName(result.value())}});
}

const Registration kSide("side", sideOperation);

}  // namespace
}  // namespace solid_space::corpus
