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
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "forwarding_nodes.hpp"
#include "household_bill.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

// A Celsius scale with no symbol: an offset unit the rounding clause must name by its size and its zero.
inline constexpr formula::Unit UnnamedCelsius { .dimension = formula::dim::Temperature,
                                                .offsetNumerator = 27315,
                                                .offsetDenominator = 100 };
struct UnnamedReading: formula::Quantity<UnnamedReading, "T_u", "a reading on an unnamed scale", UnnamedCelsius>
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

// The electricity bill's money: a base dimension of its own, so that a price
// times an energy reads in euros, the coherent unit of money.
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit EuroPerKilowattHour { .dimension = Euro.dimension / formula::dim::Energy,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 3'600'000,
                                                     .symbolText = formula::symbol("EUR/kWh"),
                                                     .decimals = 4 };
struct OvenPower: formula::Quantity<OvenPower, "oven_kw", "the oven's power", unit::Kilowatt>
{
};
struct OvenHours: formula::Quantity<OvenHours, "oven_h", "the oven's hours a month", unit::Hour>
{
};
struct SolarYield: formula::Quantity<SolarYield, "solar", "the solar yield of a month", unit::KilowattHour>
{
};
struct SelfUsedEnergy: formula::Quantity<SelfUsedEnergy, "self_used", "the solar energy used at home", unit::KilowattHour>
{
};
struct GridPrice: formula::Quantity<GridPrice, "price", "the grid price", EuroPerKilowattHour>
{
};
struct BaseFee: formula::Quantity<BaseFee, "base_fee", "the monthly base fee", Euro>
{
};

// The particles of a class, a count.
struct ParticleCount: formula::Quantity<ParticleCount, "n", "particles in a class", unit::One>
{
};

// A consumer's operation over a series of masses: its lowest element and the
// span of the series, each a mass.
struct LowestAndSpan
{
    static constexpr std::string_view name = "lowest and span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "lowest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const spread = formula::RepTraits<Rep>::subtract(most, least);
        if (!spread.has_value())
            return std::unexpected { spread.error() };
        return std::array { least, *spread };
    }
};

inline constexpr auto lowestAndSpan =
    formula::opaque<LowestAndSpan>({ .reference = "Example Standard 12" }, formula::series<SampleMass, 3>);

// Three determinations in grams, and a tare.
inline constexpr auto determinations = formula::environment(
    formula::measured_series<SampleMass>(formula::Measured<SampleMass> { Rational { 402, 10 } },
                                         formula::Measured<SampleMass> { Rational { 398, 10 } },
                                         formula::Measured<SampleMass> { Rational { 433, 10 } }),
    formula::Measured<TareMass> { Rational { 7 } });

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

/// The decimal digits at the start of @p spelled, as an exact number, and
/// @p spelled advanced past them; nothing when it does not start with one.
std::optional<Rational> take_whole(std::string_view& spelled)
{
    if (spelled.empty() || spelled.front() < '0' || spelled.front() > '9')
        return std::nullopt;
    Rational parsed {};
    while (!spelled.empty() && spelled.front() >= '0' && spelled.front() <= '9')
    {
        std::expected<Rational, formula::ArithmeticError> const shifted = formula::checked_mul(parsed, Rational { 10 });
        if (!shifted)
            return std::nullopt;
        std::expected<Rational, formula::ArithmeticError> const added =
            formula::checked_add(*shifted, Rational { spelled.front() - '0' });
        if (!added)
            return std::nullopt;
        parsed = *added;
        spelled.remove_prefix(1);
    }
    return parsed;
}

/// A value as a trace writes it in the fraction style: `-a/b unit`, `a`,
/// `a/b`, each with or without a unit after a space.
struct ShownValue
{
    Rational shownNumber;
    std::string_view unitText;
};

std::optional<ShownValue> parse_shown(std::string_view spelled)
{
    bool const negative = spelled.starts_with('-');
    if (negative)
        spelled.remove_prefix(1);
    std::optional<Rational> const wholeNumber = take_whole(spelled);
    if (!wholeNumber)
        return std::nullopt;
    Rational parsed = *wholeNumber;
    if (spelled.starts_with('/'))
    {
        spelled.remove_prefix(1);
        std::optional<Rational> const below = take_whole(spelled);
        if (!below)
            return std::nullopt;
        std::expected<Rational, formula::ArithmeticError> const divided = formula::checked_div(parsed, *below);
        if (!divided)
            return std::nullopt;
        parsed = *divided;
    }
    if (negative)
    {
        std::expected<Rational, formula::ArithmeticError> const negated = formula::checked_negate(parsed);
        if (!negated)
            return std::nullopt;
        parsed = *negated;
    }
    if (spelled.starts_with(' '))
        spelled.remove_prefix(1);
    else if (!spelled.empty())
        return std::nullopt;
    return ShownValue { parsed, spelled };
}

/// For every step of @p recorded that holds a value: the text after the
/// number names the step's own unit, the coherent unit, or -- only for a
/// dimensionless step -- nothing; and the number, read back from that unit
/// into the coherent one, is exactly the value recorded. The unit is taken
/// from the text, not from the rule that chose it, so a value written in one
/// scale and labelled with another fails here.
///
/// It reads `Step::value` only, as `value_in_declared_unit` shows it. The other
/// numbers a line states -- an opaque call's output rows, a series' elements, a
/// rejection's clauses, a table's bounds and rows -- come from side tables and
/// other fields, and are pinned by their own tests.
void check_each_value_is_in_the_unit_written_after_it(formula::Trace<> const& recorded)
{
    std::size_t checkedSteps = 0;
    for (formula::Step<Rational> const& recordedStep: recorded.steps)
    {
        if (!recordedStep.value.has_value() || recordedStep.error.has_value())
            continue;
        std::string const shownText =
            formula::detail::value_in_declared_unit(recordedStep, recordedStep.value, formula::NumberStyle::fraction());
        INFO("step shown as: " << shownText);
        std::optional<ShownValue> const parsed = parse_shown(shownText);
        REQUIRE(parsed.has_value());
        formula::Unit const coherentUnit = formula::coherent(recordedStep.dimension);
        std::optional<formula::Unit> namedUnit;
        if (!parsed->unitText.empty() && parsed->unitText == formula::detail::unit_symbol_text(recordedStep.unit))
            namedUnit = recordedStep.unit;
        else if (!parsed->unitText.empty() && parsed->unitText == formula::detail::coherent_unit_text(recordedStep.dimension))
            namedUnit = coherentUnit;
        else if (parsed->unitText.empty() && recordedStep.dimension == formula::dim::Scalar)
            namedUnit = recordedStep.unit;
        REQUIRE(namedUnit.has_value());
        std::expected<Rational, formula::ArithmeticError> const backInCoherent =
            formula::checked_convert(parsed->shownNumber, *namedUnit, coherentUnit);
        REQUIRE(backInCoherent.has_value());
        CHECK(*backInCoherent == *recordedStep.value);
        ++checkedSteps;
    }
    CHECK(checkedSteps > 0);
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

TEST_CASE("a rounding in a unit with no symbol names that unit by its size", "[trace-render][shown-unit][rounding]")
{
    // 3.141 of the unnamed gram to 2 places is 3.14 of it: 157/50000 kg. The
    // places count in the unnamed gram, and the line says so in the coherent
    // unit the value is written in.
    auto const masses = formula::environment(formula::Measured<UnnamedMass> { Rational { 3141, 1000 } });
    CHECK(trace_text(formula::rounded<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                         var<UnnamedMass>),
                     masses)
          == "1. m_u = 3141/1000000 kg\n"
             "2. round(#1, to 2 dp of 1/1000 kg) = 157/50000 kg [nearest, ties to even]\n");
    CHECK(trace_text(formula::rounded_to_digits<UnnamedGram, formula::SignificantDigits { 2 }, formula::RoundingMode::HalfEven>(
                         var<UnnamedMass>),
                     masses)
              .find("2. round(#1, to 2 sf of 1/1000 kg) = 31/10000 kg")
          != std::string::npos);
    // 20.5 on the unnamed Celsius scale, rounded to 0 places of it: 21, which
    // is 294.15 K. The places count from that scale's zero, 273.15 K.
    auto const readings = formula::environment(formula::Measured<UnnamedReading> { Rational { 41, 2 } });
    CHECK(trace_text(formula::rounded<UnnamedCelsius, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
                         var<UnnamedReading>),
                     readings)
          == "1. T_u = 5873/20 K\n"
             "2. round(#1, to 0 dp of 1 K from 5463/20 K) = 5883/20 K [nearest, ties away from zero]\n");

    // A rounded root, a rounded output of an opaque operation and a method's
    // rounding rule name the unit the same way.
    CHECK(trace_text(formula::rounded_sqrt<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                         var<UnnamedMass> * var<UnnamedMass>),
                     masses)
              .find("round(sqrt(#3), to 2 dp of 1/1000 kg) = 157/50000 kg")
          != std::string::npos);
    CHECK(trace_text(formula::rounded_output<"span", UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                         lowestAndSpan),
                     determinations)
              .find(", to 2 dp of 1/1000 kg) = 7/2000 kg")
          != std::string::npos);
    auto const doubled = formula::method(
        formula::variants(formula::variant<UnnamedTotal>(var<UnnamedMass> * Rational { 2 })),
        formula::rounding_rule<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(),
        formula::constraints());
    formula::Trace<> ruleTrace {};
    (void) formula::evaluate_method<UnnamedTotal>(doubled, masses, formula::RecordingSink<> { ruleTrace });
    CHECK(formula::render_trace(ruleTrace, { .maxSteps = 20 }).find(", in 1/1000 kg) = 157/25000 kg") != std::string::npos);
}

TEST_CASE("a constant and a numeric value in a unit with no symbol say what scale their number is on",
          "[trace][units]")
{
    // A constant typed as 3 of a unit of 1/1000 kg with no symbol: render()
    // writes it in the coherent unit, as a trace does, never as a bare 3.
    auto const typedMass = formula::constant<UnnamedGram>(formula::Rational { 3 });
    CHECK(formula::render(typedMass) == "3/1000 kg");
    // Two tokens, so a power of it brackets as one of a constant with a symbol does.
    CHECK(formula::render(formula::pow<2>(typedMass)) == "(3/1000 kg)^2");

    // numeric(x, in <unit>) names the unit its bare number is taken in by its
    // size, in render() and in the trace line alike.
    auto const bareMass = formula::numeric_value_of<UnnamedGram, "the table is in unnamed grams">(typedMass);
    CHECK(formula::render(bareMass) == "numeric(3/1000 kg, in 1/1000 kg)");
    std::string const traced = trace_text(bareMass, formula::environment());
    CHECK(traced.find("numeric(#1, in 1/1000 kg)") != std::string::npos);
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
    // A pure number divided by a mass is no mass: the coherent unit, kg^-1.
    CHECK(trace_text(Rational { 2 } / var<SampleMass>, inputs)
          == "1. 2\n"
             "2. m = 413/10 g\n"
             "3. #1 / #2 = 20000/413 kg^-1\n");
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

TEST_CASE("a precision limit's first pass reads in the unit of the level it restates", "[trace-render][shown-unit][precision]")
{
    // The level is a constant in grams and the limit names no quantity, so
    // nothing in the types says grams: pass 1 reads off the step it restates,
    // 40 g, never 1/25 kg.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<unit::Gram>(Rational { 40 }), formula::constant<unit::Gram>(Rational { 1 })),
                     determinations)
              .starts_with("1. 40 g\n"
                           "2. level (pass 1 of 2) = #1 = 40 g\n"));
    // A level constant in a unit with no symbol cannot lend its unit
    // (`restated_unit_or` borrows only a unit with a symbol): pass 1 stays in
    // the unit the types give, the coherent kilogram, as before.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<UnnamedGram>(Rational { 40000 }), formula::constant<unit::Gram>(Rational { 1 })),
                     determinations)
              .find("2. level (pass 1 of 2) = #1 = 40 kg\n")
          != std::string::npos);
    // A level constant in degrees Celsius is a point on an offset scale, which
    // `borrowable_for_a_point` lets a restating step show: pass 1 reads in
    // degrees Celsius, as the constant's own line does, never as a kelvin
    // difference.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<unit::Celsius>(Rational { 20 }), formula::constant<unit::Celsius>(Rational { 1 })),
                     determinations)
              .find("2. level (pass 1 of 2) = #1 = 20 \xc2\xb0" "C\n")
          != std::string::npos);
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

TEST_CASE("a binary step over a node on its right that records no step of its own reads in the coherent unit",
          "[trace-render][shown-unit]")
{
    // The mirror of the case above: the pure number is the left operand and
    // the forwarding node the right. The second step claimed is the
    // forwarding node's operand, in grams, not the node: the product's unit
    // is not read off it.
    auto const grams = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    CHECK(trace_text(Rational { 2 } * forwarding::rise_above(var<SampleMass>, Rational { 1, 100 }), grams)
          == "1. 2\n"
             "2. m = 413/10 g\n"
             "3. #1 * #2 = 313/5000 kg\n");
}

TEST_CASE("a derivation's header shows a value in a unit with a symbol without passing through the coherent unit",
          "[trace-render][shown-unit][worksheet]")
{
    // 10^35 kWh is 3.6 * 10^41 J, past the 2^127 a Rational holds: a header
    // that converted it from kWh to kWh through joules took a detour that can
    // only fail. Typed in, it reads as typed.
    constexpr Rational::Int tenToSeventeen = 100'000'000'000'000'000;
    constexpr Rational::Int tenToThirtyFive = tenToSeventeen * tenToSeventeen * 10;
    auto sheet = formula::worksheet(household::bill, household::bill_environment(household::billValues));
    sheet.set(formula::entered(formula::Measured<household::NetDraw> { Rational { tenToThirtyFive } }));
    CHECK(formula::render_derivation(formula::explain_worksheet<household::NetDraw>(sheet), { .maxSteps = 20 })
          == "net_draw = 100000000000000000000000000000000000 kWh, entered by hand in place of monthly_load - "
             "self_used\n");
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

TEST_CASE("two different rows that cannot be shown are not read as one row", "[trace-render][shown-unit][lookup]")
{
    // A hand-built trace, as a `Trace` is a public aggregate: rows declared
    // over a zero denominator, 1/0 and 2/0, in a unit with no symbol. No table
    // compiles with such a row, and neither names a number, so each reads
    // `(not shown: ...)` -- but they are two different rows, and the line must
    // not say one.
    formula::Trace<> recorded = recorded_trace(
        formula::interpolating_lookup<UnnamedGram, UnnamedRows, unit::Percent>(var<UnnamedMass>,
                                                                               { Rational { 10 }, Rational { 30 } }),
        formula::environment(formula::Measured<UnnamedMass> { Rational { 9 } }));
    REQUIRE(recorded.steps.size() == 2);
    REQUIRE(recorded.steps[1].coveredRange.has_value());

    // A miss: the curve runs from one row to the other.
    recorded.steps[1].coveredRange = formula::LookupRange { 1, 0, 2, 0 };
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
              .ends_with("[outside the curve, which runs (not shown: division by zero) to "
                         "(not shown: division by zero) kg]\n"));

    // A value between the two rows.
    recorded.steps[1].error.reset();
    recorded.steps[1].lookupFailure = formula::LookupFailure::None;
    recorded.steps[1].value = Rational { 1, 5 };
    recorded.steps[1].selectedSegment =
        formula::Segment { formula::Breakpoint { .numerator = 1, .denominator = 0 },
                           formula::Breakpoint { .numerator = 2, .denominator = 0 } };
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
              .ends_with("[between (not shown: division by zero) and (not shown: division by zero) kg]\n"));

    // A well-formed unit with no symbol can still fail to move a bound into
    // the coherent unit, through its offset: 1/(2^63 - 1) times a magnitude
    // of 1/(2^63 - 25), plus an offset of 1/(2^63 - 165), overflows.
    constexpr formula::Unit WideOffsetGram { .dimension = formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = INT64_MAX - 24,
                                             .offsetNumerator = 1,
                                             .offsetDenominator = INT64_MAX - 164 };
    CHECK(formula::detail::shown_bound_text(1, INT64_MAX, WideOffsetGram, formula::NumberStyle::fraction())
          == "(not shown: overflow in exact arithmetic)");
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

TEST_CASE("an opaque output that cannot be shown says so, with no unit after it", "[trace-render][shown-unit][opaque]")
{
    formula::Trace<> recorded = recorded_trace(formula::opaque_output<"span">(lowestAndSpan), determinations);
    REQUIRE(recorded.opaqueSteps.size() == 1);
    REQUIRE(recorded.opaqueSteps[0].outputs.size() == 2);
    // A row built by hand, as a `Trace` is a public aggregate: the span said
    // to be in metres, which no mass converts into.
    recorded.opaqueSteps[0].outputs[1].unit = unit::Metre;
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
              .find("; span = (not shown: argument outside the domain of the operation) [inside not shown]")
          != std::string::npos);
}

TEST_CASE("every value a trace shows is in the unit written after it", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } },
                                             formula::Measured<HeavyMass> { Rational { 1 } },
                                             formula::Measured<Edge> { Rational { 5 } },
                                             formula::Measured<Breadth> { Rational { 8 } },
                                             formula::Measured<StartTemperature> { Rational { 20 } },
                                             formula::Measured<EndTemperature> { Rational { 25 } },
                                             formula::Measured<Strength> { Rational { 60 } },
                                             formula::Measured<UnnamedMass> { Rational { 3 } },
                                             formula::Measured<FineMass> { Rational { 12345, 10000 } });
    // A power, a quotient in the coherent unit, scaling, sums in one unit and
    // in two, an offset difference, negations of both kinds, an absolute
    // value, conditionals over both kinds of branch, and a unit with no symbol.
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::pow<2>(var<Edge>) / var<Breadth> + var<Edge>, inputs));
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::abs(var<SampleMass> - var<HeavyMass>) * Rational { 3, 50 } + var<FineMass>, inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::when(var<EndTemperature> > var<StartTemperature>, var<EndTemperature> - var<StartTemperature>,
                      -var<StartTemperature>),
        inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::when(var<Strength> > formula::constant<unit::Megapascal>(Rational { 473, 10 }), var<Strength> / Rational { 2 },
                      -var<Strength>),
        inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(var<UnnamedMass> * Rational { 2 } - var<TareMass>, inputs));
    // A pure number over a mass: kg^-1 after a fraction.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(Rational { 2 } / var<SampleMass>, inputs));
    // A rounding in a unit with no symbol.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::rounded<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(var<UnnamedMass>),
        inputs));
}

TEST_CASE("every value of a rejection, a bill, the statistics, a precision limit and an opaque call is in the unit written after it",
          "[trace-render][shown-unit]")
{
    // The outlier rejection: a 6 % deviation from each pass's mean, over
    // 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g.
    constexpr auto rejection =
        formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
            formula::series<SampleMass, 6>,
            formula::deviation_from_mean(Rational { 6, 100 } * formula::pass_mean<SampleMass>),
            formula::Verdict { "discard the determinations and repeat the test" },
            formula::Citation { .title = "Example Standard", .section = "7.4" });
    auto const sample = formula::environment(formula::measured_series<SampleMass>(
        formula::Measured<SampleMass> { Rational { 402, 10 } }, formula::Measured<SampleMass> { Rational { 398, 10 } },
        formula::Measured<SampleMass> { Rational { 405, 10 } }, formula::Measured<SampleMass> { Rational { 44 } },
        formula::Measured<SampleMass> { Rational { 40 } }, formula::Measured<SampleMass> { Rational { 433, 10 } }));
    formula::Trace<> rejected {};
    (void) formula::checked_evaluate_rejection<SampleMass>(rejection, sample, formula::RecordingSink<> { rejected });
    CHECK(formula::render_trace(rejected, { .maxSteps = 20 }).find("4. #2 * #3 = 1239/500 g\n") != std::string::npos);
    check_each_value_is_in_the_unit_written_after_it(rejected);

    // An electricity bill: kWh - kWh, a power times a time, and a price per
    // kWh times an energy, in euros.
    auto const bill = formula::environment(formula::Measured<OvenPower> { Rational { 5, 2 } },
                                           formula::Measured<OvenHours> { Rational { 30 } },
                                           formula::Measured<SolarYield> { Rational { 150 } },
                                           formula::Measured<SelfUsedEnergy> { Rational { 120 } },
                                           formula::Measured<GridPrice> { Rational { 8, 25 } },
                                           formula::Measured<BaseFee> { Rational { 25, 2 } });
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(var<SolarYield> - var<SelfUsedEnergy>, bill));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        (var<OvenPower> * var<OvenHours> - var<SelfUsedEnergy> * Rational { 1, 2 }) * var<GridPrice> + var<BaseFee>, bill));

    // The statistics of three determinations: a mean, a variance and a range.
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::sample_mean(formula::series<SampleMass, 3>) - var<TareMass>, determinations));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::sample_variance(formula::series<SampleMass, 3>) / var<TareMass>
            + formula::sample_range(formula::series<SampleMass, 3>),
        determinations));

    // A precision limit at the mean of two results, and one over typed
    // constants.
    auto const pair = formula::environment(formula::Measured<SampleMass> { Rational { 40 } },
                                           formula::Measured<TareMass> { Rational { 40905, 1000 } },
                                           formula::Measured<HeavyMass> { Rational { 2, 7 } });
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::precision_limit<formula::PrecisionKind::Repeatability>(
            (var<SampleMass> + var<TareMass>) / Rational { 2 },
            formula::constant<unit::Gram>(Rational { 1, 10 }) + Rational { 1, 50 } * formula::precision_level<SampleMass>),
        pair));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::constant<unit::Kilogram>(Rational { 1, 3 }),
                                                                        formula::constant<unit::Kilogram>(Rational { 1, 7 }))
            * var<HeavyMass>,
        pair));
    // A precision limit over a level constant in grams.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::constant<unit::Gram>(Rational { 40 }),
                                                                        formula::constant<unit::Gram>(Rational { 1 })),
        pair));

    // An opaque call, its outputs, and a sum over one of them.
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::opaque_output<"span">(lowestAndSpan) + var<TareMass>, determinations));
}
