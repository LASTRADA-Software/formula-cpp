// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/dimension.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <iterator>
#include <type_traits>

using formula::Exponent;
using formula::exponent;

// ---- structural, and therefore usable as a template argument ----
//
// The reason this type is not `Rational`: it must be usable as a non-type
// template parameter, which requires it to be *structural* -- every non-static
// data member public, recursively. This is not decoration. If someone makes a
// member private, or gives the type a non-public base, THIS is what fails, and
// it fails at compile time with a clear message rather than in some distant
// dimension-arithmetic test.

template <Exponent E>
struct ExponentTag
{
    static constexpr Exponent value = E;
};

static_assert(ExponentTag<exponent(2, 4)>::value == exponent(1, 2));
// Canonicalisation and type identity are the same thing here: equal exponents
// must name the SAME specialisation, not merely compare equal. `operator==`
// returning true is the weaker claim, and it is the only one the assertions
// below make.
static_assert(std::is_same_v<ExponentTag<exponent(2, 4)>, ExponentTag<exponent(1, 2)>>);
static_assert(!std::is_same_v<ExponentTag<exponent(1, 2)>, ExponentTag<exponent(1, 3)>>);

// ---- canonical by construction ----
//
// This is load-bearing, not tidiness: an Exponent is part of a Dimension, and a
// Dimension is a non-type template parameter. Two equal-but-uncanonical
// exponents would make one physical dimension into two distinct,
// non-interoperating types.

static_assert(exponent(2, 4) == exponent(1, 2));
static_assert(exponent(-1, -2) == exponent(1, 2));
static_assert(exponent(1, -2) == exponent(-1, 2));
static_assert(exponent(0, 5) == exponent(0, 1));
static_assert(exponent(6, 3) == exponent(2, 1));
static_assert(exponent(5).denominator == 1);

static_assert(exponent(2, 4).numerator == 1);
static_assert(exponent(2, 4).denominator == 2);
static_assert(exponent(-6, 4).numerator == -3);
static_assert(exponent(-6, 4).denominator == 2);

// The denominator is always positive, so the sign lives in one place only.
static_assert(exponent(1, -2).denominator > 0);
static_assert(exponent(-1, -2).denominator > 0);

// ---- arithmetic ----

static_assert(exponent(1, 2) + exponent(1, 2) == exponent(1));
static_assert(exponent(1, 3) + exponent(1, 6) == exponent(1, 2));
static_assert(exponent(1) - exponent(1, 3) == exponent(2, 3));
static_assert(-exponent(2, 3) == exponent(-2, 3));
static_assert(exponent(1, 2) * 4 == exponent(2));
static_assert(exponent(2) / 4 == exponent(1, 2));
static_assert(exponent(1, 3) * 3 == exponent(1));

// The case integer exponents cannot express at all.
static_assert(exponent(1) / 2 == exponent(1, 2));
static_assert(exponent(2) / 3 == exponent(2, 3));
static_assert((exponent(1) / 3) * 2 == exponent(2, 3));

static_assert(formula::is_zero(exponent(0)));
static_assert(!formula::is_zero(exponent(1, 2)));
static_assert(formula::is_integer(exponent(3)));
static_assert(!formula::is_integer(exponent(1, 2)));
static_assert(formula::is_integer(exponent(6, 3)));

TEST_CASE("exponents canonicalise on construction", "[dimension]")
{
    CHECK(exponent(2, 4) == exponent(1, 2));
    CHECK(exponent(10, 5) == exponent(2));
    CHECK(exponent(0, 7).denominator == 1);

    // Equal values must be bit-identical, because that is what makes two
    // spellings of one dimension the same template argument.
    Exponent const a = exponent(3, 9);
    Exponent const b = exponent(1, 3);
    CHECK(a.numerator == b.numerator);
    CHECK(a.denominator == b.denominator);
}

TEST_CASE("exponent arithmetic stays canonical", "[dimension]")
{
    for (std::int32_t numerator = -6; numerator <= 6; ++numerator)
    {
        for (std::int32_t denominator = 1; denominator <= 6; ++denominator)
        {
            Exponent const e = exponent(numerator, denominator);
            INFO(e.numerator << "/" << e.denominator);
            CHECK(e.denominator > 0);

            Exponent const doubled = e + e;
            CHECK(doubled.denominator > 0);
            CHECK(doubled == e * 2);

            CHECK(e - e == exponent(0));
            CHECK((e * 3) / 3 == e);
            CHECK(-(-e) == e);
        }
    }
}

// ---- the dimension vector ----

using formula::Dimension;
using formula::nth_root;
using formula::power;

namespace dim = formula::dim;

static_assert(dim::Volume == Dimension { .length = exponent(3) });
static_assert(dim::Area == dim::Length * dim::Length);
static_assert(dim::Volume == dim::Area * dim::Length);
static_assert(dim::Area / dim::Length == dim::Length);
static_assert(dim::Volume / dim::Volume == dim::Scalar);
static_assert(dim::Density == dim::Mass / dim::Volume);
static_assert(formula::is_dimensionless(dim::Scalar));
static_assert(!formula::is_dimensionless(dim::Length));

static_assert(power(dim::Length, 3) == dim::Volume);
static_assert(power(dim::Length, 0) == dim::Scalar);
static_assert(power(dim::Length, -1) == dim::Scalar / dim::Length);

// Rational exponents: what this whole design is for.
static_assert(nth_root(dim::Area, 2) == dim::Length);
static_assert(nth_root(dim::Volume, 3) == dim::Length);

constexpr Dimension HalfLength = nth_root(dim::Length, 2);
static_assert(HalfLength.length == exponent(1, 2));
static_assert(HalfLength != dim::Length);
static_assert(HalfLength * HalfLength == dim::Length);

// Pressure to the two-thirds, the spec's stated driver for rational exponents.
constexpr Dimension PressureTwoThirds = power(nth_root(dim::Pressure, 3), 2);
static_assert(PressureTwoThirds.mass == exponent(2, 3));
static_assert(PressureTwoThirds.length == exponent(-2, 3));
static_assert(PressureTwoThirds.time == exponent(-4, 3));
static_assert(power(PressureTwoThirds, 3) == power(dim::Pressure, 2));

// ---- the property that makes a Dimension usable as a template argument ----

template <Dimension D>
struct Tagged
{
    int value {};
};

static_assert(std::is_same_v<Tagged<dim::Area * dim::Length>, Tagged<dim::Volume>>,
              "a computed dimension and a literal one must be the SAME type");
static_assert(std::is_same_v<Tagged<dim::Volume / dim::Volume>, Tagged<dim::Scalar>>,
              "a ratio of like dimensions must be the scalar type");
static_assert(!std::is_same_v<Tagged<dim::Length>, Tagged<dim::Mass>>,
              "different dimensions must be different types");
static_assert(std::is_same_v<Tagged<Dimension { .length = exponent(2, 4) }>,
                             Tagged<Dimension { .length = exponent(1, 2) }>>,
              "uncanonical exponents would split one dimension into two types");

// ---- named base dimensions ----
//
// Base dimensions the application declares itself -- here two currencies and
// three invented bases. Each is a dimension of its own, and every property
// above has to hold for them too: the operators keep the named bases in one
// canonical order, so two spellings of a dimension are still one type.

using formula::NamedBase;
using formula::symbol;

constexpr Dimension EUR = formula::base_dimension("EUR");
constexpr Dimension JPY = formula::base_dimension("JPY");

static_assert(EUR == formula::base_dimension("EUR"), "the name is the identity, byte for byte");
static_assert(EUR != JPY, "two currencies are two dimensions");
static_assert(EUR != dim::Scalar, "euros are not a bare ratio");
static_assert(!formula::is_dimensionless(EUR));
// Also the check g++ 13.3 and 14.2 fail when the merge copies a slot's
// exponent out of a `const` local: see `merged_dimension`.
static_assert(std::is_same_v<Tagged<EUR * dim::Energy / dim::Energy>, Tagged<EUR>>);
static_assert(EUR * JPY == JPY * EUR);
static_assert(std::is_same_v<Tagged<EUR * JPY>, Tagged<JPY * EUR>>, "the order of a product is not part of its type");
static_assert((EUR / JPY) * JPY == EUR);
static_assert(std::is_same_v<Tagged<EUR / EUR>, Tagged<dim::Scalar>>);
static_assert(dim::Energy * (EUR / dim::Energy) == EUR, "kWh times EUR per kWh is EUR");
static_assert(power(EUR, 0) == dim::Scalar);
static_assert(nth_root(power(EUR, 2), 2) == EUR);
static_assert(nth_root(EUR, 2).namedBases[0].exponent == exponent(1, 2));
// `power` and `nth_root` build their slots on their own, not through the merge,
// so they get the same type-identity checks: `==` cannot see g++ splitting one
// dimension into two template arguments (see `merged_dimension`).
static_assert(std::is_same_v<Tagged<power(EUR, -1)>, Tagged<dim::Scalar / EUR>>);
static_assert(std::is_same_v<Tagged<power(EUR / JPY, 2)>, Tagged<(EUR * EUR) / (JPY * JPY)>>);
static_assert(std::is_same_v<Tagged<nth_root(power(EUR, 2), 2)>, Tagged<EUR>>);
static_assert(std::is_same_v<Tagged<nth_root(power(EUR / JPY, 2), 2)>, Tagged<EUR / JPY>>);

// Sorted by name, whichever order the operands came in, and packed: a slot
// that falls out is closed up, and every slot after the last one in use is
// `NamedBase {}` again.
static_assert((JPY * EUR).namedBases[0].name == symbol("EUR"));
static_assert((JPY * EUR).namedBases[1].name == symbol("JPY"));
static_assert((JPY * EUR).namedBases[2] == NamedBase {});
static_assert(((EUR * JPY) / EUR).namedBases[0] == NamedBase { symbol("JPY"), exponent(1) });
static_assert(((EUR * JPY) / EUR).namedBases[1] == NamedBase {});
// The right operand of a quotient is negated whether it sorts before or after the left one.
static_assert((EUR / JPY).namedBases[1] == NamedBase { symbol("JPY"), exponent(-1) });
static_assert((JPY / EUR).namedBases[0] == NamedBase { symbol("EUR"), exponent(-1) });
static_assert((JPY / EUR).namedBases[1] == NamedBase { symbol("JPY"), exponent(1) });

// Capacity: four distinct bases fit, a fifth does not -- multiplied or divided
// in -- and a base that cancels frees its slot before the count is taken.
constexpr Dimension AcmeCredit = formula::base_dimension("AcmeCredit");
constexpr Dimension Token = formula::base_dimension("Token");
constexpr Dimension Voucher = formula::base_dimension("Voucher");
constexpr Dimension FourBases = Voucher * JPY * AcmeCredit * EUR;
static_assert(FourBases.namedBases[0].name == symbol("AcmeCredit"));
static_assert(FourBases.namedBases[3].name == symbol("Voucher"));
static_assert(!formula::detail::merged_dimension(FourBases, Token, false).fits);
static_assert(!formula::detail::merged_dimension(FourBases, Token, true).fits);
static_assert(formula::detail::merged_dimension(FourBases / JPY, Token, false).fits);
static_assert((FourBases / JPY * Token).namedBases[2].name == symbol("Token"));
static_assert(formula::detail::merged_dimension(FourBases, Token / Voucher, false).fits,
              "five names go in, one cancels, four come out");

// The longest name that fits, and digits after the first letter.
static_assert(formula::base_dimension("AcmeLoyaltyUnit").namedBases[0].name == symbol("AcmeLoyaltyUnit"));
static_assert(formula::base_dimension("Credit2").namedBases[0].name == symbol("Credit2"));

// g++ 13.3 and 14.2 miscompile what copy_then_overwrite_last_slot() does when
// `Dimension::namedBases` is default-initialised as `{}` rather than as four
// spelled-out elements: the copy's write reaches the original, whose last
// slot's exponent becomes 0/0, and the original stops being the same template
// argument as an equal dimension spelled another way. Only the type identity
// below notices at compile time -- `Original == ...` still holds -- and only
// g++ can fail it: cl, clang-cl and clang++ never showed the fault.
constexpr Dimension Original = formula::base_dimension("Original");

consteval Dimension copy_then_overwrite_last_slot()
{
    Dimension copied = Original;
    copied.namedBases[formula::NamedBaseCapacity - 1].exponent = exponent(0);
    return copied;
}

constexpr Dimension Overwritten = copy_then_overwrite_last_slot();
static_assert(Overwritten == Original, "writing an unused slot's zero exponent back changes nothing");
static_assert(std::is_same_v<Tagged<Original>, Tagged<Original * dim::Energy / dim::Energy>>,
              "writing to a copy must leave the original the same template argument");

TEST_CASE("dimension algebra composes the way physics does", "[dimension]")
{
    // Each derived constant is stated as the exponent vector physics says it has,
    // NOT as the expression that defines it. `CHECK(dim::Force == dim::Mass *
    // dim::Acceleration)` would read like a test and be none: `Force` is DEFINED
    // as `Mass * Acceleration`, so both sides go through the same `operator*` and
    // a broken one would satisfy the check just as happily. Writing the answer out
    // independently is what makes these able to fail.
    CHECK(dim::Velocity == Dimension { .length = exponent(1), .time = exponent(-1) });
    CHECK(dim::Acceleration == Dimension { .length = exponent(1), .time = exponent(-2) });
    CHECK(dim::Force == Dimension { .length = exponent(1), .mass = exponent(1), .time = exponent(-2) });
    CHECK(dim::Pressure == Dimension { .length = exponent(-1), .mass = exponent(1), .time = exponent(-2) });
    CHECK(dim::Energy == Dimension { .length = exponent(2), .mass = exponent(1), .time = exponent(-2) });
    CHECK(dim::Power == Dimension { .length = exponent(2), .mass = exponent(1), .time = exponent(-3) });
    CHECK(dim::Frequency == Dimension { .time = exponent(-1) });
    CHECK(dim::Density == Dimension { .length = exponent(-3), .mass = exponent(1) });
    CHECK(dim::MassPerArea == Dimension { .length = exponent(-2), .mass = exponent(1) });
    CHECK(dim::ForcePerLength == Dimension { .mass = exponent(1), .time = exponent(-2) });
    CHECK(dim::DynamicViscosity
          == Dimension { .length = exponent(-1), .mass = exponent(1), .time = exponent(-1) });
    CHECK(dim::KinematicViscosity == Dimension { .length = exponent(2), .time = exponent(-1) });

    // The four base dimensions none of these touch must stay at exactly zero --
    // a stray exponent there is invisible to every check above -- and so must
    // the named bases: a first slot not in use means no named base at all.
    for (Dimension const& d: { dim::Velocity, dim::Acceleration, dim::Force, dim::Pressure, dim::Energy,
                               dim::Power, dim::Frequency, dim::Density, dim::MassPerArea, dim::ForcePerLength,
                               dim::DynamicViscosity, dim::KinematicViscosity })
    {
        CHECK(d.current == exponent(0));
        CHECK(d.temperature == exponent(0));
        CHECK(d.amount == exponent(0));
        CHECK(d.luminosity == exponent(0));
        CHECK(d.namedBases[0] == formula::NamedBase {});
    }
}

TEST_CASE("the derived dimensions that read alike are still not equal", "[dimension]")
{
    // Equality across the whole exponent vector is what keeps two quantities
    // from being substituted for each other, and the pairs below are the ones a
    // reader is most likely to assume interchangeable. Each is checked against
    // the OTHER member of the pair rather than against a written-out vector:
    // the vectors are pinned in the case above, and what matters here is the
    // inequality itself.
    //
    // Dynamic and kinematic viscosity are the pair this exists for -- both are
    // called "viscosity", and they differ by a factor of density, so a value in
    // one is simply not a value in the other.
    CHECK_FALSE(dim::DynamicViscosity == dim::KinematicViscosity);
    CHECK(dim::DynamicViscosity == dim::KinematicViscosity * dim::Density);

    // Mass per area is one length away from a density, which is exactly close
    // enough to be mistaken for it.
    CHECK_FALSE(dim::MassPerArea == dim::Density);
    CHECK(dim::Density == dim::MassPerArea / dim::Length);

    // Force per length is not a pressure: a force divided by a width is not a
    // force divided by an area.
    CHECK_FALSE(dim::ForcePerLength == dim::Pressure);
    CHECK(dim::Pressure == dim::ForcePerLength / dim::Length);

    // Power is energy per time, and an energy is what a power delivers over a
    // time -- a kilowatt-hour is not a kilowatt. A force moving at a velocity
    // reaches the same dimension by a different route.
    CHECK_FALSE(dim::Power == dim::Energy);
    CHECK(dim::Power * dim::Time == dim::Energy);
    CHECK(dim::Power == dim::Force * dim::Velocity);

    // And no two of the four new ones collide with each other or with anything
    // the library already had. A pairwise sweep, because a new dimension that
    // silently equals an existing one would let the type system pass a value of
    // one where the other was meant -- the single failure this whole layer
    // exists to prevent. Two currencies and a tariff join it: a named base must
    // not equal any SI dimension, another named base, or itself per energy.
    Dimension const named[] = { dim::Scalar,           dim::Length,        dim::Mass,
                                dim::Time,             dim::Area,          dim::Volume,
                                dim::Density,          dim::Velocity,      dim::Acceleration,
                                dim::Force,            dim::Pressure,      dim::Energy,
                                dim::Power,            dim::Frequency,     dim::MassPerArea,
                                dim::ForcePerLength,   dim::DynamicViscosity, dim::KinematicViscosity,
                                EUR,                   JPY,                EUR / dim::Energy };
    for (std::size_t i = 0; i < std::size(named); ++i)
        for (std::size_t j = i + 1; j < std::size(named); ++j)
        {
            INFO("named dimensions " << i << " and " << j);
            CHECK_FALSE(named[i] == named[j]);
        }
}

TEST_CASE("multiplying by a dimension and dividing by it again is an identity", "[dimension]")
{
    Dimension const all[] = { dim::Scalar,           dim::Length,         dim::Mass,
                              dim::Time,             dim::Area,           dim::Volume,
                              dim::Density,          dim::Force,          dim::Pressure,
                              dim::Energy,           dim::Power,          dim::MassPerArea,
                              dim::ForcePerLength,   dim::DynamicViscosity, dim::KinematicViscosity,
                              EUR,                   JPY,                 EUR / dim::Energy };
    for (Dimension const& a: all)
    {
        for (Dimension const& b: all)
        {
            CHECK((a * b) / b == a);
            CHECK(power(nth_root(a, 2), 2) == a);
        }
    }
}

// ---- the mismatch helper ----

static_assert(formula::SameDimension<dim::Volume, dim::Area * dim::Length>);
static_assert(formula::SameDimension<dim::Scalar, dim::Volume / dim::Volume>);
static_assert(!formula::SameDimension<dim::Volume, dim::Mass>);
static_assert(!formula::SameDimension<dim::Length, nth_root(dim::Length, 2)>);

// Instantiating the helper on matching dimensions must be fine.
static_assert(formula::RequireSameDimension<dim::Volume, dim::Area * dim::Length>::value);

TEST_CASE("the same-dimension predicate agrees with equality", "[dimension]")
{
    CHECK(formula::SameDimension<dim::Force, dim::Mass * dim::Acceleration>);
    CHECK(formula::SameDimension<dim::Energy, dim::Force * dim::Length>);
    CHECK_FALSE(formula::SameDimension<dim::Energy, dim::Force>);
}

#include "dimension_cross_tu.hpp"

TEST_CASE("a dimension template argument has the same identity in every translation unit",
          "[dimension]")
{
    // Defined in dimension_cross_tu_b.cpp. Called here with a dimension spelled
    // differently but equal -- so this linking at all is the assertion.
    CHECK(formula_test::consume_volume(formula_test::Tagged<dim::Area * dim::Length> { 10 }) == 11);
    CHECK(formula_test::consume_root_of_area(formula_test::Tagged<dim::Length> { 20 }) == 22);

    // With named bases: each is declared in the header with one spelling,
    // defined in dimension_cross_tu_b.cpp with a second and called here with a
    // third, so the three must mangle alike for this to link.
    CHECK(formula_test::consume_tariff(formula_test::Tagged<EUR * power(dim::Energy, -1)> { 30 }) == 33);
    CHECK(formula_test::consume_yen_euro(formula_test::Tagged<(EUR / dim::Time) * (dim::Time * JPY)> { 40 }) == 44);
}

TEST_CASE("a copy written during constant evaluation leaves its original intact", "[dimension]")
{
    // The run-time half of the check on copy_then_overwrite_last_slot() above:
    // the object g++ emitted for the original, read back.
    CHECK(Original.namedBases[formula::NamedBaseCapacity - 1] == NamedBase {});
    CHECK(Original.namedBases[formula::NamedBaseCapacity - 1].exponent.denominator == 1);
    CHECK(Original == Original * dim::Energy / dim::Energy);
}
