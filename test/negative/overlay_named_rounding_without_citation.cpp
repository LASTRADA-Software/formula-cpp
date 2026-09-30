// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: with_rounding<...>() was given no citation
//
// A rounding rule named as a DecimalRounding and stated with no citation: refused
// in the same words as the three-argument spelling, since a trace that says "by
// jurisdiction overlay" with nothing to check says nothing.
//
// This must not compile.
#include <formula-cpp/overlay.hpp>

namespace
{
inline constexpr formula::DecimalRounding tenthMpa { formula::unit::Megapascal,
                                                     formula::DecimalPlaces { 1 },
                                                     formula::RoundingMode::HalfAwayFromZero };
} // namespace

int main()
{
    constexpr auto operation = formula::with_rounding<tenthMpa>();
    return sizeof(operation) > 0 ? 0 : 1;
}
