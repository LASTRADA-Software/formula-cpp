// SPDX-License-Identifier: Apache-2.0
#include "quantity_cross_tu.hpp"

#include <formula-cpp/quantity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

namespace dim = formula::dim;
namespace unit = formula::unit;

namespace
{

struct Rise: formula::Quantity<Rise, "h", "height gained", unit::Millimetre>
{
};

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

} // namespace

// ---- the metadata is in the type ----

static_assert(Rise::symbol == std::string_view { "h" });
static_assert(Rise::description == std::string_view { "height gained" });
static_assert(Rise::unit == unit::Millimetre);

// The dimension is DERIVED from the unit, never declared beside it, so
// there is no second place for it to disagree with.
static_assert(Rise::dimension == dim::Length);
static_assert(SpecimenMass::dimension == dim::Mass);
static_assert(Rise::dimension == unit::Millimetre.dimension);

// ---- identity ----

static_assert(!std::is_same_v<Rise, SpecimenMass>);

// Two quantities alike in EVERYTHING but their tag are still different types.
// Without this the library would happily let a report label one measurement with
// another's name.
static_assert(!std::is_same_v<cross::Rise, cross::Run>);
static_assert(cross::Rise::symbol == cross::Run::symbol);
static_assert(cross::Rise::unit == cross::Run::unit);

TEST_CASE("a quantity type means the same thing in every translation unit", "[quantity]")
{
    // Defined in quantity_cross_tu_b.cpp. If the two translation units disagreed
    // about what cross::Rise is, this would not link.
    CHECK(cross::symbol_of_rise() == std::string_view { "h" });
    CHECK(cross::rise_and_run_are_distinct());

    // The stronger claim, which the link failure alone does not make: both
    // translation units named the SAME specialisation. See the note in
    // quantity_cross_tu.hpp for why this compares `dimension` and not `symbol` --
    // `symbol` aliases a shared template parameter object and is equal even
    // between two different quantities.
    CHECK(cross::address_of_rise_dimension() == &cross::Rise::dimension);
    CHECK(cross::address_of_rise_dimension() != &cross::Run::dimension);
}

TEST_CASE("a quantity reports its own metadata", "[quantity]")
{
    CHECK(Rise::symbol == std::string_view { "h" });
    CHECK(Rise::description == std::string_view { "height gained" });
    CHECK(formula::view(Rise::unit.symbolText) == std::string_view { "mm" });
    CHECK(Rise::unit.decimals == 1);
    CHECK(SpecimenMass::unit.decimals == 3);
}

// ---- Describe: one way in, for our types and foreign ones alike ----

using formula::Describe;

static_assert(Describe<Rise>::symbol == std::string_view { "h" });
static_assert(Describe<Rise>::description == std::string_view { "height gained" });
static_assert(Describe<Rise>::unit == unit::Millimetre);
static_assert(Describe<Rise>::dimension == dim::Length);

// Reading through Describe must agree with reading the type directly. If these
// ever diverge, every consumer above this layer is reading something else.
static_assert(Describe<Rise>::symbol == Rise::symbol);
static_assert(Describe<SpecimenMass>::unit == SpecimenMass::unit);

static_assert(formula::Described<Rise>);
static_assert(!formula::Described<int>);

// RequireDescribed's POSITIVE path. test/negative/describe_undeclared_type.cpp
// proves it rejects a type that declares nothing; nothing proved it accepts one
// that does, and a `value` hardwired to false would have satisfied the whole
// suite -- measured: mutating it left 79/79 green. Spelled with `::value`
// because the assertion is in the class body, so a bare alias instantiates
// nothing and checks nothing.
static_assert(formula::RequireDescribed<Rise>::value);
static_assert(formula::RequireDescribed<SpecimenMass>::value);

// A foreign type, standing in for a `double` or a vendor SDK type: no base, no
// cooperation, not ours to change. Specialising Describe is the sanctioned way
// to bring it into a formula.
struct ForeignTemperature
{
    double celsius {};
};

template <>
struct formula::Describe<ForeignTemperature>
{
    static constexpr std::string_view symbol = "theta";
    static constexpr std::string_view description = "a temperature from somebody else's library";
    static constexpr Unit unit = formula::unit::Celsius;
    static constexpr Dimension dimension = formula::unit::Celsius.dimension;
};

static_assert(formula::Described<ForeignTemperature>);
static_assert(Describe<ForeignTemperature>::symbol == std::string_view { "theta" });
static_assert(Describe<ForeignTemperature>::dimension == dim::Temperature);

TEST_CASE("metadata reads the same through Describe as off the type", "[quantity]")
{
    CHECK(Describe<Rise>::symbol == Rise::symbol);
    CHECK(Describe<Rise>::description == Rise::description);
    CHECK(Describe<Rise>::unit == Rise::unit);
    CHECK(Describe<Rise>::dimension == Rise::dimension);
}

TEST_CASE("a foreign type joins on the same terms as ours", "[quantity]")
{
    // The point: nothing here knows that one of these was declared with the CRTP
    // base and the other by specialisation.
    CHECK(Describe<ForeignTemperature>::symbol == std::string_view { "theta" });
    CHECK(formula::view(Describe<ForeignTemperature>::unit.symbolText)
          == formula::view(unit::Celsius.symbolText));
    CHECK(Describe<ForeignTemperature>::dimension == Describe<ForeignTemperature>::unit.dimension);
}

// ---- a quantity's own decimal places ----

namespace
{

// Invented: milliamperes, declared to whole milliamperes.
inline constexpr formula::Unit Milliampere { .dimension = formula::dim::Current,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000,
                                             .symbolText = formula::symbol("mA"),
                                             .decimals = 0 };
struct FineCurrent: formula::Quantity<FineCurrent, "I_f", "a current read to a tenth of a milliampere", Milliampere,
                                      formula::DecimalPlaces { 1 }>
{
};
struct CoarseCurrent: formula::Quantity<CoarseCurrent, "I_c", "a current read to whole milliamperes", Milliampere>
{
};

} // namespace

TEST_CASE("Quantity: its own decimal places, in the unit it shares", "[quantity][decimals]")
{
    STATIC_REQUIRE(formula::Describe<FineCurrent>::unit.decimals == 1);
    STATIC_REQUIRE(formula::Describe<CoarseCurrent>::unit.decimals == 0);
    STATIC_REQUIRE(formula::same_unit(formula::Describe<FineCurrent>::unit, Milliampere));
    STATIC_REQUIRE(!(formula::Describe<FineCurrent>::unit == Milliampere));
    STATIC_REQUIRE(formula::Describe<CoarseCurrent>::unit == Milliampere);
}

TEST_CASE("same_unit: what a unit is, not its decimals or bounds", "[unit][same_unit]")
{
    constexpr formula::Unit Base { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                   .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm") };
    constexpr formula::Unit OtherDecimals { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                            .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                            .decimals = 1, .bounds = formula::at_least(0, 1) };
    STATIC_REQUIRE(formula::same_unit(Base, OtherDecimals));
    STATIC_REQUIRE(!formula::same_unit(Base, formula::unit::Metre));                 // magnitude
    STATIC_REQUIRE(!formula::same_unit(formula::unit::Kelvin, formula::unit::Celsius)); // offset
    constexpr formula::Unit Unreduced { .dimension = formula::dim::Length, .magnitudeNumerator = 2,
                                        .magnitudeDenominator = 2000, .symbolText = formula::symbol("mm") };
    STATIC_REQUIRE(!formula::same_unit(Base, Unreduced)); // the factor as declared, not reduced
    constexpr formula::Unit OtherSymbol { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 1000, .symbolText = formula::symbol("MM") };
    STATIC_REQUIRE(!formula::same_unit(Base, OtherSymbol));
    constexpr formula::Unit OtherKey { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                       .asciiText = formula::symbol("millimetre") };
    STATIC_REQUIRE(!formula::same_unit(Base, OtherKey));
    constexpr formula::Unit SameKeySpelledOut { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                                .asciiText = formula::symbol("mm") };
    STATIC_REQUIRE(formula::same_unit(Base, SameKeySpelledOut)); // the same key, declared or read off the symbol
    constexpr formula::Unit Area { .dimension = formula::dim::Length * formula::dim::Length,
                                   .magnitudeNumerator = 1, .magnitudeDenominator = 1000,
                                   .symbolText = formula::symbol("mm") };
    STATIC_REQUIRE(!formula::same_unit(Base, Area)); // dimension
}
