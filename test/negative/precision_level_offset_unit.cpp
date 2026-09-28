// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_level names a quantity whose unit has an offset
//
// A level that is the spread of two temperatures, 1 K, read through a
// placeholder naming a quantity in degrees Celsius: the trace would show the
// level as -272.15 degC. Refused where the placeholder is named, in one
// message, whether the level is a spread or a mean. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct TempA: formula::Quantity<TempA, "T_A", "first temperature", formula::unit::Celsius>
{
};
struct TempB: formula::Quantity<TempB, "T_B", "second temperature", formula::unit::Celsius>
{
};
struct Spread: formula::Quantity<Spread, "L", "limit", formula::unit::Kelvin>
{
};
} // namespace

int main()
{
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::abs(formula::var<TempA> - formula::var<TempB>),
        formula::Rational { 1, 2 } * formula::precision_level<TempA>);
    auto const outcome =
        formula::checked_evaluate<Spread>(broken,
                                          formula::environment(formula::Measured<TempA> { formula::Rational { 21 } },
                                                               formula::Measured<TempB> { formula::Rational { 20 } }));
    return outcome.has_value() ? 0 : 1;
}
