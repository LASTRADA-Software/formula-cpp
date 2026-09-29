// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these definitions read one another in a cycle, so none of them can be calculated first
// REJECT: formula: this worksheet's environment
// REJECT: formula: set() names a quantity this worksheet's calculation neither reads nor defines
// REJECT: formula: this worksheet's calculation neither defines nor reads the quantity asked for
// REJECT: formula: clear_override or is_overridden names an input of the calculation
// REJECT: no matching
//
// A worksheet of a calculation refused for a cycle: the cycle's message is
// the one, and the worksheet -- over an environment that misses the input
// and holds an entry nobody reads, set, asked, asked about and cleared of an
// override of an input, and asked for the derivation of a quantity its
// calculation does not hold and of one it defines -- says nothing more.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/trace.hpp>

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
    constexpr auto circular =
        formula::calculation(formula::define<Share>(var<Doubled> + var<Factor>),
                             formula::define<Doubled>(var<Share> * formula::number(formula::Rational { 2 })));
    auto sheet =
        formula::worksheet(circular, formula::environment(formula::Measured<Unrelated> { formula::Rational { 1 } }));
    sheet.set(formula::Measured<Unrelated> { formula::Rational { 2 } });
    sheet.clear_override<Factor>();
    if (!formula::explain_worksheet<Unrelated>(sheet).entries.empty()
        || !formula::explain_worksheet<Share>(sheet).entries.empty())
        return 2;
    return sheet.checked_calculate<Unrelated>().has_value() || sheet.is_overridden<Factor>() ? 0 : 1;
}
