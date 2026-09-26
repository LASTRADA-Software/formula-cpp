// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's limit expression reads a precision_level whose quantity does not measure the dimension of the level expression
//
// The same mistake as `precision_level_dimension_mismatch.cpp`, on the route
// it cannot reach: the node aggregate-initialised directly, with no call to
// `precision_limit()`. Only this case tells a check in the class body from
// one in the factory. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", formula::unit::Gram>
{
};
struct Width: formula::Quantity<Width, "w", "specimen width", formula::unit::Millimetre>
{
};
} // namespace

namespace
{
inline constexpr auto level = formula::var<ResultA>;
inline constexpr auto limit = formula::Rational { 1, 50 } * formula::precision_level<Width>;
inline constexpr formula::PrecisionLimitNode<formula::PrecisionKind::Repeatability, decltype(level), decltype(limit)> broken {
    {}, level, limit
};
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
