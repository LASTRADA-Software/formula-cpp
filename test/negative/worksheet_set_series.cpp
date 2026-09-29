// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a worksheet holds single values, and this entry is a series or raw observations
// REJECT: formula: set() names a quantity this worksheet's calculation neither reads nor defines
// REJECT: holds a series for this quantity, not a single value
// REJECT: to non-scalar type
// REJECT: no viable conversion
// REJECT: cannot convert from
// REJECT: no matching
//
// set() is given a series for the other factor.
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
    sheet.set(formula::measured_series<Other>(formula::Measured<Other> { formula::Rational { 2 } },
                                              formula::Measured<Other> { formula::Rational { 4 } }));
    return 0;
}
