// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// A level written in terms of the level, the placeholder hidden inside an
// exponential: the level checks must see inside it (`LevelChildren` for
// `TranscendentalNode`). This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};
} // namespace

int main()
{
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::var<Ratio> * formula::exp(formula::precision_level<Ratio> - formula::precision_level<Ratio>),
        formula::Rational { 1, 50 } * formula::precision_level<Ratio>);
    auto const outcome = formula::checked_evaluate<Ratio>(
        broken, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }));
    return outcome.has_value() ? 0 : 1;
}
