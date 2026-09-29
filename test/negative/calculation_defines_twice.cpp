// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this calculation defines the same quantity more than once
// REJECT: formula: this definition reads the quantity it defines
// REJECT: formula: these definitions read one another in a cycle
// REJECT: formula: a calculation holds at most 64 quantities
// REJECT: formula: this calculation neither defines nor reads this quantity
//
// The share defined twice, by two different formulas: neither is guessed.
// The graph is not built from the two, so no later check adds a message, and
// a query answers nothing.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
struct Doubled: formula::Quantity<Doubled, "s_2", "an invented share, doubled", unit::One>
{
};
struct Halved: formula::Quantity<Halved, "s_h", "an invented share, halved", unit::One>
{
};
struct Unrelated: formula::Quantity<Unrelated, "u", "an invented quantity nothing reads", unit::One>
{
};
} // namespace
int main()
{
    constexpr auto shares = formula::calculation(formula::define<Share>(var<Factor>),
                                                 formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })),
                                                 formula::define<Share>(var<Other>));
    return formula::dependencies_of<Doubled>(shares).size() == 0 ? 0 : 1;
}
