// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "plane_data.hpp"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace solid_space::corpus {
namespace {

[[noreturn]] void malformed(std::string_view path, std::string_view reason) {
    throw std::runtime_error(std::string(path) + ": " + std::string(reason));
}

const Value& member(const Value& group, std::string_view key, std::string_view path) {
    const auto fullPath = std::string(path) + "." + std::string(key);
    if (group.kind() != Value::Kind::group) {
        malformed(fullPath, "expected a group");
    }
    const auto* value = group.find(key);
    if (value == nullptr) {
        malformed(fullPath, "missing key");
    }
    return *value;
}

const Value::List& listOf(const Value& value, std::string_view path) {
    if (value.kind() != Value::Kind::list) {
        malformed(path, "expected a list");
    }
    return value.asList();
}

std::string_view textOf(const Value& value, std::string_view path) {
    if (value.kind() != Value::Kind::text && value.kind() != Value::Kind::name) {
        malformed(path, "expected text");
    }
    return value.asString();
}

}  // namespace

const Value& field(const Value& group, std::string_view key) {
    return member(group, key, "ingresso");
}

double readNumber(const Value& value, std::string_view path) {
    if (value.kind() == Value::Kind::number) {
        return value.asNumber();
    }
    const auto text = textOf(value, path);
    if (text == "nan") {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (text == "inf") {
        return std::numeric_limits<double>::infinity();
    }
    if (text == "-inf") {
        return -std::numeric_limits<double>::infinity();
    }
    auto digits = text;
    const bool negative = digits.starts_with("-");
    if (negative || digits.starts_with("+")) {
        digits.remove_prefix(1);
    }
    if (!digits.starts_with("0x") && !digits.starts_with("0X")) {
        malformed(path, "expected a number or hexadecimal double");
    }
    digits.remove_prefix(2);
    if (digits.find_first_of("pP") == std::string_view::npos) {
        malformed(path, "hexadecimal double requires an exponent");
    }
    double result = 0.0;
    const auto* end = digits.data() + digits.size();
    const auto parsed = std::from_chars(digits.data(), end, result, std::chars_format::hex);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
        malformed(path, "invalid hexadecimal double");
    }
    return negative ? -result : result;
}

robust::Point2 readPoint(const Value& value, std::string_view path) {
    const auto& coordinates = listOf(value, path);
    if (coordinates.size() != 2) {
        malformed(path, "expected two coordinates");
    }
    return {readNumber(coordinates[0], std::string(path) + "[0]"),
            readNumber(coordinates[1], std::string(path) + "[1]")};
}

geom2d::Curve readCurve(const Value& value, std::string_view path) {
    if (value.kind() != Value::Kind::group || value.asGroup().size() != 1) {
        malformed(path, "expected exactly one curve key");
    }
    const auto& [kind, data] = value.asGroup().front();
    const auto prefix = std::string(path) + "." + kind;
    const auto point = [&](std::string_view key) {
        return readPoint(member(data, key, prefix), prefix + "." + std::string(key));
    };
    const auto number = [&](std::string_view key) {
        return readNumber(member(data, key, prefix), prefix + "." + std::string(key));
    };
    if (kind == "segmento") {
        return geom2d::Segment{point("inizio"), point("fine")};
    }
    if (kind == "arco") {
        return geom2d::Arc{point("centro"), number("raggio"), number("angolo_iniziale"),
                           number("ampiezza")};
    }
    if (kind == "cerchio") {
        return geom2d::Circle{point("centro"), number("raggio")};
    }
    if (kind == "pezzo") {
        return geom2d::PolylineSegment{point("inizio"), point("fine"), number("bulge")};
    }
    malformed(prefix, "unknown curve key");
}

geom2d::Polyline readPolyline(const Value& value, std::string_view path) {
    geom2d::Polyline result;
    const auto& closed = member(value, "chiusa", path);
    if (closed.kind() != Value::Kind::boolean) {
        malformed(std::string(path) + ".chiusa", "expected a boolean");
    }
    result.closed = closed.asBoolean();
    const auto& vertices = listOf(member(value, "vertici", path), std::string(path) + ".vertici");
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto prefix = std::string(path) + ".vertici[" + std::to_string(i) + "]";
        const auto& coordinates = listOf(vertices[i], prefix);
        if (coordinates.size() != 3) {
            malformed(prefix, "expected x, y and bulge");
        }
        const auto x = readNumber(coordinates[0], prefix + "[0]");
        const auto y = readNumber(coordinates[1], prefix + "[1]");
        const robust::Point2 point = {x, y};
        result.vertices.push_back({point, readNumber(coordinates[2], prefix + "[2]")});
    }
    return result;
}

geom2d::Extension readExtension(const Value& value, std::string_view path) {
    const auto text = textOf(value, path);
    if (text == "none") {
        return geom2d::Extension::none;
    }
    if (text == "extended") {
        return geom2d::Extension::extended;
    }
    malformed(path, "expected none or extended");
}

Value writePoint(robust::Point2 point) {
    return Value::list({Value::number(point.x), Value::number(point.y)});
}

Value writeCurve(const geom2d::Curve& curve) {
    return std::visit(
        [](const auto& data) {
            using T = std::decay_t<decltype(data)>;
            Value::Group fields;
            std::string kind;
            if constexpr (std::is_same_v<T, geom2d::Segment> ||
                          std::is_same_v<T, geom2d::PolylineSegment>) {
                fields.emplace_back("inizio", writePoint(data.start));
                fields.emplace_back("fine", writePoint(data.end));
                if constexpr (std::is_same_v<T, geom2d::PolylineSegment>) {
                    kind = "pezzo";
                    fields.emplace_back("bulge", Value::number(data.bulge));
                } else {
                    kind = "segmento";
                }
            } else {
                fields.emplace_back("centro", writePoint(data.center));
                fields.emplace_back("raggio", Value::number(data.radius));
                if constexpr (std::is_same_v<T, geom2d::Arc>) {
                    kind = "arco";
                    fields.emplace_back("angolo_iniziale", Value::number(data.startAngle));
                    fields.emplace_back("ampiezza", Value::number(data.sweep));
                } else {
                    kind = "cerchio";
                }
            }
            return Value::group({{kind, Value::group(std::move(fields))}});
        },
        curve);
}

Value writePosition(const geom2d::CurvePosition& position) {
    return Value::group({{"posizione", writeName(position.location)},
                         {"stazione", Value::number(position.station)}});
}

Value writeDiagnosis(const robust::Diagnosis& diagnosis) {
    return Value::group({{"diagnosi", Value::group({{"codice", writeName(diagnosis.code)}})}});
}

Value writeDiagnosis(const geom2d::Diagnosis& diagnosis) {
    auto fields = Value::group({{"codice", writeName(diagnosis.code)},
                                {"dato", Value::exact(std::to_string(diagnosis.input))}});
    return Value::group({{"diagnosi", std::move(fields)}});
}

Value writeName(geom2d::Location value) {
    switch (value) {
        case geom2d::Location::beforeStart:
            return Value::name("beforeStart");
        case geom2d::Location::atStart:
            return Value::name("atStart");
        case geom2d::Location::inside:
            return Value::name("inside");
        case geom2d::Location::atEnd:
            return Value::name("atEnd");
        case geom2d::Location::afterEnd:
            return Value::name("afterEnd");
    }
    malformed("Location", "unknown enumerator");
}

Value writeName(geom2d::Contact value) {
    switch (value) {
        case geom2d::Contact::crossing:
            return Value::name("crossing");
        case geom2d::Contact::touching:
            return Value::name("touching");
        case geom2d::Contact::sameCurve:
            return Value::name("sameCurve");
    }
    malformed("Contact", "unknown enumerator");
}

Value writeName(geom2d::Side value) {
    switch (value) {
        case geom2d::Side::left:
            return Value::name("left");
        case geom2d::Side::on:
            return Value::name("on");
        case geom2d::Side::right:
            return Value::name("right");
    }
    malformed("Side", "unknown enumerator");
}

Value writeName(geom2d::DiagnosisCode value) {
    switch (value) {
        case geom2d::DiagnosisCode::invalidValue:
            return Value::name("invalidValue");
        case geom2d::DiagnosisCode::outOfDomain:
            return Value::name("outOfDomain");
        case geom2d::DiagnosisCode::degenerateCurve:
            return Value::name("degenerateCurve");
        case geom2d::DiagnosisCode::notCircular:
            return Value::name("notCircular");
    }
    malformed("DiagnosisCode", "unknown enumerator");
}

Value writeName(robust::DiagnosisCode value) {
    switch (value) {
        case robust::DiagnosisCode::invalidValue:
            return Value::name("invalidValue");
    }
    malformed("DiagnosisCode", "unknown enumerator");
}

}  // namespace solid_space::corpus
