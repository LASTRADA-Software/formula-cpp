// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: has no detail::LevelChildren specialisation
// REJECT: formula: this definition's expression measures a different dimension from the quantity it defines
// REJECT: no matching
//
// A single value made of a series -- a share of a sum -- is still a series
// read, and a calculation holds single values. The series is read twice,
// and each walk through a node over it (the sums, the division) could have
// added a message of its own: one message, as the count pins.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Retained: formula::Quantity<Retained, "m_r", "an invented retained mass", unit::Gram>
{
};
struct Total: formula::Quantity<Total, "m_t", "an invented total mass", unit::Gram>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto share =
        formula::define<Share>(formula::sum(formula::series<Retained, 3>) / formula::var<Total>
                               * formula::sum(formula::series<Retained, 3>) / formula::var<Total>);
    return 0;
}
