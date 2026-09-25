// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

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

// Declared and never defined: a tag is never instantiated, so it need not be
// complete. See the test that uses it.
struct Unfinished;

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
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

TEST_CASE("a variant's tag need not be a complete type", "[method]")
{
    // The tag rule refuses what can never be selected -- `void`, `int`, a
    // reference, a cv-qualified class -- and deliberately stops short of
    // `std::is_empty_v`, which would demand a complete type. This is what
    // pins that choice: a rule tightened to `is_empty_v` refuses this file.
    constexpr auto only = formula::variant<Unfinished>(var<Force> / (var<EdgeX> * var<EdgeY>) );

    STATIC_REQUIRE(std::is_same_v<decltype(only)::tag, Unfinished>);
}

TEST_CASE("the expression a variant was given is the expression it stores", "[method]")
{
    // A REACHABILITY PROBE, and judged as one. The other tests here inspect
    // types; this is the only one that takes a variant back out of the pack
    // and evaluates it, which is what says the pack holds usable formulas
    // rather than merely well-formed types.
    //
    // It kills no mutation uniquely, and that is characteristic of the kind
    // rather than a defect in this one: dropping the expression in `variant()`
    // leaves its parameter unreferenced and `/W4 /WX` rejects the build before
    // any test runs. What a reachability probe catches is ABSENCE -- phase 9
    // shipped an overload that worked, was tested, and no user could call;
    // phase 10 shipped a `document()` walk that compiled for nothing. The
    // spike compiled this pack and never ran it, so nothing until now had
    // established that a stored variant still evaluates at all.
    //
    // `ConstantNode` because it is the node with runtime state reachable from
    // the operators THIS fixture uses: a `VarNode` holds no bytes, and a
    // `BinaryNode`'s bytes are only its children. That is a property of this
    // fixture and not of node kinds in general -- lookups carry their
    // corrections as runtime state for the same reason `ConstantNode` carries
    // its number, and they are exactly what a method's variants are likely to
    // hold, so a round-trip probe over one of those would not be pointless.
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
