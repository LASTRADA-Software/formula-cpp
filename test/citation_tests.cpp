// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Every citation in this repository is invented. Naming a real standard would
// put copyrighted material in a public repository.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });

} // namespace

TEST_CASE("citation: a citation carries every field it was given", "[citation]")
{
    STATIC_REQUIRE(ratio.citation.title == std::string_view { "Water/cement ratio" });
    STATIC_REQUIRE(ratio.citation.reference == std::string_view { "Example Standard 1:2020" });
    STATIC_REQUIRE(ratio.citation.section == std::string_view { "5.4.2" });
    STATIC_REQUIRE(ratio.citation.equation == std::string_view { "(3)" });
    STATIC_REQUIRE(ratio.citation.text == std::string_view { "Ratio of water content to cement content." });
}

TEST_CASE("citation: a field not named is empty, not absent", "[citation]")
{
    constexpr auto sparse = formula::documented(var<WaterVolume>, { .title = "A volume" });

    STATIC_REQUIRE(sparse.citation.title == std::string_view { "A volume" });
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
    STATIC_REQUIRE(decltype(formula::documented(var<WaterVolume>, {}))::dimension == formula::dim::Volume);
}

TEST_CASE("citation: the wrapper keeps the expression it wrapped", "[citation]")
{
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.inner),
                                  formula::BinaryNode<formula::BinaryOperator::Divide,
                                                      formula::VarNode<WaterVolume>,
                                                      formula::VarNode<CementVolume>>>);
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
    constexpr auto inputs =
        formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(300) });

    constexpr auto wrapped = formula::checked_evaluate<Ratio>(ratio, inputs);
    constexpr auto bare = formula::checked_evaluate<Ratio>(var<WaterVolume> / var<CementVolume>, inputs);

    STATIC_REQUIRE(wrapped.has_value());
    STATIC_REQUIRE(wrapped->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(wrapped->measurement() == bare->measurement());
}

TEST_CASE("citation: an absent input still propagates through a wrapper", "[citation]")
{
    constexpr auto partial =
        formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("citation: a citation survives being wrapped again", "[citation]")
{
    constexpr auto outer = formula::documented(ratio, { .title = "Water/cement ratio, per cent" });

    STATIC_REQUIRE(outer.citation.title == std::string_view { "Water/cement ratio, per cent" });
    STATIC_REQUIRE(outer.inner.citation.title == std::string_view { "Water/cement ratio" });
    STATIC_REQUIRE(decltype(outer)::dimension == formula::dim::Scalar);
}
