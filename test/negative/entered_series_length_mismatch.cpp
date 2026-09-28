// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a series of a different length for this quantity than the one read
// REJECT: holds a single value for this quantity
//
// A person entered four elements for a series result of three. The entered
// series is what the result would be, so its length is held to the
// expression's, exactly as a measured one is.
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

inline constexpr auto inputs = formula::environment(
    formula::entered(formula::measured_series<Retained>(retained(1), retained(2), retained(3), retained(4))),
    formula::measured_series<Sieved>(sieved(500), sieved(600), sieved(700)));

int main()
{
    return formula::checked_evaluate_series<Retained>(formula::series<Sieved, 3>, inputs).has_value() ? 0 : 1;
}
