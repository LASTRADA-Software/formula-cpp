// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: replace_variant<Tag>(expression) was given no citation
// REJECT: no matching
//
// A bound formula an overlay states as a replacement with no citation: refused
// once, in the words the formula it holds draws, and not as a missing
// overload.
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
    constexpr auto replacement = formula::yields<Factor>(var<Other> / formula::Rational { 2 });
    constexpr auto operation = formula::replace_variant<Cube>(replacement);
    return sizeof(operation) > 0 ? 0 : 1;
}
