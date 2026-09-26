// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a single value for this quantity, not a series
// REJECT: provides no value for this quantity
// REJECT: holds a series of a different length
//
// A series result whose quantity a person entered as ONE value. The entered
// value must not be computed over, and a series cannot be answered with one
// number, so the read of the entered value is refused (D3 in task 2's
// report). Were checked_evaluate_series to ask is_entered_series instead of
// is_entered, it would compute over the person's value silently and this
// case would compile.
#include <formula-cpp/series.hpp>

#include <cstdint>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Sieved: formula::Quantity<Sieved, "m_s", "mass passing a screen", formula::unit::Gram>
{
};

[[nodiscard]] constexpr formula::Measured<Retained> retained(std::int64_t grams)
{
    return formula::Measured<Retained> { formula::Rational { grams } };
}
[[nodiscard]] constexpr formula::Measured<Sieved> sieved(std::int64_t grams)
{
    return formula::Measured<Sieved> { formula::Rational { grams } };
}

inline constexpr auto inputs = formula::environment(formula::entered(retained(131)),
                                                    formula::measured_series<Sieved>(sieved(500), sieved(600), sieved(700)));

int main()
{
    return formula::checked_evaluate_series<Retained>(formula::series<Sieved, 3>, inputs).has_value() ? 0 : 1;
}
