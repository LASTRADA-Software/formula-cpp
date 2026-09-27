// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
//
// Three series of one dimension, the shorter one in the MIDDLE of a + b + c:
// a check of only the first or only the last pair would miss it. Refused
// once, naming both lengths (as the series types in RequireSeriesLengthsAgree's
// template arguments), and nothing past the refusal asks for a dimension or an
// evaluation: the sum `a + b` is marked refused, so `+ c` asks nothing and
// adds no second message.
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

inline constexpr auto combined = formula::series<Retained, 5> + formula::series<Sieved, 4> + formula::series<TotalMass, 5>;

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
