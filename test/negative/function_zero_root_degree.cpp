// SPDX-License-Identifier: Apache-2.0
// EXPECT: a root of degree zero describes no operation
#include <formula-cpp/function.hpp>

struct Area: formula::Quantity<Area, "A", "cross-sectional area", formula::unit::SquareMetre>
{
};

int main()
{
    constexpr auto broken = formula::root<0>(formula::var<Area>);
    return formula::is_dimensionless(decltype(broken)::dimension) ? 0 : 1;
}
