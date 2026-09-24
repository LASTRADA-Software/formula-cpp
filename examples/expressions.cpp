// SPDX-License-Identifier: Apache-2.0
//
// Expressions.
//
// Generic physics only, no standard cited: a formula built from a constant
// (pi) and a power, a result reported in a unit its own input never appears
// in, an absent input propagating to an empty result rather than a wrong
// number, a dimensional mismatch that is refused at compile time (shown but
// not compiled, below), and a manually entered result that replaces what the
// formula would have computed while saying so honestly.

#include <formula-cpp/formula.hpp>

#include <cstdio>

namespace
{
namespace unit = formula::unit;
using formula::var;

// ---- 1 & 3: a circular area -- a constant, a power, and later an absence ----

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", unit::SquareMetre>
{
};

// A = pi * d^2 / 4, with an exact-rational pi. `pi` is a constant node,
// `pow<2>` a power node -- the result, Area, is declared in square metres
// while Diameter is declared in millimetres, so this one formula also
// demonstrates a result reported in a different unit from its input.
constexpr auto circularArea = formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 };

// ---- 2: the same point made without a power in the way, for its own line ----

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "specimen mass", unit::Gram>
{
};
struct MassInKilogram: formula::Quantity<MassInKilogram, "m", "specimen mass", unit::Kilogram>
{
};

// ---- 4: a dimensional mismatch, shown but not compiled ----
//
// Diameter measures length; Area measures length squared. Adding them has no
// meaning, and the expression layer refuses it at the formula's own source
// line, not at evaluation:
//
//     constexpr auto broken = formula::var<Diameter> + formula::var<Area>;
//
// Uncommenting the line above does not compile. See docs/expressions.md for
// the exact diagnostic text a real build prints for this mistake.

// ---- 5: a formula whose result a person may override ----

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", unit::Litre>
{
};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", unit::One>
{
};

constexpr auto waterCementRatio = var<WaterVolume> / var<CementVolume>;

} // namespace

int main()
{
    // ---- 1. A formula with a constant and a power ----
    constexpr auto diameterKnown = formula::environment(formula::Measured<Diameter> { formula::Rational { 100 } });
    constexpr auto area = formula::checked_evaluate<Area>(circularArea, diameterKnown);
    std::printf("circular area of a 100 mm diameter = %f m2 (%s)\n",
                area->measurement().value().to_double(),
                area->is_value() ? "computed" : "no value");

    // ---- 2. A result quantity in a different unit from its input ----
    constexpr auto massInGrams = formula::environment(formula::Measured<SpecimenMass> { formula::Rational { 2500 } });
    constexpr auto massConverted = formula::checked_evaluate<MassInKilogram>(var<SpecimenMass>, massInGrams);
    std::printf("2500 g reported as %s = %f kg\n",
                formula::Describe<MassInKilogram>::symbol.data(),
                massConverted->measurement().value().to_double());

    // ---- 3. An absent input propagates to an empty result, not a zero ----
    constexpr auto diameterUnknown = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto emptyArea = formula::checked_evaluate<Area>(circularArea, diameterUnknown);
    std::printf("area with no diameter measured: %s\n", emptyArea->is_empty() ? "empty" : "a number");

    // ---- 4. A dimensional error is a compile error, not a runtime one ----
    std::printf("a diameter plus an area does not compile: see the comment above main() and docs/expressions.md\n");

    // ---- 5. A manually entered result replaces the computed one ----
    auto const batch =
        formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                             formula::Measured<CementVolume> { formula::Rational { 300 } },
                             formula::entered(formula::Measured<WaterCementRatio> { formula::Rational { 1, 2 } }));
    auto const ratio = formula::checked_evaluate<WaterCementRatio>(waterCementRatio, batch);
    std::printf("%s = %f (%s)\n",
                formula::Describe<WaterCementRatio>::symbol.data(),
                ratio->measurement().value().to_double(),
                ratio->is_overridden() ? "entered" : "computed");

    // Every number printed above is checked here; nothing is printed that this
    // bool does not also cover.
    bool const circularAreaIsCorrect = area.has_value() && area->is_value()
                                       && area->measurement().value().to_double() > 0.00785398
                                       && area->measurement().value().to_double() < 0.00785399;
    bool const massConvertsExactly = massConverted.has_value() && massConverted->is_value()
                                     && massConverted->measurement().value() == formula::Rational { 5, 2 };
    bool const absenceStaysEmpty = emptyArea.has_value() && emptyArea->is_empty();
    bool const overrideWinsOutright = ratio.has_value() && ratio->is_overridden()
                                      && ratio->source() == formula::ValueSource::ManuallyEntered
                                      && ratio->measurement().value() == formula::Rational { 1, 2 };

    bool const allChecksPassed = circularAreaIsCorrect && massConvertsExactly && absenceStaysEmpty && overrideWinsOutright;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
