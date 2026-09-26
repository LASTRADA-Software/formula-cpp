// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// A nested limit's level sums a series with the level added to every
// element: the inner level would silently be computed from the outer one.
// The level check sees through every series node kind -- the sum and the
// elementwise sum with its broadcast placeholder here -- so the inner limit
// is refused where it is declared. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct Retained: formula::Quantity<Retained, "m_r", "retained mass", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto inner = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::sum(formula::series<Retained, 3> + formula::precision_level<ResultA>), formula::precision_level<ResultA>);
    constexpr auto outer = formula::precision_limit<formula::PrecisionKind::Reproducibility>(formula::var<ResultA>, inner);
    auto const outcome = formula::checked_evaluate<ResultA>(
        outer,
        formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } },
                             formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 1 } },
                                                                formula::Measured<Retained> { formula::Rational { 2 } },
                                                                formula::Measured<Retained> { formula::Rational { 3 } })));
    return outcome.has_value() ? 0 : 1;
}
