// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay lists the same operation twice
//
// Two rounding overrides of DIFFERENT granularities in one overlay. They are
// two different types, so a repeat rule comparing operation types alone lets
// them through -- and the second then silently replaces the first, because a
// method has one rounding rule. The repeat rule compares each operation's
// identity instead, and every rounding override has the same one.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::constraints());
} // namespace

int main()
{
    constexpr auto twice = formula::overlay(formula::with_rounding<formula::unit::Megapascal,
                                                                   formula::DecimalPlaces { 2 },
                                                                   formula::RoundingMode::HalfAwayFromZero>(),
                                            formula::with_rounding<formula::unit::Megapascal,
                                                                   formula::DecimalPlaces { 0 },
                                                                   formula::RoundingMode::HalfAwayFromZero>());
    constexpr auto overlaid = formula::apply(twice, m);
    return overlaid.rounding.places.value == 0 ? 0 : 1;
}
