// SPDX-License-Identifier: Apache-2.0
// EXPECT: binned counts raw observations into classes, and this is not a set of observations
// REJECT: no matching
// REJECT: this binning's key unit
// REJECT: not raw observations
// REJECT: must be initialized by a constant expression
//
// A series binned as though it were raw observations: refused once, and
// the refused binning evaluates, renders and documents with nothing more said.
#include <formula-cpp/binning.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>

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

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::series<Size, 3>);

int main()
{
    return formula::checked_evaluate_series<Count>(counted, inputs).has_value() || formula::render(counted).empty()
                   || formula::document(counted).symbols.empty()
               ? 0
               : 1;
}
