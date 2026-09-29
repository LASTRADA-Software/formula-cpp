// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this worksheet's environment provides no value for an input of its calculation
// REJECT: formula: this environment provides no value for this quantity
// REJECT: no matching
//
// The environment has no entry for the other factor, an input. Asking the
// worksheet afterwards reads the input it does not hold as absent, and adds
// no message of its own.
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
    auto sheet = formula::worksheet(shares, formula::environment(formula::Measured<Factor> { formula::Rational { 3 } }));
    return sheet.checked_calculate<Doubled>().has_value() ? 0 : 1;
}
