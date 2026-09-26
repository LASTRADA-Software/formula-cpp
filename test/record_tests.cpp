// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Reference
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
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 103 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 103 } });

constexpr auto ctx = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

TEST_CASE("a context evaluates exactly as its own record's environment", "[record]")
{
    constexpr auto strength = var<Force> / (var<EdgeX> * var<EdgeY>);
    constexpr auto viaContext = formula::checked_evaluate<Strength>(strength, ctx);
    constexpr auto viaEnvironment = formula::checked_evaluate<Strength>(strength, here);

    // 6 MPa from this record. The reference's 4 MPa would mean the context
    // read the wrong record; equality with `here` alone could not tell a
    // context that forwards from one that ignores its records entirely,
    // so the reference is deliberately different.
    STATIC_REQUIRE(viaContext->measurement() == formula::Measured<Strength> { formula::Rational { 6 } });
    STATIC_REQUIRE(viaContext == viaEnvironment);
}

TEST_CASE("a context binds each role to the record it was given", "[record]")
{
    STATIC_REQUIRE(ctx.template record<Reference>().key()
                   == formula::record_key(formula::sample_id(23), formula::test_id(3)));
    STATIC_REQUIRE(ctx.this_record().key().test().value() == 5);
}

// Every entry point that takes an environment takes a context unchanged,
// because the context *is* its own record's environment. Each case below
// compares the context's result with the plain environment's, over a
// reference record that holds every quantity this record does, each one
// that matters with a different value. A context that read from the
// reference instead of from its own record would still compile, and would
// give the reference's answer; each case names that answer.
namespace
{
struct Cylinder
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", unit::One>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};

constexpr auto hereInFull = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                                 formula::Measured<EdgeX> { formula::Rational { 139 } },
                                                 formula::Measured<EdgeY> { formula::Rational { 103 } },
                                                 formula::Measured<ShapeFactor> { formula::Rational { 1 } },
                                                 formula::Measured<Diameter> { formula::Rational { 162 } });
constexpr auto thereInFull = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                                  formula::Measured<EdgeX> { formula::Rational { 139 } },
                                                  formula::Measured<EdgeY> { formula::Rational { 103 } },
                                                  formula::Measured<ShapeFactor> { formula::Rational { 1 } },
                                                  formula::Measured<Diameter> { formula::Rational { 113 } });
constexpr auto fullContext = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), hereInFull),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), thereInFull));

constexpr auto strengthFormula = var<Force> / (var<EdgeX> * var<EdgeY>);

// Two variants whose results differ: 6 MPa for a cube and 4.4 MPa for a
// cylinder from this record, so a context that selected the wrong one fails.
constexpr auto twoShapes = formula::method(
    formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 70'000 }),
                                             formula::Verdict { "below minimum load" }),
                         formula::constraint(var<EdgeX> <= formula::constant<unit::Millimetre>(formula::Rational { 127 }),
                                             formula::Verdict { "too wide" })));

inline constexpr formula::BandTable<3> DiameterBands { formula::band(0, 1, 127, 1), formula::band(127, 1, 197, 1),
                                                       formula::band(197, 1, 277, 1) };
inline constexpr formula::BreakpointTable<3> DiameterPoints { formula::breakpoint(0), formula::breakpoint(127),
                                                              formula::breakpoint(197) };
} // namespace

TEST_CASE("a context passes for its record's environment in checked_evaluate", "[record-context]")
{
    // 6 MPa here; 4 MPa is the reference's.
    constexpr auto viaContext = formula::checked_evaluate<Strength>(strengthFormula, fullContext);
    STATIC_REQUIRE(viaContext == formula::checked_evaluate<Strength>(strengthFormula, hereInFull));
    STATIC_REQUIRE(viaContext->measurement() == formula::Measured<Strength> { formula::Rational { 6 } });
    STATIC_REQUIRE(viaContext->source() == formula::ValueSource::Derived);
}

TEST_CASE("a context carries whether its record's result was typed in", "[record-context]")
{
    // This record holds the strength as typed in, 7 MPa; the reference holds
    // no strength at all. A context that lost `is_entered` would derive
    // 6 MPa, and one that read the reference would derive 4 MPa: only
    // inheriting the own record's environment gives 7, typed in.
    constexpr auto typedIn =
        formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                             formula::Measured<EdgeX> { formula::Rational { 139 } },
                             formula::Measured<EdgeY> { formula::Rational { 103 } },
                             formula::entered(formula::Measured<Strength> { formula::Rational { 7 } }));
    constexpr auto typedInContext = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), typedIn),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    constexpr auto viaContext = formula::checked_evaluate<Strength>(strengthFormula, typedInContext);
    STATIC_REQUIRE(decltype(typedInContext)::is_entered<Strength>);
    STATIC_REQUIRE(viaContext == formula::checked_evaluate<Strength>(strengthFormula, typedIn));
    STATIC_REQUIRE(viaContext->source() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(viaContext->measurement() == formula::Measured<Strength> { formula::Rational { 7 } });
}

TEST_CASE("a context passes for its record's environment in evaluate_method", "[record-context]")
{
    // Coherent SI, rounded to 0.1 MPa. Here: a cube 6 MPa, a cylinder 4.4 MPa
    // (85 902 N over 139 x 139 mm). The reference would give 4 MPa and 3 MPa.
    // A context that selected the cylinder for a cube would give 4.4 MPa, and
    // the reverse 6 MPa.
    constexpr auto cube = formula::evaluate_method<Cube>(twoShapes, fullContext);
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(twoShapes, fullContext);
    STATIC_REQUIRE(cube == formula::evaluate_method<Cube>(twoShapes, hereInFull));
    STATIC_REQUIRE(cylinder == formula::evaluate_method<Cylinder>(twoShapes, hereInFull));
    STATIC_REQUIRE(**cube == formula::Rational { 6'000'000 });
    STATIC_REQUIRE(**cylinder == formula::Rational { 4'400'000 });
}

TEST_CASE("a context passes for its record's environment in check_method and check_all", "[record-context]")
{
    // Here the load is satisfied and the width violated; the reference's
    // 57 268 N would violate both.
    constexpr auto viaContext = formula::check_method(twoShapes, fullContext);
    STATIC_REQUIRE(viaContext == formula::check_method(twoShapes, hereInFull));
    STATIC_REQUIRE(viaContext[0].is_satisfied());
    STATIC_REQUIRE(viaContext[1].is_violated());
    constexpr auto allViaContext = formula::check_all(twoShapes.constraintSet, fullContext);
    STATIC_REQUIRE(allViaContext == formula::check_all(twoShapes.constraintSet, hereInFull));
    STATIC_REQUIRE(allViaContext[0].is_satisfied());
    STATIC_REQUIRE(allViaContext[1].is_violated());
}

TEST_CASE("a context passes for its record's environment in a predicate", "[record-context]")
{
    // 85 902 N > 70 000 N holds here; the reference's 57 268 N would not.
    constexpr auto heavy = var<Force> > formula::constant<unit::Newton>(formula::Rational { 70'000 });
    constexpr auto viaContext = formula::checked_evaluate_predicate(heavy, fullContext);
    STATIC_REQUIRE(viaContext == formula::checked_evaluate_predicate(heavy, hereInFull));
    STATIC_REQUIRE(**viaContext);
}

TEST_CASE("a context passes for its record's environment in the lookups", "[record-context]")
{
    // A 162 mm diameter falls in the second band, 2, and interpolates to 3.
    // The reference's 113 mm would give 1 and 240/127.
    constexpr auto banded = formula::banded_lookup<unit::Millimetre, DiameterBands, unit::One>(
        var<Diameter>, { formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 3 } });
    constexpr auto interpolated = formula::interpolating_lookup<unit::Millimetre, DiameterPoints, unit::One>(
        var<Diameter>, { formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 4 } });
    constexpr auto bandedViaContext = formula::checked_evaluate_si<formula::Rational>(banded, fullContext);
    constexpr auto interpolatedViaContext = formula::checked_evaluate_si<formula::Rational>(interpolated, fullContext);
    STATIC_REQUIRE(bandedViaContext == formula::checked_evaluate_si<formula::Rational>(banded, hereInFull));
    STATIC_REQUIRE(interpolatedViaContext == formula::checked_evaluate_si<formula::Rational>(interpolated, hereInFull));
    STATIC_REQUIRE(**bandedViaContext == formula::Rational { 2 });
    STATIC_REQUIRE(**interpolatedViaContext == formula::Rational { 3 });
}

TEST_CASE("a context passes for its record's environment in an overlaid method", "[record-context]")
{
    // 0.97 x 6 MPa = 5.82, rounded to 5.8 MPa. The reference's 0.97 x 4 MPa
    // would round to 3.9 MPa, and the method without the overlay gives 6.
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, formula::Citation { .reference = "Example Standard 14:2022 NA" })), twoShapes);
    constexpr auto viaContext = formula::evaluate_method<Cube>(overlaid, fullContext);
    STATIC_REQUIRE(viaContext == formula::evaluate_method<Cube>(overlaid, hereInFull));
    STATIC_REQUIRE(**viaContext == formula::Rational { 5'800'000 });
}

TEST_CASE("a context passes for its record's environment in explain", "[record-context]")
{
    // The whole rendered trace, not only the outcome: a context that read the
    // reference would print 57 268 N and 4 MPa on its lines.
    auto const viaContext = formula::explain<Strength>(strengthFormula, fullContext);
    auto const viaEnvironment = formula::explain<Strength>(strengthFormula, hereInFull);
    REQUIRE(viaContext.outcome == viaEnvironment.outcome);
    REQUIRE(viaContext.outcome.measurement() == formula::Measured<Strength> { formula::Rational { 6 } });
    REQUIRE(viaContext.trace.steps.size() == 5);
    REQUIRE(formula::render_trace(viaContext.trace, { .maxSteps = 200 })
            == formula::render_trace(viaEnvironment.trace, { .maxSteps = 200 }));
}

TEST_CASE("a context passes for its record's environment through a recording sink", "[record-context]")
{
    formula::Trace<> viaContext {};
    formula::Trace<> viaEnvironment {};
    auto const contextResult =
        formula::evaluate_method<Cube>(twoShapes, fullContext, formula::RecordingSink { viaContext });
    auto const environmentResult =
        formula::evaluate_method<Cube>(twoShapes, hereInFull, formula::RecordingSink { viaEnvironment });
    REQUIRE(contextResult == environmentResult);
    REQUIRE(**contextResult == formula::Rational { 6'000'000 });
    REQUIRE(!viaContext.empty());
    REQUIRE(formula::render_trace(viaContext, { .maxSteps = 200 })
            == formula::render_trace(viaEnvironment, { .maxSteps = 200 }));
}
