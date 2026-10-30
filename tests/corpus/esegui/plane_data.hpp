// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/side.hpp"
#include "solid_space/robust/diagnosis.hpp"
#include "value.hpp"

namespace solid_space::corpus {

// Every reader reports malformed data with a runtime_error containing its path.
[[nodiscard]] const Value& field(const Value& group, std::string_view key);
[[nodiscard]] double readNumber(const Value& value, std::string_view path = "numero");
[[nodiscard]] robust::Point2 readPoint(const Value& value, std::string_view path = "punto");
[[nodiscard]] geom2d::Curve readCurve(const Value& value, std::string_view path = "curva");
[[nodiscard]] geom2d::Polyline readPolyline(const Value& value,
                                            std::string_view path = "polilinea");
[[nodiscard]] geom2d::Extension readExtension(const Value& value,
                                           std::string_view path = "prolungamento");

[[nodiscard]] Value writePoint(robust::Point2 point);
[[nodiscard]] Value writeCurve(const geom2d::Curve& curve);
[[nodiscard]] Value writePosition(const geom2d::CurvePosition& position);
// Includes the outer "diagnosi" key, ready as a whole operation result.
[[nodiscard]] Value writeDiagnosis(const robust::Diagnosis& diagnosis);
[[nodiscard]] Value writeDiagnosis(const geom2d::Diagnosis& diagnosis);
[[nodiscard]] Value writeName(geom2d::Location value);
[[nodiscard]] Value writeName(geom2d::Contact value);
[[nodiscard]] Value writeName(geom2d::Side value);
[[nodiscard]] Value writeName(geom2d::DiagnosisCode value);
[[nodiscard]] Value writeName(robust::DiagnosisCode value);

}  // namespace solid_space::corpus
