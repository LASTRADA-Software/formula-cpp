// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this calculation neither defines nor reads this quantity
// REJECT: no matching
//
// A question about a quantity the calculation does not hold, asked twice:
// one message, since it is one mistake, whichever query asks it.
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
    constexpr auto shares = formula::calculation(formula::define<Share>(var<Factor> * var<Other>));
    return formula::dependencies_of<Unrelated>(shares).size() + (formula::depends_on<Share, Unrelated>(shares) ? 1 : 0)
                   == 0
               ? 0
               : 1;
}
