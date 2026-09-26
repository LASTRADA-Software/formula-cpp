// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
// REJECT: RequireSeriesLengthsAgree<formula::CumulativeNode
//
// A refused sum under a running total, then combined:
// cumulative(b4 + a5) + c5. The running total passes the refusal upward, so
// the outer sum asks nothing. The second REJECT fires if cumulative drops the
// flag (checked on g++ and clang++).
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

inline constexpr auto combined = formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Sieved, 4> + formula::series<Retained, 5>)
    + formula::series<TotalMass, 5>;

int main()
{
    return static_cast<int>(decltype(combined)::length);
}
