// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula is bound to its result quantity already
// REJECT: no viable conversion
// REJECT: no matching
//
// A series bound to its result quantity, bound again. Refused where it is
// written, once. Evaluated and traced as a series, it adds nothing: the
// series verbs take a bound formula around a bound formula only to stay
// silent about it, where they would otherwise find no overload to take it.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", formula::unit::Gram>;

inline constexpr auto inputs = formula::environment(formula::measured_series<Retained>(131, 211));

int main()
{
    constexpr auto retained = formula::yields<Retained>(formula::series<Retained, 2>);
    constexpr auto rebound = formula::yields<Retained>(retained);
    auto const evaluated = formula::checked_evaluate_series(rebound, inputs);
    auto const explained = formula::explain_series(rebound, inputs);
    return evaluated.has_value() && explained.outcome.has_value() ? 0 : 1;
}
