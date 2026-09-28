// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the record being evaluated cannot be unbound
//
// An unbound record is a test not done yet: ordinary for a reference, and
// meaningless for the specimen being evaluated, whose environment the
// context is. The role is plain and nothing else is built, so the plain-role
// rule and the context's rules have nothing to say: making ThisRecord's
// record unbound is the only thing wrong.
//
// This must not compile.
#include <formula-cpp/record.hpp>

namespace
{
struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

using HereEnvironment = formula::Environment<formula::Measured<Force>>;

constexpr auto broken = formula::Record<formula::ThisRecord, HereEnvironment>::unbound();
} // namespace

int main()
{
    return broken.is_bound() ? 0 : 1;
}
