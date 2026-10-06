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
using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;

// ...or as a struct deriving from it, which is its own tag. The two spellings
// are read the same way everywhere, and can be used together in one formula.
struct TubeMass: formula::Quantity<TubeMass, "m", "mass of the frame tube", unit::Kilogram>
{
};

using RiseInKilometres = formula::Quantity<struct RiseInKilometresTag, "h", "height gained", unit::Kilometre>;

// The height gained on the way back, over another climb: the same symbol, the
// same description and the same unit as Rise above, all of them honestly --
// only the tag differs: ReturnRiseTag, where Rise has RiseTag.
using ReturnRise = formula::Quantity<struct ReturnRiseTag, "h", "height gained", unit::Metre>;

// combine's operands and result quantity, for step 6 below: the tube's
// mass and the volume of its material combine into a THIRD quantity, the
// material's density, sharing neither the mass's nor the volume's tag,
// symbol or unit. Naming a result that reuses an operand's quantity is
// exactly the mislabelling combine's signature no longer allows.
using TubeVolume = formula::Quantity<struct TubeVolumeTag, "V", "volume of the frame tube's material", unit::CubicMetre>;
using Density =
    formula::Quantity<struct DensityTag, "rho", "density of the frame tube's material", unit::KilogramPerCubicMetre>;

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
    std::println("Rise: symbol={} description=\"{}\" unit={}",
                 Describe<Rise>::symbol,
                 Describe<Rise>::description,
                 Describe<Rise>::unit);
    std::println("TubeMass: symbol={} description=\"{}\" unit={}",
                 Describe<TubeMass>::symbol,
                 Describe<TubeMass>::description,
                 Describe<TubeMass>::unit);

    bool const metadataReadsBackAsDeclared = Describe<Rise>::symbol == "h" && Describe<Rise>::description == "height gained"
                                             && Describe<Rise>::unit == unit::Metre && Describe<TubeMass>::symbol == "m"
                                             && Describe<TubeMass>::description == "mass of the frame tube"
                                             && Describe<TubeMass>::unit == unit::Kilogram;
    std::println("metadata reads back exactly as declared: {}", metadataReadsBackAsDeclared ? "yes" : "no");

    // ---- 2. Two quantities alike in symbol, description and unit, distinct in type ----
    bool const metadataCoincides = Describe<Rise>::symbol == Describe<ReturnRise>::symbol
                                   && Describe<Rise>::description == Describe<ReturnRise>::description
                                   && Describe<Rise>::unit == Describe<ReturnRise>::unit;
    bool const tagKeepsThemDistinct = !std::is_same_v<Rise, ReturnRise>;
    std::println("Rise and ReturnRise share symbol, description and unit: {}", metadataCoincides ? "yes" : "no");
    std::println("...but the tag keeps them different types: {}", tagKeepsThemDistinct ? "yes" : "no");

    // ---- 3. A foreign type, joined by specialising Describe ----
    std::println("ForeignTemperature: symbol={} dimension is temperature: {}",
                 Describe<ForeignTemperature>::symbol,
                 Describe<ForeignTemperature>::dimension == formula::dim::Temperature ? "yes" : "no");
    bool const foreignTypeJoinsTheSameWay = formula::Described<ForeignTemperature>
                                            && Describe<ForeignTemperature>::symbol == "theta"
                                            && Describe<ForeignTemperature>::dimension == formula::dim::Temperature;

    // ---- 4. A present measurement, converted exactly between quantities (450 m to km) ----
    //
    // A conversion, a rounding and a bounds check each return a std::expected
    // -- the value, or the arithmetic error that stopped it. These run over
    // constants, so a static_assert checks each and an error would stop the
    // build.
    constexpr Measured<Rise> presentRise { 450 };
    constexpr auto convertedPresent = formula::checked_convert_to<RiseInKilometres>(presentRise);
    static_assert(convertedPresent.has_value());
    std::println("{} converted to {} = {}", presentRise, Describe<RiseInKilometres>::unit, *convertedPresent);
    bool const presentValueConvertsExactly = formula::number_of(convertedPresent) == 0.45_r;

    // ---- 5. An absent measurement surviving conversion, rounding and a bounds check ----
    constexpr Measured<Rise> absentRise {};
    constexpr auto convertedAbsent = formula::checked_convert_to<RiseInKilometres>(absentRise);
    constexpr auto roundedAbsent = formula::checked_round_to_declared(absentRise, RoundingMode::HalfAwayFromZero);
    constexpr auto boundsOfAbsent = formula::checked_within_bounds(absentRise);
    static_assert(convertedAbsent.has_value() && roundedAbsent.has_value() && boundsOfAbsent.has_value());

    std::println("an absent measurement, converted: {}", *convertedAbsent);
    std::println("an absent measurement, rounded: {}", *roundedAbsent);
    std::println("an absent measurement, bounds-checked: {}", *boundsOfAbsent);

    bool const absenceSurvivesEveryOperation =
        convertedAbsent->is_absent() && roundedAbsent->is_absent() && *boundsOfAbsent == BoundsCheck::NotMeasured;

    // ---- 6. combine: absent if EITHER input is, not only if both are, and the
    //         RESULT is named by the caller, not inherited from either operand ----
    constexpr Measured<TubeVolume> presentVolume { 0.0002_r };
    Measured<TubeMass> const absentMass {};
    auto const combinedWithAnAbsentInput =
        formula::combine<Density>(presentVolume, absentMass, [](Rational volume, Rational mass) { return mass / volume; });
    std::println("a present volume combined with an absent mass: {}", combinedWithAnAbsentInput);
    bool const combineIsAbsentWhenEitherInputIs = combinedWithAnAbsentInput.is_absent();
    // The static TYPE is Measured<Density> -- neither the volume's nor the
    // mass's own quantity. An earlier signature deduced the result as the
    // right-hand operand's quantity, so this line's type used to be
    // Measured<TubeMass>: a mass's label, in kg, on a value that was a
    // density.
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
