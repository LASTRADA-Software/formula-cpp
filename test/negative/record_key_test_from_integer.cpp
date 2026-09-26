// SPDX-License-Identifier: Apache-2.0
// EXPECT: 'int' to '
//
// A bare integer where the test key belongs; the sample key is spelt
// properly. No library static_assert refuses this, and none should fire:
// `TestId`'s constructor is `explicit`, so an integer does not convert to
// it. That `explicit` is the guard -- without it this compiles -- so the
// expected text is the compiler's own, and the registration rejects every
// library message. The sample key is well formed so that `SampleId`'s own
// `explicit`, which `record_key_sample_from_integer.cpp` pins, cannot refuse
// this in TestId's place.
//
// This must not compile.
#include <formula-cpp/record.hpp>

int main()
{
    auto const fromInteger = formula::record_key(formula::sample_id(17), 5);
    return fromInteger == formula::record_key(formula::sample_id(17), formula::test_id(5)) ? 0 : 1;
}
