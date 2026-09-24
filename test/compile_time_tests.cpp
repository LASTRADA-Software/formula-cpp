// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <type_traits>

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

TEST_CASE("a dependent quantity is derived, not supplied", "[evaluation]")
{
    using Calculation =
        formula::Evaluation<formula::EvaluationArguments<Alpha, Beta>,
                            formula::EvaluationFunctors { [](auto const& ctx) -> Beta {
                                return Beta { formula::get<Alpha>(ctx).value * 2 };
                            } }>;

    auto const result = Calculation().set(Alpha { 21 }).calculate(Beta {});
    CHECK(result.value == 42);
}
