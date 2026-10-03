// SPDX-License-Identifier: Apache-2.0
//
// A trace step's number is always shown with the unit it is in: borrowed from
// the operand steps where that is safe, the coherent unit's symbol otherwise,
// and nothing only for a dimensionless value.
#include <formula-cpp/binning.hpp>
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/conformity.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "forwarding_nodes.hpp"
#include "household_bill.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

struct SampleMass: formula::Quantity<SampleMass, "m", "sample mass", unit::Gram>
{
};
struct TareMass: formula::Quantity<TareMass, "m_t", "tare mass", unit::Gram>
{
};
struct Edge: formula::Quantity<Edge, "a", "edge length", unit::Millimetre>
{
};
struct Breadth: formula::Quantity<Breadth, "b", "edge breadth", unit::Millimetre>
{
};

// Grams with no symbol: a scale the number alone cannot name.
inline constexpr formula::Unit UnnamedGram { .dimension = formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000 };
struct UnnamedMass: formula::Quantity<UnnamedMass, "m_u", "mass in an unnamed unit", UnnamedGram>
{
};
struct UnnamedTotal: formula::Quantity<UnnamedTotal, "m_ut", "total mass in an unnamed unit", UnnamedGram>
{
};

struct HeavyMass: formula::Quantity<HeavyMass, "M", "heavy mass", unit::Kilogram>
{
};
struct StartTemperature: formula::Quantity<StartTemperature, "T_0", "start temperature", unit::Celsius>
{
};
struct EndTemperature: formula::Quantity<EndTemperature, "T_1", "end temperature", unit::Celsius>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

// Grams read to four decimals: grams still, whatever the precision.
inline constexpr formula::Unit FineGram { .dimension = formula::dim::Mass,
                                          .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 1000,
                                          .symbolText = formula::symbol("g"),
                                          .decimals = 4 };
struct FineMass: formula::Quantity<FineMass, "m_f", "finely read mass", FineGram>
{
};

// A dimensionless value whose unit has a symbol.
struct Share: formula::Quantity<Share, "s", "invented share", unit::Percent>
{
};

template <typename Expression, typename Bound>
formula::Trace<> recorded_trace(Expression const& formulaExpression, Bound const& inputs)
{
    formula::Trace<> recorded {};
    formula::RecordingSink<> recordingSink { recorded };
    (void) formula::checked_evaluate_si<Rational>(formulaExpression, inputs, recordingSink);
    return recorded;
}

template <typename Expression, typename Bound>
std::string trace_text(Expression const& formulaExpression, Bound const& inputs)
{
    return formula::render_trace(recorded_trace(formulaExpression, inputs), { .maxSteps = 20 });
}

// The particles of a class, a count.
struct ParticleCount: formula::Quantity<ParticleCount, "n", "particles in a class", unit::One>
{
};

// The permitted values, the bands and the rows below, all in the unnamed
// gram: a line that wrote them in that scale would write numbers a thousand
// times those of the kilograms written after them.
inline constexpr formula::BreakpointTable<2> UnnamedPermitted { formula::breakpoint(3), formula::breakpoint(5) };
inline constexpr formula::BandTable<2> UnnamedBands { formula::band(0, 1, 5, 1), formula::band(5, 1, 8, 1) };
inline constexpr formula::BreakpointTable<2> UnnamedRows { formula::breakpoint(2), formula::breakpoint(6) };
inline constexpr formula::BreakpointTable<1> UnnamedOnlyRow { formula::breakpoint(2) };

template <typename Expression>
std::string unnamed_trace_text(Expression const& formulaExpression, Rational unnamedGrams)
{
    return trace_text(formulaExpression, formula::environment(formula::Measured<UnnamedMass> { unnamedGrams }));
}
} // namespace

TEST_CASE("a product of two lengths names the coherent unit of an area", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<Edge> { Rational { 5 } },
                                             formula::Measured<Breadth> { Rational { 8 } });
    CHECK(trace_text(var<Edge> * var<Breadth>, inputs)
          == "1. a = 5 mm\n"
             "2. b = 8 mm\n"
             "3. #1 * #2 = 1/25000 m^2\n");
}

TEST_CASE("a value in a unit with no symbol is shown in the coherent unit, with its symbol", "[trace-render][shown-unit]")
{
    // 3 of an unnamed gram is 3/1000 kg: shown bare, the 3 would claim a scale
    // nothing on the line names.
    auto const inputs = formula::environment(formula::Measured<UnnamedMass> { Rational { 3 } });
    CHECK(trace_text(var<UnnamedMass> * Rational { 2 }, inputs).starts_with("1. m_u = 3/1000 kg\n"));
}

TEST_CASE("a dimensionless value is still a bare number", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } });
    CHECK(trace_text(var<SampleMass> / var<TareMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. m_t = 7 g\n"
             "3. #1 / #2 = 59/10\n");
}

TEST_CASE("a conformity row is shown in the unit its value is shown in", "[trace-render][shown-unit][conformity]")
{
    // The check states its limits in grams with no symbol. Its values read in
    // kilograms, so its rows do too: 36 of the unnamed gram against a row of
    // 30 to 40 of them is 9/250 kg against 3/100 to 1/25 kg.
    constexpr formula::Envelope<2> envelope { formula::LimitRow { formula::limit(Rational { 30 }),
                                                                  formula::limit(Rational { 40 }) },
                                              formula::LimitRow { formula::limit(Rational { 50 }), formula::unbounded } };
    constexpr auto massCheck = formula::conformity<UnnamedGram>(
        formula::series<SampleMass, 2>, envelope, formula::Verdict { "reject the specimen" });
    auto const masses = formula::environment(formula::measured_series<SampleMass>(
        formula::Measured<SampleMass> { Rational { 36 } }, formula::Measured<SampleMass> { Rational { 45 } }));
    formula::Trace<> recorded {};
    (void) formula::check_conformity(massCheck, masses, formula::RecordingSink<> { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. m = 36 g; 45 g\n"
             "2. conform(#1) [1 satisfied, 9/250 kg (from 3/100 to 1/25 kg); "
             "2 violated, 9/200 kg (at least 1/20 kg): reject the specimen]\n");
}

TEST_CASE("a derivation states a value in a unit with no symbol in the coherent unit, with its symbol",
          "[trace-render][shown-unit][worksheet]")
{
    // The header and the inputs list say what the step lines say: 3 of the
    // unnamed gram is 3/1000 kg, and twice it 3/500 kg.
    constexpr auto doubled = formula::calculation(formula::define<UnnamedTotal>(var<UnnamedMass> * Rational { 2 }));
    auto sheet = formula::worksheet(doubled, formula::environment(formula::Measured<UnnamedMass> { Rational { 3 } }));
    auto const explained = formula::explain_worksheet<UnnamedTotal>(sheet);
    CHECK(formula::render_derivation(explained, { .maxSteps = 20 })
          == "m_ut = m_u * 2 = 3/500 kg\n"
             "  1. m_u = 3/1000 kg\n"
             "  2. 2\n"
             "  3. #1 * #2 = 3/500 kg\n"
             "inputs\n"
             "  m_u = 3/1000 kg\n");
}

TEST_CASE("a value scaled by a pure number reads in its own unit", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    // On the right, as the outlier-rejection limit 6 % of the mean is written.
    CHECK(trace_text(Rational { 3, 50 } * var<SampleMass>, inputs)
          == "1. 3/50\n"
             "2. m = 413/10 g\n"
             "3. #1 * #2 = 1239/500 g\n");
    // On the left.
    CHECK(trace_text(var<SampleMass> * Rational { 3, 50 }, inputs)
          == "1. m = 413/10 g\n"
             "2. 3/50\n"
             "3. #1 * #2 = 1239/500 g\n");
    // Divided by a pure number.
    CHECK(trace_text(var<SampleMass> / Rational { 2 }, inputs)
          == "1. m = 413/10 g\n"
             "2. 2\n"
             "3. #1 / #2 = 413/20 g\n");
    // A pure number divided by a mass is no mass: the coherent unit, 1/kg.
    CHECK(trace_text(Rational { 2 } / var<SampleMass>, inputs)
          == "1. 2\n"
             "2. m = 413/10 g\n"
             "3. #1 / #2 = 20000/413 1/kg\n");
}

TEST_CASE("a sum of two values in one unit reads in it, at the finer precision", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } },
                                             formula::Measured<FineMass> { Rational { 12345, 10000 } });
    CHECK(trace_text(var<SampleMass> - var<TareMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. m_t = 7 g\n"
             "3. #1 - #2 = 343/10 g\n");
    // Grams declared at different decimals are grams: the sum is shown in
    // grams, and at the finer of the two precisions.
    formula::Trace<> const mixedPrecision = recorded_trace(var<SampleMass> + var<FineMass>, inputs);
    REQUIRE(mixedPrecision.steps.size() == 3);
    CHECK(formula::view(mixedPrecision.steps[2].unit.symbolText) == "g");
    CHECK(mixedPrecision.steps[2].unit.decimals == 4);
}

TEST_CASE("a sum of values in two units reads in the coherent unit", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<HeavyMass> { Rational { 1 } });
    CHECK(trace_text(var<SampleMass> + var<HeavyMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. M = 1 kg\n"
             "3. #1 + #2 = 10413/10000 kg\n");
}

TEST_CASE("a difference of two Celsius readings is an interval in kelvin, not a reading", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } },
                                             formula::Measured<EndTemperature> { Rational { 25 } });
    CHECK(trace_text(var<EndTemperature> - var<StartTemperature>, inputs)
          == "1. T_1 = 25 \xc2\xb0" "C\n"
             "2. T_0 = 20 \xc2\xb0" "C\n"
             "3. #1 - #2 = 5 K\n");
}

TEST_CASE("a negation and an absolute value keep their operand's unit, but not an offset one", "[trace-render][shown-unit]")
{
    auto const grams = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    CHECK(trace_text(-var<SampleMass>, grams)
          == "1. m = 413/10 g\n"
             "2. -#1 = -413/10 g\n");
    CHECK(trace_text(formula::abs(-var<SampleMass>), grams)
          == "1. m = 413/10 g\n"
             "2. -#1 = -413/10 g\n"
             "3. abs(#2) = 413/10 g\n");
    // -(20 degC) is no reading at -20 degC: the coherent unit.
    auto const celsius = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } });
    CHECK(trace_text(-var<StartTemperature>, celsius)
          == "1. T_0 = 20 \xc2\xb0" "C\n"
             "2. -#1 = -5863/20 K\n");
}

TEST_CASE("a conditional reads in its chosen branch's unit, offset or not", "[trace-render][shown-unit]")
{
    auto const strengths = formula::environment(formula::Measured<Strength> { Rational { 60 } });
    CHECK(trace_text(formula::when(var<Strength> > formula::constant<unit::Megapascal>(Rational { 473, 10 }),
                                   var<Strength>,
                                   formula::constant<unit::Megapascal>(Rational { 0 })),
                     strengths)
          == "1. f = 60 MPa\n"
             "2. 473/10 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60 MPa\n");
    // A branch's value is a point on its scale, so a Celsius branch reads in
    // degrees Celsius.
    auto const readings = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } },
                                               formula::Measured<EndTemperature> { Rational { 25 } });
    CHECK(trace_text(formula::when(var<EndTemperature> > var<StartTemperature>, var<EndTemperature>, var<StartTemperature>),
                     readings)
          == "1. T_1 = 25 \xc2\xb0" "C\n"
             "2. T_0 = 20 \xc2\xb0" "C\n"
             "3. T_1 = 25 \xc2\xb0" "C\n"
             "4. if #1 > #2 then #3 = 25 \xc2\xb0" "C\n");
}

TEST_CASE("a Celsius reading scaled by a pure number, and its absolute value, read in kelvin",
          "[trace-render][shown-unit]")
{
    // Twice 20 degC is twice 293.15 K, no reading at 40 degC; nor is the
    // absolute value of a reading shown as one.
    auto const celsius = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } });
    CHECK(trace_text(var<StartTemperature> * Rational { 2 }, celsius)
          == "1. T_0 = 20 \xc2\xb0" "C\n"
             "2. 2\n"
             "3. #1 * #2 = 5863/10 K\n");
    CHECK(trace_text(formula::abs(var<StartTemperature>), celsius)
          == "1. T_0 = 20 \xc2\xb0" "C\n"
             "2. abs(#1) = 5863/20 K\n");
}

TEST_CASE("a pure number scaled by a pure number borrows no unit", "[trace-render][shown-unit]")
{
    // Neither of two dimensionless sides says which one's unit the product
    // is in: it stays a bare number, whichever side the share is on.
    auto const shares = formula::environment(formula::Measured<Share> { Rational { 50 } });
    CHECK(trace_text(var<Share> * Rational { 3 }, shares)
          == "1. s = 50 %\n"
             "2. 3\n"
             "3. #1 * #2 = 3/2\n");
    CHECK(trace_text(Rational { 3 } * var<Share>, shares)
          == "1. 3\n"
             "2. s = 50 %\n"
             "3. #1 * #2 = 3/2\n");
}

TEST_CASE("a conditional whose branch records no step of its own reads in the coherent unit",
          "[trace-render][shown-unit]")
{
    // The branch is a consumer's node that forwards the sink: the step the
    // conditional claims last is the reading under it, 25 degC, not the
    // branch's value, 5 K below it. Shown in degrees Celsius, that value
    // would read as the 20 degC it is not.
    auto const readings = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } },
                                               formula::Measured<EndTemperature> { Rational { 25 } });
    CHECK(trace_text(formula::when(var<EndTemperature> > var<StartTemperature>,
                                   forwarding::rise_above(var<EndTemperature>, Rational { 5 }),
                                   var<StartTemperature>),
                     readings)
          == "1. T_1 = 25 \xc2\xb0" "C\n"
             "2. T_0 = 20 \xc2\xb0" "C\n"
             "3. T_1 = 25 \xc2\xb0" "C\n"
             "4. if #1 > #2 then #3 = 5863/20 K\n");
}

TEST_CASE("a binary step over a node that records no step of its own reads in the coherent unit",
          "[trace-render][shown-unit]")
{
    // Two steps are claimed: the first, in grams, is the forwarding node's
    // operand, not the node, and the second the bare 2. The product's unit is
    // not read off the first.
    auto const grams = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    CHECK(trace_text(forwarding::rise_above(var<SampleMass>, Rational { 1, 100 }) * Rational { 2 }, grams)
          == "1. m = 413/10 g\n"
             "2. 2\n"
             "3. #1 * #2 = 313/5000 kg\n");
}

TEST_CASE("a derivation's header shows a value in a unit with a symbol without passing through the coherent unit",
          "[trace-render][shown-unit][worksheet]")
{
    // 10^13 kWh is 3.6 * 10^19 J, more than 64 bits count, but it is a
    // perfectly good number of kilowatt-hours: typed in, it reads as typed.
    auto sheet = formula::worksheet(household::bill, household::bill_environment(household::billValues));
    sheet.set(formula::entered(formula::Measured<household::NetDraw> { Rational { 10'000'000'000'000 } }));
    CHECK(formula::render_derivation(formula::explain_worksheet<household::NetDraw>(sheet), { .maxSteps = 20 })
          == "net_draw = 10000000000000 kWh, entered by hand in place of monthly_load - self_used\n");
}

TEST_CASE("a snap in a unit with no symbol states its permitted values in the coherent unit",
          "[trace-render][shown-unit][snap]")
{
    // The permitted values are 3 and 5 of the unnamed gram: 3/1000 and
    // 1/200 kg, as the value snapped is.
    auto const snappedTo = [](Rational unnamedGrams) {
        return unnamed_trace_text(
            formula::snapped<UnnamedGram, UnnamedPermitted, formula::SnapTie::TowardHigher>(var<UnnamedMass>),
            unnamedGrams);
    };
    CHECK(snappedTo(Rational { 4 })
          == "1. m_u = 1/250 kg\n"
             "2. snap(#1) = 1/200 kg [3/1000 kg to 1/200 kg; tie, toward higher]\n");
    CHECK(snappedTo(Rational { 7, 2 }).ends_with("2. snap(#1) = 3/1000 kg [3/1000 kg to 1/200 kg; nearer 3/1000 kg]\n"));
    CHECK(snappedTo(Rational { 3 }).ends_with("2. snap(#1) = 3/1000 kg [on 3/1000 kg]\n"));
    CHECK(snappedTo(Rational { 6 }).ends_with("[outside the permitted set, 3/1000 kg to 1/200 kg]\n"));
}

TEST_CASE("a binning's miss in a unit with no symbol states the classes in the coherent unit",
          "[trace-render][shown-unit][binning]")
{
    // 9 of the unnamed gram is in no class; the classes cover 0 to under 8
    // of it: 9/1000 kg against 0 to under 1/125 kg.
    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_series<ParticleCount>(
        formula::binned<UnnamedGram, UnnamedBands>(formula::observations<UnnamedMass, 3>),
        formula::environment(formula::MeasuredObservations<UnnamedMass, 3>(Rational { 1 }, Rational { 9 }, Rational { 6 })),
        formula::RecordingSink<> { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
              .ends_with("at observation 2 [9/1000 kg in no class; the classes cover 0 to under 1/125 kg]\n"));
}

TEST_CASE("a lookup keyed in a unit with no symbol states its bands and rows in the coherent unit",
          "[trace-render][shown-unit][lookup]")
{
    constexpr auto banded = formula::banded_lookup<UnnamedGram, UnnamedBands, unit::Percent>(
        var<UnnamedMass>, { Rational { 10 }, Rational { 20 } });
    CHECK(unnamed_trace_text(banded, Rational { 3 })
          == "1. m_u = 3/1000 kg\n"
             "2. lookup(#1) = 10 % [0 to under 1/200 kg]\n");
    CHECK(unnamed_trace_text(banded, Rational { 9 }).ends_with("[in no band; the bands cover 0 to under 1/125 kg]\n"));

    constexpr auto interpolated = formula::interpolating_lookup<UnnamedGram, UnnamedRows, unit::Percent>(
        var<UnnamedMass>, { Rational { 10 }, Rational { 30 } });
    CHECK(unnamed_trace_text(interpolated, Rational { 3 })
          == "1. m_u = 3/1000 kg\n"
             "2. interpolate(#1) = 15 % [between 1/500 and 3/500 kg]\n");
    CHECK(unnamed_trace_text(interpolated, Rational { 2 }).ends_with("[on the row at 1/500 kg]\n"));
    CHECK(unnamed_trace_text(interpolated, Rational { 9 }).ends_with("[outside the curve, which runs 1/500 to 3/500 kg]\n"));

    constexpr auto oneRow =
        formula::interpolating_lookup<UnnamedGram, UnnamedOnlyRow, unit::Percent>(var<UnnamedMass>, { Rational { 10 } });
    CHECK(unnamed_trace_text(oneRow, Rational { 3 }).ends_with("[outside the curve, whose only row is at 1/500 kg]\n"));
}

TEST_CASE("a curve over a domain in a unit with no symbol states its rows in the coherent unit",
          "[trace-render][shown-unit][curve]")
{
    constexpr auto massCurve =
        formula::curve(formula::domain<UnnamedGram, UnnamedRows>, formula::series_constant<unit::Percent>(Rational { 10 }, Rational { 30 }));
    CHECK(unnamed_trace_text(formula::interpolate_at(massCurve, var<UnnamedMass>), Rational { 3 })
              .ends_with("interpolate(#3, at #4) = 15 % [between 1/500 and 3/500 kg]\n"));
    CHECK(unnamed_trace_text(formula::interpolate_at(massCurve, var<UnnamedMass>), Rational { 9 })
              .ends_with("[outside the curve, which runs 1/500 to 3/500 kg]\n"));
}
