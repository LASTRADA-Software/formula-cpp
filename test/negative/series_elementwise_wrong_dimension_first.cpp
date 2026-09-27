// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
// REJECT: the two series combined here have different lengths
// REJECT: RequireAddendsAgree<formula::ElementwiseBinaryNode
//
// A series of the wrong dimension FIRST in a + b + c. The first sum is
// refused in the scalar operators' words and marked refused; the sum over it,
// whose left dimension is a stand-in (the opening's), asks nothing. One
// message, counted by hand on cl 19.51, g++-14 and clang++-20. The scalar
// operators' own chain, var<w> + var<m_r> + var<m_t>, still draws two on g++
// and clang++ (measured by the task 4 review): BinaryNode carries no such
// flag, which is inherited and left for a later phase.
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
struct Opening: formula::Quantity<Opening, "w", "screen opening", formula::unit::Millimetre>
{
};

inline constexpr auto combined = formula::series<Opening, 5> + formula::series<Retained, 5> + formula::series<TotalMass, 5>;

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
