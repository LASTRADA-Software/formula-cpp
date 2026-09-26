// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record origin is the library's to state, not an author's
//
// An origin built by hand: it would make a trace say a value was read from
// sample 23, test 3, whether or not anything was. The public constructor is
// declared only to refuse this in the library's words; the constructor that
// states an origin is private, and reached only from the record a scope
// reads. Nothing else is wrong here.
//
// This must not compile.
#include <formula-cpp/record.hpp>

int main()
{
    formula::RecordOrigin const forged { "Reference", formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                         true };
    return forged.is_bound() ? 0 : 1;
}
