// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this library node kind has no detail::LevelChildren specialisation
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
//
// A library node kind the walk meets without a `LevelChildren` entry of its
// own -- an omission of the library's, which the entry's primary template
// refuses in its own words. The calculation's message for a consumer's kind
// must not follow it: it would report the one omission twice, and name the
// wrong culprit.
//
// No public spelling reaches this: the one such kind today, what arithmetic
// over a retry leaves, is refused already, and the walk asks nothing of a
// node refused already. So the walk's registry step is asked of it directly.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

int main()
{
    using Unlisted = formula::detail::RefusedRetryValue<formula::dim::Scalar>;
    return formula::detail::CalculationReadsInRegistry<Unlisted>::accepted ? 0 : 1;
}
