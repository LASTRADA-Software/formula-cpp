// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this addition or subtraction measure different dimensions
// Two mistakes: the logarithm of a length, and a sum of a scalar and a mass. Fixing either leaves the other,
// so the sum's message is still reported; the logarithm's own refusal does not hide it.
// This must not compile.

#include <formula-cpp/formula.hpp>

namespace
{
struct Height: formula::Quantity<Height, "h", "an invented height", formula::unit::Metre>
{
};
struct Mass: formula::Quantity<Mass, "m", "an invented mass", formula::unit::Kilogram>
{
};
} // namespace

inline constexpr auto broken = formula::ln(formula::var<Height>) + formula::var<Mass>;

int main()
{
    return decltype(broken)::dimension == formula::dim::Scalar ? 0 : 1;
}
