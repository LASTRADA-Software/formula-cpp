// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
// The refusal of expression_add_dimension_mismatch.cpp, for quantities
// declared by alias: the library's own message, whichever way the operands
// were declared.
#include <formula-cpp/expression.hpp>

using Volume = formula::Quantity<struct VolumeTag, "V", "a volume", formula::unit::Litre>;
using Length = formula::Quantity<struct LengthTag, "L", "a length", formula::unit::Metre>;

// A volume plus a length has no meaning, and must not compile.
inline constexpr auto broken = formula::var<Volume> + formula::var<Length>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
