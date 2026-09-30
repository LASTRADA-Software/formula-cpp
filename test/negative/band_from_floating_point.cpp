// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a band's bounds are exact numbers
//
// A band whose low numerator is a double. The integer overload would truncate
// 12.7 to 12 and say nothing; it is refused, once, in the library's words.
#include <formula-cpp/band.hpp>

inline constexpr auto declared = formula::band(12.7, 1, 17, 1);

int main()
{
    return 0;
}
