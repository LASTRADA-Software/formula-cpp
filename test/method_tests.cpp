// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

// The coherent SI unit of force. Declared here rather than taken from
// `unit.hpp`, which has no Newton: `dim::Force` exists, the unit does not, and
// adding one to the published unit table is a decision about this library's
// surface that belongs to whoever wants a Newton for its own sake -- not to
// the first test that happens to need a force. `measured_tests.cpp` and
// `examples/composition.cpp` declare their own units the same way.
inline constexpr formula::Unit Newton { .dimension = formula::dim::Force,
                                        .symbolText = formula::symbol("N"),
                                        .decimals = 1 };

// The discriminators. Empty tags, never instantiated: what a variant applies
// to is a type, so that selecting one is a compile-time fact the type system
// can state rather than a runtime string nobody checks.
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};
struct Core
{
};

struct Force: formula::Quantity<Force, "F", "applied force", Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto specimen(long long force, long long edgeX, long long edgeY)
{
    return formula::environment(formula::Measured<Force> { formula::Rational { force } },
                                formula::Measured<EdgeX> { formula::Rational { edgeX } },
                                formula::Measured<EdgeY> { formula::Rational { edgeY } });
}
} // namespace

TEST_CASE("a variants pack holds structurally different expressions", "[method]")
{
    constexpr auto pack = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                            formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ));

    STATIC_REQUIRE(std::tuple_size_v<decltype(pack.cases)> == 2);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, decltype(pack.cases)>::tag, Cube>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, decltype(pack.cases)>::tag, Cylinder>);

    // The point of the pack, and the reason it is a tuple rather than an
    // array: the two variants really are different TYPES. An array would
    // require one type for both and so could not hold the spec's own pair.
    // Without this line a guard that demanded the variants be the same type
    // would pass every other assertion in this file.
    STATIC_REQUIRE(
        !std::is_same_v<std::tuple_element_t<0, decltype(pack.cases)>, std::tuple_element_t<1, decltype(pack.cases)>>);
}

TEST_CASE("a variant carries its tag, its expression and the dimension it reports", "[method]")
{
    constexpr auto only = formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) );

    STATIC_REQUIRE(std::is_same_v<decltype(only)::tag, Cube>);
    STATIC_REQUIRE(decltype(only)::dimension == formula::dim::Pressure);

    // A variant is not a `Node`: it names an expression, it is not one. D1 of
    // the phase's design decisions turns on this -- a method selects among
    // formulas rather than standing where a number stands -- and the cheapest
    // way for that to stop being true is for someone to give `VariantCase` a
    // `NodeBase` it does not need.
    STATIC_REQUIRE(!formula::Node<decltype(only)>);
}

TEST_CASE("the expression a variant was given is the expression it stores", "[method]")
{
    // The only test here that EVALUATES what a variant stores rather than
    // inspecting its type, and the reason it uses a `ConstantNode`: that is
    // the one node with runtime state, so a pack that carried the right types
    // and the wrong bytes is visible here and nowhere else -- the half would
    // come back as zero.
    //
    // Stated honestly, because it was measured rather than assumed: no
    // mutation of `method.hpp` was found that ONLY this test catches.
    // Dropping the expression in `variant()` leaves its parameter unreferenced
    // and `/W4 /WX` rejects the build before any test runs. What this test
    // does buy is the end-to-end reachability the spike compiled but never ran
    // -- that a variant taken back out of the pack is still a formula that
    // evaluates -- which is the property tasks built on this one assume.
    constexpr auto pack = formula::variants(
        formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
        formula::variant<Cylinder>(var<Force> / (formula::number(formula::Rational { 1, 2 }) * var<EdgeX> * var<EdgeX>) ));

    // 1000 N over a 100 mm square face is 100 kPa, which is 0.1 MPa.
    constexpr auto cube = formula::checked_evaluate<Strength>(std::get<0>(pack.cases).expression, specimen(1000, 100, 100));
    REQUIRE(cube.has_value());
    CHECK(cube->measurement().value() == formula::Rational { 1, 10 });

    // The same load over half that area is twice the strength -- and that
    // factor of a half exists only in the stored expression.
    constexpr auto cylinder =
        formula::checked_evaluate<Strength>(std::get<1>(pack.cases).expression, specimen(1000, 100, 100));
    REQUIRE(cylinder.has_value());
    CHECK(cylinder->measurement().value() == formula::Rational { 1, 5 });
}

TEST_CASE("four variants that agree in dimension without agreeing in type are accepted", "[method]")
{
    // The positive twin of `negative/method_variants_disagree.cpp`: the same
    // four-variant shape, and a third variant that measures what the other
    // three measure. Without this, a guard that refused every four-variant
    // pack -- or refused whenever the variants' types differ at all -- would
    // be indistinguishable from the one this library wants.
    constexpr auto pack = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                            formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                            formula::variant<Prism>(var<Force> / (var<EdgeY> * var<EdgeY>) ),
                                            formula::variant<Core>(var<Force> / (var<EdgeY> * var<EdgeX>) ));

    STATIC_REQUIRE(std::tuple_size_v<decltype(pack.cases)> == 4);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<2, decltype(pack.cases)>::tag, Prism>);
    STATIC_REQUIRE(std::tuple_element_t<0, decltype(pack.cases)>::dimension
                   == std::tuple_element_t<2, decltype(pack.cases)>::dimension);
}
