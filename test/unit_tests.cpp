// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/unit.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

using formula::Dimension;
using formula::Symbol;
using formula::Unit;

namespace dim = formula::dim;
namespace unit = formula::unit;

// ---- the symbol ----

// view() takes a Symbol by const lvalue reference only: a deleted rvalue
// overload turns `view(symbol("l"))` into a compile error instead of a view
// into an already-destroyed temporary, so these bind through named locals.
namespace
{
    inline constexpr Symbol SymbolL = formula::symbol("l");
    inline constexpr Symbol SymbolMm = formula::symbol("mm");
    inline constexpr Symbol SymbolEmpty = formula::symbol("");
} // namespace

static_assert(formula::view(SymbolL) == std::string_view { "l" });
static_assert(formula::view(SymbolMm) == std::string_view { "mm" });
static_assert(formula::view(SymbolEmpty).empty());
static_assert(formula::symbol("l") == formula::symbol("l"));
static_assert(!(formula::symbol("l") == formula::symbol("L")));

// The companion to negative/unit_symbol_too_long.cpp. That case pins what must
// NOT compile -- a symbol that does not fit SymbolCapacity; before symbol()
// refused, it truncated silently and could merge two distinct units into one
// type.

// ---- units carry their dimension ----

static_assert(unit::Metre.dimension == dim::Length);
static_assert(unit::Litre.dimension == dim::Volume);
static_assert(unit::CubicMetre.dimension == dim::Volume);
static_assert(unit::Kilogram.dimension == dim::Mass);
static_assert(unit::Celsius.dimension == dim::Temperature);
static_assert(unit::Pascal.dimension == dim::Pressure);
static_assert(unit::One.dimension == dim::Scalar);
static_assert(unit::Percent.dimension == dim::Scalar);

// ---- the coherent SI units have magnitude 1 and no offset ----

static_assert(unit::Metre.magnitudeNumerator == 1 && unit::Metre.magnitudeDenominator == 1);
static_assert(unit::Kilogram.magnitudeNumerator == 1 && unit::Kilogram.magnitudeDenominator == 1);
static_assert(unit::Kelvin.offsetNumerator == 0);

// ---- scaled units state an EXACT ratio, never a rounded factor ----

static_assert(unit::Millimetre.magnitudeNumerator == 1 && unit::Millimetre.magnitudeDenominator == 1000);
static_assert(unit::Litre.magnitudeNumerator == 1 && unit::Litre.magnitudeDenominator == 1000);
static_assert(unit::Tonne.magnitudeNumerator == 1000 && unit::Tonne.magnitudeDenominator == 1);
static_assert(unit::Minute.magnitudeNumerator == 60 && unit::Minute.magnitudeDenominator == 1);
static_assert(unit::Hour.magnitudeNumerator == 3600 && unit::Hour.magnitudeDenominator == 1);
static_assert(unit::Percent.magnitudeNumerator == 1 && unit::Percent.magnitudeDenominator == 100);

// The affine one, which is why an offset field exists at all: 0 degC is 273,15 K.
static_assert(unit::Celsius.offsetNumerator == 27315 && unit::Celsius.offsetDenominator == 100);

// ---- a Unit is a template argument, which is what phase 4 needs ----

template <Unit U>
struct Measured
{
    double value {};
};

static_assert(std::is_same_v<Measured<unit::Litre>, Measured<unit::Litre>>);
static_assert(!std::is_same_v<Measured<unit::Litre>, Measured<unit::CubicMetre>>);
static_assert(!std::is_same_v<Measured<unit::Metre>, Measured<unit::Millimetre>>);

// Two units alike in everything but spelling must stay distinct, or a relabelled
// unit would silently collapse into another.
inline constexpr Unit LitreSpelledL { .dimension = dim::Volume,
                                      .magnitudeNumerator = 1,
                                      .magnitudeDenominator = 1000,
                                      .symbolText = formula::symbol("L"),
                                      .decimals = 1 };
static_assert(!std::is_same_v<Measured<unit::Litre>, Measured<LitreSpelledL>>);

TEST_CASE("reading a symbol never runs off the end of its storage", "[unit]")
{
    // `Symbol` is a public aggregate -- it has to be, or `Unit` is not structural
    // and cannot be a template argument -- so a caller can fill `characters`
    // directly, and exactly SymbolCapacity bytes of text leaves no room for a
    // terminator. An unbounded scan then reads whatever follows in memory. The
    // neighbouring array is here so that a regression has something to run into:
    // before `view()` was bounded, this returned 23 characters from a 16-byte
    // array.
    struct Adjacent
    {
        formula::Symbol symbolText;
        char neighbour[8];
    };

    Adjacent adjacent {};
    for (std::size_t i = 0; i < formula::SymbolCapacity; ++i)
        adjacent.symbolText.characters[i] = static_cast<char>('a' + (i % 26));
    for (char& c: adjacent.neighbour)
        c = 'X';
    adjacent.neighbour[std::size(adjacent.neighbour) - 1] = '\0';

    CHECK(formula::view(adjacent.symbolText).size() == formula::SymbolCapacity);
    CHECK(formula::view(adjacent.symbolText).find('X') == std::string_view::npos);

    // And the ordinary terminated case still stops at the terminator. Named
    // locals, not temporaries: view()'s deleted rvalue overload forbids the
    // latter.
    Symbol const mm = formula::symbol("mm");
    Symbol const empty = formula::symbol("");
    CHECK(formula::view(mm).size() == 2);
    CHECK(formula::view(empty).empty());
}

TEST_CASE("units report a readable symbol", "[unit]")
{
    CHECK(formula::view(unit::Litre.symbolText) == std::string_view { "l" });
    CHECK(formula::view(unit::Kilogram.symbolText) == std::string_view { "kg" });
    CHECK(formula::view(unit::Celsius.symbolText) == std::string_view { "\xc2\xb0" "C" });
    CHECK(formula::view(unit::Percent.symbolText) == std::string_view { "%" });
}

TEST_CASE("every named unit carries a plausible declared precision", "[unit]")
{
    Unit const all[] = { unit::Metre,  unit::Millimetre, unit::Litre,  unit::CubicMetre, unit::Kilogram,
                         unit::Gram,   unit::Second,     unit::Kelvin, unit::Celsius,    unit::Pascal,
                         unit::Percent, unit::One };
    for (Unit const& u: all)
    {
        INFO(formula::view(u.symbolText));
        CHECK(u.decimals >= 0);
        CHECK(u.decimals <= 18);
        CHECK(u.magnitudeDenominator > 0);
        CHECK(u.offsetDenominator > 0);
        CHECK(u.magnitudeNumerator != 0);
    }
}

TEST_CASE("every named unit's dimension, magnitude and declared decimals match the physics", "[unit]")
{
    // The loop above enumerates only 12 of the 20 named units, and even that
    // one checks general sanity (decimals in range, denominators positive),
    // never a specific value. Centimetre, SquareMetre and Millilitre in
    // particular appear in no test, no example and no documentation code
    // block. Mutation-proved: corrupting Millilitre's magnitudeDenominator by
    // 1000x, giving SquareMetre the wrong dimension, and shifting
    // Centimetre's magnitudeDenominator by 10x all left the suite green
    // before this test existed.
    //
    // Every expected value below is written out independently of unit.hpp's
    // own initialisers -- read the physics, not the expression that defines
    // the constant -- so a broken initialiser cannot satisfy the row that
    // checks it. Same lesson as dimension_tests.cpp's ground-truth CHECKs
    // (Ruling P3-4).
    struct Expected
    {
        Unit actual;
        Dimension dimension;
        std::int64_t magnitudeNumerator;
        std::int64_t magnitudeDenominator;
        std::int32_t decimals;
    };

    Expected const table[] = {
        { unit::One, dim::Scalar, 1, 1, 3 },
        { unit::Percent, dim::Scalar, 1, 100, 1 },
        { unit::Metre, dim::Length, 1, 1, 3 },
        { unit::Centimetre, dim::Length, 1, 100, 1 },
        { unit::Millimetre, dim::Length, 1, 1000, 1 },
        { unit::Kilometre, dim::Length, 1000, 1, 3 },
        { unit::SquareMetre, dim::Area, 1, 1, 4 },
        { unit::CubicMetre, dim::Volume, 1, 1, 4 },
        { unit::Litre, dim::Volume, 1, 1000, 1 },
        { unit::Millilitre, dim::Volume, 1, 1000000, 1 },
        { unit::Kilogram, dim::Mass, 1, 1, 3 },
        { unit::Gram, dim::Mass, 1, 1000, 1 },
        { unit::Tonne, dim::Mass, 1000, 1, 3 },
        { unit::Second, dim::Time, 1, 1, 2 },
        { unit::Minute, dim::Time, 60, 1, 2 },
        { unit::Hour, dim::Time, 3600, 1, 2 },
        { unit::Kelvin, dim::Temperature, 1, 1, 2 },
        { unit::Celsius, dim::Temperature, 1, 1, 1 },
        { unit::Pascal, dim::Pressure, 1, 1, 0 },
        { unit::Megapascal, dim::Pressure, 1000000, 1, 1 },
    };
    CHECK(std::size(table) == 20); // every named unit, not a subset

    for (Expected const& row: table)
    {
        INFO(formula::view(row.actual.symbolText));
        CHECK(row.actual.dimension == row.dimension);
        CHECK(row.actual.magnitudeNumerator == row.magnitudeNumerator);
        CHECK(row.actual.magnitudeDenominator == row.magnitudeDenominator);
        CHECK(row.actual.decimals == row.decimals);
    }
}

// ---- exact conversion ----

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Rational;

namespace
{
/// Converts in a constant expression, asserting success.
consteval Rational converted(std::int64_t numerator, std::int64_t denominator, Unit from, Unit to)
{
    return formula::convert(*Rational::make(numerator, denominator), from, to);
}
} // namespace

// A volume the spec itself uses: 450 litres is exactly 9/20 of a cubic metre.
static_assert(converted(450, 1, unit::Litre, unit::CubicMetre) == *Rational::make(9, 20));
// And back again, with nothing lost.
static_assert(converted(9, 20, unit::CubicMetre, unit::Litre) == *Rational::make(450, 1));

// Scaling within one dimension.
static_assert(converted(1, 1, unit::Metre, unit::Millimetre) == *Rational::make(1000, 1));
static_assert(converted(1000, 1, unit::Millimetre, unit::Metre) == *Rational::make(1, 1));
static_assert(converted(1, 1, unit::Kilometre, unit::Metre) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::Tonne, unit::Kilogram) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::Hour, unit::Second) == *Rational::make(3600, 1));

// 30 MPa is exactly 30000000 Pa, and converts back to exactly 30 -- the spec's
// own worked example of why a precomputed floating-point factor is not enough.
static_assert(converted(30, 1, unit::Megapascal, unit::Pascal) == *Rational::make(30000000, 1));
static_assert(converted(30000000, 1, unit::Pascal, unit::Megapascal) == *Rational::make(30, 1));

// A percentage is a scalar with a magnitude, not a special case.
static_assert(converted(50, 1, unit::Percent, unit::One) == *Rational::make(1, 2));
static_assert(converted(1, 2, unit::One, unit::Percent) == *Rational::make(50, 1));

// ---- the affine case, which is why Unit carries an offset ----

static_assert(converted(0, 1, unit::Celsius, unit::Kelvin) == *Rational::make(27315, 100));
static_assert(converted(100, 1, unit::Celsius, unit::Kelvin) == *Rational::make(37315, 100));
static_assert(converted(27315, 100, unit::Kelvin, unit::Celsius) == *Rational::make(0, 1));
static_assert(converted(37315, 100, unit::Kelvin, unit::Celsius) == *Rational::make(100, 1));

// A DIFFERENCE of temperatures is not affine, but this API converts POINTS, so
// the offset always applies. Pinned so nobody "fixes" it later by dropping it.
static_assert(converted(1, 1, unit::Celsius, unit::Kelvin) == *Rational::make(27415, 100));

// Converting to the same unit is the identity, offset and all.
static_assert(converted(37, 1, unit::Celsius, unit::Celsius) == *Rational::make(37, 1));
static_assert(converted(5, 2, unit::Litre, unit::Litre) == *Rational::make(5, 2));

// The compile-time guard's positive path. `RequireSameUnitDimension` is only
// exercised negatively by test/negative/unit_dimension_mismatch.cpp, which proves
// it rejects; this proves it accepts, and that it accepts two units that differ
// in magnitude while sharing a dimension -- the case that actually matters.
//
// Spelled with `::value` on purpose: the assertion lives in the class body, so a
// bare alias would not instantiate the template and would check nothing. See the
// note on RequireSameDimension in dimension.hpp.
static_assert(formula::RequireSameUnitDimension<unit::Litre, unit::CubicMetre>::value);
static_assert(formula::RequireSameUnitDimension<unit::Celsius, unit::Kelvin>::value);
static_assert(formula::RequireSameUnitDimension<unit::Megapascal, unit::Pascal>::value);

TEST_CASE("conversion round-trips exactly, in both directions", "[unit]")
{
    struct Pair
    {
        Unit from;
        Unit to;
    };
    Pair const pairs[] = { { unit::Litre, unit::CubicMetre }, { unit::Millimetre, unit::Metre },
                           { unit::Metre, unit::Kilometre },  { unit::Gram, unit::Kilogram },
                           { unit::Minute, unit::Hour },      { unit::Celsius, unit::Kelvin },
                           { unit::Megapascal, unit::Pascal }, { unit::Percent, unit::One } };

    std::int64_t const numerators[] = { -7, -1, 0, 1, 3, 450, 30000 };
    std::size_t roundTripped = 0;

    for (Pair const& pair: pairs)
    {
        for (std::int64_t numerator: numerators)
        {
            auto const original = Rational::make(numerator, 4);
            REQUIRE(original.has_value());

            auto const there = formula::checked_convert(*original, pair.from, pair.to);
            if (!there)
                continue; // a genuinely unrepresentable intermediate is allowed
            auto const back = formula::checked_convert(*there, pair.to, pair.from);
            REQUIRE(back.has_value());

            INFO(formula::view(pair.from.symbolText) << " -> " << formula::view(pair.to.symbolText));
            CHECK(*back == *original);
            ++roundTripped;
        }
    }

    // Without this, the `continue` above turns the whole case into no test at
    // all: if every conversion started failing, the loop would assert nothing
    // and Catch2 would still report the case as passed. Measured -- forcing
    // every checked_convert to fail left the suite green with zero of the
    // round-trip's assertions run. None of these cases is unrepresentable
    // today, so the count is exact; if a future unit makes one so, this fails
    // loudly and somebody decides that deliberately rather than by silence.
    CHECK(roundTripped == std::size(pairs) * std::size(numerators));
}

TEST_CASE("conversion reports failure rather than producing a wrong number", "[unit]")
{
    // A unit whose magnitude would overflow the intermediate.
    Unit const absurd { .dimension = dim::Length,
                        .magnitudeNumerator = 9223372036854775807LL,
                        .magnitudeDenominator = 1,
                        .symbolText = formula::symbol("huge"),
                        .decimals = 0 };

    auto const result = formula::checked_convert(*Rational::make(9223372036854775807LL, 1), absurd, unit::Metre);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);
}

TEST_CASE("conversion refuses a zero magnitude rather than converting to zero", "[unit]")
{
    // A magnitude of zero describes no scale. `Unit` is a public aggregate --
    // it has to be, or it is not structural -- so this bypasses `bounds()`'s
    // sibling factories entirely and hits checked_convert with a malformed
    // descriptor directly, the same bypass test/unit_tests.cpp already
    // exercises against checked_within_bounds. Before this guard existed,
    // converting 450 through a zero-magnitude source silently returned 0/1
    // and reported success.
    Unit const zeroSource { .dimension = dim::Volume,
                            .magnitudeNumerator = 0,
                            .magnitudeDenominator = 1,
                            .symbolText = formula::symbol("zsrc"),
                            .decimals = 0 };
    Unit const zeroTarget { .dimension = dim::Volume,
                            .magnitudeNumerator = 0,
                            .magnitudeDenominator = 1,
                            .symbolText = formula::symbol("ztgt"),
                            .decimals = 0 };

    // Zero on the source.
    auto const zeroOnSource = formula::checked_convert(*Rational::make(450, 1), zeroSource, unit::CubicMetre);
    REQUIRE_FALSE(zeroOnSource.has_value());
    CHECK(zeroOnSource.error() == ArithmeticError::DomainError);

    // Zero on the target.
    auto const zeroOnTarget = formula::checked_convert(*Rational::make(450, 1), unit::Litre, zeroTarget);
    REQUIRE_FALSE(zeroOnTarget.has_value());
    CHECK(zeroOnTarget.error() == ArithmeticError::DomainError);

    // Zero on both.
    auto const zeroOnBoth = formula::checked_convert(*Rational::make(450, 1), zeroSource, zeroTarget);
    REQUIRE_FALSE(zeroOnBoth.has_value());
    CHECK(zeroOnBoth.error() == ArithmeticError::DomainError);

    // Zero on neither: the ordinary case must still succeed, so the guard
    // above is not accidentally refusing everything.
    auto const ordinary = formula::checked_convert(*Rational::make(450, 1), unit::Litre, unit::CubicMetre);
    CHECK(ordinary.has_value());

    // This is about the magnitude NUMERATOR. A zero DENOMINATOR is a
    // different, pre-existing failure -- Rational::make's own
    // DivisionByZero -- and must not change.
    Unit const zeroDenominator { .dimension = dim::Volume,
                                 .magnitudeNumerator = 1,
                                 .magnitudeDenominator = 0,
                                 .symbolText = formula::symbol("zden"),
                                 .decimals = 0 };
    auto const zeroDenominatorResult = formula::checked_convert(*Rational::make(450, 1), zeroDenominator, unit::CubicMetre);
    REQUIRE_FALSE(zeroDenominatorResult.has_value());
    CHECK(zeroDenominatorResult.error() == ArithmeticError::DivisionByZero);
}

TEST_CASE("checked_convert refuses a dimension mismatch at run time", "[unit]")
{
    // RequireSameUnitDimension is the COMPILE-TIME guard, and is exercised
    // negatively by test/negative/unit_dimension_mismatch.cpp -- but
    // convert()/checked_convert() take Unit as ordinary function parameters,
    // so a mismatch reaching them is necessarily a runtime DomainError, and
    // that path had no test of its own. Before this test existed, deleting
    // checked_convert's dimension guard left the whole suite green, and
    // convert(450, unit::Litre, unit::Kilogram) returned 0.45 "kilograms".
    auto const result = formula::checked_convert(*Rational::make(450, 1), unit::Litre, unit::Kilogram);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::DomainError);
}

// ---- bounds ----

using formula::BoundsCheck;

namespace
{
/// A unit with bounds, for the tests: 0 to 100, in its own scale.
inline constexpr Unit BoundedPercent { .dimension = dim::Scalar,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 100,
                                       .symbolText = formula::symbol("%"),
                                       .decimals = 1,
                                       .bounds = formula::bounds(0, 1, 100, 1) };
} // namespace

static_assert(!unit::Litre.bounds.present);
static_assert(BoundedPercent.bounds.present);

static_assert(*formula::checked_within_bounds(*Rational::make(50, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(0, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(100, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(-1, 1), BoundedPercent)
              == BoundsCheck::BelowMinimum);
static_assert(*formula::checked_within_bounds(*Rational::make(101, 1), BoundedPercent)
              == BoundsCheck::AboveMaximum);

// An unbounded unit is NOT "within bounds" -- it was never checked, and saying
// otherwise is how an unvalidated value comes to look validated.
static_assert(*formula::checked_within_bounds(*Rational::make(1000000, 1), unit::Litre)
              == BoundsCheck::NotChecked);

static_assert(!formula::describe(BoundsCheck::WithinBounds).empty());
static_assert(formula::describe(BoundsCheck::NotChecked) != formula::describe(BoundsCheck::WithinBounds));

// ---- declared decimals ----

static_assert(formula::declared_decimals(unit::Litre).value == 1);
static_assert(formula::declared_decimals(unit::CubicMetre).value == 4);
static_assert(formula::declared_decimals(unit::Pascal).value == 0);

static_assert(*formula::checked_round_to_declared(*Rational::make(1234, 1000), unit::Litre,
                                          formula::RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(12, -1));
static_assert(*formula::checked_round_to_declared(*Rational::make(15, 10), unit::Pascal,
                                          formula::RoundingMode::HalfAwayFromZero)
              == *Rational::make(2, 1));

namespace
{
/// Unwraps a bounds result for the RUNTIME cases.
///
/// Dereferencing a valueless `std::expected` is undefined behaviour, and under
/// MSVC's debug runtime it aborts into a dialog box -- which hangs a headless
/// run instead of failing it. Measured here while mutation-testing the
/// inverted-range guard: relaxing its `>` to `>=` turned three CHECKs into UB
/// and the suite stopped responding rather than going red. A regression has to
/// produce a red test, not a hung one, so the runtime cases go through this.
///
/// The `static_assert`s above need no such care: in a constant expression the
/// same dereference is a compile error, which is already a loud failure.
[[nodiscard]] BoundsCheck unwrapped(std::expected<BoundsCheck, formula::ArithmeticError> const& result)
{
    REQUIRE(result.has_value());
    return *result;
}
} // namespace

TEST_CASE("bounds distinguish not-checked from checked-and-passed", "[unit]")
{
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(1, 2), BoundedPercent)) == BoundsCheck::WithinBounds);
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(1, 2), unit::One)) == BoundsCheck::NotChecked);

    // Every outcome has its own distinct wording.
    BoundsCheck const all[] = { BoundsCheck::WithinBounds, BoundsCheck::BelowMinimum, BoundsCheck::AboveMaximum,
                                BoundsCheck::NotChecked };
    for (std::size_t i = 0; i < std::size(all); ++i)
    {
        CHECK_FALSE(formula::describe(all[i]).empty());
        for (std::size_t j = i + 1; j < std::size(all); ++j)
            CHECK(formula::describe(all[i]) != formula::describe(all[j]));
    }
}

// A fourth outcome, added with Measured: a value that was never taken is not the
// same fact as a unit that declares no range.
static_assert(!formula::describe(formula::BoundsCheck::NotMeasured).empty());
static_assert(formula::describe(formula::BoundsCheck::NotMeasured)
              != formula::describe(formula::BoundsCheck::NotChecked));

TEST_CASE("every bounds outcome still has its own distinct wording", "[unit]")
{
    BoundsCheck const all[] = { BoundsCheck::WithinBounds,
                                BoundsCheck::BelowMinimum,
                                BoundsCheck::AboveMaximum,
                                BoundsCheck::NotChecked,
                                BoundsCheck::NotMeasured };
    for (std::size_t i = 0; i < std::size(all); ++i)
    {
        CHECK_FALSE(formula::describe(all[i]).empty());
        for (std::size_t j = i + 1; j < std::size(all); ++j)
            CHECK(formula::describe(all[i]) != formula::describe(all[j]));
    }
}

TEST_CASE("rounding to a unit's declared precision uses that unit's decimals", "[unit]")
{
    Rational const value = *Rational::make(123456, 1000); // 123,456

    CHECK(*formula::checked_round_to_declared(value, unit::Litre, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(1235, -1));
    CHECK(*formula::checked_round_to_declared(value, unit::Pascal, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::make(123, 1));
    CHECK(*formula::checked_round_to_declared(value, unit::CubicMetre, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(123456, -3));
}

TEST_CASE("round_to_declared throws exactly where checked_round_to_declared reports an error", "[unit]")
{
    // A unit whose declared decimals fall outside what a rounding step can
    // represent -- the same guard checked_round's own tests exercise directly.
    // Pinned the way convert()/checked_convert() are: the checked form reports
    // the error, and the throwing form throws it, on the very same input.
    Unit const unrepresentableDecimals { .dimension = dim::Scalar, .symbolText = formula::symbol("bad"), .decimals = 19 };

    auto const checked = formula::checked_round_to_declared(
        *Rational::make(1, 1), unrepresentableDecimals, formula::RoundingMode::HalfAwayFromZero);
    REQUIRE_FALSE(checked.has_value());
    CHECK(checked.error() == ArithmeticError::Overflow);

    CHECK_THROWS_AS(
        formula::round_to_declared(*Rational::make(1, 1), unrepresentableDecimals, formula::RoundingMode::HalfAwayFromZero),
        ArithmeticException);

    // The pair's other half: on a SUCCEEDING input, round_to_declared must
    // return the same value checked_round_to_declared reports, not merely
    // "doesn't throw". Only the throwing case was pinned until now -- a
    // rename (Ruling G) that ignored its own `mode` argument and always
    // rounded TowardZero left the whole suite green.
    CHECK(formula::round_to_declared(*Rational::make(123456, 1000), unit::Litre, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(1235, -1));
}

TEST_CASE("a malformed unit refuses to answer rather than answer wrong", "[unit]")
{
    // The declared minimum exceeds the declared maximum. Neither BelowMinimum
    // nor AboveMaximum would be a true statement about the value here -- the
    // unit itself is broken -- so this must fail rather than pick one.
    Unit const invertedBounds { .dimension = dim::Scalar,
                                .symbolText = formula::symbol("inv"),
                                .decimals = 0,
                                .bounds = formula::bounds(100, 1, 0, 1) };
    auto const invertedResult = formula::checked_within_bounds(*Rational::make(50, 1), invertedBounds);
    REQUIRE_FALSE(invertedResult.has_value());
    CHECK(invertedResult.error() == ArithmeticError::DomainError);

    // The declared bounds are not themselves representable (a zero
    // denominator). This must surface as the same error make() would give, not
    // as some default bounds outcome.
    Unit const unrepresentableBounds { .dimension = dim::Scalar,
                                       .symbolText = formula::symbol("bad"),
                                       .decimals = 0,
                                       .bounds = formula::bounds(1, 0, 100, 1) };
    auto const unrepresentableResult = formula::checked_within_bounds(*Rational::make(50, 1), unrepresentableBounds);
    REQUIRE_FALSE(unrepresentableResult.has_value());
    CHECK(unrepresentableResult.error() == ArithmeticError::DivisionByZero);

    // A single-point range is coherent, not malformed: the guard must be a
    // strict `>`, so low == high still answers the ordinary three outcomes.
    // Nothing else in the suite would notice `>` turning into `>=`, which would
    // reject every exact-value bound the library ever declares.
    Unit const singlePoint { .dimension = dim::Scalar,
                             .symbolText = formula::symbol("one"),
                             .decimals = 0,
                             .bounds = formula::bounds(42, 1, 42, 1) };
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(42, 1), singlePoint)) == BoundsCheck::WithinBounds);
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(41, 1), singlePoint)) == BoundsCheck::BelowMinimum);
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(43, 1), singlePoint)) == BoundsCheck::AboveMaximum);

    // `Bounds` is a public aggregate -- it has to be, or `Unit` is not
    // structural -- so the `bounds()` factory can be bypassed entirely. The
    // check reads the raw fields, so it must catch an inverted range built this
    // way too. This is the same bypass that let an unterminated `Symbol` reach
    // `view()`; there the consumer had to stop assuming its input came from the
    // factory, and the same reasoning applies here.
    Unit const invertedByAggregate { .dimension = dim::Scalar,
                                     .symbolText = formula::symbol("agg"),
                                     .decimals = 0,
                                     .bounds = { true, 100, 1, 0, 1 } };
    auto const aggregateResult = formula::checked_within_bounds(*Rational::make(50, 1), invertedByAggregate);
    REQUIRE_FALSE(aggregateResult.has_value());
    CHECK(aggregateResult.error() == ArithmeticError::DomainError);

    // And an inverted range that was never declared present is still simply
    // unchecked: a unit nobody gave bounds to must not start reporting errors.
    Unit const invertedButAbsent { .dimension = dim::Scalar,
                                   .symbolText = formula::symbol("abs"),
                                   .decimals = 0,
                                   .bounds = { false, 100, 1, 0, 1 } };
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(50, 1), invertedButAbsent)) == BoundsCheck::NotChecked);
}

// ---- cross-translation-unit identity ----

#include "unit_cross_tu.hpp"

TEST_CASE("a unit template argument has the same identity in every translation unit", "[unit]")
{
    // Defined in unit_cross_tu_b.cpp. Called here with a Litre rebuilt
    // field-by-field rather than named from unit::Litre -- so this linking at
    // all is the assertion, mirroring dimension_tests.cpp's cross-TU case for
    // Dimension. Unit nests Symbol and Bounds inside the NTTP, a strictly
    // richer mangling than Dimension's, and phase 4's Quantity<Unit> is the
    // consumer that will depend on it.
    constexpr Unit LitreRebuilt { .dimension = dim::Volume,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 1000,
                                  .symbolText = formula::symbol("l"),
                                  .decimals = 1 };
    CHECK(formula_test::consume_litre(formula_test::TaggedUnit<LitreRebuilt> { 10 }) == 11);
}
