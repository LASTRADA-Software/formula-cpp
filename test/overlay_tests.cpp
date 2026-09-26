// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
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

inline constexpr formula::Citation roundingAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.4.1" };

/// Two decimal places of a megapascal where the base method keeps one.
inline constexpr auto tighterRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>());

/// The same, citing where the jurisdiction states it.
inline constexpr auto citedTighterRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        roundingAnnex));

/// The base method's own granularity, restated by a jurisdiction.
inline constexpr auto sameRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>());

// 90100 N over 150 mm by 100 mm is 6.00666... MPa: 6.0 to one decimal and
// 6.01 to two, so a rounding rule that did not change shows in the value.
inline constexpr auto unevenInputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'100 } },
                                                          formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                          formula::Measured<EdgeY> { formula::Rational { 100 } },
                                                          formula::Measured<ShapeFactor> { formula::Rational { 1 } });

/// The derivation of @p m's Cube variant against `unevenInputs`, rendered.
template <typename M>
[[nodiscard]] std::string traceOf(M const& m)
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Cube>(m, unevenInputs, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 20 });
}

/// A method of one Cube variant, `expression`, reported to three decimals of
/// a dimensionless ratio, with `Ratio` fixed at 4 by an overlay.
template <typename Expr>
[[nodiscard]] constexpr auto withRatioFixedAtFourMethod(Expr expression)
{
    constexpr auto fixed = formula::overlay(formula::with_constant<Ratio>(formula::Rational { 4 }));
    return formula::apply(
        fixed,
        formula::method(
            formula::variants(formula::variant<Cube>(expression)),
            formula::rounding_rule<unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
            formula::constraints()));
}

/// `withRatioFixedAtFourMethod(expression)`, evaluated against an environment
/// holding nothing at all -- so it compiles only if `apply` rewrote every use
/// of `Ratio` in `expression`.
template <typename Expr>
[[nodiscard]] constexpr formula::Rational withRatioFixedAtFour(Expr expression)
{
    return formula::evaluate_method<Cube>(withRatioFixedAtFourMethod(expression), formula::environment()).value().value();
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

TEST_CASE("the documentation marks an overridden constant as fixed, with its value and citation", "[overlay][document]")
{
    constexpr auto cube = std::get<0>(formula::apply(national, baseMethod).variantSet.cases).expression;
    auto const documentation = formula::document(cube);

    // Its row says it is fixed, at what and on whose authority: a plain row
    // would ask a reader to supply a value the formula never reads.
    REQUIRE(documentation.symbols.size() == 4);
    formula::SymbolEntry const& factor = documentation.symbols[0];
    CHECK(factor.symbol == "k_s");
    REQUIRE(factor.fixedValue.has_value());
    CHECK(*factor.fixedValue == formula::Rational { 97, 100 });
    CHECK(factor.fixedBy == nationalAnnex);
    // Every use of it is fixed, so it is not read from the specimen at all.
    CHECK(!factor.alsoReadAsInput);

    // The rows the specimen supplies are not marked.
    for (std::size_t row = 1; row < documentation.symbols.size(); ++row)
    {
        CHECK(!documentation.symbols[row].fixedValue.has_value());
        CHECK(documentation.symbols[row].fixedBy == formula::Citation {});
    }
}

TEST_CASE("a quantity read plainly and then fixed is documented as both", "[overlay][document]")
{
    // Only a formula assembled by hand holds both, since `apply` fixes every
    // use. The plain use comes first, so a walk keeping the first row it met
    // would hide the fixed value the formula does read -- and one that only
    // marked it fixed would hide that the specimen's value is read too.
    constexpr formula::OverriddenConstantNode<ShapeFactor> fixed { {}, formula::Rational { 97, 100 }, nationalAnnex };
    auto const documentation = formula::document(var<ShapeFactor> + fixed);

    REQUIRE(documentation.symbols.size() == 1);
    REQUIRE(documentation.symbols[0].fixedValue.has_value());
    CHECK(*documentation.symbols[0].fixedValue == formula::Rational { 97, 100 });
    CHECK(documentation.symbols[0].alsoReadAsInput);
}

TEST_CASE("a quantity fixed and then read plainly is documented as both", "[overlay][document]")
{
    // The other order: the fixed use comes first, so a walk that let the
    // later plain read change nothing would say only "fixed at 97/100" of a
    // quantity the formula also takes from the specimen.
    constexpr formula::OverriddenConstantNode<ShapeFactor> fixed { {}, formula::Rational { 97, 100 }, nationalAnnex };
    auto const documentation = formula::document(fixed + var<ShapeFactor>);

    REQUIRE(documentation.symbols.size() == 1);
    REQUIRE(documentation.symbols[0].fixedValue.has_value());
    CHECK(*documentation.symbols[0].fixedValue == formula::Rational { 97, 100 });
    CHECK(documentation.symbols[0].alsoReadAsInput);
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
    // was rebuilt with its own contents: an operand, a citation, a table. Each
    // goes through `apply` and `evaluate_method`, the path an author's method
    // takes, so the values are rounded to the method's three decimals.
    namespace f = formula;
    using f::Rational;
    constexpr auto r = var<Ratio>;

    STATIC_REQUIRE(withRatioFixedAtFour(-r) == Rational { -4 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::pow<2>(r)) == Rational { 16 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::sqrt(r)) == Rational { 2 });
    STATIC_REQUIRE(withRatioFixedAtFour(r + f::number(Rational { 1 })) == Rational { 5 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::documented(r, nationalAnnex)) == Rational { 4 });

    // The citation is the one piece of a rebuilt node no value can show, and
    // provenance is what this phase exists for: the rewritten wrapper must
    // still cite what the original cited.
    STATIC_REQUIRE(
        std::get<0>(withRatioFixedAtFourMethod(f::documented(r, nationalAnnex)).variantSet.cases).expression.citation
        == nationalAnnex);
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
    STATIC_REQUIRE(withRatioFixedAtFour(r * f::pi) == Rational { 12'566, 1'000 });
}

TEST_CASE("an overlay fixes a constant under a const child type", "[overlay]")
{
    // Off the factory path, a node declared with `decltype` of a `constexpr`
    // variable holds a `const` child. The rewrite must see through the
    // qualifier -- a const `VarNode` is still a variable, and a const
    // `DocumentedNode` still a wrapper -- rather than refuse either as a node
    // kind it cannot see inside.
    constexpr auto factor = var<ShapeFactor>;
    constexpr auto cited = formula::DocumentedNode<decltype(factor)> { {}, factor, nationalAnnex };
    constexpr auto pressure = cited * var<Force> / (var<EdgeX> * var<EdgeY>);
    STATIC_REQUIRE(std::is_const_v<decltype(cited.inner)>);

    constexpr auto m = formula::method(formula::variants(formula::VariantCase<Cube, decltype(pressure)> { pressure }),
                                       OneDecimalOfMegapascal {},
                                       formula::constraints());
    constexpr auto overlaid = formula::apply(national, m);

    STATIC_REQUIRE(formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 5'800'000 });
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

TEST_CASE("an overridden constant is traced as fixed by the overlay, holding its value", "[overlay][trace]")
{
    // Its own step kind, not a variable's: a variable step reads as a number
    // the specimen supplied. Under the quantity's symbol and unit, with the
    // value the formula used and the overlay's citation.
    constexpr auto overlaid = formula::apply(national, baseMethod);

    formula::Trace<> trace;
    auto const result =
        formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor, formula::RecordingSink<> { trace });
    REQUIRE(result.has_value());

    std::size_t factorSteps = 0;
    for (auto const& step: trace.steps)
    {
        if (step.symbol != "k_s")
            continue;
        ++factorSteps;
        CHECK(step.kind == formula::StepKind::OverriddenConstant);
        CHECK(step.unit == unit::One);
        CHECK(step.citation == nationalAnnex);
        REQUIRE(step.value.has_value());
        CHECK(*step.value == formula::Rational { 97, 100 });
    }
    CHECK(factorSteps == 1);

    // And it says so where a person reads it.
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(rendered.find("1. k_s = 97/100 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, "
                        "NA.2.3]\n")
          != std::string::npos);
}

TEST_CASE("an overridden constant the overlay cited nothing for is still traced as fixed", "[overlay][trace]")
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 })), baseMethod);

    formula::Trace<> trace;
    (void) formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 }).find("1. k_s = 97/100 [fixed by jurisdiction overlay]\n")
          != std::string::npos);
}

TEST_CASE("the trace says where the rounding rule came from", "[trace][overlay]")
{
    auto const fromMethod = traceOf(baseMethod);
    CHECK(fromMethod.find("rounded to 1 dp (method default)") != std::string::npos);

    auto const fromOverlay = traceOf(formula::apply(tighterRounding, baseMethod));
    CHECK(fromOverlay.find("rounded to 2 dp (jurisdiction overlay)") != std::string::npos);

    // The two must differ. A test asserting only one provenance passes
    // whether or not the distinction exists.
    CHECK(fromMethod != fromOverlay);

    // Nor may the granularity be what tells them apart: the base method's own
    // rule, restated by a jurisdiction, is the jurisdiction's.
    auto const restated = traceOf(formula::apply(sameRounding, baseMethod));
    CHECK(restated.find("rounded to 1 dp (jurisdiction overlay)") != std::string::npos);
    CHECK(restated != fromMethod);

    // And which jurisdiction, when the overlay says.
    auto const cited = traceOf(formula::apply(citedTighterRounding, baseMethod));
    CHECK(cited.find("8. round(#7, in MPa) = 601/100 MPa [rounded to 2 dp (jurisdiction overlay: Example Standard "
                     "12:2021 NA, NA.4.1); nearest, ties away from zero]\n")
          != std::string::npos);
}

namespace
{
/// Two decimals of a megapascal, citing the national annex: the operation
/// alone, to be listed beside another.
inline constexpr auto citedTwoDecimals =
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        roundingAnnex);

/// What a trace says of `citedTwoDecimals` wherever it holds.
inline constexpr char citedRuleText[] = "rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, NA.4.1)";
} // namespace

// The three tests below each list the rounding override beside one other
// operation, in both orders. Each other operation rebuilds the method, and
// one that rebuilt the rule too -- `Rounding {}` for `rounding` -- would reset
// it to the method's own and drop the citation.

TEST_CASE("a rounding override survives a pin, in either order", "[overlay][trace]")
{
    CHECK(traceOf(formula::apply(formula::overlay(citedTwoDecimals, formula::pin_variant<Cube>()), baseMethod))
              .find(citedRuleText)
          != std::string::npos);
    CHECK(traceOf(formula::apply(formula::overlay(formula::pin_variant<Cube>(), citedTwoDecimals), baseMethod))
              .find(citedRuleText)
          != std::string::npos);
}

TEST_CASE("a rounding override survives a prune, in either order", "[overlay][trace]")
{
    CHECK(traceOf(formula::apply(formula::overlay(citedTwoDecimals, formula::prune_variant<Cylinder>()), baseMethod))
              .find(citedRuleText)
          != std::string::npos);
    CHECK(traceOf(formula::apply(formula::overlay(formula::prune_variant<Cylinder>(), citedTwoDecimals), baseMethod))
              .find(citedRuleText)
          != std::string::npos);
}

TEST_CASE("a rounding override survives a fixed constant, in either order", "[overlay][trace]")
{
    constexpr auto fixed = formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, nationalAnnex);
    CHECK(traceOf(formula::apply(formula::overlay(citedTwoDecimals, fixed), baseMethod)).find(citedRuleText)
          != std::string::npos);
    CHECK(traceOf(formula::apply(formula::overlay(fixed, citedTwoDecimals), baseMethod)).find(citedRuleText)
          != std::string::npos);

    // And an overlay that fixes a constant and says nothing of rounding
    // leaves the method's own rule the method's own.
    CHECK(traceOf(formula::apply(national, baseMethod)).find("rounded to 1 dp (method default)") != std::string::npos);
}

TEST_CASE("with_rounding rounds the method's result by the overlay's rule", "[overlay]")
{
    // 6.00666... MPa: 6.0 by the method's rule, 6.01 by the overlay's. In
    // pascals, as `evaluate_method` answers.
    constexpr auto overlaid = formula::apply(tighterRounding, baseMethod);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(baseMethod, unevenInputs)->value() == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cube>(overlaid, unevenInputs)->value() == formula::Rational { 6'010'000 });

    // Every variant, not only the first. The second is checked at a coarser
    // rule, against a load that lands on a tie there.
    constexpr auto coarser =
        formula::apply(formula::overlay(formula::with_rounding<unit::Megapascal,
                                                               formula::DecimalPlaces { 0 },
                                                               formula::RoundingMode::HalfAwayFromZero>()),
                       baseMethod);
    constexpr auto slanted = formula::environment(formula::Measured<Force> { formula::Rational { 101'250 } },
                                                  formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                  formula::Measured<EdgeY> { formula::Rational { 100 } });
    // 101250 N over 150 mm squared is 4.5 MPa: 4.5 by the method, 5 by the
    // overlay, rounding half away from zero.
    STATIC_REQUIRE(formula::evaluate_method<Cylinder>(baseMethod, slanted)->value() == formula::Rational { 4'500'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cylinder>(coarser, slanted)->value() == formula::Rational { 5'000'000 });
}

TEST_CASE("a later overlay's rounding rule holds over an earlier one's", "[overlay]")
{
    // Regional to two decimals, then national back to none. The national rule
    // is the one applied, and the national citation the one carried.
    constexpr formula::Citation regional { .reference = "Example Standard 12:2021 RA" };
    constexpr auto first = formula::apply(
        formula::overlay(
            formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                regional)),
        baseMethod);
    constexpr auto second = formula::apply(
        formula::overlay(
            formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
                roundingAnnex)),
        first);

    STATIC_REQUIRE(formula::evaluate_method<Cube>(first, unevenInputs)->value() == formula::Rational { 6'010'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cube>(second, unevenInputs)->value() == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(second.rounding.source() == roundingAnnex);
    STATIC_REQUIRE(second.rounding.provenance() == formula::RoundingProvenance::JurisdictionOverlay);
}
