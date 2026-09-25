// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an exact lookup's keys must be enumerators of a scoped enumeration
//
// `keys_match` on two `int`s. The predicate every "is this the same key"
// question in `lookup.hpp` is built on is public, so it refuses the same key
// types the node refuses -- see `exact_lookup_int_key_table.cpp` for the
// reason, and note that the two files exist separately so that removing
// either guard on its own is caught rather than masked by the other. This
// must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    inline constexpr bool matched = formula::keys_match(1, 1);
} // namespace

int main()
{
    return matched ? 0 : 1;
}
