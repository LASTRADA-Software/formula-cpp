// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement
// REJECT: formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines
// REJECT: formula: this worksheet's environment provides no value for an input of its calculation
// REJECT: no matching
//
// The share, which the calculation calculates, is given as a measurement: a
// value typed in over a calculated one is an override, entered(...).
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
                                                formula::Measured<Other> { formula::Rational { 2 } },
                                                formula::Measured<Share> { formula::Rational { 5 } }));
    return 0;
}
