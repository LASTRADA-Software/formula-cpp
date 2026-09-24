// SPDX-License-Identifier: Apache-2.0
#include "cross_tu.hpp"

#include <formula-cpp/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("version macros agree with each other", "[version]")
{
    CHECK(FORMULA_VERSION_MAJOR == 0);
    CHECK(FORMULA_VERSION_MINOR == 1);
    CHECK(FORMULA_VERSION_PATCH == 0);
    CHECK(std::string_view { FORMULA_VERSION_STRING } == "0.1.0");
}

TEST_CASE("quantity types survive a translation unit boundary", "[linkage]")
{
    CHECK(consumeFirst(First { 7 }) == 7);
}
