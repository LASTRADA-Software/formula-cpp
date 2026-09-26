// SPDX-License-Identifier: Apache-2.0
//
// Runs what `consumer_globals_tests.cpp` compiled. The guard is that that
// translation unit builds at all under cl /W4 /WX; this checks that each entry
// point it uses did answer as expected -- which says nothing about entry
// points it does not use: see that file for which templates it instantiates.
#include "consumer_globals.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

TEST_CASE("a consumer's ordinary globals do not break a build that includes every header", "[hygiene]")
{
    ConsumerGlobalsProbe const probe = probe_consumer_globals();
    REQUIRE(probe.checks.size() == 46);
    for (std::size_t index = 0; index < probe.checks.size(); ++index)
    {
        INFO("check " << index);
        CHECK(probe.checks[index]);
    }
}
