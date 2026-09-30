// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a band's bounds are exact numbers
//
// A floating-point value in the fourth argument (the high denominator) of `band(0, 1, 17, 1.5)`.
// The integer overload would truncate it and say nothing; it is refused, once,
// in the library's words.
#include <formula-cpp/band.hpp>

inline constexpr auto declared = formula::band(0, 1, 17, 1.5);

int main()
{
    return declared.lowNumerator == 12 ? 1 : 0;
}
