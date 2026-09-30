// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a breakpoint's key is an exact number
//
// A key written as a double. The integer overload would truncate 1.5 to 1 and
// say nothing; it is refused, once, in the library's words.
#include <formula-cpp/lookup.hpp>

inline constexpr auto key = formula::breakpoint(1.5);

int main()
{
    return key.numerator == 1 ? 1 : 0;
}
