// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Run: formula::Quantity<Run, "L", "horizontal distance covered", formula::unit::Millimetre>
{
};
struct Ratio: formula::Quantity<Ratio, "s", "road gradient", formula::unit::One>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Every citation of a standard in this repository is invented. Naming a real
// standard would put copyrighted material in a public repository.
constexpr auto ratio = formula::documented(var<Rise> / var<Run>,
                                           { .title = "Road gradient",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Height gained over horizontal distance covered." });

} // namespace

TEST_CASE("citation: a citation carries every field it was given", "[citation]")
{
    STATIC_REQUIRE(ratio.citation.title == std::string_view { "Road gradient" });
    STATIC_REQUIRE(ratio.citation.reference == std::string_view { "Example Standard 1:2020" });
    STATIC_REQUIRE(ratio.citation.section == std::string_view { "5.4.2" });
    STATIC_REQUIRE(ratio.citation.equation == std::string_view { "(3)" });
    STATIC_REQUIRE(ratio.citation.text == std::string_view { "Height gained over horizontal distance covered." });
}

TEST_CASE("citation: a field not named is empty, not absent", "[citation]")
{
    constexpr auto sparse = formula::documented(var<Rise>, { .title = "A height" });

    STATIC_REQUIRE(sparse.citation.title == std::string_view { "A height" });
    STATIC_REQUIRE(sparse.citation.reference.empty());
    STATIC_REQUIRE(sparse.citation.section.empty());
    STATIC_REQUIRE(sparse.citation.equation.empty());
    STATIC_REQUIRE(sparse.citation.text.empty());
}

TEST_CASE("citation: two citations with the same fields compare equal", "[citation]")
{
    constexpr formula::Citation left { .title = "A", .section = "1" };
    constexpr formula::Citation right { .title = "A", .section = "1" };
    constexpr formula::Citation other { .title = "A", .section = "2" };

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("citation: wrapping does not change the dimension", "[citation]")
{
    STATIC_REQUIRE(decltype(ratio)::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::documented(var<Rise>, {}))::dimension == formula::dim::Length);
}

TEST_CASE("citation: the wrapper keeps the expression it wrapped", "[citation]")
{
    STATIC_REQUIRE(
        std::is_same_v<decltype(ratio.inner),
                       formula::BinaryNode<formula::BinaryOperator::Divide, formula::VarNode<Rise>, formula::VarNode<Run>>>);
}

TEST_CASE("citation: the wrapper keeps the expression's own state, not a fresh one", "[citation]")
{
    // `is_same_v` on the inner type is not enough: every node in the tree used
    // by the test above is stateless, so a wrapper that discarded its argument
    // and stored `Inner {}` would pass that check. A constant carries a number.
    constexpr auto wrapped =
        formula::documented(formula::constant<formula::unit::Millimetre>(rat(150)), { .title = "A declared span" });

    STATIC_REQUIRE(wrapped.inner.number == rat(150));
}

TEST_CASE("citation: a documented expression is still an expression", "[citation]")
{
    STATIC_REQUIRE(formula::Node<decltype(ratio)>);

    // It composes like any other node, and the composite has the right dimension.
    constexpr auto scaled = ratio * rat(100);
    STATIC_REQUIRE(formula::is_dimensionless(decltype(scaled)::dimension));
}

TEST_CASE("citation: a wrapped formula evaluates to what it wrapped", "[citation]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Rise> { rat(180) }, formula::Measured<Run> { rat(300) });

    constexpr auto wrapped = formula::checked_evaluate<Ratio>(ratio, inputs);
    constexpr auto bare = formula::checked_evaluate<Ratio>(var<Rise> / var<Run>, inputs);

    STATIC_REQUIRE(wrapped.has_value());
    STATIC_REQUIRE(wrapped->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(wrapped->measurement() == bare->measurement());
}

TEST_CASE("citation: an absent input still propagates through a wrapper", "[citation]")
{
    constexpr auto partial = formula::environment(formula::Measured<Rise>::absent(), formula::Measured<Run> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("citation: a citation survives being wrapped again", "[citation]")
{
    constexpr auto outer = formula::documented(ratio, { .title = "Road gradient, per cent" });

    STATIC_REQUIRE(outer.citation.title == std::string_view { "Road gradient, per cent" });
    STATIC_REQUIRE(outer.inner.citation.title == std::string_view { "Road gradient" });
    STATIC_REQUIRE(decltype(outer)::dimension == formula::dim::Scalar);
}
