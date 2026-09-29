// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a worksheet holds single values, and this entry is a series or raw observations
// REJECT: formula: this worksheet's environment provides no value for an input of its calculation
// REJECT: holds a series for this quantity, not a single value
// REJECT: no matching
//
// The other factor is given as a series. The worksheet never reads the
// series as a single value, so the environment's own refusal of that read
// does not follow.
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
    [[maybe_unused]] auto sheet = formula::worksheet(
        shares,
        formula::environment(formula::Measured<Factor> { formula::Rational { 3 } },
                             formula::measured_series<Other>(formula::Measured<Other> { formula::Rational { 2 } },
                                                             formula::Measured<Other> { formula::Rational { 4 } })));
    return 0;
}
