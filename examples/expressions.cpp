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

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- 1 & 3: a circular area -- a constant, a power, and later an absence ----

using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using Area = formula::Quantity<struct AreaTag, "A", "cross-sectional area", unit::SquareMetre>;

// A = pi * d^2 / 4, with an exact-rational pi. `pi` is a constant node,
// `pow<2>` a power node -- the result, Area, is declared in square metres
// while Diameter is declared in millimetres, so this one formula also
// demonstrates a result reported in a different unit from its input.
// `yields<Area>` names that result once, here, for both evaluations below.
constexpr auto circularArea = formula::yields<Area>(formula::pi * formula::pow<2>(var<Diameter>) / 4);

// ---- 2: the same point made without a power in the way, for its own line ----

using SpecimenMass = formula::Quantity<struct SpecimenMassTag, "m", "specimen mass", unit::Gram>;
using MassInKilogram = formula::Quantity<struct MassInKilogramTag, "m", "specimen mass", unit::Kilogram>;

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

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

constexpr auto waterCementRatio = formula::yields<WaterCementRatio>(var<WaterVolume> / var<CementVolume>);

} // namespace

int main()
{
    // Every checked_evaluate below returns a std::expected -- the outcome, or
    // the arithmetic error that stopped it -- and each is checked before it is
    // read. The first three are constants, so a static_assert checks them and
    // an error would stop the build; the last is checked when it runs.

    // ---- 1. A formula with a constant and a power ----
    //
    // The exact area has no short decimal -- pi is a rational convergent -- so
    // it is printed rounded to six places, and marked as rounded.
    constexpr auto diameterKnown = formula::environment(formula::Measured<Diameter> { 103 });
    constexpr auto area = formula::checked_evaluate(circularArea, diameterKnown);
    static_assert(area.has_value());
    std::println("circular area of a 103 mm diameter = {:~.6HalfAwayFromZero} ({})", *area, area->source());

    // ---- 2. A result quantity in a different unit from its input ----
    constexpr auto massInGrams = formula::environment(formula::Measured<SpecimenMass> { 2500 });
    constexpr auto massConverted = formula::checked_evaluate<MassInKilogram>(var<SpecimenMass>, massInGrams);
    static_assert(massConverted.has_value());
    std::println("2500 g reported as {} = {}", formula::symbol_of<MassInKilogram>(), *massConverted);

    // ---- 3. An absent input propagates to an empty result, not a zero ----
    constexpr auto diameterUnknown = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto emptyArea = formula::checked_evaluate(circularArea, diameterUnknown);
    static_assert(emptyArea.has_value());
    std::println("area with no diameter measured: {}", emptyArea->kind());

    // ---- 4. A dimensional error is a compile error, not a runtime one ----
    std::println("a diameter plus an area does not compile: see the comment above main() and docs/expressions.md");

    // ---- 5. A manually entered result replaces the computed one ----
    auto const batch = formula::environment(formula::Measured<WaterVolume> { 180 },
                                            formula::Measured<CementVolume> { 300 },
                                            formula::entered(formula::Measured<WaterCementRatio> { 0.5_r }));
    auto const ratio = formula::checked_evaluate(waterCementRatio, batch);
    if (!ratio)
    {
        std::println("water/cement ratio: {}", ratio.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<WaterCementRatio>(), *ratio, ratio->source());

    // Every number printed above is checked here; nothing is printed that this
    // bool does not also cover. number_of is empty for a result that is not a
    // number, so comparing it is a complete check.
    auto const areaInSquareMetres = formula::number_of(area);
    bool const circularAreaIsCorrect = areaInSquareMetres && *areaInSquareMetres > 0.00833228_r
                                       && *areaInSquareMetres < 0.00833229_r
                                       && area->source() == formula::ValueSource::Derived;
    bool const massConvertsExactly = formula::number_of(massConverted) == 2.5_r;
    bool const absenceStaysEmpty = emptyArea->is_empty();
    bool const overrideWinsOutright = ratio->is_overridden() && formula::number_of(ratio) == 0.5_r;

    bool const allChecksPassed = circularAreaIsCorrect && massConvertsExactly && absenceStaysEmpty && overrideWinsOutright;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
