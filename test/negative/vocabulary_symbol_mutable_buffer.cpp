// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: renames<Q> was given a buffer that is not const
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A buffer with static storage is a permitted constant-expression result, but
// nothing stops it being written to after a trace kept a view of it.
int main()
{
    static char buffer[] = "R";
    auto const entry = formula::renames<Strength>(buffer);
    return entry.symbol().size() == 1 ? 0 : 1;
}
