// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these definitions read one another in a cycle, so none of them can be calculated first
// REJECT: formula: attempt_input reads the determination recorded for the attempt that is running
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// A calculation refused for a cycle, documented: the cycle's message is the
// one. The page says nothing more of it -- not even that one of its
// definitions reads attempt_input, which a page of a calculation that was not
// refused would refuse.
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
    constexpr auto circular = formula::calculation(
        formula::define<Doubled>(formula::attempt_input<Determination> * formula::Rational { 2 } + var<Mass>),
        formula::define<Mass>(var<Doubled> * formula::Rational { 1, 2 }));
    return formula::document(circular).symbols.empty() ? 0 : 1;
}
