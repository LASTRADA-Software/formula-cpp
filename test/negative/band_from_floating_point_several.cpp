// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a band's bounds are exact numbers
//
// A floating-point value in the first argument (a double) and in the third (a
// float) of `band(12.7, 1, 17.3f, 1)`. The two types are different on purpose:
// the guard is keyed on the type, so it gives one message per distinct type,
// and two doubles could not tell a broken gate from a working one. Two
// mistakes of one kind are still one message. The integer overload would
// truncate them and say nothing; they are refused, once, in the library's
// words.
#include <formula-cpp/band.hpp>

inline constexpr auto declared = formula::band(12.7, 1, 17.3f, 1);

int main()
{
    return 0;
}
