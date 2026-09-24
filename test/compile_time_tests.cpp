// SPDX-License-Identifier: Apache-2.0
// `formula.hpp` no longer pulls this header in -- it is `detail`, and the only
// thing left using it is this test of the utility itself, so it is included
// directly rather than dragged through the umbrella for one TEST_CASE.
#include <formula-cpp/detail/type_list.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>

namespace
{
struct Alpha
{
    int value {};
};
struct Beta
{
    int value {};
};
struct Gamma
{
    int value {};
};

using Bag = std::tuple<Alpha, Beta, Gamma>;
} // namespace

TEST_CASE("index_in_tuple locates each type", "[type_list]")
{
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Alpha, Bag> == 0);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Beta, Bag> == 1);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Gamma, Bag> == 2);
}
