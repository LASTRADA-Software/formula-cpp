// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", unit::One>
{
};
struct Ratio: formula::Quantity<Ratio, "r", "a dimensionless ratio", unit::One>
{
};

using OneDecimalOfMegapascal =
    formula::RoundingRule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

// Only the Cube variant reads the shape factor, so an overlay fixing it
// changes Cube and leaves Cylinder alone.
inline constexpr auto baseMethod =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    OneDecimalOfMegapascal {},
                    formula::constraints());

// Three variants reading three different expressions, so each one's result
// names which variant produced it: Cube 6 MPa, Cylinder 4 MPa, Prism 9 MPa.
inline constexpr auto threeVariants =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                      formula::variant<Prism>(var<Force> / (var<EdgeY> * var<EdgeY>) )),
                    OneDecimalOfMegapascal {},
                    formula::constraints());

// The base method's shape factor is 1, supplied by the specimen.
inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                    formula::Measured<EdgeY> { formula::Rational { 100 } },
                                                    formula::Measured<ShapeFactor> { formula::Rational { 1 } });

// The same specimen with no shape factor at all. `environment.get<Q>()` of a
// quantity an environment does not provide is a build error, so a formula
// evaluated against this one provably never asks for the shape factor.
inline constexpr auto inputsWithoutShapeFactor =
    formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                         formula::Measured<EdgeX> { formula::Rational { 150 } },
                         formula::Measured<EdgeY> { formula::Rational { 100 } });

inline constexpr formula::Citation nationalAnnex { .title = "Shape factor",
                                                   .reference = "Example Standard 12:2021 NA",
                                                   .section = "NA.2.3" };

inline constexpr auto national =
    formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, nationalAnnex));

/// `expression` with `Ratio` fixed at 4, evaluated against an environment
/// holding nothing at all -- so it compiles only if every use of `Ratio` in
/// `expression` was rewritten.
///
/// Asks the rewrite `with_constant` applies to each variant directly rather
/// than through a method, because no lookup -- banded, exact or interpolating,
/// at the root of a variant or nested inside it -- can be in a variant's
/// expression on clang++ 20 with libstdc++ 14 today. `Variants` holds its
/// variants in a `std::tuple`, whose default-constructor check makes clang
/// instantiate the zero-argument constructor of the lookup's `Corrections`,
/// and that constructor is the refusal of a short corrections list. cl,
/// clang-cl and g++ accept the same method. That is a defect of `lookup.hpp`
/// and `method.hpp` together, not of the overlay, and the tests above already
/// take the rewrite through `apply` for the node kinds a method can hold
/// everywhere.
template <typename Expr>
[[nodiscard]] constexpr formula::Rational withRatioFixedAtFour(Expr expression)
{
    using Rewrite = formula::detail::ConstantRewrite<Ratio, Expr>;
    static_assert(Rewrite::known && Rewrite::mentions);
    auto const evaluated = formula::checked_evaluate_si(
        Rewrite::apply(expression, formula::with_constant<Ratio>(formula::Rational { 4 })), formula::environment());
    return evaluated.value().value();
}

enum class Shape : std::uint8_t
{
    Square,
    Round,
};

inline constexpr formula::KeyTable<Shape, 2> ShapeKeys { Shape::Square, Shape::Round };
inline constexpr formula::BandTable<2> RatioBands { formula::band(0, 1, 2, 1), formula::band(2, 1, 6, 1) };
inline constexpr formula::BreakpointTable<2> RatioPoints { formula::breakpoint(0), formula::breakpoint(8) };
} // namespace

TEST_CASE("an overlay overrides a constant and yields a method", "[overlay]")
{
    constexpr auto overlaid = formula::apply(national, baseMethod);
    STATIC_REQUIRE(formula::detail::IsWellFormedMethod<std::remove_cv_t<decltype(overlaid)>>::value);

    // 90000 N over 150 mm by 100 mm is 6 MPa. The base method's factor is 1,
    // the overlay's 0.97, and 6 * 0.97 = 5.82 MPa, which the method's own
    // rule rounds to 5.8. Both are in pascals, as `evaluate_method` answers.
    // The two results differ AFTER rounding: a fixture where they agreed could
    // not tell an applied overlay from an ignored one.
    constexpr auto base = formula::evaluate_method<Cube>(baseMethod, inputs);
    constexpr auto after = formula::evaluate_method<Cube>(overlaid, inputs);
    STATIC_REQUIRE(base->value() == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(after->value() == formula::Rational { 5'800'000 });
    STATIC_REQUIRE(base->value() != after->value());

    // The overlay's value wins over the 1 the environment supplies above, and
    // the overlaid method does not need the environment to supply one at all.
    constexpr auto without = formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor);
    STATIC_REQUIRE(without->value() == formula::Rational { 5'800'000 });
}

TEST_CASE("an overlay's constant is stated in its quantity's declared unit", "[overlay]")
{
    // The shape factor above is dimensionless, where every unit is the same
    // unit; this fixes a length. 120 mm read as 120 mm gives 90000 N over
    // 150 mm by 120 mm, 5 MPa. Read as 120 m -- the coherent SI unit -- it
    // would give 5 kPa, which rounds to 0.0 MPa.
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<EdgeY>(formula::Rational { 120 })), baseMethod);

    // No edge Y in the environment, so that only the overlay's can be read.
    constexpr auto withoutEdgeY = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                       formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                       formula::Measured<ShapeFactor> { formula::Rational { 1 } });
    constexpr auto after = formula::evaluate_method<Cube>(overlaid, withoutEdgeY);
    STATIC_REQUIRE(after->value() == formula::Rational { 5'000'000 });
}

TEST_CASE("an overridden constant keeps its quantity's identity", "[overlay]")
{
    constexpr auto overlaid = formula::apply(national, baseMethod);
    constexpr auto cube = std::get<0>(overlaid.variantSet.cases).expression;

    // `var<ShapeFactor> * var<Force> / (...)` parses as `(k * F) / (...)`, so
    // the factor is the left operand of the left operand.
    using Factor = std::remove_cvref_t<decltype(cube.lhs.lhs)>;
    STATIC_REQUIRE(std::is_same_v<Factor, formula::OverriddenConstantNode<ShapeFactor>>);
    STATIC_REQUIRE(std::is_same_v<Factor::quantity, ShapeFactor>);
    STATIC_REQUIRE(cube.lhs.lhs.value == formula::Rational { 97, 100 });
    STATIC_REQUIRE(cube.lhs.lhs.source == nationalAnnex);

    // It reads as the shape factor, not as 97/100: the question a reader asks
    // of a jurisdiction's constant is which quantity it fixes.
    CHECK(formula::render(cube) == "k_s * F / (x_m * y_m)");
    auto const documentation = formula::document(cube);
    REQUIRE(!documentation.symbols.empty());
    CHECK(documentation.symbols.front().symbol == "k_s");
    CHECK(documentation.symbols.front().description == "shape factor");
}

TEST_CASE("an overlay fixes a constant in every variant and every constraint", "[overlay]")
{
    constexpr auto everywhere = formula::method(
        formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                          formula::variant<Cylinder>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeX>) )),
        OneDecimalOfMegapascal {},
        formula::constraints(formula::constraint(var<ShapeFactor> <= formula::number(formula::Rational { 1 }),
                                                 formula::Verdict { "the shape factor exceeds one" },
                                                 nationalAnnex)));
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 11, 10 })), everywhere);

    // The second variant, against an environment without the factor: this
    // compiles only if the rewrite reached past the first variant. 90000 N
    // over 150 mm squared is 4 MPa, and 4 * 1.1 = 4.4.
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(overlaid, inputsWithoutShapeFactor);
    STATIC_REQUIRE(cylinder->value() == formula::Rational { 4'400'000 });

    // The constraint reads the overlay's 1.1, not a factor the environment
    // never had, and keeps its own verdict and citation.
    constexpr auto limit = std::get<0>(overlaid.constraintSet.items);
    constexpr auto outcome = formula::check(limit, inputsWithoutShapeFactor);
    STATIC_REQUIRE(outcome.is_violated());
    STATIC_REQUIRE(outcome.verdict() == formula::Verdict { "the shape factor exceeds one" });
    STATIC_REQUIRE(limit.citation == nationalAnnex);
}

TEST_CASE("an overlay fixes a constant inside every node kind", "[overlay]")
{
    // Each call compiles only if the rewrite reached `var<Ratio>` through that
    // node kind -- see `withRatioFixedAtFour` -- and the value says the node
    // was rebuilt with its own contents: an operand, a citation, a table. The
    // values are unrounded: no method, so no method's rounding rule.
    namespace f = formula;
    using f::Rational;
    constexpr auto r = var<Ratio>;

    STATIC_REQUIRE(withRatioFixedAtFour(-r) == Rational { -4 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::pow<2>(r)) == Rational { 16 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::sqrt(r)) == Rational { 2 });
    STATIC_REQUIRE(withRatioFixedAtFour(r + f::number(Rational { 1 })) == Rational { 5 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::documented(r, nationalAnnex)) == Rational { 4 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::rounded<unit::One, f::DecimalPlaces { 0 }, f::RoundingMode::HalfAwayFromZero>(
                       r / f::number(Rational { 3 })))
                   == Rational { 1 });
    STATIC_REQUIRE(
        withRatioFixedAtFour(f::rounded_to_digits<unit::One, f::SignificantDigits { 1 }, f::RoundingMode::HalfAwayFromZero>(
            r * f::number(Rational { 3 })))
        == Rational { 10 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::numeric_value_of<unit::One, "Example Standard 12:2021 states it bare">(r))
                   == Rational { 4 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::when(r > f::number(Rational { 1 }), r, f::number(Rational { 0 })))
                   == Rational { 4 });
    STATIC_REQUIRE(
        withRatioFixedAtFour(f::banded_lookup<unit::One, RatioBands, unit::One>(r, { Rational { 1 }, Rational { 7 } }))
        == Rational { 7 });
    STATIC_REQUIRE(withRatioFixedAtFour(
                       f::interpolating_lookup<unit::One, RatioPoints, unit::One>(r, { Rational { 0 }, Rational { 80 } }))
                   == Rational { 40 });

    // The leaves that name no quantity are carried over, contents and all.
    STATIC_REQUIRE(
        withRatioFixedAtFour(r * f::exact_lookup<ShapeKeys, unit::One>(Shape::Round, { Rational { 2 }, Rational { 3 } }))
        == Rational { 12 });
    STATIC_REQUIRE(withRatioFixedAtFour(r * f::pi) == Rational { 4 } * f::Pi);
}

TEST_CASE("pin_variant keeps only the variant it names", "[overlay]")
{
    // The middle variant, so that keeping the first or the last would not pass.
    constexpr auto pinned = formula::apply(formula::overlay(formula::pin_variant<Cylinder>()), threeVariants);

    STATIC_REQUIRE(std::tuple_size_v<decltype(pinned.variantSet.cases)> == 1);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, decltype(pinned.variantSet.cases)>::tag, Cylinder>);
    STATIC_REQUIRE(formula::evaluate_method<Cylinder>(pinned, inputs)->value() == formula::Rational { 4'000'000 });
}

TEST_CASE("pinning a method's only variant is accepted", "[overlay]")
{
    // It changes nothing, and is still not refused: the refusals exist to
    // catch a mistaken name, and this pin names its variant correctly. See
    // the file comment of `overlay.hpp`.
    //
    // A method declared with one variant rather than pinned down to one, so
    // that a pin choosing the wrong variant -- which `pin_variant keeps only
    // the variant it names` exists to catch -- cannot fail here as well.
    constexpr auto single =
        formula::method(formula::variants(formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                        OneDecimalOfMegapascal {},
                        formula::constraints());
    constexpr auto pinned = formula::apply(formula::overlay(formula::pin_variant<Cylinder>()), single);

    STATIC_REQUIRE(std::is_same_v<std::remove_cv_t<decltype(pinned)>, std::remove_cv_t<decltype(single)>>);
}

TEST_CASE("prune_variant deletes only the variant it names", "[overlay]")
{
    constexpr auto pruned = formula::apply(formula::overlay(formula::prune_variant<Cylinder>()), threeVariants);

    // The survivors keep their declaration order and their own formulas.
    STATIC_REQUIRE(std::tuple_size_v<decltype(pruned.variantSet.cases)> == 2);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, decltype(pruned.variantSet.cases)>::tag, Cube>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, decltype(pruned.variantSet.cases)>::tag, Prism>);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(pruned, inputs)->value() == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(formula::evaluate_method<Prism>(pruned, inputs)->value() == formula::Rational { 9'000'000 });
}

TEST_CASE("a later overlay fixes a constant an earlier one fixed", "[overlay]")
{
    // A regional overlay, then a national one over it. The national value is
    // the one that holds, and the national citation the one carried.
    constexpr formula::Citation regional { .reference = "Example Standard 12:2021 RA" };
    constexpr auto first = formula::apply(
        formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 9, 10 }, regional)), baseMethod);
    constexpr auto second = formula::apply(national, first);

    STATIC_REQUIRE(formula::evaluate_method<Cube>(first, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 5'400'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cube>(second, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 5'800'000 });
    STATIC_REQUIRE(std::get<0>(second.variantSet.cases).expression.lhs.lhs.source == nationalAnnex);
}

TEST_CASE("an empty overlay yields the method unchanged", "[overlay]")
{
    constexpr auto unchanged = formula::apply(formula::overlay(), baseMethod);

    STATIC_REQUIRE(std::is_same_v<std::remove_cv_t<decltype(unchanged)>, std::remove_cv_t<decltype(baseMethod)>>);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(unchanged, inputs)->value() == formula::Rational { 6'000'000 });
}

TEST_CASE("an overridden constant is traced as its quantity, holding the overlay's value", "[overlay][trace]")
{
    // Interim: until the trace has a step kind of its own for an overridden
    // constant, it records an ordinary variable step. This pins that the
    // constant is in the trace at all, under the quantity's symbol and with
    // the value the formula used.
    constexpr auto overlaid = formula::apply(national, baseMethod);

    formula::Trace<> trace;
    auto const result =
        formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor, formula::RecordingSink<> { trace });
    REQUIRE(result.has_value());

    std::size_t factorSteps = 0;
    for (auto const& step: trace.steps)
    {
        if (step.kind != formula::StepKind::Variable || step.symbol != "k_s")
            continue;
        ++factorSteps;
        REQUIRE(step.value.has_value());
        CHECK(*step.value == formula::Rational { 97, 100 });
    }
    CHECK(factorSteps == 1);
}
