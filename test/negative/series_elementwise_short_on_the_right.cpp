// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
// REJECT: RequireSeriesLengthsAgree<formula::SeriesVarNode<TotalMass, 5>
//
// The refused sum on the RIGHT of the node over it: c5 + (b4 + a5). The outer
// sum asks nothing, because its right operand is refused. The second REJECT
// names what only the second message prints, as g++ and clang++ print it;
// it fires if the right operand's flag is ignored (checked on both).
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

inline constexpr auto combined = formula::series<TotalMass, 5> + (formula::series<Sieved, 4> + formula::series<Retained, 5>);

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
