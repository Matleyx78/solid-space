// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Corpus operations of the curves of geom2d: pieces, magnitude, endpoints and
// length (stage 0.2, package P7; vocabulary in docs/tappe/0.2/P7-curve.md).

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/curves.hpp"

#include <utility>

namespace solid_space::corpus {
namespace {

Value piecesOperation(const Value& input) {
    const auto polyline = readPolyline(field(input, "polilinea"), "polilinea");
    Value::List result;
    for (const auto& piece : geom2d::pieces(polyline)) {
        result.push_back(Value::group({
            {"inizio", writePoint(piece.start)},
            {"fine", writePoint(piece.end)},
            {"bulge", Value::number(piece.bulge)},
        }));
    }
    return Value::group({{"pezzi", Value::list(std::move(result))}});
}

Value magnitudeOperation(const Value& input) {
    const auto result = geom2d::magnitude(readCurve(field(input, "curva"), "curva"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({{"grandezza", Value::number(result.value())}});
}

Value endpointsOperation(const Value& input) {
    const auto result = geom2d::endpoints(readCurve(field(input, "curva"), "curva"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({
        {"inizio", writePoint(result.value().start)},
        {"fine", writePoint(result.value().end)},
    });
}

Value lengthOperation(const Value& input) {
    const auto result = geom2d::length(readCurve(field(input, "curva"), "curva"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({{"lunghezza", Value::number(result.value())}});
}

const Registration kPieces("pieces", piecesOperation);
const Registration kMagnitude("magnitude", magnitudeOperation);
const Registration kEndpoints("endpoints", endpointsOperation);
const Registration kLength("length", lengthOperation);

}  // namespace
}  // namespace solid_space::corpus
