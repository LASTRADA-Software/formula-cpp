// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines
// REJECT: formula: this worksheet's environment provides no value for an input of its calculation
// REJECT: no matching
//
// The other factor's value is given under the wrong quantity. One mistake,
// one message: the entry nobody reads is refused, and the input it left
// without an entry is not looked for until every entry is accepted.
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
    [[maybe_unused]] auto sheet =
        formula::worksheet(shares,
                           formula::environment(formula::Measured<Factor> { formula::Rational { 3 } },
                                                formula::Measured<Unrelated> { formula::Rational { 2 } }));
    return 0;
}
