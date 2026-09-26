// SPDX-License-Identifier: Apache-2.0
// EXPECT: TestId' to '
//
// The test key where the sample key belongs, and the sample key where the
// test key belongs. No library static_assert refuses this, and none should
// fire: `SampleId` and `TestId` are two types, each with an `explicit`
// constructor, so neither converts to the other and `record_key` has no
// overload that takes them in this order. The strong types are the guard,
// which is why the expected text is the compiler's own and the registration
// rejects every library message.
//
// This must not compile.
#include <formula-cpp/record.hpp>

int main()
{
    auto const swapped = formula::record_key(formula::test_id(3), formula::sample_id(23));
    return static_cast<int>(swapped.sample().value());
}
