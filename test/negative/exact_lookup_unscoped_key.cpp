// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an exact lookup's keys must be enumerators of a scoped enumeration
//
// An UNSCOPED enumeration as a key type. It looks like it works -- the table
// declares a key and the lookup names it -- but an unscoped enumerator
// converts to and from arithmetic silently, which destroys the one property
// the choice of an enumerated key was made for: that a mistyped key is a
// compile error at the offending token rather than a value that quietly
// selects the wrong row, or no row at all. `int` keys are refused for the
// same reason. This must not compile.
//
// ONE row, deliberately. With two or more, `detail::key_table_is_valid` runs
// its pairwise sweep, which reaches `keys_match` and would refuse this table
// through that guard instead -- so a table of two would still fail if
// `ExactLookupNode`'s own `RequireScopedEnumKey` assertion were deleted. At
// `N < 2` the sweep short-circuits and `keys_match` is never instantiated, so
// the node's own guard is the only thing left that can refuse this, which is
// exactly what this file exists to pin. Its companions
// `exact_lookup_keys_match_int_key.cpp` and `exact_lookup_int_key_table.cpp`
// pin the other entry points the same way.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum SpecimenShape
    {
        Cube100,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 1> ShapeKeys { Cube100 };

    inline constexpr auto broken =
        formula::exact_lookup<ShapeKeys, formula::unit::One>(Cube100, { formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
