// SPDX-License-Identifier: Apache-2.0
// EXPECT: provides no value for this quantity
// REJECT: holds a single value for this quantity
// REJECT: holds a series of a different length
//
// A series read for a quantity the environment does not hold at all.
// RequireProvided's refusal, once: neither the shape nor the length can be
// wrong for a quantity that is not there.
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

inline constexpr auto inputs = formula::environment(formula::measured_series<Sieved>(sieved(500), sieved(600)));

int main()
{
    return inputs.get_series<Retained, 2>().element(0).has_value() ? 0 : 1;
}
