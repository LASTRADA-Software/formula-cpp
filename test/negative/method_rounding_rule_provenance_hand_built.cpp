// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a rounding rule's provenance is the library's to state, not an author's
//
// A method whose own rounding rule claims to be a jurisdiction overlay's. Every
// trace of it would then say "(jurisdiction overlay: ...)" of a rule nobody
// overlaid -- a false statement in the one place an inspector reads to learn
// whose rule it was. A rule's provenance is set only by `rounding_rule<...>()`
// and by `with_rounding` applied through an overlay.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

using Rule =
    formula::RoundingRule<formula::unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

inline constexpr formula::Citation claimed { .reference = "Example Standard 12:2021 NA" };

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          Rule { formula::RoundingProvenance::JurisdictionOverlay, claimed },
                                          formula::constraints());
} // namespace

int main()
{
    return m.rounding.provenance() == formula::RoundingProvenance::JurisdictionOverlay ? 0 : 1;
}
