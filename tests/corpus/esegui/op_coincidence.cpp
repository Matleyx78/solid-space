// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/coincidence.hpp"

#include <stdexcept>
#include <string>
#include <variant>

namespace solid_space::corpus {
namespace {

robust::TolerancePolicy policyOf(const Value& input) {
    const auto policy = robust::TolerancePolicy::forUnit(
        readNumber(field(input, "unita_in_metri"), "unita_in_metri"));
    if (!policy.hasValue()) {
        throw std::runtime_error("unita_in_metri: invalid unit: " + policy.diagnosis().message);
    }
    return policy.value();
}

Value writeAnswer(const geom2d::Result<bool>& result) {
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({{"risposta", Value::boolean(result.value())}});
}

template <typename T>
T readAs(const Value& input, const char* key) {
    const auto curve = readCurve(field(input, key), key);
    if (const auto* value = std::get_if<T>(&curve)) {
        return *value;
    }
    throw std::runtime_error(std::string(key) + ": unexpected curve type");
}

Value degenerateOperation(const Value& input) {
    return writeAnswer(geom2d::isDegenerate(readCurve(field(input, "curva")), policyOf(input)));
}

Value straightOperation(const Value& input) {
    return writeAnswer(geom2d::isStraight(readAs<geom2d::PolylineSegment>(input, "curva"),
                                         policyOf(input)));
}

Value alignedOperation(const Value& input) {
    return writeAnswer(geom2d::areAligned(readAs<geom2d::Segment>(input, "prima"),
                                         readAs<geom2d::Segment>(input, "seconda"), policyOf(input)));
}

Value concentricOperation(const Value& input) {
    return writeAnswer(geom2d::areConcentric(readCurve(field(input, "prima")),
                                            readCurve(field(input, "seconda")), policyOf(input)));
}

Value sameCircleOperation(const Value& input) {
    return writeAnswer(geom2d::onSameCircle(readCurve(field(input, "prima")),
                                           readCurve(field(input, "seconda")), policyOf(input)));
}

const Registration kDegenerate("isDegenerate", degenerateOperation);
const Registration kStraight("isStraight", straightOperation);
const Registration kAligned("areAligned", alignedOperation);
const Registration kConcentric("areConcentric", concentricOperation);
const Registration kSameCircle("onSameCircle", sameCircleOperation);

}  // namespace
}  // namespace solid_space::corpus
