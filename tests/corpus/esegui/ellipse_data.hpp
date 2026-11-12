// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "plane_data.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <string>
#include <string_view>

namespace solid_space::corpus {

[[nodiscard]] inline geom2d::Ellipse readEllipse(const Value& value,
                                                 std::string_view path = "ellisse") {
    const std::string prefix = std::string(path) + ".";
    return {
        readPoint(field(value, "centro"), prefix + "centro"),
        readPoint(field(value, "asse_maggiore"), prefix + "asse_maggiore"),
        readNumber(field(value, "rapporto"), prefix + "rapporto"),
        readNumber(field(value, "parametro_iniziale"), prefix + "parametro_iniziale"),
        readNumber(field(value, "parametro_finale"), prefix + "parametro_finale"),
    };
}

[[nodiscard]] inline Value writeEllipseLocation(geom2d::Location location) {
    return writeName(location);
}

}  // namespace solid_space::corpus
