// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this exact lookup table declares the same key twice
// REJECT: must be initialized by a constant expression
//
// TWO rows, both the same key: the smallest table that can contain a
// duplicate at all, and the case the other lookup tests never reach. An
// empty table has no pair, a one-row table has no pair, and every other
// duplicate case here uses five rows -- so the boundary between "no pair to
// check" and "one pair to check" is checked by nothing else. Should the pair
// filter in `detail::KeyPairCheck` skip a table of two rows, duplicate
// detection silently switches off for every two-row table and the whole
// positive suite stays green; this file is what fails. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum class SpecimenShape
    {
        CubeSmall,
        CubeLarge,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 2> DuplicatedKeys {
        SpecimenShape::CubeLarge,
        SpecimenShape::CubeLarge,
    };

    inline constexpr auto broken = formula::exact_lookup<DuplicatedKeys, formula::unit::One>(
        SpecimenShape::CubeLarge, { formula::Rational { 1127, 1000 }, formula::Rational { 853, 1000 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
