// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "forwarding_nodes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <concepts>
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
struct StartTemperature: formula::Quantity<StartTemperature, "T_0", "start temperature", unit::Celsius>
{
};
struct EndTemperature: formula::Quantity<EndTemperature, "T_1", "end temperature", unit::Celsius>
{
};

using OneDecimalOfMegapascal =
    formula::RoundingRule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

// Only the Cube variant reads the shape factor, so an overlay fixing it
// changes Cube and leaves Cylinder alone. Its one constraint of its own reads
// only the load, which no overlay here fixes or derives.
inline constexpr auto baseMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    OneDecimalOfMegapascal {},
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 47'300 }),
                                             formula::Verdict { "the load is below the method's minimum" })));

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
    formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 863, 1'000 }, nationalAnnex));

/// The shape factor as `national` fixes it, taken out of the method `apply`
/// produced: only an overlay builds an overridden constant, so a formula that
/// both fixes and reads a quantity is assembled from a copy of one.
[[nodiscard]] constexpr auto fixedShapeFactor()
{
    // `k_s * F / (...)` parses as `(k_s * F) / (...)`.
    return std::get<0>(formula::apply(national, baseMethod).variantSet.cases).expression.lhs.lhs;
}

inline constexpr formula::Citation roundingAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.4.1" };

/// Two decimal places of a megapascal where the base method keeps one.
inline constexpr auto tighterRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        formula::Citation { .reference = "Example Standard 12:2021 NA" }));

/// The same, citing where the jurisdiction states it.
inline constexpr auto citedTighterRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        roundingAnnex));

/// The base method's own granularity, restated by a jurisdiction.
inline constexpr auto sameRounding = formula::overlay(
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
        formula::Citation { .reference = "Example Standard 12:2021 NA" }));

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
    constexpr auto fixed = formula::overlay(formula::with_constant<Ratio>(
        formula::Rational { 4 }, formula::Citation { .reference = "Example Standard 12:2021 NA" }));
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
inline constexpr formula::BandTable<2> RatioBands { formula::band(0, 1, 277, 100), formula::band(277, 100, 613, 100) };
inline constexpr formula::BreakpointTable<2> RatioPoints { formula::breakpoint(0), formula::breakpoint(831, 100) };

inline constexpr formula::Citation acceptanceAnnex { .title = "Acceptance",
                                                     .reference = "Example Standard 12:2021 NA",
                                                     .section = "NA.6" };

inline constexpr formula::Citation laterAcceptanceAnnex { .reference = "Example Standard 12:2024 NA", .section = "NA.6" };

/// A jurisdiction's three constraints in place of the base method's one --
/// two of them with no counterpart in the base. Against `inputs` (90 kN over
/// 150 mm by 100 mm, shape factor 1) the first holds (150 mm <= 173 mm), the
/// second does not (90 kN < 97.3 kN), and the third holds (1 <= 1); with the
/// shape factor left unmeasured the third is not checked. Three different
/// outcomes at three indices, so a result read at the wrong index shows.
inline constexpr auto threeConstraintSet =
    formula::constraints(formula::constraint(var<EdgeX> <= var<EdgeY> * formula::number(formula::Rational { 173, 100 }),
                                             formula::Verdict { "the edges differ by more than the annex allows" }),
                         formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 97'300 }),
                                             formula::Verdict { "the load is below the annex's minimum" }),
                         formula::constraint(var<ShapeFactor> <= formula::number(formula::Rational { 1 }),
                                             formula::Verdict { "the shape factor exceeds one" }));

inline constexpr auto threeConstraints = formula::overlay(formula::with_constraints(threeConstraintSet, acceptanceAnnex));

/// One constraint, reading only the load -- 90 kN < 97.3 kN, so violated -- for
/// the cases where a constraint reading the shape factor would be refused.
inline constexpr auto oneConstraintSet =
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 97'300 }),
                                             formula::Verdict { "the load is below the annex's minimum" }));

/// `inputs` with the shape factor left unmeasured.
inline constexpr auto inputsWithUnmeasuredFactor =
    formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                         formula::Measured<EdgeX> { formula::Rational { 150 } },
                         formula::Measured<EdgeY> { formula::Rational { 100 } },
                         formula::Measured<ShapeFactor> {});

/// The derivation `check_method` records for @p m against @p environment,
/// rendered.
template <typename M, typename Env>
[[nodiscard]] std::string acceptanceTraceOf(M const& m, Env const& environment)
{
    formula::Trace<> trace {};
    (void) formula::check_method(m, environment, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 40 });
}
} // namespace

TEST_CASE("an overlay overrides a constant and yields a method", "[overlay]")
{
    constexpr auto overlaid = formula::apply(national, baseMethod);
    STATIC_REQUIRE(formula::detail::IsWellFormedMethod<std::remove_cv_t<decltype(overlaid)>>::value);

    // 90000 N over 150 mm by 100 mm is 6 MPa. The base method's factor is 1,
    // the overlay's 0.863, and 6 * 0.863 = 5.178 MPa, which the method's own
    // rule rounds to 5.2. Both are in pascals, as `evaluate_method` answers.
    // The two results differ AFTER rounding: a fixture where they agreed could
    // not tell an applied overlay from an ignored one.
    constexpr auto base = formula::evaluate_method<Cube>(baseMethod, inputs);
    constexpr auto after = formula::evaluate_method<Cube>(overlaid, inputs);
    STATIC_REQUIRE(base->value() == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(after->value() == formula::Rational { 5'200'000 });
    STATIC_REQUIRE(base->value() != after->value());

    // The overlay's value wins over the 1 the environment supplies above, and
    // the overlaid method does not need the environment to supply one at all.
    constexpr auto without = formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor);
    STATIC_REQUIRE(without->value() == formula::Rational { 5'200'000 });
}

TEST_CASE("an overlay's constant is stated in its quantity's declared unit", "[overlay]")
{
    // The shape factor above is dimensionless, where every unit is the same
    // unit; this fixes a length. 127 mm read as 127 mm gives 90000 N over
    // 150 mm by 127 mm, 4.72... MPa, which rounds to 4.7. Read as 127 m --
    // the coherent SI unit -- it would give 4.72... kPa, which rounds to
    // 0.0 MPa.
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<EdgeY>(
                           formula::Rational { 127 }, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       baseMethod);

    // No edge Y in the environment, so that only the overlay's can be read.
    constexpr auto withoutEdgeY = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                       formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                       formula::Measured<ShapeFactor> { formula::Rational { 1 } });
    constexpr auto after = formula::evaluate_method<Cube>(overlaid, withoutEdgeY);
    STATIC_REQUIRE(after->value() == formula::Rational { 4'700'000 });
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
    STATIC_REQUIRE(cube.lhs.lhs.value() == formula::Rational { 863, 1'000 });
    STATIC_REQUIRE(cube.lhs.lhs.source() == nationalAnnex);

    // It reads as the shape factor, not as 863/1000: the question a reader asks
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
    CHECK(*factor.fixedValue == formula::Rational { 863, 1'000 });
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
    constexpr auto fixed = fixedShapeFactor();
    auto const documentation = formula::document(var<ShapeFactor> + fixed);

    REQUIRE(documentation.symbols.size() == 1);
    REQUIRE(documentation.symbols[0].fixedValue.has_value());
    CHECK(*documentation.symbols[0].fixedValue == formula::Rational { 863, 1'000 });
    CHECK(documentation.symbols[0].alsoReadAsInput);
}

TEST_CASE("a quantity fixed and then read plainly is documented as both", "[overlay][document]")
{
    // The other order: the fixed use comes first, so a walk that let the
    // later plain read change nothing would say only "fixed at 863/1000" of a
    // quantity the formula also takes from the specimen.
    constexpr auto fixed = fixedShapeFactor();
    auto const documentation = formula::document(fixed + var<ShapeFactor>);

    REQUIRE(documentation.symbols.size() == 1);
    REQUIRE(documentation.symbols[0].fixedValue.has_value());
    CHECK(*documentation.symbols[0].fixedValue == formula::Rational { 863, 1'000 });
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
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<ShapeFactor>(
            formula::Rational { 1'127, 1'000 }, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        everywhere);

    // The second variant, against an environment without the factor: this
    // compiles only if the rewrite reached past the first variant. 90000 N
    // over 150 mm squared is 4 MPa, and 4 * 1.127 = 4.508, which rounds to
    // 4.5.
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(overlaid, inputsWithoutShapeFactor);
    STATIC_REQUIRE(cylinder->value() == formula::Rational { 4'500'000 });

    // The constraint reads the overlay's 1.127, not a factor the environment
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
    // sqrt(12) = 3.4641..., rounded by the node to 3.46 and left there by the
    // method's three decimals.
    STATIC_REQUIRE(
        withRatioFixedAtFour(f::rounded_sqrt<unit::One, f::DecimalPlaces { 2 }, f::RoundingMode::HalfAwayFromZero>(
            r * f::number(Rational { 3 })))
        == Rational { 173, 50 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::when(r > f::number(Rational { 1 }), r, f::number(Rational { 0 })))
                   == Rational { 4 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::banded_lookup<unit::One, RatioBands, unit::One>(
                       r, { Rational { 1'043, 1'000 }, Rational { 719, 100 } }))
                   == Rational { 719, 100 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::interpolating_lookup<unit::One, RatioPoints, unit::One>(
                       r, { Rational { 137, 100 }, Rational { 839, 10 } }))
                   == Rational { 41'096, 1'000 });

    // An absolute value's operand, and both passes of a precision limit: the
    // level expression reads the fixed 4, and the limit, twice the level,
    // reads it through the placeholder -- which is itself no input and is
    // left alone.
    STATIC_REQUIRE(withRatioFixedAtFour(f::abs(r - f::number(Rational { 6 }))) == Rational { 2 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::precision_limit<f::PrecisionKind::Repeatability>(
                       r, f::precision_level<Ratio> * f::number(Rational { 2 })))
                   == Rational { 8 });

    // The count of a critical-value lookup: 4 selects the second of the rows
    // 3, 4 and 6, and the values come along unchanged. The values are
    // invented and deliberately unrealistic.
    STATIC_REQUIRE(withRatioFixedAtFour(f::critical_value<f::SampleSizeTable<3> { 3, 4, 6 }, unit::One>(
                       r, { Rational { 70 }, Rational { 20 }, Rational { 90 } }))
                   == Rational { 20 });

    // The leaves that name no quantity are carried over, contents and all.
    STATIC_REQUIRE(
        withRatioFixedAtFour(
            r * f::exact_lookup<ShapeKeys, unit::One>(Shape::Round, { Rational { 219, 100 }, Rational { 317, 100 } }))
        == Rational { 1'268, 100 });
    STATIC_REQUIRE(withRatioFixedAtFour(r * f::pi) == Rational { 12'566, 1'000 });
}

namespace
{
/// A gram squared, for a variance of masses in grams.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassSquared: formula::Quantity<MassSquared, "s2", "variance of the determinations", GramSquared>
{
};
} // namespace

TEST_CASE("with_constant reaches the radicand of a rounded square root", "[overlay]")
{
    // A jurisdiction fixing the variance at fixture A's 427/125 g^2. The
    // environment holds nothing, so this compiles only if the rewrite reached
    // the radicand; the value, 1.85 g (37/20000 kg), says the node was rebuilt
    // with its own unit, places and mode.
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(
            formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                var<MassSquared>))),
        formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<MassSquared>(formula::Rational { 427, 125 }, nationalAnnex)), m);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(overlaid, formula::environment())->value()
                   == formula::Rational { 37, 20'000 });

    // And the trace says whose number the root was taken of.
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Cube>(overlaid, formula::environment(), formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. s2 = 427/125 g2 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.3]\n"
             "2. round(sqrt(#1), to 2 dp of g) = 37/20 g [nearest, ties away from zero]\n"
             "3. round(#2, in g) = 37/20 g [rounded to 2 dp (method default); nearest, ties away from zero]\n"
             "4. #3 = 37/20 g [variant Cube (1st of 1), selected by tag]\n");
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
                   == formula::Rational { 5'200'000 });
}

TEST_CASE("pin_variant keeps only the variant it names", "[overlay]")
{
    // The middle variant, so that keeping the first or the last would not pass.
    constexpr auto pinned = formula::apply(
        formula::overlay(formula::pin_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        threeVariants);

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
    constexpr auto pinned = formula::apply(
        formula::overlay(formula::pin_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        single);

    STATIC_REQUIRE(std::is_same_v<std::remove_cv_t<decltype(pinned)>, std::remove_cv_t<decltype(single)>>);
}

TEST_CASE("prune_variant deletes only the variant it names", "[overlay]")
{
    constexpr auto pruned = formula::apply(
        formula::overlay(formula::prune_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        threeVariants);

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
        formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 781, 1'000 }, regional)), baseMethod);
    constexpr auto second = formula::apply(national, first);

    STATIC_REQUIRE(formula::evaluate_method<Cube>(first, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 4'700'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cube>(second, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 5'200'000 });
    STATIC_REQUIRE(std::get<0>(second.variantSet.cases).expression.lhs.lhs.source() == nationalAnnex);
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
        CHECK(*step.value == formula::Rational { 863, 1'000 });
    }
    CHECK(factorSteps == 1);

    // And it says so where a person reads it.
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(rendered.find("1. k_s = 863/1000 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, "
                        "NA.2.3]\n")
          != std::string::npos);
}

TEST_CASE("an overridden constant the overlay cited nothing for is traced as fixed, and as uncited", "[overlay][trace]")
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 863, 1'000 }, formula::Citation {})),
        baseMethod);

    formula::Trace<> trace;
    (void) formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
              .find("1. k_s = 863/1000 [fixed by jurisdiction overlay (no citation given)]\n")
          != std::string::npos);
}

TEST_CASE("the trace says where the rounding rule came from", "[trace][overlay]")
{
    auto const fromMethod = traceOf(baseMethod);
    CHECK(fromMethod.find("rounded to 1 dp (method default)") != std::string::npos);

    auto const fromOverlay = traceOf(formula::apply(tighterRounding, baseMethod));
    CHECK(fromOverlay.find("rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA)") != std::string::npos);

    // The two must differ. A test asserting only one provenance passes
    // whether or not the distinction exists.
    CHECK(fromMethod != fromOverlay);

    // Nor may the granularity be what tells them apart: the base method's own
    // rule, restated by a jurisdiction, is the jurisdiction's.
    auto const restated = traceOf(formula::apply(sameRounding, baseMethod));
    CHECK(restated.find("rounded to 1 dp (jurisdiction overlay: Example Standard 12:2021 NA)") != std::string::npos);
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
    CHECK(traceOf(formula::apply(formula::overlay(citedTwoDecimals,
                                                  formula::pin_variant<Cube>(
                                                      formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                                 baseMethod))
              .find(citedRuleText)
          != std::string::npos);
    CHECK(traceOf(formula::apply(formula::overlay(formula::pin_variant<Cube>(
                                                      formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                                                  citedTwoDecimals),
                                 baseMethod))
              .find(citedRuleText)
          != std::string::npos);
}

TEST_CASE("a rounding override survives a prune, in either order", "[overlay][trace]")
{
    CHECK(traceOf(formula::apply(formula::overlay(citedTwoDecimals,
                                                  formula::prune_variant<Cylinder>(
                                                      formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                                 baseMethod))
              .find(citedRuleText)
          != std::string::npos);
    CHECK(traceOf(formula::apply(formula::overlay(formula::prune_variant<Cylinder>(
                                                      formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                                                  citedTwoDecimals),
                                 baseMethod))
              .find(citedRuleText)
          != std::string::npos);
}

TEST_CASE("a rounding override survives a fixed constant, in either order", "[overlay][trace]")
{
    constexpr auto fixed = formula::with_constant<ShapeFactor>(formula::Rational { 863, 1'000 }, nationalAnnex);
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
    constexpr auto coarser = formula::apply(
        formula::overlay(
            formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
                formula::Citation { .reference = "Example Standard 12:2021 NA" })),
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

// ---------------------------------------------------------------------------
// Wholesale replacement and derived quantities
//
// Results are coherent SI and rounded by the method's one-decimal rule, so
// every fixture below is chosen so that the right and the plausible wrong
// answer stay apart after both: the base Cylinder is 4.0 MPa, its replacement
// 5.1 MPa; the base Cube 6.0 MPa, with a derived shape factor 4.0 MPa.
// ---------------------------------------------------------------------------

namespace
{
struct Diameter: formula::Quantity<Diameter, "d_m", "measured diameter", unit::Millimetre>
{
};

// The specimen with a diameter and no shape factor: a formula evaluated
// against it provably never asks for the shape factor.
inline constexpr auto roundSpecimen = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                           formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                           formula::Measured<EdgeY> { formula::Rational { 100 } },
                                                           formula::Measured<Diameter> { formula::Rational { 150 } });

inline constexpr formula::Citation replacementAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.3.1" };

// A cylinder's area from its measured diameter: 90000 N over pi * 150^2 / 4
// mm^2 is 5.09... MPa, which the method's rule rounds to 5.1.
inline constexpr auto areaFromDiameter =
    var<Force> / (formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 });

/// The derivation of @p m's `Tag` variant against @p environment, rendered.
template <typename Tag, typename M, typename Env>
[[nodiscard]] std::string traceOfVariant(M const& m, Env const& environment)
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Tag>(m, environment, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 40 });
}

inline constexpr formula::Citation pinAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.1.1" };
inline constexpr formula::Citation pruneAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.1.2" };
inline constexpr formula::Citation laterPruneAnnex { .reference = "Example Standard 12:2024 NA", .section = "NA.1.3" };

TEST_CASE("a pin says which jurisdiction made the variant mandatory", "[overlay][trace]")
{
    // Final review of phase 11, M3: a pinned method traced its selection
    // exactly as the base method did, so nothing said a jurisdiction had made
    // the variant mandatory.
    constexpr auto pinned = formula::apply(formula::overlay(formula::pin_variant<Cylinder>(pinAnnex)), threeVariants);

    CHECK(traceOfVariant<Cylinder>(pinned, inputs)
              .ends_with(" [variant Cylinder (2nd of 3), selected by tag; pinned by jurisdiction overlay: Example Standard "
                         "12:2021 NA, "
                         "NA.1.1]\n"));
    // The method not overlaid says nothing of a pin.
    CHECK(traceOfVariant<Cylinder>(threeVariants, inputs).ends_with(" [variant Cylinder (2nd of 3), selected by tag]\n"));
}

TEST_CASE("a prune says how many variants a jurisdiction deleted, and whose was the last", "[overlay][trace]")
{
    constexpr auto once = formula::apply(formula::overlay(formula::prune_variant<Cube>(pruneAnnex)), threeVariants);
    constexpr auto twice = formula::apply(formula::overlay(formula::prune_variant<Prism>(laterPruneAnnex)), once);

    CHECK(traceOfVariant<Cylinder>(once, inputs)
              .ends_with(
                  " [variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example Standard "
                  "12:2021 NA, NA.1.2]\n"));
    // Two overlays each pruned one: both counted, the later one named.
    CHECK(traceOfVariant<Cylinder>(twice, inputs)
              .ends_with(
                  " [variant Cylinder (2nd of 3), selected by tag; 2 of 3 pruned, the last by jurisdiction overlay: Example "
                  "Standard 12:2024 NA, NA.1.3]\n"));
}

TEST_CASE("a prune and a later pin are both said, each with its own citation", "[overlay][trace]")
{
    // One overlay cannot both pin and prune -- that is refused -- but one
    // jurisdiction may prune what a later one pins, and both decisions stand.
    constexpr auto prunedThenPinned =
        formula::apply(formula::overlay(formula::pin_variant<Cylinder>(pinAnnex)),
                       formula::apply(formula::overlay(formula::prune_variant<Cube>(pruneAnnex)), threeVariants));

    CHECK(traceOfVariant<Cylinder>(prunedThenPinned, inputs)
              .ends_with(
                  " [variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example Standard "
                  "12:2021 NA, NA.1.2; pinned by jurisdiction overlay: Example Standard 12:2021 NA, NA.1.1]\n"));
}

TEST_CASE("the text of a pin's or a prune's citation cannot forge a clause", "[overlay][trace][escape]")
{
    // Both citations are author text, escaped as every other piece of it is.
    constexpr formula::Citation forgingPrune { .reference = "X] [replaced by jurisdiction overlay: Y" };
    constexpr formula::Citation forgingPin { .reference = "Z; 2 of 3 pruned" };
    constexpr auto forged =
        formula::apply(formula::overlay(formula::pin_variant<Cylinder>(forgingPin)),
                       formula::apply(formula::overlay(formula::prune_variant<Cube>(forgingPrune)), threeVariants));

    CHECK(
        traceOfVariant<Cylinder>(forged, inputs)
            .ends_with(
                " [variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: X\\] \\[replaced by "
                "jurisdiction overlay: Y; pinned by jurisdiction overlay: Z\\; 2 of 3 pruned]\n"));
}

// The shape factor defined as the ratio of the two edges: 100 mm over 150 mm
// is 2/3, so the Cube reads 90000 N * 2/3 over 150 mm by 100 mm = 4.0 MPa
// where the base method's factor of 1 gives 6.0.
inline constexpr auto edgeRatio = var<EdgeY> / var<EdgeX>;
} // namespace

TEST_CASE("an overlay replaces a formula wholesale and the trace still explains it", "[overlay]")
{
    constexpr auto nationalB = formula::overlay(formula::replace_variant<Cylinder>(
        areaFromDiameter, formula::Citation { .reference = "Example Standard 12:2021 NA" }));
    constexpr auto overlaid = formula::apply(nationalB, baseMethod);

    auto const rendered = traceOfVariant<Cylinder>(overlaid, roundSpecimen);
    CHECK(rendered.find("variant Cylinder") != std::string::npos);
    CHECK(rendered.find("replaced by jurisdiction overlay") != std::string::npos);

    // The replacement is what ran: 5.1 MPa, where the base formula gives 4.0.
    STATIC_REQUIRE(formula::evaluate_method<Cylinder>(baseMethod, roundSpecimen)->value()
                   == formula::Rational { 4'000'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cylinder>(overlaid, roundSpecimen)->value() == formula::Rational { 5'100'000 });

    // The variant NOT replaced says nothing of a replacement, and still runs
    // the base formula.
    CHECK(traceOfVariant<Cube>(overlaid, inputs).find("replaced") == std::string::npos);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(overlaid, inputs)->value() == formula::Rational { 6'000'000 });
}

TEST_CASE("a replacement names whose formula it is, and keeps the variant's place", "[overlay][trace]")
{
    // The Cube pruned first, so that the Cylinder sits at position 0 of the
    // pack the replacement meets: a replacement that rebuilt the layout
    // instead of carrying it would report the 1st of 2.
    constexpr auto cited = formula::apply(
        formula::overlay(formula::prune_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::replace_variant<Cylinder>(areaFromDiameter, replacementAnnex)),
        threeVariants);
    auto const rendered = traceOfVariant<Cylinder>(cited, roundSpecimen);

    // Which jurisdiction, and the variant still the 2nd of 3 as published,
    // with the prune said.
    CHECK(rendered.find(" [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3.1]\n") != std::string::npos);
    CHECK(rendered.find("[variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example "
                        "Standard 12:2021 NA]")
          != std::string::npos);

    // It renders as the formula that runs, and documents its citation.
    constexpr auto replaced = std::get<0>(cited.variantSet.cases).expression;
    CHECK(formula::render(replaced) == formula::render(areaFromDiameter));
    auto const documentation = formula::document(replaced);
    REQUIRE(documentation.citations.size() == 1);
    CHECK(documentation.citations[0] == replacementAnnex);
    REQUIRE(documentation.replacedBy.size() == 1);
    CHECK(documentation.replacedBy[0] == replacementAnnex);
}

TEST_CASE("a replacement's step shows its value as the replacement's own step does", "[overlay][trace]")
{
    // The replacement rounds in megapascals, which is not the coherent SI unit
    // of a stress. The replaced-variant step computes nothing of its own, so
    // its value must read as the rounding's line does -- 4 MPa -- and not as
    // the same number in pascals with no unit at all.
    constexpr auto inMegapascals =
        formula::rounded<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Force> / (var<EdgeX> * var<EdgeX>) );
    constexpr auto replaced =
        formula::apply(formula::overlay(formula::replace_variant<Cylinder>(inMegapascals, replacementAnnex)), threeVariants);
    auto const rendered = traceOfVariant<Cylinder>(replaced, roundSpecimen);

    CHECK(rendered.find(" = 4 MPa [nearest, ties away from zero]\n") != std::string::npos);
    CHECK(rendered.find(" = 4 MPa [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3.1]\n")
          != std::string::npos);
}

TEST_CASE("a replacement by a consumer node that forwards the sink keeps the coherent SI unit", "[overlay][trace]")
{
    // The consumer's node records no step of its own, so the replaced-variant
    // step claims the node's operands, and none of them is the value it
    // passes on. Here the last is the area, a different dimension from the
    // stress the step holds: the step states the stress in coherent SI.
    constexpr auto byConsumer =
        formula::apply(formula::overlay(formula::replace_variant<Cylinder>(
                           forwarding::quotient(var<Force>, var<EdgeX> * var<EdgeX>), replacementAnnex)),
                       threeVariants);
    CHECK(traceOfVariant<Cylinder>(byConsumer, roundSpecimen)
              .find(" = 4000000 [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3.1]\n")
          != std::string::npos);

    // And over two Celsius readings, whose difference is a rise in kelvins
    // and not a reading: the last operand is a reading, of the right
    // dimension, and its unit would still state 57/5 K as a Celsius reading.
    constexpr auto baseRise = formula::method(
        formula::variants(formula::variant<Cube>(var<EndTemperature> - var<StartTemperature>)),
        formula::RoundingRule<unit::Kelvin, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero> {},
        formula::constraints());
    constexpr auto consumerRise =
        formula::apply(formula::overlay(formula::replace_variant<Cube>(
                           forwarding::difference(var<EndTemperature>, var<StartTemperature>), replacementAnnex)),
                       baseRise);
    auto const temperatures = formula::environment(formula::Measured<StartTemperature> { formula::Rational { 163, 10 } },
                                                   formula::Measured<EndTemperature> { formula::Rational { 277, 10 } });
    CHECK(traceOfVariant<Cube>(consumerRise, temperatures)
              .find(" = 57/5 [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3.1]\n")
          != std::string::npos);
}

TEST_CASE("a replacement by a consumer node with one operand of its dimension keeps the coherent SI unit",
          "[overlay][trace]")
{
    // One operand, a Celsius reading, of the method's own dimension: the
    // replaced-variant step claims exactly one step of the right dimension,
    // and only the node's kind says it is not the replacement's own. The rise
    // above 16.3 degrees C stays in coherent SI.
    constexpr auto baseRise = formula::method(
        formula::variants(formula::variant<Cube>(var<EndTemperature> - var<StartTemperature>)),
        formula::RoundingRule<unit::Kelvin, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero> {},
        formula::constraints());
    constexpr auto replaced = formula::apply(
        formula::overlay(formula::replace_variant<Cube>(
            forwarding::rise_above(var<EndTemperature>, formula::Rational { 5789, 20 }), replacementAnnex)),
        baseRise);
    auto const reading = formula::environment(formula::Measured<EndTemperature> { formula::Rational { 277, 10 } });
    CHECK(traceOfVariant<Cube>(replaced, reading)
              .find(" = 57/5 [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3.1]\n")
          != std::string::npos);
}

TEST_CASE("the documentation marks a replaced formula as replaced even when nothing was cited", "[overlay][document]")
{
    // Uncited, a replacement adds no citation -- and a page that said nothing
    // else would read exactly as the base standard's page for a formula that
    // is not the base standard's. The trace says "replaced by jurisdiction
    // overlay"; the page must say so too.
    constexpr auto replaced =
        std::get<1>(
            formula::apply(formula::overlay(formula::replace_variant<Cylinder>(areaFromDiameter, formula::Citation {})),
                           baseMethod)
                .variantSet.cases)
            .expression;
    auto const documentation = formula::document(replaced);
    CHECK(documentation.citations.empty());
    REQUIRE(documentation.replacedBy.size() == 1);
    CHECK(documentation.replacedBy[0] == formula::Citation {});

    // And a formula nothing replaced is not marked.
    CHECK(formula::document(areaFromDiameter).replacedBy.empty());
}

TEST_CASE("an overlay derives a quantity, and the method reads the definition", "[overlay]")
{
    constexpr auto derived =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(edgeRatio, nationalAnnex)), baseMethod);

    // Against a specimen with no shape factor: this compiles only if every use
    // of it was replaced by the definition.
    STATIC_REQUIRE(formula::evaluate_method<Cube>(derived, roundSpecimen)->value() == formula::Rational { 4'000'000 });
    STATIC_REQUIRE(formula::evaluate_method<Cube>(baseMethod, inputs)->value() == formula::Rational { 6'000'000 });

    // It keeps the quantity's identity: it renders as the shape factor.
    constexpr auto cube = std::get<0>(derived.variantSet.cases).expression;
    CHECK(formula::render(cube) == "k_s * F / (x_m * y_m)");
}

TEST_CASE("a derived quantity is traced as derived by the overlay", "[overlay][trace]")
{
    constexpr auto derived =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(edgeRatio, nationalAnnex)), baseMethod);
    auto const rendered = traceOfVariant<Cube>(derived, roundSpecimen);

    // The definition's own derivation first, then the quantity, equal to it.
    CHECK(rendered.find("3. #1 / #2 = 2/3\n"
                        "4. k_s = #3 = 2/3 [derived by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, "
                        "NA.2.3]\n")
          != std::string::npos);

    // Uncited, it still says so.
    constexpr auto uncited =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(edgeRatio, formula::Citation {})), baseMethod);
    CHECK(traceOfVariant<Cube>(uncited, roundSpecimen)
              .find("4. k_s = #3 = 2/3 [derived by jurisdiction overlay (no citation given)]\n")
          != std::string::npos);
}

TEST_CASE("the documentation marks a derived quantity as derived, with its definition and citation", "[overlay][document]")
{
    constexpr auto cube =
        std::get<0>(formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(edgeRatio, nationalAnnex)), baseMethod)
                        .variantSet.cases)
            .expression;
    auto const documentation = formula::document(cube);

    // The quantity's row, then the inputs its definition reads, then the rest.
    REQUIRE(documentation.symbols.size() == 4);
    formula::SymbolEntry const& factor = documentation.symbols[0];
    CHECK(factor.symbol == "k_s");
    REQUIRE(factor.derivedAs.has_value());
    CHECK(*factor.derivedAs == "y_m / x_m");
    CHECK(factor.derivedBy == nationalAnnex);
    CHECK(!factor.fixedValue.has_value());
    CHECK(!factor.alsoReadAsInput);
    CHECK(documentation.symbols[1].symbol == "y_m");
    CHECK(documentation.symbols[2].symbol == "x_m");
    CHECK(documentation.symbols[3].symbol == "F");
    for (std::size_t row = 1; row < documentation.symbols.size(); ++row)
        CHECK(!documentation.symbols[row].derivedAs.has_value());

    // In the page's own dialect, not always plain text.
    auto const latex = formula::document<formula::Dialect::LaTeX>(cube);
    REQUIRE(latex.symbols[0].derivedAs.has_value());
    CHECK(*latex.symbols[0].derivedAs == formula::render<formula::Dialect::LaTeX>(edgeRatio));
    CHECK(*latex.symbols[0].derivedAs != "y_m / x_m");
}

TEST_CASE("a quantity read plainly and derived is documented as both, in either order", "[overlay][document]")
{
    constexpr auto derivedFactor =
        std::get<0>(formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(
                                       edgeRatio, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                                   baseMethod)
                        .variantSet.cases)
            .expression.lhs.lhs;

    auto const readFirst = formula::document(var<ShapeFactor> + derivedFactor);
    REQUIRE(readFirst.symbols[0].derivedAs.has_value());
    CHECK(readFirst.symbols[0].alsoReadAsInput);

    auto const derivedFirst = formula::document(derivedFactor + var<ShapeFactor>);
    REQUIRE(derivedFirst.symbols[0].derivedAs.has_value());
    CHECK(derivedFirst.symbols[0].alsoReadAsInput);
}

TEST_CASE("a later constant reaches inside an earlier definition", "[overlay]")
{
    // The shape factor defined as the ratio `r`, then `r` fixed at 0.743 by a
    // later overlay. `r` is read only inside the definition, so this compiles
    // only if the constant reached it there -- and the specimen supplies no
    // `r` at all. 6.0 MPa * 0.743 = 4.458 MPa, which rounds to 4.5.
    constexpr auto defined =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(
                           var<Ratio>, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       baseMethod);
    constexpr auto fixed = formula::apply(
        formula::overlay(formula::with_constant<Ratio>(formula::Rational { 743, 1'000 },
                                                       formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        defined);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(fixed, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 4'500'000 });
}

TEST_CASE("a later substitution for a quantity holds over an earlier one, of either kind", "[overlay]")
{
    // Defined, then fixed: the constant holds, 6.0 * 0.863 = 5.178 -> 5.2 MPa.
    constexpr auto definedThenFixed =
        formula::apply(national,
                       formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(
                                          edgeRatio, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                                      baseMethod));
    STATIC_REQUIRE(formula::evaluate_method<Cube>(definedThenFixed, roundSpecimen)->value()
                   == formula::Rational { 5'200'000 });

    // Fixed, then defined: the definition holds, 4.0 MPa.
    constexpr auto fixedThenDefined =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(
                           edgeRatio, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       formula::apply(national, baseMethod));
    STATIC_REQUIRE(formula::evaluate_method<Cube>(fixedThenDefined, roundSpecimen)->value()
                   == formula::Rational { 4'000'000 });
}

TEST_CASE("a constant reaches inside a replacement listed before it", "[overlay]")
{
    // The Cube replaced by a formula that reads the shape factor, which the
    // same overlay then fixes: the replacement reads the overlay's 0.863, not
    // the specimen's, and the specimen supplies none. 90000 N * 0.863 over
    // 150 mm by 150 mm is 3.452 MPa, which rounds to 3.5.
    constexpr auto overlaid = formula::apply(
        formula::overlay(
            formula::replace_variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeX>),
                                           formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::with_constant<ShapeFactor>(formula::Rational { 863, 1'000 },
                                                formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        baseMethod);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(overlaid, inputsWithoutShapeFactor)->value()
                   == formula::Rational { 3'500'000 });
}

TEST_CASE("an overlay whose operations hold expressions says it cannot be default-constructed", "[overlay]")
{
    // A replacement and a definition each hold an expression, and one holding
    // a lookup cannot be default-constructed -- its corrections have to be
    // stated. The overlay holding them, and a tuple of such overlays, must
    // SAY so, rather than answer yes or fail to compile, which is what a
    // default member initialiser on an expression member does under a
    // tuple's probe (see `Corrections`, `lookup.hpp`).
    constexpr auto lookup = formula::banded_lookup<unit::One, RatioBands, unit::One>(
        var<Ratio>, { formula::Rational { 1'043, 1'000 }, formula::Rational { 719, 100 } });
    using Replacing = decltype(formula::overlay(formula::replace_variant<Cube>(
        lookup * var<Force> / (var<EdgeX> * var<EdgeY>), formula::Citation { .reference = "Example Standard 12:2021 NA" })));
    using Deriving = decltype(formula::overlay(
        formula::add_derived<ShapeFactor>(lookup, formula::Citation { .reference = "Example Standard 12:2021 NA" })));
    // Constraints hold predicates, which hold expressions, so a replacement
    // of them is held to the same.
    using Constraining = decltype(formula::overlay(formula::with_constraints(
        formula::constraints(formula::constraint(lookup > formula::number(formula::Rational { 0 }),
                                                 formula::Verdict { "the correction is not positive" })),
        formula::Citation { .reference = "Example Standard 12:2021 NA" })));

    STATIC_REQUIRE(!std::is_default_constructible_v<Replacing>);
    STATIC_REQUIRE(!std::default_initializable<Replacing>);
    STATIC_REQUIRE(!std::is_default_constructible_v<std::tuple<Replacing, Replacing>>);
    STATIC_REQUIRE(!std::is_default_constructible_v<Deriving>);
    STATIC_REQUIRE(!std::default_initializable<Deriving>);
    STATIC_REQUIRE(!std::is_default_constructible_v<std::tuple<Deriving, Deriving>>);
    STATIC_REQUIRE(!std::is_default_constructible_v<Constraining>);
    STATIC_REQUIRE(!std::default_initializable<Constraining>);
    STATIC_REQUIRE(!std::is_default_constructible_v<std::tuple<Constraining, Constraining>>);

    // The control: all three are still copyable, which `overlay()` and
    // `apply` rely on.
    STATIC_REQUIRE(std::is_copy_constructible_v<Replacing>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Deriving>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Constraining>);
}

TEST_CASE("an overlay supplies acceptance logic of a different arity", "[overlay][constraint]")
{
    // Base: one constraint. Overlay: three, including one the base has no
    // counterpart for. The RESULT ARRAY SIZE differs, which is the point.
    constexpr auto overlaid = formula::apply(threeConstraints, baseMethod);
    constexpr auto baseOutcomes = formula::check_method(baseMethod, inputs);
    constexpr auto afterOutcomes = formula::check_method(overlaid, inputs);
    STATIC_REQUIRE(baseOutcomes.size() == 1);
    STATIC_REQUIRE(afterOutcomes.size() == 3);
}

TEST_CASE("an overlay supplies fewer constraints, and a later overlay's hold", "[overlay][constraint]")
{
    // Three, replaced by a later jurisdiction's one: the later overlay's
    // constraint holds, and so does its citation -- not the earlier one's.
    constexpr auto three = formula::apply(threeConstraints, baseMethod);
    constexpr auto one =
        formula::apply(formula::overlay(formula::with_constraints(oneConstraintSet, laterAcceptanceAnnex)), three);
    constexpr auto outcomes = formula::check_method(one, inputs);
    STATIC_REQUIRE(outcomes.size() == 1);
    STATIC_REQUIRE(outcomes[0].verdict() == formula::Verdict { "the load is below the annex's minimum" });
    STATIC_REQUIRE(formula::constraint_origin(three).source() == acceptanceAnnex);
    STATIC_REQUIRE(formula::constraint_origin(one).provenance() == formula::ConstraintProvenance::JurisdictionOverlay);
    STATIC_REQUIRE(formula::constraint_origin(one).source() == laterAcceptanceAnnex);
}

TEST_CASE("an overlay removes every constraint, and the trace says by whose authority", "[overlay][constraint][trace]")
{
    // Accepted, not refused: a method declared with `constraints()` is well
    // formed, and a jurisdiction may check nothing the base standard checks.
    // What must not happen is a silent removal -- so the trace still records
    // the method's constraints, none of them, as the overlay's.
    constexpr auto none =
        formula::apply(formula::overlay(formula::with_constraints(formula::constraints(), acceptanceAnnex)), baseMethod);
    STATIC_REQUIRE(formula::check_method(none, inputs).size() == 0);

    CHECK(acceptanceTraceOf(none, inputs)
          == "1. acceptance(none) [jurisdiction overlay: Acceptance, Example Standard 12:2021 NA, NA.6]\n");

    // A method declared with no constraints of its own reads differently:
    // nothing was removed, and nobody's authority is claimed but its own.
    CHECK(acceptanceTraceOf(threeVariants, inputs) == "1. acceptance(none) [the method's own constraints]\n");
}

TEST_CASE("a method rebuilt from an overlaid method's parts keeps the jurisdiction's constraints as the jurisdiction's",
          "[overlay][constraint][trace]")
{
    // Whose the constraints are travels with them, as a rounding rule's
    // provenance travels with the rule: taken out of an overlaid method and
    // put into a new one, they are still the jurisdiction's, and say so.
    constexpr auto overlaid = formula::apply(threeConstraints, baseMethod);
    constexpr auto rebuilt = formula::method(overlaid.variantSet, overlaid.rounding, overlaid.constraintSet);
    STATIC_REQUIRE(formula::constraint_origin(rebuilt).provenance() == formula::ConstraintProvenance::JurisdictionOverlay);
    STATIC_REQUIRE(formula::constraint_origin(rebuilt).source() == acceptanceAnnex);

    auto const traced = acceptanceTraceOf(rebuilt, inputs);
    CHECK(traced.find("[the load is below the annex's minimum; jurisdiction overlay: Acceptance, "
                      "Example Standard 12:2021 NA, NA.6]")
          != std::string::npos);
    CHECK(traced.find("the method's own") == std::string::npos);

    // And the other way round: the base method's own constraints, put beside
    // an overlaid method's variants and rounding, stay the method's own.
    constexpr auto own = formula::method(overlaid.variantSet, overlaid.rounding, baseMethod.constraintSet);
    STATIC_REQUIRE(formula::constraint_origin(own).provenance() == formula::ConstraintProvenance::MethodOwn);
}

TEST_CASE("each verdict says whether the method or a jurisdiction's overlay supplied it", "[overlay][constraint][trace]")
{
    auto const own = acceptanceTraceOf(baseMethod, inputs);
    auto const overlaid = acceptanceTraceOf(formula::apply(threeConstraints, baseMethod), inputs);

    CHECK(own
          == "1. F = 90000 N\n"
             "2. 47300 N\n"
             "3. require #1 >= #2 [satisfied; the method's own constraint]\n"
             "4. acceptance(#3) [the method's own constraints]\n");
    CHECK(overlaid.find("require #6 >= #7 [the load is below the annex's minimum; jurisdiction overlay: Acceptance, "
                        "Example Standard 12:2021 NA, NA.6]")
          != std::string::npos);
    CHECK(overlaid.find("[satisfied; jurisdiction overlay: Acceptance, Example Standard 12:2021 NA, NA.6]")
          != std::string::npos);

    // The two provenances differ, and the overlaid trace names the method's
    // own nowhere: every one of its verdicts is the jurisdiction's.
    CHECK(own != overlaid);
    CHECK(overlaid.find("the method's own") == std::string::npos);
}

TEST_CASE("the nth outcome and the nth verdict in the trace answer for the nth constraint", "[overlay][constraint][trace]")
{
    // Three different outcomes at three indices -- see `threeConstraintSet` --
    // so an outcome reported at another constraint's index cannot pass.
    constexpr auto overlaid = formula::apply(threeConstraints, baseMethod);
    constexpr auto outcomes = formula::check_method(overlaid, inputsWithUnmeasuredFactor);
    STATIC_REQUIRE(outcomes[0].is_satisfied());
    STATIC_REQUIRE(outcomes[1].verdict() == formula::Verdict { "the load is below the annex's minimum" });
    STATIC_REQUIRE(outcomes[2].is_not_checked());

    // The trace holds the same verdicts in the same order: the method's
    // constraints step names them as its operands, first to last.
    formula::Trace<> trace {};
    auto const traced = formula::check_method(overlaid, inputsWithUnmeasuredFactor, formula::RecordingSink<> { trace });
    CHECK(traced == outcomes);
    auto const& acceptance = trace.steps[trace.root()];
    REQUIRE(acceptance.kind == formula::StepKind::AcceptanceChecked);
    REQUIRE(acceptance.operands.size() == outcomes.size());
    for (std::size_t index = 0; index < outcomes.size(); ++index)
    {
        CHECK(trace.steps[acceptance.operands[index]].kind == formula::StepKind::Constraint);
        CHECK(trace.steps[acceptance.operands[index]].outcome == outcomes[index]);
    }
}

TEST_CASE("an overlay's constraints stay the jurisdiction's through every other operation, in either order",
          "[overlay][constraint]")
{
    constexpr auto constrain = formula::with_constraints(oneConstraintSet, acceptanceAnnex);
    constexpr auto isTheOverlays = [](auto const& m) {
        return formula::constraint_origin(m).provenance() == formula::ConstraintProvenance::JurisdictionOverlay
               && formula::constraint_origin(m).source() == acceptanceAnnex && formula::check_method(m, inputs).size() == 1;
    };

    constexpr auto pin = formula::pin_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto prune =
        formula::prune_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto replace = formula::replace_variant<Cylinder>(
        var<Force> / (var<EdgeY> * var<EdgeY>), formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto round =
        formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto fix = formula::with_constant<ShapeFactor>(
        formula::Rational { 863, 1'000 }, formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto derive = formula::add_derived<ShapeFactor>(
        var<EdgeY> / var<EdgeX>, formula::Citation { .reference = "Example Standard 12:2021 NA" });

    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, pin), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(pin, constrain), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, prune), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(prune, constrain), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, replace), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(replace, constrain), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, round), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(round, constrain), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, fix), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(fix, constrain), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(constrain, derive), baseMethod)));
    STATIC_REQUIRE(isTheOverlays(formula::apply(formula::overlay(derive, constrain), baseMethod)));

    // And across overlays: a later overlay that leaves the constraints alone
    // leaves them the earlier jurisdiction's.
    STATIC_REQUIRE(
        isTheOverlays(formula::apply(formula::overlay(round), formula::apply(formula::overlay(constrain), baseMethod))));
}

TEST_CASE("a constant listed after an overlay's constraints reaches inside them", "[overlay][constraint]")
{
    // The third constraint reads the shape factor, which the same overlay
    // then fixes at 1.127. Checked against an environment holding no shape
    // factor at all -- which compiles only if the rewrite reached the new
    // constraints -- it reads the overlay's 1.127, and is violated.
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constraints(threeConstraintSet, acceptanceAnnex),
                                        formula::with_constant<ShapeFactor>(
                                            formula::Rational { 1'127, 1'000 },
                                            formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       baseMethod);
    constexpr auto outcomes = formula::check_method(overlaid, inputsWithoutShapeFactor);
    STATIC_REQUIRE(outcomes.size() == 3);
    STATIC_REQUIRE(outcomes[2].verdict() == formula::Verdict { "the shape factor exceeds one" });
}

TEST_CASE("an overlay's constraint judges a category code, and its verdict names the category",
          "[overlay][constraint][trace]")
{
    // Spec section 16.7's third jurisdiction, as far as it can be written
    // today: a category code in place of a numeric limit. The annex accepts
    // square specimens only -- 1 for a square, 0 for a round one. The key is
    // fixed where the constraint is written, because `exact_lookup` takes it
    // as a value and not from the environment, so this constraint judges the
    // round category for every specimen, not the specimen's own category --
    // which no constraint can judge yet. What it does pin is that the
    // verdict's derivation names the category by its enumerator.
    constexpr auto acceptedShape = formula::constraint(
        formula::exact_lookup<ShapeKeys, unit::One>(Shape::Round, { formula::Rational { 1 }, formula::Rational { 0 } })
            >= formula::number(formula::Rational { 1 }),
        formula::Verdict { "the annex does not accept this shape" });
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constraints(formula::constraints(acceptedShape), acceptanceAnnex)), baseMethod);

    constexpr auto outcomes = formula::check_method(overlaid, inputs);
    STATIC_REQUIRE(outcomes.size() == 1);
    STATIC_REQUIRE(outcomes[0].verdict() == formula::Verdict { "the annex does not accept this shape" });

    // The category by name, on the page and in the verdict's derivation --
    // never as its underlying value.
    CHECK(formula::render(std::get<0>(overlaid.constraintSet.constraintSet().items)).find("Round") != std::string::npos);
    CHECK(acceptanceTraceOf(overlaid, inputs)
          == "1. lookup(key Round) = 0\n"
             "2. 1\n"
             "3. require #1 >= #2 [the annex does not accept this shape; jurisdiction overlay: Acceptance, "
             "Example Standard 12:2021 NA, NA.6]\n"
             "4. acceptance(#3) [jurisdiction overlay: Acceptance, Example Standard 12:2021 NA, NA.6]\n");
}

TEST_CASE("an operation given an empty citation says so in every clause", "[overlay][trace]")
{
    // Final re-review of phase 11, M4: every operation takes a citation
    // argument, but an empty one compiles -- `{}`, or an operation's aggregate
    // built directly with no citation at all. A clause reading `pinned by
    // jurisdiction overlay` and nothing more would then look cited to a reader
    // who does not know it could have said more, so each says it was not.
    constexpr auto pinnedEmpty = formula::apply(formula::overlay(formula::pin_variant<Cylinder>({})), threeVariants);
    CHECK(traceOfVariant<Cylinder>(pinnedEmpty, inputs)
              .ends_with(" [variant Cylinder (2nd of 3), selected by tag; pinned by jurisdiction overlay (no citation "
                         "given)]\n"));

    constexpr auto pinnedByAggregate = formula::apply(formula::overlay(formula::VariantPin<Cylinder> {}), threeVariants);
    CHECK(traceOfVariant<Cylinder>(pinnedByAggregate, inputs)
              .ends_with(" [variant Cylinder (2nd of 3), selected by tag; pinned by jurisdiction overlay (no citation "
                         "given)]\n"));

    constexpr auto prunedEmpty = formula::apply(formula::overlay(formula::prune_variant<Cube>({})), threeVariants);
    CHECK(traceOfVariant<Cylinder>(prunedEmpty, inputs)
              .ends_with(" [variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay (no "
                         "citation given)]\n"));

    constexpr auto fixedEmpty = formula::apply(
        formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 863, 1'000 }, {})), baseMethod);
    constexpr auto fixedByAggregate = formula::apply(
        formula::overlay(formula::ConstantOverride<ShapeFactor> { formula::Rational { 863, 1'000 } }), baseMethod);
    for (auto const& fixedTrace: { traceOfVariant<Cube>(fixedEmpty, inputsWithoutShapeFactor),
                                   traceOfVariant<Cube>(fixedByAggregate, inputsWithoutShapeFactor) })
        CHECK(fixedTrace.find("1. k_s = 863/1000 [fixed by jurisdiction overlay (no citation given)]\n")
              != std::string::npos);

    constexpr auto roundedEmpty = formula::apply(
        formula::overlay(
            formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                {})),
        baseMethod);
    CHECK(traceOf(roundedEmpty).find("rounded to 2 dp (jurisdiction overlay (no citation given)); ") != std::string::npos);

    constexpr auto replacedEmpty =
        formula::apply(formula::overlay(formula::replace_variant<Cylinder>(areaFromDiameter, {})), baseMethod);
    CHECK(traceOfVariant<Cylinder>(replacedEmpty, roundSpecimen)
              .find(" [replaced by jurisdiction overlay (no citation given)]\n")
          != std::string::npos);

    constexpr auto constrainedEmpty =
        formula::apply(formula::overlay(formula::with_constraints(oneConstraintSet, {})), baseMethod);
    auto const acceptance = acceptanceTraceOf(constrainedEmpty, inputs);
    CHECK(acceptance.find(" [the load is below the annex's minimum; jurisdiction overlay (no citation given)]\n")
          != std::string::npos);
    CHECK(acceptance.ends_with(" [jurisdiction overlay (no citation given)]\n"));
}
