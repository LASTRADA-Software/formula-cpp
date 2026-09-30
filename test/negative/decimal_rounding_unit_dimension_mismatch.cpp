// SPDX-License-Identifier: Apache-2.0
// EXPECT: this rounding node names a unit that does not measure the dimension of the expression it rounds
//
// A rounding named once, in grams, applied to a length: the rounding node's own
// refusal, in its own words (RequireRoundingUnitMatches), exactly as the three
// separate arguments draw it. Naming the rounding as a value adds no second
// message, because the value only forwards to the node.
#include <formula-cpp/formula.hpp>

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};

inline constexpr formula::DecimalRounding wholeGrams { formula::unit::Gram,
                                                       formula::DecimalPlaces { 0 },
                                                       formula::RoundingMode::HalfAwayFromZero };

inline constexpr auto node = formula::rounded<wholeGrams>(formula::var<Diameter>);

int main()
{
    return static_cast<int>(sizeof(node));
}
