// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: whose a method's constraints are is the library's to state, not an author's
//
// An origin claiming a jurisdiction overlay's constraints, stated by hand.
// A `ConstraintOrigin` is what `check_method` tells a sink about whose a
// method's constraints are, and what `constraint_origin` answers; the library
// reads it off the constraint part, and an author who could state one could
// hand a sink "jurisdiction overlay: ..." for checks nobody overlaid.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
inline constexpr formula::Citation claimed { .reference = "Example Standard 12:2021 NA" };

inline constexpr formula::ConstraintOrigin origin { formula::ConstraintProvenance::JurisdictionOverlay, claimed };
} // namespace

int main()
{
    return origin.provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
