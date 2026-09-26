// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: replace_variant<Tag>(expression) was given no citation
//
// A replaced formula an overlay states with no citation. The trace exists to say why
// a number is what it is, and "by jurisdiction overlay" with no citation says
// nothing a reader can check, so the citation is required -- refused in the
// library's words rather than the compiler's "too few arguments".
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/overlay.hpp>

namespace
{
struct Cube
{
};

struct Factor: formula::Quantity<Factor, "k", "factor", formula::unit::One>
{
};
struct Other: formula::Quantity<Other, "o", "another factor", formula::unit::One>
{
};

using formula::var;
} // namespace

int main()
{
    constexpr auto operation = formula::replace_variant<Cube>(var<Factor>);
    return sizeof(operation) > 0 ? 0 : 1;
}
