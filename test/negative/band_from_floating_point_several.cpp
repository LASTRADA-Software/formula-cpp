// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a band's bounds are exact numbers
//
// A floating-point value in the first (double) and third (float) arguments, so two different types; two mistakes of one kind are still one message of `band(12.7, 1, 17.3f, 1)`.
// The integer overload would truncate it and say nothing; it is refused, once,
// in the library's words.
#include <formula-cpp/band.hpp>

inline constexpr auto declared = formula::band(12.7, 1, 17.3f, 1);

int main()
{
    return declared.lowNumerator == 12 ? 1 : 0;
}
