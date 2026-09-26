// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's limit expression reads a precision_level whose quantity does not measure the dimension of the level expression
//
// A level of masses read as a width. Refused where the limit is declared, and
// the evaluation adds no second message: the placeholder's own dimension
// check is never reached for a limit already refused. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", formula::unit::Gram>
{
};
struct Width: formula::Quantity<Width, "w", "specimen width", formula::unit::Millimetre>
{
};
} // namespace

int main()
{
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        (formula::var<ResultA> + formula::var<ResultB>) / formula::Rational { 2 },
        formula::Rational { 1, 50 } * formula::precision_level<Width>);
    auto const outcome = formula::checked_evaluate<Width>(
        broken, formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } },
                                     formula::Measured<ResultB> { formula::Rational { 41 } }));
    return outcome.has_value() ? 0 : 1;
}
