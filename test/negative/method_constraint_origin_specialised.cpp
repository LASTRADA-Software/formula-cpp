// SPDX-License-Identifier: Apache-2.0
// EXPECT: ConstraintOrigin
//
// `ConstraintOrigin`'s refusing constructor, explicitly specialised by user code with
// no `detail::` written. An explicit specialisation of a member is a member
// definition, with a member's access to the private fields, so before the
// refusing constructors took a trailing `detail::ProvenanceStatedByAuthor`
// this compiled on cl 19.51, g++ 13.3 and 14.2 and clang++ 20.1.8, and forged
// a provenance only the library may state. The specialisation must now spell
// that parameter, and so name `detail::`; without it, it matches nothing, and
// every compiler refuses it naming the type.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

namespace
{
struct Factor: formula::Quantity<Factor, "k", "factor", formula::unit::One>
{
};
} // namespace

namespace unit = formula::unit;
using formula::var;
using Rule = formula::RoundingRule<unit::One, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;
using Factors = decltype(var<Factor>);

template <>
constexpr formula::ConstraintOrigin::ConstraintOrigin(formula::ConstraintProvenance provenance,
                                                      formula::Citation cited) noexcept:
    _provenance { provenance },
    _source { cited }
{
}

int main()
{
    return 0;
}
