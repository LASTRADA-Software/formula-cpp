// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: attempt_input reads the determination recorded for the attempt that is running
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// attempt_input in a definition of a calculation documented: refused where
// the page is made, as in a formula documented on its own, so that no page
// lists a series of no determinations. The calculation itself lists a retry's
// context as reading nothing, and holds it.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/retry.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Determination: formula::Quantity<Determination, "d", "an invented determination", unit::Gram>
{
};
struct Doubled: formula::Quantity<Doubled, "d_2", "an invented determination, doubled", unit::Gram>
{
};
struct Mass: formula::Quantity<Mass, "m", "an invented mass", unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto doubling =
        formula::calculation(formula::define<Doubled>(formula::attempt_input<Determination> * formula::Rational { 2 }),
                             formula::define<Mass>(var<Doubled> + var<Determination>));
    return formula::document(doubling).symbols.empty() ? 1 : 0;
}
