// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this worksheet's calculation neither defines nor reads the quantity asked for
// REJECT: formula: this calculation neither defines nor reads this quantity
// REJECT: no matching
//
// A derivation asked of a quantity the calculation does not hold: refused as
// asking for it is, once, and no block is looked for.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/trace.hpp>
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
struct Unrelated: formula::Quantity<Unrelated, "u", "an invented quantity nothing reads", unit::One>
{
};

// The factor and the other factor are its inputs; the share and the
// doubled share are calculated.
inline constexpr auto shares =
    formula::calculation(formula::define<Share>(var<Factor> * var<Other>),
                         formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })));
} // namespace

int main()
{
    auto sheet = formula::worksheet(shares,
                                    formula::environment(formula::Measured<Factor> { formula::Rational { 3 } },
                                                         formula::Measured<Other> { formula::Rational { 2 } }));
    return formula::explain_worksheet<Unrelated>(sheet).entries.empty() ? 0 : 1;
}
