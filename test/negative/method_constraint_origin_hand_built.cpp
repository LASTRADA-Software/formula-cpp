// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: whose a method's constraints are is the library's to state, not an author's
//
// A method whose own constraints claim to be a jurisdiction overlay's. Every
// verdict `check_method` records for it would then say "jurisdiction overlay:
// ..." of a check nobody overlaid -- a false statement in the one clause an
// inspector reads to learn whose acceptance logic it was. Whose a method's
// constraints are is set only by `method(...)` and by `with_constraints`
// applied through an overlay.
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
using Pack = decltype(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )));

inline constexpr formula::Citation claimed { .reference = "Example Standard 12:2021 NA" };

inline constexpr auto m = formula::Method<Pack, Rule, formula::ConstraintSet<>> {
    formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    Rule {},
    formula::constraints(),
    formula::ConstraintOrigin { formula::ConstraintProvenance::JurisdictionOverlay, claimed },
};
} // namespace

int main()
{
    return m.constraintOrigin.provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
