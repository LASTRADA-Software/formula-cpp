// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an exact lookup node can only be evaluated with Rep = Rational
//
// `checked_evaluate_si<double>` on an exact lookup. The guard's message is
// author-facing text and therefore tested API, like every other
// `static_assert` in this phase -- and it is the ONLY place the reason for
// this refusal reaches a user, since nobody reads a header's file comment
// when a build fails. It must say what the file comment says: every lookup
// table in this library answers in one representation, NOT that selecting a
// row by key needs exact arithmetic, which is untrue of comparing two
// enumerators. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
    enum class SpecimenShape
    {
        Cube100,
        Cube150,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 2> ShapeKeys { SpecimenShape::Cube100,
                                                                     SpecimenShape::Cube150 };

    inline constexpr auto node = formula::exact_lookup<ShapeKeys, formula::unit::One>(
        SpecimenShape::Cube100, { formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    auto const computed = formula::checked_evaluate_si<double>(node, formula::environment());
    return computed.has_value() ? 0 : 1;
}
