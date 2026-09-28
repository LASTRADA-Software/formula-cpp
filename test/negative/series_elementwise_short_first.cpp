// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
// REJECT: RequireSeriesLengthsAgree<formula::ElementwiseBinaryNode
//
// The shorter series FIRST in a + b + c. The first sum is refused and marked
// refused, and the sum over it asks nothing: its length is a stand-in taken
// after the refusal, and comparing it with c's would report the one mistake
// again. REJECT cannot refuse a second copy of the EXPECTed text; the second
// REJECT names what only the second message would -- the refused node as the
// guard's left argument, as g++ and clang++ print it. Counted by hand: one
// message on cl 19.51, g++-14 and clang++-20.
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

inline constexpr auto combined = formula::series<Sieved, 4> + formula::series<Retained, 5> + formula::series<TotalMass, 5>;

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
