// SPDX-License-Identifier: Apache-2.0
// EXPECT: are not a PlacesTable<N> of one DecimalPlaces per element of its series
// REJECT: outside the bounds
// REJECT: must be initialized by a constant expression
// REJECT: no match
// REJECT: not a range
// REJECT: subscript
// REJECT: was not declared
//
// `rounded`'s spelling of a granularity, DecimalPlaces { 1 }, where a table
// of one per element belongs: refused once, as not a PlacesTable of one per
// element, and nothing past the refusal indexes or loops over it.
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

inline constexpr auto rounded =
    formula::rounded_elementwise<formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(
        formula::series<Retained, 5>);

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
