// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace unit = formula::unit;

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Measured;
using formula::Rational;

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", unit::Litre>
{
};

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

// combine's result quantity, for the mass/volume example below. Deliberately a
// THIRD quantity, sharing neither operand's tag, symbol or unit -- reusing one
// of them is exactly how F1's mislabelling passed six reviews.
struct Density: formula::Quantity<Density, "rho", "density of the specimen", unit::Gram>
{
};

// constexpr, not consteval: Task 5 calls this from TEST_CASE bodies (runtime
// code), not only from static_assert. cl.exe (19.51, MSVC v143) miscompiles a
// consteval factory returning a class type with an explicit single-argument
// constructor when the call is not manifestly constant-evaluated -- it reports
// the RESULT type as an aggregate and rejects the call with "too many
// initializers", even though nothing here is an aggregate. Isolated with a
// two-line reproduction outside this project; clang-cl accepts the identical
// consteval version without complaint. constexpr keeps every existing
// static_assert exactly as constant-evaluated as before and sidesteps the bug.
constexpr Measured<WaterVolume> measured(std::int64_t numerator, std::int64_t denominator)
{
    return Measured<WaterVolume> { *Rational::make(numerator, denominator) };
}

} // namespace

// ---- absent is the default, and it is not zero ----

static_assert(Measured<WaterVolume> {}.is_absent());
static_assert(!Measured<WaterVolume> {}.has_value());
static_assert(Measured<WaterVolume>::absent().is_absent());

// Zero is a measurement; absent is not a number. Collapsing the two is how an
// unmeasured quantity comes to look like a real reading of nothing.
static_assert(!measured(0, 1).is_absent());
static_assert(measured(0, 1) != Measured<WaterVolume> {});

// ---- a present value is exactly what was put in ----

static_assert(measured(9, 2).value() == *Rational::make(9, 2));
static_assert(measured(9, 2).has_value());
static_assert(Measured<WaterVolume> {}.value_or(*Rational::make(7, 1)) == *Rational::make(7, 1));
static_assert(measured(9, 2).value_or(*Rational::make(7, 1)) == *Rational::make(9, 2));

// ---- equality ----

static_assert(measured(9, 2) == measured(9, 2));
static_assert(measured(9, 2) != measured(9, 3));
static_assert(Measured<WaterVolume> {} == Measured<WaterVolume> {});

// ---- two quantities are two types, even with the same payload ----

static_assert(!std::is_same_v<Measured<WaterVolume>, Measured<SpecimenMass>>);

// ---- the value constructor does not accept an implicit Rational ----
//
// Removing `explicit` from Measured's constructor leaves the rest of the suite
// green -- nothing else exercises this. A bare Rational is not "a measurement
// of WaterVolume" merely because the types happen to line up; the caller must
// say so.
static_assert(!std::is_convertible_v<Rational, Measured<WaterVolume>>);

TEST_CASE("an absent measurement refuses to invent a number", "[measured]")
{
    Measured<WaterVolume> const notMeasured {};
    REQUIRE(notMeasured.is_absent());
    CHECK_THROWS_AS(notMeasured.value(), ArithmeticException);

    // The CODE, not merely that something was thrown. Swapping DomainError for
    // Overflow left the suite green until this was added.
    try
    {
        (void) notMeasured.value();
        FAIL("expected value() to throw for an absent measurement");
    }
    catch (ArithmeticException const& thrown)
    {
        CHECK(thrown.code() == ArithmeticError::DomainError);
    }

    // value_or is the sanctioned way to get a number out of one, because the
    // caller has to say what an absent reading should count as.
    CHECK(notMeasured.value_or(Rational { 0 }) == Rational { 0 });
}

TEST_CASE("stored() reports the payload directly, present or absent", "[measured]")
{
    // Nothing else in this suite calls stored() at all -- a version that
    // always answers "absent", regardless of what was constructed, passed
    // every other test in this file.
    Measured<WaterVolume> const present = measured(9, 2);
    REQUIRE(present.stored().has_value());
    CHECK(*present.stored() == *Rational::make(9, 2));

    Measured<WaterVolume> const absent {};
    CHECK_FALSE(absent.stored().has_value());
}

TEST_CASE("a measurement carries its quantity's own metadata", "[measured]")
{
    // All four readers, on two different quantities. Checking one reader against
    // one quantity is not enough: a reader hardwired to return WaterVolume's
    // answer would satisfy that, and two of these were reachable by no test at
    // all until the task review mutated them and the suite stayed green.
    CHECK(Measured<WaterVolume>::quantity_unit() == unit::Litre);
    CHECK(Measured<WaterVolume>::quantity_symbol() == std::string_view { "V_w" });
    CHECK(Measured<WaterVolume>::quantity_description() == std::string_view { "volume of water added" });
    CHECK(Measured<WaterVolume>::quantity_dimension() == formula::dim::Volume);

    CHECK(Measured<SpecimenMass>::quantity_unit() == unit::Kilogram);
    CHECK(Measured<SpecimenMass>::quantity_symbol() == std::string_view { "m" });
    CHECK(Measured<SpecimenMass>::quantity_description() == std::string_view { "mass of the specimen" });
    CHECK(Measured<SpecimenMass>::quantity_dimension() == formula::dim::Mass);

    // And each reader must disagree between the two, or it is not reading the
    // quantity at all.
    CHECK(Measured<WaterVolume>::quantity_unit() != Measured<SpecimenMass>::quantity_unit());
    CHECK(Measured<WaterVolume>::quantity_symbol() != Measured<SpecimenMass>::quantity_symbol());
    CHECK(Measured<WaterVolume>::quantity_description() != Measured<SpecimenMass>::quantity_description());
    CHECK(Measured<WaterVolume>::quantity_dimension() != Measured<SpecimenMass>::quantity_dimension());
}

// ---- absence propagates through everything ----

namespace
{

struct VolumeInCubicMetres:
    formula::Quantity<VolumeInCubicMetres, "V", "volume of water added", unit::CubicMetre>
{
};

constexpr auto doubled = [](Rational value) { return value + value; };

} // namespace

static_assert(formula::transform(measured(3, 1), doubled).value() == *Rational::make(6, 1));
static_assert(formula::transform(Measured<WaterVolume> {}, doubled).is_absent());

// combine names its OWN result quantity -- Density here, which is neither
// WaterVolume nor SpecimenMass. The static TYPE is asserted with is_same_v,
// not merely the value: mutating combine back to labelling the result with
// either operand's quantity (the F1 defect) changes no VALUE here, only the
// type, so a value-only check would stay green through that mutation.
static_assert(std::is_same_v<decltype(formula::combine<Density>(measured(3, 1),
                                                                 Measured<SpecimenMass> { *Rational::make(2, 1) },
                                                                 [](Rational a, Rational b) { return a * b; })),
                             Measured<Density>>);

// Both present, one absent, the other absent, both absent -- every combination,
// because propagation that works in three cases out of four is not propagation.
static_assert(formula::combine<Density>(measured(3, 1), Measured<SpecimenMass> { *Rational::make(2, 1) },
                                        [](Rational a, Rational b) { return a * b; })
                  .value()
              == *Rational::make(6, 1));
static_assert(formula::combine<Density>(Measured<WaterVolume> {}, Measured<SpecimenMass> { *Rational::make(2, 1) },
                                        [](Rational a, Rational b) { return a * b; })
                  .is_absent());
static_assert(formula::combine<Density>(measured(3, 1), Measured<SpecimenMass> {},
                                        [](Rational a, Rational b) { return a * b; })
                  .is_absent());
static_assert(formula::combine<Density>(Measured<WaterVolume> {}, Measured<SpecimenMass> {},
                                        [](Rational a, Rational b) { return a * b; })
                  .is_absent());

TEST_CASE("absence survives a conversion instead of becoming a number", "[measured]")
{
    // 450 litres is exactly 9/20 of a cubic metre -- the same exact conversion
    // phase 3 proved, now carrying a quantity's identity with it.
    auto const present = formula::checked_convert_to<VolumeInCubicMetres>(measured(450, 1));
    REQUIRE(present.has_value());
    REQUIRE(present->has_value());
    CHECK(present->value() == *Rational::make(9, 20));

    auto const absent = formula::checked_convert_to<VolumeInCubicMetres>(Measured<WaterVolume> {});
    REQUIRE(absent.has_value());
    CHECK(absent->is_absent());
}

TEST_CASE("converting to a quantity of another dimension is refused", "[measured]")
{
    auto const wrong = formula::checked_convert_to<SpecimenMass>(measured(450, 1));
    REQUIRE_FALSE(wrong.has_value());
    CHECK(wrong.error() == ArithmeticError::DomainError);

    // And it is refused for an ABSENT value too. A conversion nobody could
    // perform must not look like it succeeded merely because there was no number
    // to get wrong.
    auto const wrongAndAbsent = formula::checked_convert_to<SpecimenMass>(Measured<WaterVolume> {});
    REQUIRE_FALSE(wrongAndAbsent.has_value());
    CHECK(wrongAndAbsent.error() == ArithmeticError::DomainError);
}

namespace
{
/// A quantity whose unit DOES declare bounds. No shipped `unit::` constant has
/// any, so without this the absent-in-a-bounded-unit case cannot be written --
/// and that is the one case where `NotMeasured` and a real verdict actually
/// compete. The task reviewer had to build this locally to check it; it belongs
/// in the suite.
inline constexpr formula::Unit BoundedGauge { .dimension = formula::dim::Scalar,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 100,
                                              .symbolText = formula::symbol("%"),
                                              .decimals = 1,
                                              .bounds = formula::bounds(0, 1, 100, 1) };

struct GaugeReading: formula::Quantity<GaugeReading, "g", "a generic gauge reading", BoundedGauge>
{
};
} // namespace

TEST_CASE("absence outranks a declared range", "[measured]")
{
    // Absent, in a unit that DOES declare bounds: the range exists, but there is
    // no reading to judge against it. NotMeasured, not a verdict.
    auto const absentButBounded = formula::checked_within_bounds(Measured<GaugeReading> {});
    REQUIRE(absentButBounded.has_value());
    CHECK(*absentButBounded == formula::BoundsCheck::NotMeasured);

    // Present, same unit: now a real verdict, and each of the three is reachable.
    auto const inside = formula::checked_within_bounds(Measured<GaugeReading> { *Rational::make(42, 1) });
    REQUIRE(inside.has_value());
    CHECK(*inside == formula::BoundsCheck::WithinBounds);

    auto const below = formula::checked_within_bounds(Measured<GaugeReading> { *Rational::make(-1, 1) });
    REQUIRE(below.has_value());
    CHECK(*below == formula::BoundsCheck::BelowMinimum);

    auto const above = formula::checked_within_bounds(Measured<GaugeReading> { *Rational::make(101, 1) });
    REQUIRE(above.has_value());
    CHECK(*above == formula::BoundsCheck::AboveMaximum);

    // And the distinction the enum exists for, side by side: a reading nobody
    // took is not the same fact as a range nobody declared.
    auto const absentUnbounded = formula::checked_within_bounds(Measured<WaterVolume> {});
    REQUIRE(absentUnbounded.has_value());
    CHECK(*absentUnbounded == formula::BoundsCheck::NotMeasured);

    auto const presentUnbounded = formula::checked_within_bounds(measured(5, 1));
    REQUIRE(presentUnbounded.has_value());
    CHECK(*presentUnbounded == formula::BoundsCheck::NotChecked);
}

TEST_CASE("an unmeasured value is not judged against bounds", "[measured]")
{
    auto const absent = formula::checked_within_bounds(Measured<WaterVolume> {});
    REQUIRE(absent.has_value());
    CHECK(*absent == formula::BoundsCheck::NotMeasured);

    // Litre declares no bounds, which is a DIFFERENT fact from having no value.
    auto const present = formula::checked_within_bounds(measured(1000000, 1));
    REQUIRE(present.has_value());
    CHECK(*present == formula::BoundsCheck::NotChecked);
}

TEST_CASE("rounding to declared precision leaves an absent value absent", "[measured]")
{
    // Litre declares one decimal place.
    auto const rounded =
        formula::checked_round_to_declared(measured(123456, 1000), formula::RoundingMode::HalfAwayFromZero);
    REQUIRE(rounded.has_value());
    REQUIRE(rounded->has_value());
    CHECK(rounded->value() == *Rational::from_decimal(1235, -1));

    auto const stillAbsent =
        formula::checked_round_to_declared(Measured<WaterVolume> {}, formula::RoundingMode::HalfAwayFromZero);
    REQUIRE(stillAbsent.has_value());
    CHECK(stillAbsent->is_absent());
}

// ---- an inner error is an ERROR, never silently reported as absence ----
//
// Three functions delegate to a phase-3 checked_ function and, until this
// section, only ever exercised its success path. Turning the inner failure
// into `return Measured<Q> {}` (or, for bounds, `NotMeasured`) instead of
// propagating the error leaves every test above this comment green -- that
// is the whole defect: an overflow or a malformed range re-reported as
// "nobody measured it", which is the one confusion this layer exists to
// prevent.

namespace
{

struct LengthInMetres: formula::Quantity<LengthInMetres, "L", "a length expressed in metres", unit::Metre>
{
};

struct LengthInMillimetres:
    formula::Quantity<LengthInMillimetres, "L_mm", "a length expressed in millimetres", unit::Millimetre>
{
};

} // namespace

TEST_CASE("a conversion overflow is reported as an error, not re-read as absence", "[measured]")
{
    // Half of Rational's maximum numerator, in metres. Converting to
    // millimetres multiplies by 1000, which does not fit.
    Measured<LengthInMetres> const huge { *Rational::make(4611686018427387903LL, 1) };

    auto const overflowed = formula::checked_convert_to<LengthInMillimetres>(huge);
    REQUIRE_FALSE(overflowed.has_value());
    CHECK(overflowed.error() == ArithmeticError::Overflow);

    // The present-but-error result must not be confused with an absent one:
    // checking has_value() on the OUTER expected is the only correct read
    // here. Dereferencing it without checking first would be exactly the
    // mistake this suite is forbidden from making.
}

namespace
{

// decimals = 19: the same unrepresentable precision unit_tests.cpp uses to
// exercise checked_round_to_declared's own error path, carried through a
// quantity this time.
inline constexpr formula::Unit UnrepresentablePrecision { .dimension = formula::dim::Scalar,
                                                          .symbolText = formula::symbol("bad"),
                                                          .decimals = 19 };

struct UnroundableReading:
    formula::Quantity<UnroundableReading, "u", "a reading whose unit declares an unrepresentable precision",
                      UnrepresentablePrecision>
{
};

} // namespace

TEST_CASE("a rounding overflow is reported as an error, not re-read as absence", "[measured]")
{
    Measured<UnroundableReading> const present { *Rational::make(1, 1) };

    auto const overflowed = formula::checked_round_to_declared(present, formula::RoundingMode::HalfAwayFromZero);
    REQUIRE_FALSE(overflowed.has_value());
    CHECK(overflowed.error() == ArithmeticError::Overflow);
}

namespace
{

// A unit whose declared minimum exceeds its maximum -- a malformed
// descriptor, not a value to be judged. checked_within_bounds(Rational, Unit)
// refuses this with DomainError rather than reporting BelowMinimum or
// AboveMaximum; this quantity carries that malformed unit into Measured.
inline constexpr formula::Unit InvertedRange { .dimension = formula::dim::Scalar,
                                               .symbolText = formula::symbol("inv"),
                                               .bounds = formula::bounds(100, 1, 0, 1) };

struct InvertedRangeReading:
    formula::Quantity<InvertedRangeReading, "r", "a reading whose unit declares an inverted range", InvertedRange>
{
};

} // namespace

TEST_CASE("a malformed bounds descriptor is reported as an error, not NotMeasured", "[measured]")
{
    Measured<InvertedRangeReading> const present { *Rational::make(5, 1) };

    auto const failed = formula::checked_within_bounds(present);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error() == ArithmeticError::DomainError);
}

// ---- Measured works on a foreign, specialisation-described quantity too ----
//
// Every quantity above derives from formula::Quantity. Narrowing Measured's
// constraint from Described to detail::DeclaresQuantity -- which deletes the
// entire foreign-type path -- left the whole suite green, because nothing
// ever formed a Measured<Q> for a Q described only by explicit
// specialisation. This is that round trip: construct, read back, convert,
// bounds-check, round and combine, on a type this library does not own.

namespace
{

struct ForeignLength
{
    double metres {};
};

} // namespace

template <>
struct formula::Describe<ForeignLength>
{
    static constexpr std::string_view symbol = "L_f";
    static constexpr std::string_view description = "a length from somebody else's library";
    static constexpr formula::Unit unit = unit::Metre;
    static constexpr formula::Dimension dimension = unit::Metre.dimension;
};

static_assert(formula::Described<ForeignLength>);

TEST_CASE("Measured works on a foreign type described by specialisation, not only a CRTP one", "[measured]")
{
    Measured<ForeignLength> const present { *Rational::make(5, 1) };
    REQUIRE(present.has_value());
    CHECK(present.value() == *Rational::make(5, 1));
    CHECK(Measured<ForeignLength>::quantity_unit() == unit::Metre);
    CHECK(Measured<ForeignLength>::quantity_symbol() == std::string_view { "L_f" });

    // Convert it to an ordinary CRTP-declared quantity of the same dimension.
    auto const converted = formula::checked_convert_to<LengthInMillimetres>(present);
    REQUIRE(converted.has_value());
    REQUIRE(converted->has_value());
    CHECK(converted->value() == *Rational::make(5000, 1));

    // Bounds-check it. Metre declares no bounds, so a present value answers
    // NotChecked -- a different fact from NotMeasured.
    auto const bounded = formula::checked_within_bounds(present);
    REQUIRE(bounded.has_value());
    CHECK(*bounded == formula::BoundsCheck::NotChecked);

    // Round it to Metre's own declared precision.
    auto const rounded = formula::checked_round_to_declared(present, formula::RoundingMode::HalfAwayFromZero);
    REQUIRE(rounded.has_value());
    REQUIRE(rounded->has_value());
    CHECK(rounded->value() == *Rational::make(5, 1));

    // Combine it with an ordinary quantity into a third, named result.
    auto const combined =
        formula::combine<Density>(present, Measured<SpecimenMass> { *Rational::make(2, 1) },
                                  [](Rational length, Rational mass) { return length * mass; });
    REQUIRE(combined.has_value());
    CHECK(combined.value() == *Rational::make(10, 1));
}
