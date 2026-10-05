// SPDX-License-Identifier: Apache-2.0
// A Bounds written with positional initialisers in the order it had before each end got its own flag: once a range of
// 0 to 100, it would now read as a minimum of 1/100 and no maximum. Only a bool sets a BoundsEnd, so the 0 meant for
// the low numerator is refused where it lands on highPresent. This must not compile.
#include <formula-cpp/unit.hpp>

int main()
{
    formula::Bounds const percentRange { true, 0, 1, 100, 1 };
    return percentRange.lowPresent ? 0 : 1;
}
