// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this exact lookup table declares the same key twice
//
// TWO rows, both the same key: the smallest table that can contain a
// duplicate at all, and the case the rest of the phase never reaches. An
// empty table has no pair, a one-row table has no pair, and every other
// duplicate case here uses five rows -- so the boundary between "no pair to
// check" and "one pair to check" is checked by nothing else. Measured: with
// `detail::key_table_is_valid`'s underflow guard widened from
// `Keys.size() < 2` to `< 3`, duplicate detection silently switches off for
// every two-row table and the whole positive suite stays green; this file is
// what fails. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum class SpecimenShape
    {
        Cube100,
        Cube150,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 2> DuplicatedKeys {
        SpecimenShape::Cube150,
        SpecimenShape::Cube150,
    };

    inline constexpr auto broken = formula::exact_lookup<DuplicatedKeys, formula::unit::One>(
        SpecimenShape::Cube150, { formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
