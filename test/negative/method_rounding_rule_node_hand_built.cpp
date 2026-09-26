// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: only evaluate_method builds a rounding rule node
//
// A rounding node built by hand around the default rule, which claims nothing
// false about the rule -- and would still make a trace say "(method default)"
// of a rounding no method applied, wherever the node was put. Only
// `evaluate_method` builds one, around the variant it selected; an author who
// wants to round writes `rounded<...>(...)`.
//
// This must not compile.
#include <formula-cpp/method.hpp>

#include <type_traits>

namespace
{
struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto pressure = var<Force> / (var<EdgeX> * var<EdgeX>);
using Pressure = std::remove_cv_t<decltype(pressure)>;

inline constexpr formula::RoundingRuleNode<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero,
                                           Pressure>
    handBuilt { pressure };
} // namespace

int main()
{
    return handBuilt.rule().provenance() == formula::RoundingProvenance::MethodDefault ? 0 : 1;
}
