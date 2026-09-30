// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a breakpoint's key is an exact number
//
// A floating-point value in the denominator of `breakpoint(1, 2.5)`.
// The integer overload would truncate it and say nothing; it is refused, once,
// in the library's words.
#include <formula-cpp/lookup.hpp>

inline constexpr auto declared = formula::breakpoint(1, 2.5);

int main()
{
    return declared.numerator == 1 ? 1 : 0;
}
