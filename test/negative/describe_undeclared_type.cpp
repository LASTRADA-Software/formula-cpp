// SPDX-License-Identifier: Apache-2.0
// A type that declares no metadata must be refused in the library's own words,
// not accepted with empty strings and not buried in template noise.
// This must not compile.
#include <formula-cpp/quantity.hpp>

struct PlainStruct
{
    int value {};
};

int main()
{
    // RequireDescribed, not Describe<PlainStruct>::symbol: the primary Describe
    // is deliberately empty, so that spelling would give the compiler's own "no
    // member" error rather than ours. `::value` is required -- the assertion is
    // in the class body and a bare alias would instantiate nothing.
    return formula::RequireDescribed<PlainStruct>::value ? 1 : 0;
}
