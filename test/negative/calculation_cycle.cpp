// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these definitions read one another in a cycle, so none of them can be calculated first
// REJECT: formula: this definition reads the quantity it defines
// REJECT: formula: this calculation neither defines nor reads this quantity
//
// The share reads the doubled share, which reads the halved one, which reads
// the share: a cycle of three, named in one message. The other factor reads
// the share without being on the cycle. A query over the calculation answers
// nothing.
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
    constexpr auto shares = formula::calculation(
        formula::define<Share>(var<Doubled> + var<Factor>),
        formula::define<Doubled>(var<Halved> * formula::number(formula::Rational { 4 })),
        formula::define<Halved>(var<Share> * formula::number(formula::Rational { 1, 2 })),
        formula::define<Other>(var<Share>));
    return formula::affected_by<Factor>(shares).size() == 0 ? 0 : 1;
}
