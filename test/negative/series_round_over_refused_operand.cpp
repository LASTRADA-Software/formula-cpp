// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
// REJECT: RequirePlacesPerElement<
//
// A per-element rounding over a refused sum: its operand's length (4) is a
// stand-in, and the table of five would be told it has the wrong count. The
// rounding asks nothing of a refused operand. The second REJECT names the
// count check's guard, which only a second message would print (checked on
// g++ and clang++ with the gate removed).
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Sieved: formula::Quantity<Sieved, "m_s", "mass passing a screen", formula::unit::Gram>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr formula::PlacesTable<5> places { formula::DecimalPlaces { 0 },
                                                  formula::DecimalPlaces { 0 },
                                                  formula::DecimalPlaces { 0 },
                                                  formula::DecimalPlaces { 1 },
                                                  formula::DecimalPlaces { 1 } };

inline constexpr auto rounded = formula::rounded_elementwise<formula::unit::Gram, places, formula::RoundingMode::HalfEven>(
    formula::series<Sieved, 4> + formula::series<Retained, 5>);

int main()
{
    return static_cast<int>(decltype(rounded)::length);
}
