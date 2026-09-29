// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: define<Q> takes an expression of one value, and this is a series
// REJECT: formula: this definition's expression measures a different dimension from the quantity it defines
// REJECT: formula: a calculation holds single values, and this definition reads a quantity as a series
// REJECT: no matching
//
// A series where the expression of one value belongs. Refused in this
// library's words rather than the compiler's missing overload; what the
// refusal returns defines the quantity as a constant of its own dimension,
// so the definition's own checks -- its dimension, its reads -- add nothing.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Retained: formula::Quantity<Retained, "m_r", "an invented retained mass", unit::Gram>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto share = formula::define<Share>(formula::series<Retained, 3>);
    return 0;
}
