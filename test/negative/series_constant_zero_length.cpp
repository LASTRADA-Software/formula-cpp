// SPDX-License-Identifier: Apache-2.0
// EXPECT: this series constant has no elements
// REJECT: no matching
//
// A per-element constant over no points: there is nothing to sum, round or
// trace, and a sum of it would read "(not measured)" of something nobody
// could have measured. Refused once, in the library's words.
#include <formula-cpp/series.hpp>

inline constexpr auto none = formula::series_constant<formula::unit::Gram, 0>();

int main()
{
    return static_cast<int>(decltype(none)::length);
}
