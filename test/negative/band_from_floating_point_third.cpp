// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a band's bounds are exact numbers
//
// A floating-point value in the third argument (the high numerator) of
// `band(0, 1, 12.7, 1)`. The integer overload would truncate it and say
// nothing; it is refused, once, in the library's words.
#include <formula-cpp/band.hpp>

inline constexpr auto declared = formula::band(0, 1, 12.7, 1);

int main()
{
    return 0;
}
