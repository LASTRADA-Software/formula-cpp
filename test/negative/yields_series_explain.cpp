// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: no matching
//
// A bound series handed to explain, checked_explain and trace_of, which trace
// a single value: each refused as checked_evaluate refuses a series, pointing
// at checked_evaluate_series, rather than as an overload nobody matched. A
// series of its own quantity for each verb, so that each refusal is its own
// message: three.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", formula::unit::Gram>;
using Passing = formula::Quantity<struct PassingTag, "m_p", "mass passing a screen", formula::unit::Gram>;
using Sieved = formula::Quantity<struct SievedTag, "m_s", "mass sieved", formula::unit::Gram>;

inline constexpr auto inputs = formula::environment(formula::measured_series<Retained>(131, 211, 97),
                                                    formula::measured_series<Passing>(41, 43, 47),
                                                    formula::measured_series<Sieved>(53, 59, 61));

int main()
{
    auto const explained = formula::explain(formula::yields<Retained>(formula::series<Retained, 3>), inputs);
    auto const checkedExplained = formula::checked_explain(formula::yields<Passing>(formula::series<Passing, 3>), inputs);
    auto const traced = formula::trace_of(formula::yields<Sieved>(formula::series<Sieved, 3>), inputs);
    return explained.outcome.is_value() && checkedExplained.has_value() && !traced.empty() ? 0 : 1;
}
