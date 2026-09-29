// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation defines at least one quantity; calculation() with no definitions calculates nothing
// REJECT: formula: calculation(...) takes only definitions made by define<Q>(expression)
// REJECT: no matching
//
// A calculation of nothing. Refused where it is written; its queries answer
// nothing and say nothing.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

int main()
{
    constexpr auto nothing = formula::calculation();
    return formula::inputs_of(nothing).size() + formula::calculation_order(nothing).size() == 0 ? 0 : 1;
}
