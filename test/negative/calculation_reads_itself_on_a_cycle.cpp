// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this definition reads the quantity it defines, so it can never be calculated
// REJECT: formula: these definitions read one another in a cycle
//
// The share reads itself and the doubled share, which reads the share. The
// share reading itself is refused; the cycle through the doubled share is
// judged only once no definition reads itself, so the one message is this
// one, and removing the share's read of itself would bring the next.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
struct Doubled: formula::Quantity<Doubled, "s_2", "an invented share, doubled", unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto shares =
        formula::calculation(formula::define<Share>(var<Share> + var<Doubled>),
                             formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })));
    return 0;
}
