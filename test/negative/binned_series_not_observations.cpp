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
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 11, 1),
                                                     formula::band(11, 1, 29, 1),
                                                     formula::band(29, 1, 83, 1) };

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Size>(formula::Measured<Size> { formula::Rational { 4 } },
                                                        formula::Measured<Size> { formula::Rational { 11 } },
                                                        formula::Measured<Size> { formula::Rational { 47 } }));

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::series<Size, 3>);

int main()
{
    return formula::checked_evaluate_series<Count>(counted, inputs).has_value() || formula::render(counted).empty()
                   || formula::document(counted).symbols.empty()
               ? 0
               : 1;
}
