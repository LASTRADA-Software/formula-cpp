// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this definition's expression measures a different dimension from the quantity it defines
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: no matching
//
// A stress defined by a load over a width: a force per length, not per area.
// Every read of the stress would be a value it does not measure. Refused
// where the definition is written, once.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Load: formula::Quantity<Load, "F", "an invented load", unit::Newton>
{
};
struct Width: formula::Quantity<Width, "b", "an invented width", unit::Millimetre>
{
};
struct Stress: formula::Quantity<Stress, "sigma", "an invented stress", unit::Megapascal>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto stress = formula::define<Stress>(formula::var<Load> / formula::var<Width>);
    return 0;
}
