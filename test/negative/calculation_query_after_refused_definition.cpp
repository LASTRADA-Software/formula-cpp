// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations
// REJECT: formula: this calculation neither defines nor reads this quantity
// REJECT: formula: these definitions read one another in a cycle
//
// A definition refused where it was written, for reading a series, inside a
// calculation. The refused read is missing from what the definition lists,
// so a graph built from it would not hold the retained mass, and a question
// about the retained mass would draw a second, false message -- that the
// calculation does not read it. The calculation is not judged further, and
// its queries answer nothing and say nothing.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Retained: formula::Quantity<Retained, "m_r", "an invented retained mass", unit::Gram>
{
};
struct Total: formula::Quantity<Total, "m_t", "an invented total mass", unit::Gram>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
struct Doubled: formula::Quantity<Doubled, "s_2", "an invented share, doubled", unit::One>
{
};
} // namespace

int main()
{
    constexpr auto shares =
        formula::calculation(formula::define<Share>(formula::sum(formula::series<Retained, 3>) / var<Total>),
                             formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })));
    return formula::dependencies_of<Retained>(shares).size() + (formula::depends_on<Doubled, Retained>(shares) ? 1 : 0)
                   == 0
               ? 0
               : 1;
}
