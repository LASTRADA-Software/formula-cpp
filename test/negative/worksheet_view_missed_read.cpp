// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this environment provides no value for this quantity
// REJECT: formula: this worksheet
// REJECT: no matching
//
// A definition evaluated against a worksheet view that leaves out one of
// its reads -- the other factor -- is refused where it reads it, rather than
// reading a value the worksheet has not brought up to date for it. The
// worksheet never builds such a view: this pins what it would do.
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
    using Sheet = decltype(sheet);
    using Graph = formula::detail::WorksheetGraphOf<Sheet>::type;
    constexpr std::uint64_t allButOther =
        Graph::reads[Graph::slot_of<Share>] & ~(std::uint64_t { 1 } << Graph::slot_of<Other>);
    using Blinkered = formula::detail::WorksheetView<Sheet, allButOther>;
    auto const share =
        formula::checked_evaluate<Share>(std::get<0>(shares.definitions).expression, Blinkered { &sheet, nullptr });
    return share.has_value() ? 0 : 1;
}
