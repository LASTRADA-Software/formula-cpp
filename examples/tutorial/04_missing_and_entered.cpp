// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 4: missing and entered values. One side of the specimen
// is never measured, and the strength is once typed in by hand.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;
using namespace formula::literals;

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;

using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;

constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(var<Load> / loadedArea.expression);
} // namespace

int main()
{
    // --8<-- [start:measured]
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 675 });
    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("measured:        {} = {} ({})", formula::symbol_of<Strength>(), *result, result->source());
    // --8<-- [end:measured]

    // --8<-- [start:absent]
    auto const sideBMissing = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB>::absent(), formula::Measured<Load> { 675 });
    auto const withoutB = formula::checked_evaluate(strength, sideBMissing);
    if (!withoutB)
    {
        std::println("cannot calculate the strength: {}", withoutB.error());
        return 1;
    }
    if (formula::number_of(*withoutB).has_value())
    {
        std::println("a strength was calculated without side b: {}", *withoutB);
        return 1;
    }
    std::println("b not measured:  {} = {}", formula::symbol_of<Strength>(), withoutB->kind());
    // --8<-- [end:absent]

    // --8<-- [start:entered]
    auto const typedIn = formula::environment(formula::Measured<SideA> { 150 },
                                              formula::Measured<SideB> { 150 },
                                              formula::Measured<Load> { 675 },
                                              formula::entered(formula::Measured<Strength> { 31 }));
    auto const asEntered = formula::checked_evaluate(strength, typedIn);
    if (!asEntered)
    {
        std::println("cannot calculate the strength: {}", asEntered.error());
        return 1;
    }
    if (!asEntered->is_overridden())
    {
        std::println("the strength entered by hand was not used: {}", *asEntered);
        return 1;
    }
    std::println("entered by hand: {} = {} ({})", formula::symbol_of<Strength>(), *asEntered, asEntered->source());
    // --8<-- [end:entered]
    return 0;
}
