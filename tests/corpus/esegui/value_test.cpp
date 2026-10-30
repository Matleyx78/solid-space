// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "value.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace solid_space::corpus {
namespace {

TEST(CorpusValue, ComparesScalarsAndReportsPaths) {
    const std::array expected = {Value::boolean(true), Value::number(2), Value::exact("1/2", 0.5),
                                 Value::approximate(1, 0.125), Value::name("left"),
                                 Value::text("text"), Value::exact("1/3")};
    const std::array equal = {Value::boolean(true), Value::number(2), Value::number(0.5),
                              Value::number(1.125), Value::name("left"), Value::text("text"),
                              Value::exact("1/3")};
    const std::array different = {Value::boolean(false), Value::number(3), Value::number(0.25),
                                  Value::number(1.25), Value::name("right"), Value::text("other"),
                                  Value::number(1.0 / 3.0)};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_TRUE(compare(expected[i], equal[i]).empty()) << i;
        const auto differences = compare(Value::group({{"result", expected[i]}}),
                                         Value::group({{"result", different[i]}}));
        ASSERT_EQ(differences.size(), 1U) << i;
        EXPECT_TRUE(differences.front().starts_with("result:")) << differences.front();
    }
    EXPECT_TRUE(compare(Value::exact("0", 0.0), Value::number(-0.0)).empty());
    EXPECT_FALSE(compare(Value::exact("1/2", 0.5), Value::exact("2/4", 0.5)).empty());
    EXPECT_FALSE(compare(Value::name("left"), Value::text("left")).empty());
    EXPECT_FALSE(compare(Value::approximate(1, 0.1),
                         Value::number(std::numeric_limits<double>::infinity())).empty());
    EXPECT_FALSE(compare(Value::approximate(1, 0.1),
                         Value::number(std::numeric_limits<double>::quiet_NaN())).empty());
}

TEST(CorpusValue, ComparesNestedListsAndPartialGroups) {
    const auto expected = Value::group({{"punti", Value::list({Value::boolean(true),
        Value::group({{"punto", Value::list({Value::integer(2), Value::integer(3)})}})})}});
    EXPECT_TRUE(compare(expected, expected).empty());
    const auto actual = Value::group({{"extra", Value::text("ignored")},
        {"punti", Value::list({Value::boolean(true), Value::group({
            {"punto", Value::list({Value::number(9), Value::number(3)})}})})}});
    const auto differences = compare(expected, actual);
    ASSERT_EQ(differences.size(), 1U);
    EXPECT_TRUE(differences[0].starts_with("punti[1].punto[0]:"));
    EXPECT_FALSE(compare(expected, Value::group({})).empty());
    EXPECT_FALSE(compare(Value::list({}), Value::list({Value::integer(1)})).empty());
    EXPECT_FALSE(compare(Value::list({}), Value::boolean(false)).empty());
}

TEST(CorpusValue, IntegerRepresentabilityDoesNotRoundExpectedIntegers) {
    EXPECT_TRUE(compare(Value::integer(3), Value::number(3)).empty());
    EXPECT_FALSE(Value::integer(9007199254740993LL).representable().has_value());
    EXPECT_TRUE(Value::integer(9007199254740994LL).representable().has_value());
    EXPECT_TRUE(Value::integer(std::numeric_limits<std::int64_t>::min()).representable().has_value());
    EXPECT_FALSE(Value::integer(std::numeric_limits<std::int64_t>::max()).representable().has_value());
}

TEST(CorpusValue, CanonicalNumbersIncludeSpecialValuesAndEscapeText) {
    EXPECT_EQ(canonicalText(Value::number(0.5)), "0x1.0000000000000p-1");
    EXPECT_EQ(canonicalText(Value::number(-0.0)), "-0x0.0000000000000p+0");
    EXPECT_EQ(canonicalText(Value::number(std::numeric_limits<double>::denorm_min())),
              "0x0.0000000000001p-1022");
    EXPECT_EQ(canonicalText(Value::number(std::numeric_limits<double>::infinity())), "inf");
    EXPECT_EQ(canonicalText(Value::number(-std::numeric_limits<double>::infinity())), "-inf");
    EXPECT_EQ(canonicalText(Value::number(std::numeric_limits<double>::quiet_NaN())), "nan");
    EXPECT_EQ(canonicalText(Value::group({{"a", Value::list({Value::boolean(true),
               Value::integer(3), Value::name("left"), Value::text("\n\"\\")})}})),
              "{\"a\":[true,exact(\"3\"),name(\"left\"),\"\\x0a\\x22\\x5c\"]}");
    EXPECT_EQ(canonicalText(Value::approximate(1, 0.5)),
              "approx(0x1.0000000000000p+0,0x1.0000000000000p-1)");
}

TEST(CorpusPlaneData, ReadsNumbersAndNamesWithoutGeometryFunctions) {
    EXPECT_EQ(readNumber(Value::text("0x1.0000000000001p+0")), 0x1.0000000000001p+0);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(readNumber(Value::text("-0x0p+0"))),
              std::bit_cast<std::uint64_t>(-0.0));
    EXPECT_EQ(readNumber(Value::text("-0x1p-1074")), -std::numeric_limits<double>::denorm_min());
    EXPECT_EQ(readNumber(Value::text("inf")), std::numeric_limits<double>::infinity());
    EXPECT_EQ(readNumber(Value::text("-inf")), -std::numeric_limits<double>::infinity());
    const double nan = readNumber(Value::text("nan"));
    EXPECT_NE(nan, nan);
    EXPECT_EQ(readExtension(Value::text("extended")), geom2d::Extension::extended);
    EXPECT_EQ(readExtension(Value::text("none")), geom2d::Extension::none);
    EXPECT_THROW(static_cast<void>(readExtension(Value::text("bad"))), std::runtime_error);
    EXPECT_THROW(static_cast<void>(readNumber(Value::text("3.5"))), std::runtime_error);
    EXPECT_THROW(static_cast<void>(readNumber(Value::text("0x1p+0junk"))), std::runtime_error);
}

TEST(CorpusPlaneData, RoundTripsEveryCurveAndReadsPolyline) {
    const std::array<geom2d::Curve, 4> curves = {
        geom2d::Segment{{1, 2}, {3, 4}}, geom2d::Arc{{1, 2}, 3, 4, 5},
        geom2d::Circle{{1, 2}, 3}, geom2d::PolylineSegment{{1, 2}, {3, 4}, 0.5}};
    for (const auto& curve : curves) {
        const auto value = writeCurve(curve);
        EXPECT_TRUE(compare(value, writeCurve(readCurve(value))).empty());
    }
    const auto polyline = readPolyline(Value::group({
        {"vertici", Value::list({Value::list({Value::number(1), Value::number(2), Value::number(0.5)})})},
        {"chiusa", Value::boolean(true)}}));
    ASSERT_EQ(polyline.vertices.size(), 1U);
    EXPECT_EQ(polyline.vertices[0].position.x, 1);
    EXPECT_EQ(polyline.vertices[0].position.y, 2);
    EXPECT_EQ(polyline.vertices[0].bulge, 0.5);
    EXPECT_TRUE(polyline.closed);
}

TEST(CorpusPlaneData, ReportsMalformedDataWithKeyPaths) {
    try {
        static_cast<void>(readCurve(Value::group({{"cerchio", Value::group({
            {"centro", writePoint({0, 0})}, {"raggio", Value::text("oops")}})}})));
        FAIL() << "Malformed radius accepted";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("curva.cerchio.raggio"), std::string::npos);
    }
    EXPECT_THROW(static_cast<void>(readPoint(Value::list({}))), std::runtime_error);
    EXPECT_THROW(static_cast<void>(readCurve(Value::group({}))), std::runtime_error);
    EXPECT_THROW(static_cast<void>(readPolyline(Value::group({}))), std::runtime_error);
    EXPECT_THROW(static_cast<void>(field(Value::group({}), "missing")), std::runtime_error);
}

TEST(CorpusPlaneData, WritesPositionsDiagnosesAndEnumeratorNames) {
    const auto position = writePosition({geom2d::Location::atStart, 0});
    EXPECT_EQ(position.find("posizione")->asString(), "atStart");
    EXPECT_EQ(position.find("stazione")->asNumber(), 0);
    const auto diagnosis = writeDiagnosis(geom2d::Diagnosis{geom2d::DiagnosisCode::outOfDomain, 2, "x"});
    EXPECT_EQ(diagnosis.find("diagnosi")->find("codice")->asString(), "outOfDomain");
    EXPECT_EQ(diagnosis.find("diagnosi")->find("dato")->asString(), "2");
    const auto robustDiagnosis = writeDiagnosis(robust::Diagnosis{});
    EXPECT_EQ(robustDiagnosis.find("diagnosi")->find("dato"), nullptr);
    EXPECT_EQ(writeName(geom2d::Contact::sameCurve).asString(), "sameCurve");
    EXPECT_EQ(writeName(geom2d::Side::right).asString(), "right");
}

class CorpusResults : public testing::Test {
protected:
    void SetUp() override {
        // A unique directory also separates independently running ctest processes.
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() / ("ss-corpus-" + std::to_string(nonce));
        std::filesystem::create_directory(m_directory);
        m_file = m_directory / "results.txt";
    }

    void TearDown() override {
        std::filesystem::remove_all(m_directory);
    }

    static Case fixture(std::string id, int pending = 0, std::string operation = "fixture") {
        return {std::move(id), "robust", pending,
                Value::group({{"operazione", Value::text(std::move(operation))}}), Value::group({})};
    }

    static std::string contents(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    std::filesystem::path m_directory;
    std::filesystem::path m_file;
};

Value fixtureOperation(const Value&) {
    return Value::number(-0.0);
}

const Registration kFixtureOperation("fixture", fixtureOperation);

Value malformedOperation(const Value& input) {
    return Value::number(readNumber(field(input, "missing")));
}

const Registration kMalformedOperation("malformedFixture", malformedOperation);

TEST_F(CorpusResults, SortsRowsSkipsPendingAndIgnoresExpectedMismatch) {
    const std::array table = {fixture("z"), fixture("pending", 35, "unregistered"), fixture("a")};
    EXPECT_EQ(writeResults(table, m_file), 0);
    EXPECT_EQ(contents(m_file), "# casi: 2\nrobust/a -0x0.0000000000000p+0\n"
                                "robust/z -0x0.0000000000000p+0\n");
    EXPECT_FALSE(std::filesystem::exists(m_file.string() + ".tmp"));
    EXPECT_EQ(writeResults(table, m_file), 0);
}

TEST_F(CorpusResults, MissingOperationProducesErrorAndNoPublishedFile) {
    const std::array table = {fixture("missing", 0, "unregistered")};
    EXPECT_EQ(writeResults(table, m_file), 1);
    EXPECT_FALSE(std::filesystem::exists(m_file));
    EXPECT_NE(contents(m_file.string() + ".tmp").find("robust/missing ERRORE"), std::string::npos);
}

TEST_F(CorpusResults, MalformedInputProducesErrorAndPreservesPreviousCompleteFile) {
    EXPECT_EQ(writeResults({}, m_file), 0);
    const std::array table = {fixture("bad", 0, "malformedFixture")};
    EXPECT_EQ(writeResults(table, m_file), 1);
    EXPECT_EQ(contents(m_file), "# casi: 0\n");
    EXPECT_NE(contents(m_file.string() + ".tmp").find("ingresso.missing"), std::string::npos);
}

TEST_F(CorpusResults, RejectsOtherOptionsAndEmptyFileName) {
    const auto option = "--ss-results=" + m_file.string();
    const std::array<std::string_view, 2> arguments = {option, "--gtest_filter=*"};
    EXPECT_EQ(runResults({}, arguments), 2);
    const std::array<std::string_view, 1> empty = {"--ss-results="};
    EXPECT_EQ(runResults({}, empty), 2);
    EXPECT_FALSE(std::filesystem::exists(m_file));
    const std::array<std::string_view, 1> valid = {option};
    EXPECT_EQ(runResults({}, valid), 0);
    EXPECT_EQ(contents(m_file), "# casi: 0\n");
}

TEST(CorpusRegistration, DuplicateOperationIsReported) {
    // Run in a child process so the intentional duplicate cannot affect any
    // other test, regardless of GoogleTest ordering or ctest parallelism.
    EXPECT_EXIT({
        const Registration duplicate("fixture", fixtureOperation);
        std::_Exit(registrationErrors().empty() ? 1 : 0);
    }, testing::ExitedWithCode(0), "");
}

}  // namespace
}  // namespace solid_space::corpus
