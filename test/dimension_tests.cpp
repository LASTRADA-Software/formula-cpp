// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/dimension.hpp>

#include <catch2/catch_test_macros.hpp>

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
    CHECK(dim::Frequency == Dimension { .time = exponent(-1) });
    CHECK(dim::Density == Dimension { .length = exponent(-3), .mass = exponent(1) });

    // The four base dimensions none of these touch must stay at exactly zero --
    // a stray exponent there is invisible to every check above.
    for (Dimension const& d: { dim::Velocity, dim::Acceleration, dim::Force, dim::Pressure, dim::Energy,
                               dim::Frequency, dim::Density })
    {
        CHECK(d.current == exponent(0));
        CHECK(d.temperature == exponent(0));
        CHECK(d.amount == exponent(0));
        CHECK(d.luminosity == exponent(0));
    }
}

TEST_CASE("multiplying by a dimension and dividing by it again is an identity", "[dimension]")
{
    Dimension const all[] = { dim::Scalar,  dim::Length, dim::Mass,   dim::Time,     dim::Area,
                              dim::Volume,  dim::Density, dim::Force, dim::Pressure, dim::Energy };
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
}
