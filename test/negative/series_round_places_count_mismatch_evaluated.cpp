// SPDX-License-Identifier: Apache-2.0
// EXPECT: are not a PlacesTable<N> of one DecimalPlaces per element of its series
// REJECT: outside the bounds
// REJECT: must be initialized by a constant expression
// REJECT: no match
// REJECT: not a range
// REJECT: subscript
// REJECT: was not declared
//
// Four granularities for a series of five, and the rounding then evaluated
// in a constant expression, rendered, documented and traced. The count is
// refused once; everything past the refusal is gated off, so no
// out-of-bounds index, no failed constant expression and no loop over the
// table follows it.
#include <formula-cpp/series.hpp>

#include <formula-cpp/document.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <string>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } },
                                                            formula::Measured<Retained> { formula::Rational { 340 } },
                                                            formula::Measured<Retained> { formula::Rational { 28 } }));

inline constexpr formula::PlacesTable<4> places {
    formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 1 }, formula::DecimalPlaces { 1 }
};

inline constexpr auto rounded =
    formula::rounded_elementwise<formula::unit::Gram, places, formula::RoundingMode::HalfEven>(formula::series<Retained, 5>);

int main()
{
    // Evaluated in a constant expression, as this library's tests do, then
    // rendered, documented and traced: none of it may add a message.
    constexpr auto evaluated = formula::checked_evaluate_series<Retained>(rounded, inputs);
    std::string const text =
        formula::render(rounded) + formula::render<formula::Dialect::LaTeX>(rounded) + formula::document(rounded).formula;
    auto const explained = formula::explain_series<Retained>(rounded, inputs);
    return evaluated.has_value() && !text.empty() && !formula::render_trace(explained.trace, { .maxSteps = 9 }).empty() ? 0
                                                                                                                        : 1;
}
