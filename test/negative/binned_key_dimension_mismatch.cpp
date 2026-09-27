// SPDX-License-Identifier: Apache-2.0
// EXPECT: this binning's key unit does not measure the dimension of the observations it bins
// REJECT: gap or overlap
// REJECT: declares no classes
//
// Sizes in metres counted into classes stated in grams: refused once.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 127, 1),
                                                     formula::band(127, 1, 197, 1),
                                                     formula::band(197, 1, 331, 1) };

inline constexpr auto counted = formula::binned<formula::unit::Gram, sizeClasses>(formula::observations<Size, 3>);

int main()
{
    return counted.length == 3 ? 0 : 1;
}
