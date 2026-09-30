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

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>
#include <type_traits>

namespace
{
namespace unit = formula::unit;

// ---- 1 & 2: two ordinary declarations, one pair distinguished only by tag ----
//
// A quantity is declared as an alias of formula::Quantity, whose first
// argument declares a tag of its own...
using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "volume of water added", unit::Litre>;

// ...or as a struct deriving from it, which is its own tag. The two spellings
// are read the same way everywhere, and mix in one formula.
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

using VolumeInCubicMetres = formula::Quantity<struct VolumeInCubicMetresTag, "V", "volume of water added", unit::CubicMetre>;

// Same symbol, same description, same unit as WaterVolume above -- only the
// tag differs: CementVolumeTag, where WaterVolume has WaterVolumeTag.
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_w", "volume of water added", unit::Litre>;

// combine's result quantity, for step 6 below -- a THIRD quantity, sharing
// neither the mass's nor the volume's tag, symbol or unit. Naming a result
// that reuses an operand's quantity is exactly the mislabelling combine's
// signature no longer allows.
using Density = formula::Quantity<struct DensityTag, "rho", "density of the specimen", unit::Gram>;

} // namespace

// ---- 3: a foreign type, not declared through Quantity, joining by specialisation ----
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
    using namespace formula::literals;

    // ---- 1. Declaring two quantities and reading their metadata through Describe ----
    std::println("WaterVolume: symbol={} description=\"{}\" unit={}",
                 Describe<WaterVolume>::symbol,
                 Describe<WaterVolume>::description,
                 Describe<WaterVolume>::unit);
    std::println("SpecimenMass: symbol={} description=\"{}\" unit={}",
                 Describe<SpecimenMass>::symbol,
                 Describe<SpecimenMass>::description,
                 Describe<SpecimenMass>::unit);

    bool const metadataReadsBackAsDeclared =
        Describe<WaterVolume>::symbol == "V_w" && Describe<WaterVolume>::description == "volume of water added"
        && Describe<WaterVolume>::unit == unit::Litre && Describe<SpecimenMass>::symbol == "m"
        && Describe<SpecimenMass>::description == "mass of the specimen" && Describe<SpecimenMass>::unit == unit::Kilogram;
    std::println("metadata reads back exactly as declared: {}", metadataReadsBackAsDeclared ? "yes" : "no");

    // ---- 2. Two quantities alike in symbol, description and unit, distinct in type ----
    bool const metadataCoincides = Describe<WaterVolume>::symbol == Describe<CementVolume>::symbol
                                   && Describe<WaterVolume>::description == Describe<CementVolume>::description
                                   && Describe<WaterVolume>::unit == Describe<CementVolume>::unit;
    bool const tagKeepsThemDistinct = !std::is_same_v<WaterVolume, CementVolume>;
    std::println("WaterVolume and CementVolume share symbol, description and unit: {}", metadataCoincides ? "yes" : "no");
    std::println("...but the tag keeps them different types: {}", tagKeepsThemDistinct ? "yes" : "no");

    // ---- 3. A foreign type, joined by specialising Describe ----
    std::println("ForeignTemperature: symbol={} dimension is temperature: {}",
                 Describe<ForeignTemperature>::symbol,
                 Describe<ForeignTemperature>::dimension == formula::dim::Temperature ? "yes" : "no");
    bool const foreignTypeJoinsTheSameWay = formula::Described<ForeignTemperature>
                                            && Describe<ForeignTemperature>::symbol == "theta"
                                            && Describe<ForeignTemperature>::dimension == formula::dim::Temperature;

    // ---- 4. A present measurement, converted exactly between quantities (450 l to m3) ----
    //
    // A conversion, a rounding and a bounds check each return a std::expected
    // -- the value, or the arithmetic error that stopped it. These run over
    // constants, so a static_assert checks each and an error would stop the
    // build.
    constexpr Measured<WaterVolume> presentVolume { 450 };
    constexpr auto convertedPresent = formula::checked_convert_to<VolumeInCubicMetres>(presentVolume);
    static_assert(convertedPresent.has_value());
    std::println("{} converted to {} = {}", presentVolume, Describe<VolumeInCubicMetres>::unit, *convertedPresent);
    bool const presentValueConvertsExactly = formula::number_of(convertedPresent) == 0.45_r;

    // ---- 5. An absent measurement surviving conversion, rounding and a bounds check ----
    constexpr Measured<WaterVolume> absentVolume {};
    constexpr auto convertedAbsent = formula::checked_convert_to<VolumeInCubicMetres>(absentVolume);
    constexpr auto roundedAbsent = formula::checked_round_to_declared(absentVolume, RoundingMode::HalfAwayFromZero);
    constexpr auto boundsOfAbsent = formula::checked_within_bounds(absentVolume);
    static_assert(convertedAbsent.has_value() && roundedAbsent.has_value() && boundsOfAbsent.has_value());

    std::println("an absent measurement, converted: {}", *convertedAbsent);
    std::println("an absent measurement, rounded: {}", *roundedAbsent);
    std::println("an absent measurement, bounds-checked: {}", *boundsOfAbsent);

    bool const absenceSurvivesEveryOperation = convertedAbsent->is_absent() && roundedAbsent->is_absent()
                                               && *boundsOfAbsent == BoundsCheck::NotMeasured;

    // ---- 6. combine: absent if EITHER input is, not only if both are, and the
    //         RESULT is named by the caller, not inherited from either operand ----
    Measured<SpecimenMass> const absentMass {};
    auto const combinedWithAnAbsentInput = formula::combine<Density>(
        presentVolume, absentMass, [](Rational volume, Rational mass) { return volume * mass; });
    std::println("a present volume combined with an absent mass: {}", combinedWithAnAbsentInput);
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
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
