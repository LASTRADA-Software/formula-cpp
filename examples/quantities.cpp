// SPDX-License-Identifier: Apache-2.0
//
// Quantities and measurements.
//
// Generic physics only, no standard cited: a quantity's metadata carried by
// its own type and read back through a single Describe<T>, two declarations
// that agree on symbol, description and unit staying distinct types because
// their tag differs, a foreign type joining the same way through an explicit
// Describe specialisation, a present measurement converted exactly between
// two quantities, an absent measurement surviving conversion, rounding and a
// bounds check without ever becoming a number, and combine's rule that
// either absent input makes the result absent, in a result quantity the
// caller names rather than one inherited from either operand.

#include <formula-cpp/formula.hpp>

#include <cstdio>
#include <type_traits>

namespace
{
namespace unit = formula::unit;

// ---- 1 & 2: two ordinary declarations, one pair distinguished only by tag ----

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", unit::Litre>
{
};

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

struct VolumeInCubicMetres: formula::Quantity<VolumeInCubicMetres, "V", "volume of water added", unit::CubicMetre>
{
};

// Same symbol, same description, same unit as WaterVolume above -- only the
// tag (this type's own name) differs.
struct CementVolume: formula::Quantity<CementVolume, "V_w", "volume of water added", unit::Litre>
{
};

// combine's result quantity, for step 6 below -- a THIRD quantity, sharing
// neither the mass's nor the volume's tag, symbol or unit. Naming a result
// that reuses an operand's quantity is exactly the mislabelling combine's
// signature no longer allows.
struct Density: formula::Quantity<Density, "rho", "density of the specimen", unit::Gram>
{
};

} // namespace

// ---- 3: a foreign type, not derived from Quantity, joining by specialisation ----
//
// Standing in for a `double` or a vendor SDK type: no base, no cooperation,
// not ours to change. Specialising Describe is the sanctioned way to bring it
// into a formula anyway.
struct ForeignTemperature
{
    double celsius {};
};

template <>
struct formula::Describe<ForeignTemperature>
{
    static constexpr std::string_view symbol = "theta";
    static constexpr std::string_view description = "a temperature from somebody else's library";
    static constexpr formula::Unit unit = formula::unit::Celsius;
    static constexpr formula::Dimension dimension = formula::unit::Celsius.dimension;
};

int main()
{
    using formula::BoundsCheck;
    using formula::Describe;
    using formula::Measured;
    using formula::Rational;
    using formula::RoundingMode;

    // ---- 1. Declaring two quantities and reading their metadata through Describe ----
    std::printf("WaterVolume: symbol=%s description=\"%s\" unit=%s\n",
                Describe<WaterVolume>::symbol.data(),
                Describe<WaterVolume>::description.data(),
                formula::view(Describe<WaterVolume>::unit.symbolText).data());
    std::printf("SpecimenMass: symbol=%s description=\"%s\" unit=%s\n",
                Describe<SpecimenMass>::symbol.data(),
                Describe<SpecimenMass>::description.data(),
                formula::view(Describe<SpecimenMass>::unit.symbolText).data());

    bool const metadataReadsBackAsDeclared =
        Describe<WaterVolume>::symbol == std::string_view { "V_w" }
        && Describe<WaterVolume>::description == std::string_view { "volume of water added" }
        && Describe<WaterVolume>::unit == unit::Litre && Describe<SpecimenMass>::symbol == std::string_view { "m" }
        && Describe<SpecimenMass>::description == std::string_view { "mass of the specimen" }
        && Describe<SpecimenMass>::unit == unit::Kilogram;
    std::printf("metadata reads back exactly as declared: %s\n", metadataReadsBackAsDeclared ? "yes" : "no");

    // ---- 2. Two quantities alike in symbol, description and unit, distinct in type ----
    bool const metadataCoincides = Describe<WaterVolume>::symbol == Describe<CementVolume>::symbol
                                   && Describe<WaterVolume>::description == Describe<CementVolume>::description
                                   && Describe<WaterVolume>::unit == Describe<CementVolume>::unit;
    bool const tagKeepsThemDistinct = !std::is_same_v<WaterVolume, CementVolume>;
    std::printf("WaterVolume and CementVolume share symbol, description and unit: %s\n", metadataCoincides ? "yes" : "no");
    std::printf("...but the tag keeps them different types: %s\n", tagKeepsThemDistinct ? "yes" : "no");

    // ---- 3. A foreign type, joined by specialising Describe ----
    std::printf("ForeignTemperature: symbol=%s dimension is temperature: %s\n",
                Describe<ForeignTemperature>::symbol.data(),
                Describe<ForeignTemperature>::dimension == formula::dim::Temperature ? "yes" : "no");
    bool const foreignTypeJoinsTheSameWay = formula::Described<ForeignTemperature>
                                            && Describe<ForeignTemperature>::symbol == std::string_view { "theta" }
                                            && Describe<ForeignTemperature>::dimension == formula::dim::Temperature;

    // ---- 4. A present measurement, converted exactly between quantities (450 l to m3) ----
    Measured<WaterVolume> const presentVolume { *Rational::from_decimal(450, 0) };
    auto const convertedPresent = formula::checked_convert_to<VolumeInCubicMetres>(presentVolume);
    Rational const convertedValue = convertedPresent->value();
    std::printf("450 l converted to m3 = %lld/%lld\n",
                static_cast<long long>(convertedValue.numerator()),
                static_cast<long long>(convertedValue.denominator()));
    bool const presentValueConvertsExactly =
        convertedPresent.has_value() && convertedPresent->has_value() && convertedValue == *Rational::make(9, 20);

    // ---- 5. An absent measurement surviving conversion, rounding and a bounds check ----
    Measured<WaterVolume> const absentVolume {};
    auto const convertedAbsent = formula::checked_convert_to<VolumeInCubicMetres>(absentVolume);
    auto const roundedAbsent = formula::checked_round_to_declared(absentVolume, RoundingMode::HalfAwayFromZero);
    auto const boundsOfAbsent = formula::checked_within_bounds(absentVolume);

    std::printf("an absent measurement, converted: %s\n",
                convertedAbsent.has_value() && convertedAbsent->is_absent() ? "still absent" : "a number");
    std::printf("an absent measurement, rounded: %s\n",
                roundedAbsent.has_value() && roundedAbsent->is_absent() ? "still absent" : "a number");
    std::printf("an absent measurement, bounds-checked: %s\n",
                boundsOfAbsent.has_value() ? formula::describe(*boundsOfAbsent).data() : "conversion failed");

    bool const absenceSurvivesEveryOperation = convertedAbsent.has_value() && convertedAbsent->is_absent()
                                               && roundedAbsent.has_value() && roundedAbsent->is_absent()
                                               && boundsOfAbsent.has_value() && *boundsOfAbsent == BoundsCheck::NotMeasured;

    // ---- 6. combine: absent if EITHER input is, not only if both are, and the
    //         RESULT is named by the caller, not inherited from either operand ----
    Measured<SpecimenMass> const absentMass {};
    Measured<Density> const combinedWithAnAbsentInput = formula::combine<Density>(
        presentVolume, absentMass, [](Rational volume, Rational mass) { return volume * mass; });
    std::printf("a present volume combined with an absent mass: %s\n",
                combinedWithAnAbsentInput.is_absent() ? "absent" : "a number");
    bool const combineIsAbsentWhenEitherInputIs = combinedWithAnAbsentInput.is_absent();
    // The static TYPE is Measured<Density> -- neither the volume's nor the
    // mass's own quantity. An earlier signature deduced the result as the
    // right-hand operand's quantity, so this line's type used to be
    // Measured<SpecimenMass>: six "kg", labelled as a mass, for a value that
    // was actually a density.
    static_assert(std::is_same_v<decltype(combinedWithAnAbsentInput), Measured<Density> const>);

    // ---- summary ----
    //
    // Every number printed above is checked here; nothing is printed that this
    // bool does not also cover.
    bool const allChecksPassed = metadataReadsBackAsDeclared && metadataCoincides && tagKeepsThemDistinct
                                 && foreignTypeJoinsTheSameWay && presentValueConvertsExactly
                                 && absenceSurvivesEveryOperation && combineIsAbsentWhenEitherInputIs;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
