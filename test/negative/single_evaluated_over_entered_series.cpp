// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a series for this quantity, not a single value
// REJECT: provides no value for this quantity
//
// A single-valued result whose quantity a person entered as a SERIES. The
// entered series must not be computed over, and one value cannot be read
// from it, so the read is refused (D3 in task 2's report).
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

inline constexpr auto inputs =
    formula::environment(formula::entered(formula::measured_series<Retained>(retained(131), retained(211))), sieved(500));

int main()
{
    return formula::checked_evaluate<Retained>(formula::var<Sieved>, inputs).has_value() ? 0 : 1;
}
