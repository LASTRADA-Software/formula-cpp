// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an exact lookup's keys must be enumerators of a scoped enumeration
//
// `key_table_is_well_formed` on a table of `int`. The predicate is public and
// is what a runtime loader reaches for, so it must refuse exactly the key
// types `ExactLookupNode` refuses: a validator that blessed an
// `std::array<int, N>` would be certifying a table no node could ever be
// built from -- two surfaces answering the same question differently, which
// is the defect this phase keeps finding. Pinned separately from
// `exact_lookup_keys_match_int_key.cpp` so that removing either guard alone
// is caught; each file exercises exactly one entry point. This must not
// compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    inline constexpr formula::KeyTable<int, 2> NotEnumerators { 1, 2 };
    inline constexpr bool blessed = formula::key_table_is_well_formed(NotEnumerators);
} // namespace

int main()
{
    return blessed ? 0 : 1;
}
