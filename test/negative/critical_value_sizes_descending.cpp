// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this sample-size table's sizes do not strictly ascend
//
// A table written backwards: 6, 5, 4. Both neighbouring pairs descend, and
// the table is refused once, for the first of them -- not once per pair.
//
// The corrections list is also one short, which for a valid table would draw
// the arity message. Here it must not: with the table refused, the list is
// measured against a table that is about to change, so the arity check is
// gated behind the table's (`detail::SampleSizeCorrections`). One mistake,
// one message.
//
// This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<3> Backwards { 6, 5, 4 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Backwards, formula::unit::One>(
        formula::var<Specimens>, { formula::Rational { 70 }, formula::Rational { 20 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
