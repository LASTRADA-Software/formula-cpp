// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a rounding rule's provenance is the library's to state, not an author's
//
// A rounding node built by hand -- for a variant, say -- claiming that a
// jurisdiction's overlay set its rule. Were the node to carry a provenance of
// its own, an untouched method's trace would say "jurisdiction overlay:
// Nobody's NA" of a rule nobody overlaid. It holds a `RoundingRule` instead,
// and stating a rule's provenance is refused wherever the rule is built.
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

using Rule =
    formula::RoundingRule<formula::unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

inline constexpr auto pressure = var<Force> / (var<EdgeX> * var<EdgeX>);
using Pressure = std::remove_cv_t<decltype(pressure)>;

inline constexpr formula::Citation forged { .reference = "Nobody's NA" };

inline constexpr formula::RoundingRuleNode<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero,
                                           Pressure>
    claimed { { {}, pressure }, Rule { formula::RoundingProvenance::JurisdictionOverlay, forged } };
} // namespace

int main()
{
    return claimed.rule.provenance() == formula::RoundingProvenance::JurisdictionOverlay ? 0 : 1;
}
