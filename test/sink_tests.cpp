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

[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}
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

TEST_CASE("a sink is told about every node of the tree", "[sink]")
{
    // pow<2>(m) / V is four nodes: the division, the power, and two variables.
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;

    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);

    CHECK(entered == 4);
    CHECK(produced == 4);
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    CHECK(**result == formula::Rational { 12 });
}

TEST_CASE("NullSink changes neither the answer nor whether one is produced", "[sink]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    auto const environment = environmentOf(6, 3);

    auto const untraced = formula::checked_evaluate_si<formula::Rational>(density, environment);
    formula::NullSink sink {};
    auto const traced = formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    REQUIRE(untraced.has_value());
    REQUIRE(traced.has_value());
    CHECK(*untraced == *traced);
}

namespace
{
/// A consumer's own node, written against the extension point as phase 5
/// published it: two parameters, no knowledge of sinks.
struct LegacyNode: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};
} // namespace

namespace formula
{
template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(LegacyNode const&, Env const&) noexcept
{
    return detail::present<Rep>(Rep { 7 });
}
} // namespace formula

TEST_CASE("a node written against the two-parameter extension point still evaluates", "[sink]")
{
    constexpr auto expression = LegacyNode {} + var<Mass>;

    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    auto const result = formula::checked_evaluate_si<formula::Rational>(expression, environmentOf(5, 1), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    // 7 from the legacy node plus 5 kg: the legacy overload was called and its
    // answer used.
    CHECK(**result == formula::Rational { 12 });
    // Two nodes reported, not three: the legacy node is evaluated but not
    // traced, because nothing told the library how to trace it.
    CHECK(entered == 2);
    CHECK(produced == 2);
}
