// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// A level written in terms of the level: pass 1 would need its own answer.
// Refused where the limit is declared; the limit is also evaluated here, and
// that adds no second message -- a refused limit is never evaluated, so the
// placeholder in its level never asks where it stands. This must not compile.
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
        (formula::var<ResultA> + formula::precision_level<ResultA>) / formula::Rational { 2 },
        formula::Rational { 1, 50 } * formula::precision_level<ResultA>);
    auto const outcome = formula::checked_evaluate<ResultA>(
        broken, formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
