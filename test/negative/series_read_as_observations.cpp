// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a single value or a series for this quantity, not raw observations
// REJECT: provides no value for this quantity
// REJECT: different capacity
// REJECT: cannot convert
// REJECT: no viable conversion
// REJECT: could not convert
//
// A series binned as raw observations of the same quantity: the
// environment refuses the read once.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
struct Count: formula::Quantity<Count, "n", "particles in a class", formula::unit::One>
{
};
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 127, 1),
                                                     formula::band(127, 1, 197, 1),
                                                     formula::band(197, 1, 331, 1) };

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Size>(formula::Measured<Size> { formula::Rational { 103 } },
                                                        formula::Measured<Size> { formula::Rational { 127 } },
                                                        formula::Measured<Size> { formula::Rational { 241 } }));

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::observations<Size, 3>);

int main()
{
    return formula::checked_evaluate_series<Count>(counted, inputs).has_value() ? 0 : 1;
}
