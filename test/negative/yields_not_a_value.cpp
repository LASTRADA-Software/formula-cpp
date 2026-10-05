// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this bound formula is not an expression of one value
// REJECT: no matching
//
// A comparison bound to a quantity as though it were a formula, and
// evaluated. A comparison is a predicate, not a value, and none of the
// library's other kinds: refused once, in general words, rather than as an
// overload nobody matched.
#include <formula-cpp/formula.hpp>

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", formula::unit::Millimetre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });

int main()
{
    constexpr auto compared = formula::yields<Gradient>(formula::var<Rise> > formula::var<Run>);
    return formula::checked_evaluate(compared, inputs).has_value() ? 0 : 1;
}
