// SPDX-License-Identifier: Apache-2.0
/// The shortest complete formula this library can express: two measured inputs,
/// one formula, one traceable result.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

/// A quantity is a type. It carries its own symbol, its own description and the
/// unit its values are stated in, and it is distinct from every other quantity
/// even when the unit is the same.
using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", formula::unit::Metre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;

/// The formula is written once, with ordinary operators, and is a compile-time
/// entity: this line builds a type, not a computation.
inline constexpr auto gradient = formula::var<Rise> / formula::var<Run>;

int main()
{
    auto const climb = formula::environment(formula::Measured<Rise> { 90 }, formula::Measured<Run> { 3000 });

    auto const result = formula::evaluate<Gradient>(gradient, climb);

    // The result prints as its number, in its quantity's unit, and says where
    // that number came from.
    std::println("{} = {} ({})", formula::symbol_of<Gradient>(), result, result.source());
    return 0;
}
