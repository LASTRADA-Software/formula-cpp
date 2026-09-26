// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: whose a method's constraints are is the library's to state, not an author's
//
// A method whose own constraints are wrapped by hand as a jurisdiction
// overlay's. Every verdict `check_method` records for it would then say
// "jurisdiction overlay: ..." of a check nobody overlaid. Only
// `with_constraints` applied through an overlay builds `OverlaidConstraints`.
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

inline constexpr formula::Citation claimed { .reference = "Example Standard 12:2021 NA" };

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::OverlaidConstraints<> { formula::constraints(), claimed });
} // namespace

int main()
{
    return formula::constraint_origin(m).provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
