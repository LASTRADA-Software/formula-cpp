// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
// REJECT: RequireSeriesLengthsAgree<formula::ElementwiseBinaryNode
//
// The shorter series first in (b * a) / c: multiplication and division go
// through the same gate as addition. One message, counted by hand on cl
// 19.51, g++-14 and clang++-20 -- see series_elementwise_short_first.
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

inline constexpr auto combined = formula::series<Sieved, 4> * formula::series<Retained, 5> / formula::series<TotalMass, 5>;

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
