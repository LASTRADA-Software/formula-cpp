// SPDX-License-Identifier: Apache-2.0
//
// A trace step's number is always shown with the unit it is in: borrowed from
// the operand steps where that is safe, the coherent unit's symbol otherwise,
// and nothing only for a dimensionless value.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

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

template <typename Expression, typename Bound>
std::string trace_text(Expression const& formulaExpression, Bound const& inputs)
{
    formula::Trace<> recorded {};
    formula::RecordingSink<> recordingSink { recorded };
    (void) formula::checked_evaluate_si<Rational>(formulaExpression, inputs, recordingSink);
    return formula::render_trace(recorded, { .maxSteps = 20 });
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
