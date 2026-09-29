// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this calculation defines the same quantity more than once
// REJECT: formula: this definition reads the quantity it defines
// REJECT: formula: these definitions read one another in a cycle
//
// The share defined twice, the second time from itself. One mistake -- two
// definitions where one belongs -- and one message: which of the two stays
// decides whether anything reads itself, so that is not judged until only
// one does.
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
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto shares =
        formula::calculation(formula::define<Share>(var<Factor> + formula::number(formula::Rational { 1 })),
                             formula::define<Share>(var<Share> + formula::number(formula::Rational { 1 })));
    return 0;
}
