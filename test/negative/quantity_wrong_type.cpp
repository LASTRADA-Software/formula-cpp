// SPDX-License-Identifier: Apache-2.0
// Two quantities alike in symbol, description and unit are still different
// types, so one must not be usable where the other is expected. That is the
// whole reason for the CRTP tag. This must not compile.
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

struct Rise: formula::Quantity<Rise, "x", "a distance", formula::unit::Millimetre>
{
};

struct Run: formula::Quantity<Run, "x", "a distance", formula::unit::Millimetre>
{
};

void takes_rise(Rise);

int main()
{
    takes_rise(Run {});
    return 0;
}
