// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_base_dimension_name_must_not_be_empty
//
// A base dimension needs a name: the empty one is refused, naming the rule.
// This must not compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension Refused = formula::base_dimension("");

int main()
{
    return Refused.length.numerator;
}
