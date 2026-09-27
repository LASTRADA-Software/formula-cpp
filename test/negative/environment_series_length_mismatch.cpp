// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a series of a different length for this quantity than the one read
// REJECT: holds a single value for this quantity
//
// Four screens supplied, five read. The length is part of the method, so this
// is a compile error naming both lengths, as RequireSeriesLength's template
// arguments Supplied = 4 and Read = 5 -- never a read past the end, and never
// a fifth element quietly absent.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } },
                                                            formula::Measured<Retained> { formula::Rational { 340 } }));

int main()
{
    return formula::checked_evaluate_series<Retained>(formula::series<Retained, 5>, inputs).has_value() ? 0 : 1;
}
