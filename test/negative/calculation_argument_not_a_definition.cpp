// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: calculation(...) takes only definitions made by define<Q>(expression)
// REJECT: formula: a calculation defines at least one quantity
// REJECT: formula: this calculation neither defines nor reads this quantity
// REJECT: no type named
// REJECT: is not a member
// REJECT: no matching
//
// An expression where a definition belongs: it says how to calculate
// something, but not which quantity. Refused where the calculation is
// written; its graph is not built, so nothing else asks what the expression
// defines, and the queries over it answer nothing and say nothing.
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
struct Halved: formula::Quantity<Halved, "s_h", "an invented share, halved", unit::One>
{
};
struct Unrelated: formula::Quantity<Unrelated, "u", "an invented quantity nothing reads", unit::One>
{
};
} // namespace
int main()
{
    constexpr auto shares = formula::calculation(formula::define<Share>(var<Factor> * var<Other>),
                                                 var<Share> * formula::number(formula::Rational { 2 }));
    return formula::dependencies_of<Share>(shares).size() + (formula::depends_on<Share, Factor>(shares) ? 1 : 0) == 0
               ? 0
               : 1;
}
