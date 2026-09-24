// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/sink.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Volume: formula::Quantity<Volume, "V", "specimen volume", unit::CubicMetre>
{
};

/// Counts the nodes it is told about. Not `NullSink`: if the dispatcher ever
/// silently preferred the two-parameter overload, this would count zero while
/// still producing the right number -- which is exactly the failure a codegen
/// comparison cannot see.
struct CountingSink
{
    int* enteredCount {};
    int* producedCount {};

    template <formula::Node N>
    constexpr void entered(N const&) noexcept
    {
        ++*enteredCount;
    }

    template <formula::Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
        ++*producedCount;
    }
};
} // namespace

TEST_CASE("NullSink is empty, stateless, and satisfies the seam for every node kind", "[sink]")
{
    // Empty matters: the measurements that justify passing a sink by value
    // depend on there being nothing to pass.
    STATIC_REQUIRE(std::is_empty_v<formula::NullSink>);

    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    STATIC_REQUIRE(formula::SinkFor<formula::NullSink, decltype(density), formula::Rational>);
    STATIC_REQUIRE(formula::SinkFor<formula::NullSink, decltype(var<Mass>), formula::Rational>);

    // Callable, and callable in a constant expression.
    formula::NullSink sink {};
    sink.entered(density);
    sink.produced(density, formula::Rational { 12 });
}

TEST_CASE("CountingSink satisfies the seam too, so Task 2 can use it", "[sink]")
{
    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    sink.entered(density);
    sink.produced(density, formula::Rational { 12 });

    CHECK(entered == 1);
    CHECK(produced == 1);
}
