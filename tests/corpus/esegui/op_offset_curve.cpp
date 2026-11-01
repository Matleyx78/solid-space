// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/offset.hpp"

namespace solid_space::corpus {
namespace {

Value offsetCurveOperation(const Value& input) {
    const auto result = geom2d::offsetCurve(readCurve(field(input, "curva")),
                                           readNumber(field(input, "distanza")));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    if (result.value().outcome == geom2d::OffsetOutcome::vanished) {
        return Value::group({{"esito", Value::name("vanished")}});
    }
    return Value::group({{"esito", Value::name("done")},
                         {"curva", writeCurve(result.value().curve)}});
}

const Registration kOffsetCurve("offsetCurve", offsetCurveOperation);

}  // namespace
}  // namespace solid_space::corpus
