// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: set() was given the same quantity more than once
// REJECT: formula: set() names a quantity this worksheet's calculation neither reads nor defines
// REJECT: tuple index
// REJECT: no matching
//
// set() names one quantity twice -- and one the calculation does not hold,
// which is not judged until each quantity is named once: one message.
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
    auto sheet = formula::worksheet(shares,
                                    formula::environment(formula::Measured<Factor> { formula::Rational { 3 } },
                                                         formula::Measured<Other> { formula::Rational { 2 } }));
    sheet.set(formula::Measured<Unrelated> { formula::Rational { 1 } },
              formula::Measured<Unrelated> { formula::Rational { 2 } });
    return 0;
}
