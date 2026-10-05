// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/unit.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Dimension;
using formula::Rational;
using formula::Symbol;
using formula::Unit;

namespace dim = formula::dim;
namespace unit = formula::unit;

namespace
{
/// The coherent SI unit of @p value's dimension: magnitude one, offset zero, no
/// symbol. Written out here rather than taken from `evaluate.hpp`'s
/// `coherent()`, which sits a layer above `unit.hpp` and has no business being
/// pulled down into this header's own test.
[[nodiscard]] constexpr Unit coherentSi(Unit value) noexcept
{
    return Unit { .dimension = value.dimension };
}
} // namespace

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
static_assert(unit::Fahrenheit.dimension == dim::Temperature);
static_assert(unit::Watt.dimension == dim::Power);
static_assert(unit::Kilowatt.dimension == dim::Power);
static_assert(unit::WattHour.dimension == dim::Energy);
static_assert(unit::KilowattHour.dimension == dim::Energy);
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

// The affine one, which is why an offset field exists at all: 0 degC is 273.15 K.
static_assert(unit::Celsius.offsetNumerator == 27315 && unit::Celsius.offsetDenominator == 100);
// The other one: zero degrees Fahrenheit is 459.67 * 5/9 = 45967/180 K, and a degree is 5/9 K.
static_assert(unit::Fahrenheit.offsetNumerator == 45967 && unit::Fahrenheit.offsetDenominator == 180);
static_assert(unit::Fahrenheit.magnitudeNumerator == 5 && unit::Fahrenheit.magnitudeDenominator == 9);

// ---- a Unit is a template argument, which is what Quantity needs ----

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
    // before `view()` was bounded, this returned 23 characters from what was
    // then a 16-byte array.
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

TEST_CASE("checked_symbol: run-time text that fits is kept byte for byte", "[unit][symbol]")
{
    // µmol/(L·min·kg): 18 bytes of UTF-8.
    constexpr std::string_view compound = "\xc2\xb5mol/(L\xc2\xb7min\xc2\xb7kg)";
    STATIC_REQUIRE(compound.size() == 18);
    constexpr std::expected<formula::Symbol, formula::SymbolError> built = formula::checked_symbol(compound);
    STATIC_REQUIRE(built.has_value());
    STATIC_REQUIRE(formula::view(*built) == compound);

    // 31 bytes, the most that fits, ending in a two-byte character: kept whole.
    constexpr std::string_view widest = "abcdefghijklmnopqrstuvwxyz012\xc2\xb5";
    STATIC_REQUIRE(widest.size() == formula::SymbolCapacity - 1);
    constexpr std::expected<formula::Symbol, formula::SymbolError> widestBuilt = formula::checked_symbol(widest);
    STATIC_REQUIRE(widestBuilt.has_value());
    STATIC_REQUIRE(formula::view(*widestBuilt) == widest);

    constexpr std::expected<formula::Symbol, formula::SymbolError> empty = formula::checked_symbol("");
    STATIC_REQUIRE(empty.has_value());
    STATIC_REQUIRE(formula::view(*empty).empty());

    // The same at run time, from text the compiler cannot see.
    std::string const fromCatalogue { compound };
    std::expected<formula::Symbol, formula::SymbolError> const atRunTime = formula::checked_symbol(fromCatalogue);
    REQUIRE(atRunTime.has_value());
    REQUIRE(formula::view(*atRunTime) == compound);
}

TEST_CASE("checked_symbol: text that does not fit, or holds a NUL, is refused", "[unit][symbol]")
{
    constexpr std::string_view tooLong = "abcdefghijklmnopqrstuvwxyz0123\xc2\xb5"; // 32 bytes
    STATIC_REQUIRE(tooLong.size() == formula::SymbolCapacity);
    STATIC_REQUIRE(formula::checked_symbol(tooLong).error() == formula::SymbolError::TooLong);

    constexpr std::string_view withNull { "mg\0L", 4 };
    STATIC_REQUIRE(formula::checked_symbol(withNull).error() == formula::SymbolError::EmbeddedNull);
}

TEST_CASE("describe(SymbolError) names each refusal", "[unit][symbol]")
{
    STATIC_REQUIRE(formula::describe(formula::SymbolError::TooLong)
                   == "the symbol does not fit SymbolCapacity bytes, terminator included");
    STATIC_REQUIRE(formula::describe(formula::SymbolError::EmbeddedNull) == "the symbol contains a NUL byte");
    STATIC_REQUIRE(formula::describe(formula::SymbolError::NotAscii)
                   == "the symbol holds a byte outside printable ASCII");
}

TEST_CASE("symbol(): a compound UTF-8 laboratory unit fits", "[unit][symbol]")
{
    constexpr formula::Symbol compound = formula::symbol("\xc2\xb5mol/(L\xc2\xb7min\xc2\xb7kg)");
    STATIC_REQUIRE(formula::view(compound).size() == 18);

    // 31 bytes, the most that fits, ending in a two-byte character: kept whole.
    constexpr formula::Symbol widestLiteral = formula::symbol("abcdefghijklmnopqrstuvwxyz012\xc2\xb5");
    STATIC_REQUIRE(formula::view(widestLiteral).size() == formula::SymbolCapacity - 1);
    STATIC_REQUIRE(formula::view(widestLiteral).ends_with("\xc2\xb5"));
}

TEST_CASE("units report a readable symbol", "[unit]")
{
    CHECK(formula::view(unit::Litre.symbolText) == std::string_view { "l" });
    CHECK(formula::view(unit::Kilogram.symbolText) == std::string_view { "kg" });
    CHECK(formula::view(unit::Celsius.symbolText) == std::string_view { "\xc2\xb0" "C" });
    CHECK(formula::view(unit::Fahrenheit.symbolText) == std::string_view { "\xc2\xb0" "F" });
    CHECK(formula::view(unit::Watt.symbolText) == std::string_view { "W" });
    CHECK(formula::view(unit::Kilowatt.symbolText) == std::string_view { "kW" });
    CHECK(formula::view(unit::WattHour.symbolText) == std::string_view { "Wh" });
    CHECK(formula::view(unit::KilowattHour.symbolText) == std::string_view { "kWh" });
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
    // The loop above enumerates only 12 of the 21 named units, and even that
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
    // checks it. Same lesson as dimension_tests.cpp's ground-truth CHECKs.
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
        { unit::PerMille, dim::Scalar, 1, 1000, 1 },
        { unit::PartsPerMillion, dim::Scalar, 1, 1000000, 0 },
        { unit::MilligramPerKilogram, dim::Scalar, 1, 1000000, 0 },
        { unit::Metre, dim::Length, 1, 1, 3 },
        { unit::Centimetre, dim::Length, 1, 100, 1 },
        { unit::Millimetre, dim::Length, 1, 1000, 1 },
        { unit::Decimillimetre, dim::Length, 1, 10000, 0 },
        { unit::Micrometre, dim::Length, 1, 1000000, 0 },
        { unit::Kilometre, dim::Length, 1000, 1, 3 },
        { unit::SquareMetre, dim::Area, 1, 1, 4 },
        { unit::SquareCentimetre, dim::Area, 1, 10000, 2 },
        { unit::SquareMillimetre, dim::Area, 1, 1000000, 0 },
        { unit::CubicMetre, dim::Volume, 1, 1, 4 },
        { unit::Litre, dim::Volume, 1, 1000, 1 },
        { unit::Millilitre, dim::Volume, 1, 1000000, 1 },
        { unit::CubicCentimetre, dim::Volume, 1, 1000000, 1 },
        { unit::CubicMillimetre, dim::Volume, 1, 1000000000, 0 },
        { unit::Kilogram, dim::Mass, 1, 1, 3 },
        { unit::Gram, dim::Mass, 1, 1000, 1 },
        { unit::Milligram, dim::Mass, 1, 1000000, 1 },
        { unit::Tonne, dim::Mass, 1000, 1, 3 },
        { unit::Second, dim::Time, 1, 1, 2 },
        { unit::Minute, dim::Time, 60, 1, 2 },
        { unit::Hour, dim::Time, 3600, 1, 2 },
        { unit::Day, dim::Time, 86400, 1, 0 },
        { unit::Kelvin, dim::Temperature, 1, 1, 2 },
        { unit::Celsius, dim::Temperature, 1, 1, 1 },
        { unit::Fahrenheit, dim::Temperature, 5, 9, 1 },
        { unit::Newton, dim::Force, 1, 1, 1 },
        { unit::Kilonewton, dim::Force, 1000, 1, 2 },
        { unit::Pascal, dim::Pressure, 1, 1, 0 },
        { unit::Kilopascal, dim::Pressure, 1000, 1, 1 },
        { unit::Megapascal, dim::Pressure, 1000000, 1, 1 },
        { unit::NewtonPerSquareMillimetre, dim::Pressure, 1000000, 1, 1 },
        { unit::Gigapascal, dim::Pressure, 1000000000, 1, 1 },
        { unit::Joule, dim::Energy, 1, 1, 1 },
        { unit::Kilojoule, dim::Energy, 1000, 1, 1 },
        { unit::Watt, dim::Power, 1, 1, 1 },
        { unit::Kilowatt, dim::Power, 1000, 1, 3 },
        { unit::WattHour, dim::Energy, 3600, 1, 1 },
        { unit::KilowattHour, dim::Energy, 3600000, 1, 3 },
        { unit::Hertz, dim::Frequency, 1, 1, 1 },
        { unit::MetrePerSecond, dim::Velocity, 1, 1, 3 },
        { unit::MillimetrePerMinute, dim::Velocity, 1, 60000, 2 },
        { unit::KilogramPerCubicMetre, dim::Density, 1, 1, 0 },
        { unit::GramPerCubicCentimetre, dim::Density, 1000, 1, 3 },
        { unit::MegagramPerCubicMetre, dim::Density, 1000, 1, 3 },
        { unit::KilogramPerSquareMetre, dim::MassPerArea, 1, 1, 3 },
        { unit::GramPerSquareMetre, dim::MassPerArea, 1, 1000, 0 },
        { unit::KilonewtonPerMetre, dim::ForcePerLength, 1000, 1, 1 },
        { unit::NewtonPerMillimetre, dim::ForcePerLength, 1000, 1, 1 },
        { unit::PascalSecond, dim::DynamicViscosity, 1, 1, 3 },
        { unit::MillipascalSecond, dim::DynamicViscosity, 1, 1000, 1 },
        { unit::SquareMillimetrePerSecond, dim::KinematicViscosity, 1, 1000000, 1 },
    };
    CHECK(std::size(table) == 56); // every named unit, not a subset

    for (Expected const& row: table)
    {
        INFO(formula::view(row.actual.symbolText));
        CHECK(row.actual.dimension == row.dimension);
        CHECK(row.actual.magnitudeNumerator == row.magnitudeNumerator);
        CHECK(row.actual.magnitudeDenominator == row.magnitudeDenominator);
        CHECK(row.actual.decimals == row.decimals);
    }

    // The same table, put through the conversion machinery rather than read
    // field by field. One of any unit, converted into the coherent SI unit of
    // its dimension, IS that unit's magnitude -- as an exact `Rational`, never
    // a double comparison. This is the check that a declared factor is also the
    // factor `checked_convert` applies; the field checks above only prove the
    // struct holds what it was written to hold.
    std::size_t convertedRows = 0;
    for (Expected const& row: table)
    {
        // Celsius and Fahrenheit are affine, so one degree of either is not one
        // kelvin times a magnitude; their offsets are pinned by the
        // static_asserts further down instead. Counted rather than silently
        // skipped -- see the round-trip case below for why a `continue` that
        // nobody counts is how a loop quietly stops testing anything.
        if (row.actual.offsetNumerator != 0)
            continue;

        INFO(formula::view(row.actual.symbolText));
        auto const inSi = formula::checked_convert(*Rational::make(1, 1), row.actual, coherentSi(row.actual));
        REQUIRE(inSi.has_value());
        CHECK(*inSi == *Rational::make(row.magnitudeNumerator, row.magnitudeDenominator));
        ++convertedRows;
    }
    CHECK(convertedRows == std::size(table) - 2); // Celsius and Fahrenheit are the affine ones
}

TEST_CASE("every named unit's symbol is readable, unique and safe to render", "[unit]")
{
    // `symbol()` already refuses anything that does not fit `SymbolCapacity` --
    // that is a compile error, not a runtime one, and test/negative/
    // unit_symbol_too_long.cpp pins it. What is checked here is what compiles
    // fine and is still wrong.
    Unit const all[] = { unit::One,
                         unit::Percent,
                         unit::PerMille,
                         unit::PartsPerMillion,
                         unit::MilligramPerKilogram,
                         unit::Metre,
                         unit::Centimetre,
                         unit::Millimetre,
                         unit::Decimillimetre,
                         unit::Micrometre,
                         unit::Kilometre,
                         unit::SquareMetre,
                         unit::SquareCentimetre,
                         unit::SquareMillimetre,
                         unit::CubicMetre,
                         unit::Litre,
                         unit::Millilitre,
                         unit::CubicCentimetre,
                         unit::CubicMillimetre,
                         unit::Kilogram,
                         unit::Gram,
                         unit::Milligram,
                         unit::Tonne,
                         unit::Second,
                         unit::Minute,
                         unit::Hour,
                         unit::Day,
                         unit::Kelvin,
                         unit::Celsius,
                         unit::Fahrenheit,
                         unit::Newton,
                         unit::Kilonewton,
                         unit::Pascal,
                         unit::Kilopascal,
                         unit::Megapascal,
                         unit::NewtonPerSquareMillimetre,
                         unit::Gigapascal,
                         unit::Joule,
                         unit::Kilojoule,
                         unit::Watt,
                         unit::Kilowatt,
                         unit::WattHour,
                         unit::KilowattHour,
                         unit::Hertz,
                         unit::MetrePerSecond,
                         unit::MillimetrePerMinute,
                         unit::KilogramPerCubicMetre,
                         unit::GramPerCubicCentimetre,
                         unit::MegagramPerCubicMetre,
                         unit::KilogramPerSquareMetre,
                         unit::GramPerSquareMetre,
                         unit::KilonewtonPerMetre,
                         unit::NewtonPerMillimetre,
                         unit::PascalSecond,
                         unit::MillipascalSecond,
                         unit::SquareMillimetrePerSecond };
    CHECK(std::size(all) == 56);

    for (Unit const& u: all)
    {
        std::string_view const text = formula::view(u.symbolText);
        INFO(text);

        // Terminated inside its storage: `view()` scans at most
        // `SymbolCapacity` bytes, so a symbol that filled the array would come
        // back exactly that long and have run out of room for the terminator.
        CHECK(text.size() < formula::SymbolCapacity);

        // `render.hpp` appends this text to a number with no escaping, in
        // every dialect -- measured, not assumed. An asterisk is Markdown
        // emphasis, and two of them on one rendered line italicise everything
        // between; brackets and a parenthesis read as a Markdown link, which
        // `render_tests.cpp` already guards for the same reason. A unit symbol
        // must therefore carry none of them. This is why a pascal second is
        // spelled `Pa.s` here rather than `Pa*s`.
        CHECK(text.find('*') == std::string_view::npos);
        CHECK(text.find('[') == std::string_view::npos);
        CHECK(text.find(']') == std::string_view::npos);
        CHECK(text.find('(') == std::string_view::npos);
        CHECK(text.find(')') == std::string_view::npos);
    }

    // Only `One` is unlabelled; every other unit has to print as something.
    std::size_t empty = 0;
    for (Unit const& u: all)
        if (formula::view(u.symbolText).empty())
            ++empty;
    CHECK(empty == 1);

    // And no two units share a symbol. Two units printing the same text would
    // make a report ambiguous about which one a number is in, and a
    // copy-and-pasted declaration is exactly how that happens.
    for (std::size_t i = 0; i < std::size(all); ++i)
        for (std::size_t j = i + 1; j < std::size(all); ++j)
        {
            INFO(formula::view(all[i].symbolText) << " vs " << formula::view(all[j].symbolText));
            CHECK_FALSE(all[i].symbolText == all[j].symbolText);
        }
}

TEST_CASE("units that are two names for one magnitude stay exactly equal", "[unit]")
{
    // Several quantities in this vocabulary have two spellings that are the
    // same magnitude, because both spellings are written in practice. That is
    // deliberate, not duplication -- but nothing in the type system ties the
    // two declarations together, so an edit to one can move it away from the
    // other in silence. These pin them.
    struct Pair
    {
        Unit left;
        Unit right;
    };
    Pair const pairs[] = {
        { unit::Megapascal, unit::NewtonPerSquareMillimetre },  // a newton on a square millimetre
        { unit::PartsPerMillion, unit::MilligramPerKilogram },  // a milligram in a kilogram
        { unit::Millilitre, unit::CubicCentimetre },            // a millilitre is a cubic centimetre
        { unit::GramPerCubicCentimetre, unit::MegagramPerCubicMetre },
        { unit::KilonewtonPerMetre, unit::NewtonPerMillimetre },
    };

    std::int64_t const numerators[] = { -7, 0, 1, 3, 450, 30000 };
    std::size_t checkedValues = 0;

    for (Pair const& pair: pairs)
    {
        INFO(formula::view(pair.left.symbolText) << " vs " << formula::view(pair.right.symbolText));

        // Same dimension and same exact factor, read off the descriptors.
        CHECK(pair.left.dimension == pair.right.dimension);
        CHECK(*Rational::make(pair.left.magnitudeNumerator, pair.left.magnitudeDenominator)
              == *Rational::make(pair.right.magnitudeNumerator, pair.right.magnitudeDenominator));

        // And the conversion itself is the identity, both ways round -- which
        // is the property a caller actually depends on.
        for (std::int64_t numerator: numerators)
        {
            auto const value = Rational::make(numerator, 4);
            REQUIRE(value.has_value());

            auto const rightwards = formula::checked_convert(*value, pair.left, pair.right);
            REQUIRE(rightwards.has_value());
            CHECK(*rightwards == *value);

            auto const leftwards = formula::checked_convert(*value, pair.right, pair.left);
            REQUIRE(leftwards.has_value());
            CHECK(*leftwards == *value);
            ++checkedValues;
        }
    }
    CHECK(checkedValues == std::size(pairs) * std::size(numerators));
}

// ---- exact conversion ----

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

// ---- the broader vocabulary, in a constant expression ----
//
// The runtime table above walks every named unit; these are the individual
// conversions worth reading as sentences, proved during translation rather than
// at run time. Each expected value is the physics written out, not the
// initialiser in unit.hpp re-derived.

// A penetration is a whole number of tenths of a millimetre: 40 of them is
// exactly 4 mm, and exactly 4/1000 of a metre.
static_assert(converted(40, 1, unit::Decimillimetre, unit::Millimetre) == *Rational::make(4, 1));
static_assert(converted(40, 1, unit::Decimillimetre, unit::Metre) == *Rational::make(1, 250));
// A fine aperture: 67 um is 67/1000 mm.
static_assert(converted(67, 1, unit::Micrometre, unit::Millimetre) == *Rational::make(67, 1000));
// Areas and volumes scale by the square and the cube, and the factors are exact.
static_assert(converted(1, 1, unit::SquareCentimetre, unit::SquareMillimetre) == *Rational::make(100, 1));
static_assert(converted(1, 1, unit::SquareMetre, unit::SquareMillimetre) == *Rational::make(1000000, 1));
static_assert(converted(1, 1, unit::CubicCentimetre, unit::CubicMillimetre) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::Litre, unit::CubicCentimetre) == *Rational::make(1000, 1));
// A curing age: 31 days is 31 * 86400 seconds, and 744 hours.
static_assert(converted(31, 1, unit::Day, unit::Hour) == *Rational::make(744, 1));
static_assert(converted(31, 1, unit::Day, unit::Second) == *Rational::make(2678400, 1));
// A load frame's reading.
static_assert(converted(1, 1, unit::Kilonewton, unit::Newton) == *Rational::make(1000, 1));
// A loading rate: 50 mm/min is exactly 1/1200 m/s -- and back, with nothing lost.
static_assert(converted(50, 1, unit::MillimetrePerMinute, unit::MetrePerSecond) == *Rational::make(1, 1200));
static_assert(converted(1, 1200, unit::MetrePerSecond, unit::MillimetrePerMinute) == *Rational::make(50, 1));
// A density: 2.4 g/cm3 is exactly 2400 kg/m3.
static_assert(converted(12, 5, unit::GramPerCubicCentimetre, unit::KilogramPerCubicMetre)
              == *Rational::make(2400, 1));
static_assert(converted(2400, 1, unit::KilogramPerCubicMetre, unit::GramPerCubicCentimetre)
              == *Rational::make(12, 5));
// Mass per area, which is NOT a density: 200 g/m2 is exactly 1/5 kg/m2.
static_assert(converted(200, 1, unit::GramPerSquareMetre, unit::KilogramPerSquareMetre) == *Rational::make(1, 5));
// Force per width, which is NOT a stress.
static_assert(converted(1, 1, unit::NewtonPerMillimetre, unit::KilonewtonPerMetre) == *Rational::make(1, 1));
// The two viscosities, each within its own dimension.
static_assert(converted(1, 1, unit::PascalSecond, unit::MillipascalSecond) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::SquareMillimetrePerSecond, coherentSi(unit::SquareMillimetrePerSecond))
              == *Rational::make(1, 1000000));
static_assert(converted(1, 1, unit::PascalSecond, coherentSi(unit::PascalSecond)) == *Rational::make(1, 1));
// A per-mille and a part per million are ordinary scalars with a magnitude.
static_assert(converted(5, 1, unit::PerMille, unit::Percent) == *Rational::make(1, 2));
static_assert(converted(2500, 1, unit::PartsPerMillion, unit::Percent) == *Rational::make(1, 4));

// ---- the same-magnitude pairs, pinned during translation ----
//
// Both spellings occur in practice and both are declared here; nothing in the
// type system links the two declarations, so an edit to one could move it away
// from the other in silence. The runtime case further down sweeps every such
// pair over a range of values; these two are the ones the vocabulary was asked
// to guarantee by name.
static_assert(converted(30, 1, unit::Megapascal, unit::NewtonPerSquareMillimetre) == *Rational::make(30, 1));
static_assert(converted(30, 1, unit::NewtonPerSquareMillimetre, unit::Megapascal) == *Rational::make(30, 1));
static_assert(converted(1, 1, unit::NewtonPerSquareMillimetre, unit::Pascal) == *Rational::make(1000000, 1));
static_assert(converted(250, 1, unit::MilligramPerKilogram, unit::PartsPerMillion) == *Rational::make(250, 1));
static_assert(converted(250, 1, unit::PartsPerMillion, unit::MilligramPerKilogram) == *Rational::make(250, 1));

// The two viscosities are NOT such a pair, and must never become one: they
// differ by a density, so the dimensions differ and no conversion between them
// exists. The runtime refusal is checked in the case further down; this says it
// during translation, where `RequireSameUnitDimension` would also catch it.
static_assert(!formula::SameDimension<unit::PascalSecond.dimension, unit::SquareMillimetrePerSecond.dimension>);
static_assert(!formula::SameDimension<unit::KilogramPerSquareMetre.dimension, unit::KilogramPerCubicMetre.dimension>);
static_assert(!formula::SameDimension<unit::KilonewtonPerMetre.dimension, unit::Megapascal.dimension>);
static_assert(formula::RequireSameUnitDimension<unit::PascalSecond, unit::MillipascalSecond>::value);
static_assert(formula::RequireSameUnitDimension<unit::GramPerSquareMetre, unit::KilogramPerSquareMetre>::value);

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

// The second affine unit. -40 is where the two scales meet; 32 and 212 are the
// freezing and boiling points of water, which together fix the factor and the
// offset; and 98.6 is 493/5.
static_assert(converted(-40, 1, unit::Fahrenheit, unit::Celsius) == *Rational::make(-40, 1));
static_assert(converted(-40, 1, unit::Celsius, unit::Fahrenheit) == *Rational::make(-40, 1));
static_assert(converted(32, 1, unit::Fahrenheit, unit::Celsius) == *Rational::make(0, 1));
static_assert(converted(212, 1, unit::Fahrenheit, unit::Celsius) == *Rational::make(100, 1));
static_assert(converted(493, 5, unit::Fahrenheit, unit::Celsius) == *Rational::make(37, 1));
static_assert(converted(37, 1, unit::Celsius, unit::Fahrenheit) == *Rational::make(493, 5));
// Points, not differences: 0 degF is 45967/180 K, and one degree above it is
// 46067/180 K, not 5/9 K.
static_assert(converted(0, 1, unit::Fahrenheit, unit::Kelvin) == *Rational::make(45967, 180));
static_assert(converted(1, 1, unit::Fahrenheit, unit::Kelvin) == *Rational::make(46067, 180));
static_assert(converted(32, 1, unit::Fahrenheit, unit::Kelvin) == *Rational::make(27315, 100));
static_assert(converted(27315, 100, unit::Kelvin, unit::Fahrenheit) == *Rational::make(32, 1));
// A fraction of a degree stays exact: 100 degF is 340/9 degC, not a rounded 37.78.
static_assert(converted(100, 1, unit::Fahrenheit, unit::Celsius) == *Rational::make(340, 9));
static_assert(converted(1, 1, unit::Fahrenheit, unit::Fahrenheit) == *Rational::make(1, 1));

// Power and energy: the watt-hour is 3600 J, so a kilowatt-hour is 3.6 MJ and a
// thousand watt-hours.
static_assert(converted(1, 1, unit::Kilowatt, unit::Watt) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::WattHour, unit::Joule) == *Rational::make(3600, 1));
static_assert(converted(1, 1, unit::KilowattHour, unit::Joule) == *Rational::make(3600000, 1));
static_assert(converted(1, 1, unit::KilowattHour, unit::WattHour) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::KilowattHour, unit::Kilojoule) == *Rational::make(3600, 1));
static_assert(converted(3600, 1, unit::Kilojoule, unit::KilowattHour) == *Rational::make(1, 1));
// A power and an energy are different dimensions, though a kilowatt and a
// kilowatt-hour differ only by a factor of time.
static_assert(!formula::SameDimension<unit::Kilowatt.dimension, unit::KilowattHour.dimension>);
static_assert(!formula::SameDimension<unit::Watt.dimension, unit::Joule.dimension>);
static_assert(formula::RequireSameUnitDimension<unit::KilowattHour, unit::Joule>::value);
static_assert(formula::RequireSameUnitDimension<unit::Kilowatt, unit::Watt>::value);
static_assert(formula::RequireSameUnitDimension<unit::Fahrenheit, unit::Celsius>::value);

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
    Pair const pairs[] = { { unit::Litre, unit::CubicMetre },
                           { unit::Millimetre, unit::Metre },
                           { unit::Metre, unit::Kilometre },
                           { unit::Gram, unit::Kilogram },
                           { unit::Minute, unit::Hour },
                           { unit::Celsius, unit::Kelvin },
                           { unit::Megapascal, unit::Pascal },
                           { unit::Percent, unit::One },
                           // and the same property across the broadened vocabulary
                           { unit::Decimillimetre, unit::Micrometre },
                           { unit::SquareMillimetre, unit::SquareCentimetre },
                           { unit::CubicMillimetre, unit::CubicCentimetre },
                           { unit::Milligram, unit::Gram },
                           { unit::Day, unit::Hour },
                           { unit::Kilonewton, unit::Newton },
                           { unit::Gigapascal, unit::Kilopascal },
                           { unit::Kilojoule, unit::Joule },
                           { unit::Kilowatt, unit::Watt },
                           { unit::KilowattHour, unit::Joule },
                           { unit::WattHour, unit::Kilojoule },
                           { unit::Fahrenheit, unit::Celsius },
                           { unit::MillimetrePerMinute, unit::MetrePerSecond },
                           { unit::GramPerCubicCentimetre, unit::KilogramPerCubicMetre },
                           { unit::GramPerSquareMetre, unit::KilogramPerSquareMetre },
                           { unit::NewtonPerMillimetre, unit::KilonewtonPerMetre },
                           { unit::MillipascalSecond, unit::PascalSecond },
                           { unit::SquareMillimetrePerSecond, unit::SquareMillimetrePerSecond },
                           { unit::PerMille, unit::PartsPerMillion } };

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

    auto const result =
        formula::checked_convert(Rational { std::numeric_limits<Rational::Int>::max() }, absurd, unit::Metre);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);

    // The largest 64-bit integer of them, which overflowed 64 bits, is
    // (2^63 - 1)^2 m.
    auto const fits = formula::checked_convert(Rational { 9223372036854775807LL }, absurd, unit::Metre);
    REQUIRE(fits.has_value());
    CHECK(*fits == Rational { Rational::Int { 9223372036854775807LL } * 9223372036854775807LL });
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

    // The pair a reader is most likely to assume interchangeable. A dynamic
    // viscosity and a kinematic one differ by a density, so there is no factor
    // between them and asking for one has to be refused rather than answered
    // with the number that would come out if the dimensions were ignored.
    auto const viscosities =
        formula::checked_convert(*Rational::make(1, 1), unit::PascalSecond, unit::SquareMillimetrePerSecond);
    REQUIRE_FALSE(viscosities.has_value());
    CHECK(viscosities.error() == ArithmeticError::DomainError);

    // And mass per area against density, which is one length apart.
    auto const perAreaAgainstDensity = formula::checked_convert(*Rational::make(200, 1),
                                                                unit::KilogramPerSquareMetre,
                                                                unit::KilogramPerCubicMetre);
    REQUIRE_FALSE(perAreaAgainstDensity.has_value());
    CHECK(perAreaAgainstDensity.error() == ArithmeticError::DomainError);
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

static_assert(!unit::Litre.bounds.lowPresent && !unit::Litre.bounds.highPresent);
static_assert(BoundedPercent.bounds.lowPresent && BoundedPercent.bounds.highPresent);

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
    Rational const value = *Rational::make(123456, 1000); // 123.456

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
    // rename that ignored its own `mode` argument and always
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
                                     .bounds = { .lowPresent = true,
                                                 .highPresent = true,
                                                 .lowNumerator = 100,
                                                 .lowDenominator = 1,
                                                 .highNumerator = 0,
                                                 .highDenominator = 1 } };
    auto const aggregateResult = formula::checked_within_bounds(*Rational::make(50, 1), invertedByAggregate);
    REQUIRE_FALSE(aggregateResult.has_value());
    CHECK(aggregateResult.error() == ArithmeticError::DomainError);

    // And an inverted range with neither end declared present is still simply
    // unchecked: a unit nobody gave bounds to must not start reporting errors.
    Unit const invertedButAbsent { .dimension = dim::Scalar,
                                   .symbolText = formula::symbol("abs"),
                                   .decimals = 0,
                                   .bounds = { .lowPresent = false,
                                               .highPresent = false,
                                               .lowNumerator = 100,
                                               .lowDenominator = 1,
                                               .highNumerator = 0,
                                               .highDenominator = 1 } };
    CHECK(unwrapped(formula::checked_within_bounds(*Rational::make(50, 1), invertedButAbsent)) == BoundsCheck::NotChecked);
}

TEST_CASE("checked_within: either end, both, or neither", "[unit][bounds]")
{
    using formula::BoundsCheck;
    using formula::Rational;
    constexpr std::optional<Rational> none {};
    STATIC_REQUIRE(*formula::checked_within(Rational { 5 }, Rational { 0 }, none) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { -1 }, Rational { 0 }, none) == BoundsCheck::BelowMinimum);
    STATIC_REQUIRE(*formula::checked_within(Rational { 0 }, Rational { 0 }, none) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 21 }, none, Rational { 20 }) == BoundsCheck::AboveMaximum);
    STATIC_REQUIRE(*formula::checked_within(Rational { 20 }, none, Rational { 20 }) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 7 }, Rational { 0 }, Rational { 20 }) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 7 }, none, none) == BoundsCheck::NotChecked);
    STATIC_REQUIRE(formula::checked_within(Rational { 7 }, Rational { 20 }, Rational { 0 }).error()
                   == formula::ArithmeticError::DomainError);
    REQUIRE(formula::within(Rational { 7 }, none, Rational { 5 }) == BoundsCheck::AboveMaximum);
    REQUIRE_THROWS_AS(formula::within(Rational { 7 }, Rational { 9 }, Rational { 5 }), formula::ArithmeticException);
}

TEST_CASE("checked_within_bounds: a unit declared with at_least or at_most", "[unit][bounds]")
{
    using formula::BoundsCheck;
    using formula::Rational;
    constexpr formula::Unit NonNegative { .dimension = formula::dim::Scalar,
                                          .symbolText = formula::symbol("x"),
                                          .bounds = formula::at_least(0, 1) };
    constexpr formula::Unit AtMostTwenty { .dimension = formula::dim::Scalar,
                                           .symbolText = formula::symbol("y"),
                                           .bounds = formula::at_most(20, 1) };
    STATIC_REQUIRE(NonNegative.bounds.lowPresent && !NonNegative.bounds.highPresent);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { 1'000'000 }, NonNegative) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { -1, 2 }, NonNegative) == BoundsCheck::BelowMinimum);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { -1'000'000 }, AtMostTwenty) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { 41, 2 }, AtMostTwenty) == BoundsCheck::AboveMaximum);

    // A declared end is built with Rational::make, so a zero denominator on a
    // one-sided unit is refused as it is on a two-sided one.
    constexpr formula::Unit ZeroDenominatorMinimum { .dimension = formula::dim::Scalar,
                                                     .symbolText = formula::symbol("zmin"),
                                                     .bounds = formula::at_least(1, 0) };
    constexpr formula::Unit ZeroDenominatorMaximum { .dimension = formula::dim::Scalar,
                                                     .symbolText = formula::symbol("zmax"),
                                                     .bounds = formula::at_most(1, 0) };
    STATIC_REQUIRE(formula::checked_within_bounds(Rational { 5 }, ZeroDenominatorMinimum).error()
                   == formula::ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(formula::checked_within_bounds(Rational { 5 }, ZeroDenominatorMaximum).error()
                   == formula::ArithmeticError::DivisionByZero);

    // An undeclared end's fields are ignored: its zero denominator is never
    // built, so the declared minimum alone answers.
    constexpr formula::Unit UndeclaredMaximum { .dimension = formula::dim::Scalar,
                                                .symbolText = formula::symbol("umax"),
                                                .bounds = { .lowPresent = true,
                                                            .lowNumerator = 0,
                                                            .lowDenominator = 1,
                                                            .highDenominator = 0 } };
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { 5 }, UndeclaredMaximum) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { -5 }, UndeclaredMaximum) == BoundsCheck::BelowMinimum);
}

// ---- cross-translation-unit identity ----

#include "unit_cross_tu.hpp"

TEST_CASE("a unit template argument has the same identity in every translation unit", "[unit]")
{
    // Defined in unit_cross_tu_b.cpp. Called here with a Litre rebuilt
    // field-by-field rather than named from unit::Litre -- so this linking at
    // all is the assertion, mirroring dimension_tests.cpp's cross-TU case for
    // Dimension. Unit nests Symbol and Bounds inside the NTTP, a strictly
    // richer mangling than Dimension's, and `Quantity<…, Unit>` is the
    // consumer that depends on it.
    constexpr Unit LitreRebuilt { .dimension = dim::Volume,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 1000,
                                  .symbolText = formula::symbol("l"),
                                  .decimals = 1 };
    CHECK(formula_test::consume_litre(formula_test::TaggedUnit<LitreRebuilt> { 10 }) == 11);
}

// ---- money: base dimensions the application names ----
//
// The library ships no currencies, so these units are this file's own: euros
// and yen, each a named base dimension, and a cent, a hundredth of a euro. The
// tariff is in euros per kilowatt-hour, `unit::KilowattHour` being the
// library's.

namespace
{
constexpr Dimension EuroAmount = formula::base_dimension("EUR");
constexpr Dimension YenAmount = formula::base_dimension("JPY");

constexpr Unit Euro { .dimension = EuroAmount, .symbolText = formula::symbol("EUR"), .decimals = 2 };
constexpr Unit EuroCent { .dimension = EuroAmount,
                          .magnitudeNumerator = 1,
                          .magnitudeDenominator = 100,
                          .symbolText = formula::symbol("ct"),
                          .decimals = 0 };
constexpr Unit Yen { .dimension = YenAmount, .symbolText = formula::symbol("JPY"), .decimals = 0 };
constexpr Unit EuroPerKilowattHour { .dimension = EuroAmount / dim::Energy,
                                     .magnitudeNumerator = 1,
                                     .magnitudeDenominator = 3600000,
                                     .symbolText = formula::symbol("EUR/kWh"),
                                     .decimals = 4 };
} // namespace

// A cent is a hundredth of a euro, both ways, exactly.
static_assert(converted(250, 1, Euro, EuroCent) == *Rational::make(25000, 1));
static_assert(converted(25000, 1, EuroCent, Euro) == *Rational::make(250, 1));
static_assert(formula::RequireSameUnitDimension<Euro, EuroCent>::value);

// Euros never become yen: each currency is its own dimension, and an exchange
// rate is data, not a conversion factor.
static_assert(!formula::checked_convert(Rational { 1 }, Euro, Yen).has_value());
static_assert(formula::checked_convert(Rational { 1 }, Euro, Yen).error() == ArithmeticError::DomainError);

// A kilowatt-hour at a tariff in euros per kilowatt-hour is euros.
static_assert(unit::KilowattHour.dimension * EuroPerKilowattHour.dimension == Euro.dimension);

// Yen are rounded to whole yen, euros to the cent: 245/2 is 122.5, which a
// unit declaring two decimals would keep as it is.
static_assert(*formula::checked_round_to_declared(*Rational::make(245, 2), Yen, formula::RoundingMode::HalfAwayFromZero)
              == *Rational::make(123, 1));
static_assert(*formula::checked_round_to_declared(*Rational::make(12345, 1000), Euro,
                                                  formula::RoundingMode::HalfAwayFromZero)
              == *Rational::make(1235, 100));

TEST_CASE("euros convert to cents and back exactly and never to yen", "[unit][money]")
{
    auto const inCents = formula::checked_convert(Rational { 250 }, Euro, EuroCent);
    REQUIRE(inCents.has_value());
    CHECK(*inCents == Rational { 25000 });
    auto const backInEuros = formula::checked_convert(*inCents, EuroCent, Euro);
    REQUIRE(backInEuros.has_value());
    CHECK(*backInEuros == Rational { 250 });

    auto const inYen = formula::checked_convert(Rational { 1 }, Euro, Yen);
    REQUIRE_FALSE(inYen.has_value());
    CHECK(inYen.error() == ArithmeticError::DomainError);

    CHECK(*formula::checked_round_to_declared(*Rational::make(245, 2), Yen, formula::RoundingMode::HalfAwayFromZero)
          == Rational { 123 });
}

TEST_CASE("a unit carrying a named base dimension has the same identity in every translation unit", "[unit][money]")
{
    // Declared in unit_cross_tu.hpp with its dimension spelt euros per energy,
    // defined in unit_cross_tu_b.cpp as euros times euros, over euros times
    // energy, raised to the first power, and called here with euros times
    // energy to the minus one. A call spelt as a different type would not
    // compile here, and a definition spelt as one would be an overload the
    // call cannot reach, so the program would not link: compiling and linking
    // at all is the assertion.
    constexpr Unit TariffRebuilt { .dimension = EuroAmount * formula::power(dim::Energy, -1),
                                   .magnitudeNumerator = 1,
                                   .magnitudeDenominator = 3600000,
                                   .symbolText = formula::symbol("EUR/kWh"),
                                   .decimals = 4 };
    static_assert(TariffRebuilt == EuroPerKilowattHour);
    CHECK(formula_test::consume_tariff_unit(formula_test::TaggedUnit<TariffRebuilt> { 20 }) == 22);
}

TEST_CASE("a dimensionless unit with a scale and no symbol is the one a declaration refuses", "[unit]")
{
    // Hundredths with no symbol: one half would read 50, in a scale nothing names.
    constexpr Unit unlabelledHundredth { .dimension = dim::Scalar, .magnitudeNumerator = 1, .magnitudeDenominator = 100 };
    // The same scale with an offset only.
    constexpr Unit unlabelledShifted { .dimension = dim::Scalar, .offsetNumerator = 1, .offsetDenominator = 2 };
    // Scale 1 written as 7/7: still scale 1.
    constexpr Unit unlabelledSevenSevenths { .dimension = dim::Scalar, .magnitudeNumerator = 7, .magnitudeDenominator = 7 };
    // A dimensioned unit with no symbol is shown in the coherent unit instead, and is not refused.
    constexpr Unit unlabelledGram { .dimension = dim::Mass, .magnitudeNumerator = 1, .magnitudeDenominator = 1000 };

    STATIC_REQUIRE(formula::detail::unnamed_scaled_scalar(unlabelledHundredth));
    STATIC_REQUIRE(formula::detail::unnamed_scaled_scalar(unlabelledShifted));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unlabelledSevenSevenths));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unlabelledGram));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::One));
    // Every shipped scaled dimensionless unit has a symbol.
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::Percent));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::PerMille));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::PartsPerMillion));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::MilligramPerKilogram));
    STATIC_REQUIRE(formula::detail::RequireNamedScaledScalar<unit::Percent>::value);
    STATIC_REQUIRE(formula::detail::RequireNamedScaledScalar<unit::One>::value);
}

// ---- the ASCII key ----

TEST_CASE("view_ascii: a unit's key, or its symbol when that is ASCII", "[unit][ascii]")
{
    STATIC_REQUIRE(formula::view_ascii(unit::Millimetre) == "mm");
    STATIC_REQUIRE(formula::view_ascii(unit::PerMille) == "permille");
    STATIC_REQUIRE(formula::view_ascii(unit::Micrometre) == "um");
    STATIC_REQUIRE(formula::view_ascii(unit::Celsius) == "degC");
    STATIC_REQUIRE(formula::view_ascii(unit::Fahrenheit) == "degF");
    // Bound to a name first: `view_ascii` of a temporary is deleted, as `view` of one is.
    constexpr Unit coherentMass = formula::coherent(dim::Mass);
    STATIC_REQUIRE(formula::view_ascii(coherentMass).empty());
    // The display symbol is unchanged.
    STATIC_REQUIRE(formula::view(unit::Micrometre.symbolText) == "\xc2\xb5m");
}

TEST_CASE("has_ascii_key: false for a non-ASCII symbol without a key, and for a non-ASCII key", "[unit][ascii]")
{
    constexpr Unit MicrogramPerLitreUnkeyed { .dimension = dim::Mass / dim::Volume,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1'000'000,
                                              .symbolText = formula::symbol("\xc2\xb5g/L") };
    STATIC_REQUIRE(!formula::has_ascii_key(MicrogramPerLitreUnkeyed));

    constexpr Unit MicrogramPerLitre { .dimension = dim::Mass / dim::Volume,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1'000'000,
                                       .symbolText = formula::symbol("\xc2\xb5g/L"),
                                       .asciiText = formula::symbol("ug/L") };
    STATIC_REQUIRE(formula::has_ascii_key(MicrogramPerLitre));
    STATIC_REQUIRE(formula::view_ascii(MicrogramPerLitre) == "ug/L");

    // An ASCII symbol with a key that is not ASCII is refused like a missing key: the key is what a serialiser trusts.
    constexpr Unit BadKey { .dimension = dim::Length,
                            .magnitudeNumerator = 1,
                            .magnitudeDenominator = 1'000'000,
                            .symbolText = formula::symbol("um"),
                            .asciiText = formula::symbol("\xc2\xb5m") };
    STATIC_REQUIRE(!formula::has_ascii_key(BadKey));
    STATIC_REQUIRE(formula::has_ascii_key(unit::Millimetre));
}

TEST_CASE("checked_ascii_symbol: printable ASCII only", "[unit][ascii]")
{
    // Bound to a name first: `view` of the temporary would be the deleted `view(Symbol&&)`.
    constexpr std::expected<formula::Symbol, formula::SymbolError> asciiKey = formula::checked_ascii_symbol("ug/L");
    STATIC_REQUIRE(asciiKey.has_value());
    STATIC_REQUIRE(formula::view(*asciiKey) == "ug/L");
    STATIC_REQUIRE(formula::checked_ascii_symbol("\xc2\xb5g/L").error() == formula::SymbolError::NotAscii);
    STATIC_REQUIRE(formula::checked_ascii_symbol("tab\there").error() == formula::SymbolError::NotAscii);
    STATIC_REQUIRE(formula::checked_ascii_symbol("abcdefghijklmnopqrstuvwxyz012345").error()
                   == formula::SymbolError::TooLong);
    STATIC_REQUIRE(formula::checked_ascii_symbol(std::string_view { "u\0g", 3 }).error()
                   == formula::SymbolError::EmbeddedNull);
}
