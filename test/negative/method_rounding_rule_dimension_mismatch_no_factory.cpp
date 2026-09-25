// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report
//
// The same mistake as `method_rounding_rule_dimension_mismatch.cpp`, on the
// route that case cannot reach: a `Method<...>` declared directly, with no
// call to `method()` anywhere. `method()` returns a `Method` by value and so
// completes the class either way; only this case tells a check in the class
// body from one in the factory.
//
// The pack and the rule are named by `decltype` of `constexpr` variables, so
// they are `const`-qualified, as they would be for anyone writing this out.
// That also pins the `remove_cv_t` in `Method`'s check: without it no
// specialisation of `VariantsDimension` matches a `const Variants<...>`, the
// rule is never asked, and this file compiles.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto pack = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                               formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ));
inline constexpr auto rule = formula::
    rounding_rule<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>();
inline constexpr auto checks = formula::constraints();

inline constexpr formula::Method<decltype(pack), decltype(rule), decltype(checks)> m { pack, rule, checks };
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(m.variantSet.cases)>);
}
