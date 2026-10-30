// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace solid_space::corpus {

int writeResults(std::span<const Case> table, const std::filesystem::path& file) {
    std::vector<const Case*> required;
    for (const auto& testCase : table) {
        if (testCase.pendingIssue == 0) {
            required.push_back(&testCase);
        }
    }
    std::ranges::sort(required, [](const Case* first, const Case* second) {
        return first->component + "/" + first->id < second->component + "/" + second->id;
    });
    auto temporary = file;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Cannot write " << temporary << '\n';
        return 1;
    }
    output << "# casi: " << required.size() << '\n';
    bool success = registrationErrors().empty();
    for (const auto* testCase : required) {
        output << testCase->component << '/' << testCase->id << ' ';
        try {
            const auto actual = execute(*testCase);
            output << canonicalText(actual) << '\n';
        } catch (const std::runtime_error& error) {
            const auto message = canonicalText(Value::text(error.what()));
            output << "ERRORE " << message << '\n';
            std::cerr << testCase->component << '/' << testCase->id << " ERRORE " << message
                      << '\n';
            success = false;
        }
    }
    output.close();
    if (!output || !success) {
        return 1;
    }
    std::error_code error;
    // rename is atomic on supported filesystems. Windows does not replace an
    // existing destination, so remove an old complete result first there.
#ifdef _WIN32
    std::filesystem::remove(file, error);
    if (error) {
        std::cerr << error.message() << '\n';
        return 1;
    }
#endif
    std::filesystem::rename(temporary, file, error);
    if (error) {
        std::cerr << error.message() << '\n';
        return 1;
    }
    return 0;
}

int runResults(std::span<const Case> table, std::span<const std::string_view> arguments) {
    constexpr std::string_view kPrefix = "--ss-results=";
    if (arguments.size() != 1 || !arguments.front().starts_with(kPrefix) ||
        arguments.front().size() == kPrefix.size()) {
        std::cerr << "Usage: solid_space_corpus_test --ss-results=<file> (no other options)\n";
        return 2;
    }
    return writeResults(table, std::string(arguments.front().substr(kPrefix.size())));
}

#ifndef SS_CORPUS_TESTING
namespace {

class CorpusTest : public testing::Test {
public:
    explicit CorpusTest(const Case* testCase) : m_case(testCase) {}

private:
    void TestBody() override {
        if (m_case->pendingIssue != 0) {
            GTEST_SKIP() << "Pending issue #" << m_case->pendingIssue;
        }
        try {
            const auto actual = execute(*m_case);
            for (const auto& difference : compare(m_case->expected, actual)) {
                ADD_FAILURE() << difference;
            }
        } catch (const std::runtime_error& error) {
            FAIL() << error.what();
        }
    }

    const Case* m_case;
};

TEST(CorpusInfrastructure, UniqueOperations) {
    for (const auto& error : registrationErrors()) {
        ADD_FAILURE() << error;
    }
}

}  // namespace
#endif

}  // namespace solid_space::corpus

#ifndef SS_CORPUS_TESTING
int main(int argc, char** argv) {
    using namespace solid_space::corpus;
    std::vector<std::string_view> arguments;
    bool results = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        arguments.push_back(argument);
        results = results || argument.starts_with("--ss-results");
    }
    if (results) {
        return runResults(cases(), arguments);
    }
    testing::InitGoogleTest(&argc, argv);
    for (const auto& testCase : cases()) {
        auto name = testCase.component + "_" + testCase.id;
        std::ranges::replace(name, '-', '_');
        const auto factory = [entry = &testCase]() -> CorpusTest* {
            return std::make_unique<CorpusTest>(entry).release();
        };
        testing::RegisterTest("Corpus", name.c_str(), nullptr, nullptr, __FILE__, __LINE__,
                              factory);
    }
    return RUN_ALL_TESTS();
}
#endif
