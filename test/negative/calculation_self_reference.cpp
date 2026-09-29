// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this definition reads the quantity it defines, so it can never be calculated
// REJECT: formula: these definitions read one another in a cycle
// REJECT: formula: this calculation neither defines nor reads this quantity
//
// A share that reads itself. Refused as reading itself, and not a second
// time as a cycle of one; a query over the calculation answers nothing.
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
    constexpr auto shares =
        formula::calculation(formula::define<Share>(var<Share> * var<Factor>),
                             formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })));
    return formula::upstream_of<Doubled>(shares).size() == 0 ? 0 : 1;
}
