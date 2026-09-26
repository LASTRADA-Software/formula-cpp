// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using formula::var;

/// A gram squared, the unit a variance of masses in grams is stated in.
/// Invented here rather than shipped: the library has no squared mass unit.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };

/// A variance of masses, entered in g^2 and held by the evaluator in kg^2.
struct MassSquared: formula::Quantity<MassSquared, "s2", "variance of the determinations", GramSquared>
{
};
/// The spread the root reports, in grams.
struct Spread: formula::Quantity<Spread, "s", "spread of the determinations", unit::Gram>
{
};

/// @p gramsSquared as the only input.
[[nodiscard]] constexpr auto variance(Rational gramsSquared)
{
    return formula::environment(formula::Measured<MassSquared> { gramsSquared });
}

/// The root of @p gramsSquared, rounded to @p Places of a gram under @p Mode,
/// through the node, in grams. Zero when the evaluation fails or is absent,
/// which no fixture below expects.
template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr Rational rootInGrams(Rational gramsSquared)
{
    constexpr auto node = formula::rounded_sqrt<unit::Gram, Places, Mode>(var<MassSquared>);
    auto const outcome = formula::checked_evaluate<Spread>(node, variance(gramsSquared));
    return outcome.has_value() && outcome->is_value() ? outcome->measurement().value() : Rational { -1 };
}
} // namespace

TEST_CASE("rounded_sqrt rounds the exact root correctly, in the unit declared, in every mode", "[rounded_root]")
{
    // Fixture A's variance, 427/125 g^2, stored in SI as 427/125000000 kg^2.
    // Its root is 1.84824... g: the half modes and the upward ones give 1.85,
    // the downward ones 1.84. Rounding in kg instead gives 0.00 in every mode,
    // which is what this test is here to refuse.
    constexpr auto inputs = variance(Rational { 427, 125 });
    constexpr auto half =
        formula::rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero>(var<MassSquared>);
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(half, inputs)->measurement().value() == Rational { 37, 20 });
    constexpr auto down = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::Floor>(var<MassSquared>);
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(down, inputs)->measurement().value() == Rational { 46, 25 });

    // The other two half modes go up too, where a half mode mistaken for a
    // directed one would give 1.84; TowardZero goes down, where one mistaken
    // for a half mode would give 1.85.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::HalfEven>(Rational { 427, 125 }) == Rational { 37, 20 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::HalfTowardZero>(Rational { 427, 125 })
                   == Rational { 37, 20 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::TowardZero>(Rational { 427, 125 }) == Rational { 46, 25 });

    // The node reports a mass, whatever the radicand's dimension was.
    STATIC_REQUIRE(decltype(half)::dimension == formula::dim::Mass);
}

TEST_CASE("rounded_sqrt separates Ceiling from HalfAwayFromZero (fixture B)", "[rounded_root]")
{
    // 4057/600 g^2 -> 2.60032... g. Fixture A cannot tell these two apart
    // (both give 1.85); here HalfAwayFromZero gives 2.60 and Ceiling 2.61.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero>(Rational { 4057, 600 })
                   == Rational { 13, 5 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::Ceiling>(Rational { 4057, 600 }) == Rational { 261, 100 });
    // AwayFromZero is Ceiling's pair on a root, which is never negative.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 2 }, RoundingMode::AwayFromZero>(Rational { 4057, 600 })
                   == Rational { 261, 100 });
    // The places are the node's: at 3 dp, Ceiling gives 2.601 and HalfEven 2.600.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 3 }, RoundingMode::Ceiling>(Rational { 4057, 600 })
                   == Rational { 2601, 1000 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 3 }, RoundingMode::HalfEven>(Rational { 4057, 600 }) == Rational { 13, 5 });
}

TEST_CASE("an exactly representable root is the only tie, and the mode decides it (fixture F)", "[rounded_root]")
{
    // 9/4 g^2 -> 3/2 g exactly, halfway between 1 and 2 at 0 dp. Only on a
    // tie do the half modes disagree. A half test comparing the radicand with
    // (f + 1/2)^2 meets equality on this path, so whichever way it breaks the
    // tie, one of these three fails; the delegation to checked_round is what
    // decides it.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfEven>(Rational { 9, 4 }) == Rational { 2 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfTowardZero>(Rational { 9, 4 }) == Rational { 1 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 9, 4 }) == Rational { 2 });

    // At 3/2 the floor, 1, is odd, so HalfEven and HalfAwayFromZero agree and
    // HalfEven read as "round half up" would pass. 25/4 g^2 -> 5/2 g has an
    // even floor, 2: HalfEven stays there and HalfAwayFromZero goes to 3.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfEven>(Rational { 25, 4 }) == Rational { 2 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 25, 4 }) == Rational { 3 });
}

TEST_CASE("the half-way test compares the remainder when the floor sits exactly on f*f + f", "[rounded_root]")
{
    // Task 1: fixtures A, B and F do not kill the mutation `q >= f*f + f`
    // (only 4 of 200,000 random radicands did). These two do, one each way:
    // 21/10 at 0 dp: q = 2 = f*f + f with f = 1, and r/b = 1/10 < 1/4, so the
    //   root, 1.449..., is below the half-way point and rounds to 1.
    // 23/10 at 0 dp: the same q, and r/b = 3/10 > 1/4, so 1.516... rounds to 2.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 21, 10 }) == Rational { 1 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 23, 10 }) == Rational { 2 });
    // 22/10 = 11/5 at 0 dp: the same q, and r = 1 = floor(B/4) with B = 5, so
    // 4r = 4 < 5 and 1.483... rounds to 1. The code compares `r > B / 4`; the
    // off-by-one `r >= B / 4` is true here, although 4r >= B is not, and gives 2.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 22, 10 }) == Rational { 1 });
}

TEST_CASE("rounded_sqrt to a negative number of places rounds the root to whole tens", "[rounded_root]")
{
    // 1234 g^2 -> 35.128... g. To whole tens: 30 down, 40 up and to nearest,
    // since 35.128 is above the half-way 35. Here the whole part sits on
    // f*f + f again (q = 12, f = 3) and the remainder, 34/100 > 1/4, decides.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 1234 }) == Rational { 40 });
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { -1 }, RoundingMode::Floor>(Rational { 1234 }) == Rational { 30 });
    // 1216 g^2 -> 34.871... g, below the half-way 35: to nearest is 30.
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 1216 }) == Rational { 30 });
}

TEST_CASE("rounded_sqrt of a negative radicand is a domain error, never a clamp to zero", "[rounded_root]")
{
    constexpr auto node = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::Ceiling>(var<MassSquared>);
    constexpr auto outcome = formula::checked_evaluate<Spread>(node, variance(Rational { -427, 125 }));
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("rounded_sqrt of zero is zero exactly (Review Focus 1)", "[rounded_root]")
{
    // Under Ceiling, so that zero taken down the irrational path -- rather
    // than recognised as the exact root it is -- would come out as 0.01 g.
    constexpr auto node = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::Ceiling>(var<MassSquared>);
    constexpr auto outcome = formula::checked_evaluate<Spread>(node, variance(Rational { 0 }));
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->is_value());
    STATIC_REQUIRE(outcome->measurement().value() == Rational { 0 });
}

TEST_CASE("rounded_sqrt of an absent radicand is absent", "[rounded_root]")
{
    constexpr auto node = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::Ceiling>(var<MassSquared>);
    constexpr auto outcome =
        formula::checked_evaluate<Spread>(node, formula::environment(formula::Measured<MassSquared>::absent()));
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->is_empty());
}

TEST_CASE("rounded_sqrt reports overflow rather than a wrapped result", "[rounded_root]")
{
    // Task 1: an integer radicand of about 10^6 fits at 6 dp and overflows at
    // 7 dp, because floor(v) * 10^(2p) must stay below 2^64. 1000001 rather
    // than 10^6 itself, whose root is exactly 1000 and would take the tie
    // path, which has headroom of its own. Its root is 1000.000499999875...
    STATIC_REQUIRE(rootInGrams<DecimalPlaces { 6 }, RoundingMode::Floor>(Rational { 1'000'001 })
                   == Rational { 1'000'000'499, 1'000'000 });
    constexpr auto node = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 7 }, RoundingMode::Floor>(var<MassSquared>);
    constexpr auto outcome = formula::checked_evaluate<Spread>(node, variance(Rational { 1'000'001 }));
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("rounded_sqrt reports overflow at the exact 2^64 edge of the whole part", "[rounded_root]")
{
    // v * 10^4 for 1844674407370955161/1000 is 18446744073709551610, six
    // below 2^64: it fits, and its root rounds down to 42949672.95. One more in
    // the numerator puts v * 10^4 at 2^64 + 4. There floor(v) * 10^4 still
    // fits and it is adding the remainder's share that crosses, so this pins
    // the check on that addition, which the case above never reaches.
    using formula::detail::rounded_square_root;
    STATIC_REQUIRE(
        rounded_square_root(Rational { 1'844'674'407'370'955'161, 1000 }, DecimalPlaces { 2 }, RoundingMode::Floor).value()
        == Rational { 858'993'459, 20 });
    STATIC_REQUIRE(
        rounded_square_root(Rational { 1'844'674'407'370'955'162, 1000 }, DecimalPlaces { 2 }, RoundingMode::Floor).error()
        == formula::ArithmeticError::Overflow);
}

TEST_CASE("rounded_sqrt reports overflow when a negative number of places widens the denominator past 2^64",
          "[rounded_root]")
{
    // At -1 places the divisor is b * 10^2. With b = 2^63 - 1 that leaves
    // 64 bits, so the answer is Overflow -- although under Ceiling the true
    // answer, 10, would fit: the documented headroom, on the denominator.
    using formula::detail::rounded_square_root;
    constexpr auto tiny = Rational::make(1, std::numeric_limits<std::int64_t>::max()).value();
    STATIC_REQUIRE(rounded_square_root(tiny, DecimalPlaces { -1 }, RoundingMode::Ceiling).error()
                   == formula::ArithmeticError::Overflow);
    // A denominator well inside the bound fits: 100/((2^63 - 1)/100) is far
    // below 25, so its root is below 5, and to whole tens under Ceiling is 10.
    constexpr auto small = Rational::make(100, std::numeric_limits<std::int64_t>::max() / 100).value();
    STATIC_REQUIRE(rounded_square_root(small, DecimalPlaces { -1 }, RoundingMode::Ceiling).value() == Rational { 10 });
}

TEST_CASE("rounded_sqrt runs its 64-bit path at runtime too", "[rounded_root]")
{
    // Every case above is a STATIC_REQUIRE, evaluated by the compiler, so a
    // sanitizer never sees the uint64 arithmetic execute. These run it: the
    // table is read at runtime, so each call happens there, in clang-ubsan as
    // everywhere else.
    struct Case
    {
        Rational radicand;
        DecimalPlaces places;
        RoundingMode mode;
        Rational expected;
    };
    std::array<Case, 7> const cases { {
        { Rational { 427, 125 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, Rational { 37, 20 } },
        { Rational { 4057, 600 }, DecimalPlaces { 2 }, RoundingMode::Ceiling, Rational { 261, 100 } },
        { Rational { 9, 4 }, DecimalPlaces { 0 }, RoundingMode::HalfTowardZero, Rational { 1 } },
        { Rational { 22, 10 }, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero, Rational { 1 } },
        { Rational { 1234 }, DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero, Rational { 40 } },
        { Rational { 1'000'001 }, DecimalPlaces { 6 }, RoundingMode::Floor, Rational { 1'000'000'499, 1'000'000 } },
        { Rational { 1'844'674'407'370'955'161, 1000 },
          DecimalPlaces { 2 },
          RoundingMode::Floor,
          Rational { 858'993'459, 20 } },
    } };
    for (Case const& each: cases)
    {
        auto const rooted = formula::detail::rounded_square_root(each.radicand, each.places, each.mode);
        REQUIRE(rooted.has_value());
        CHECK(*rooted == each.expected);
    }

    auto const wrapped = formula::detail::rounded_square_root(
        Rational { 1'844'674'407'370'955'162, 1000 }, DecimalPlaces { 2 }, RoundingMode::Floor);
    REQUIRE(!wrapped.has_value());
    CHECK(wrapped.error() == formula::ArithmeticError::Overflow);

    // And through the node's conversion, g^2 in, kg out, at runtime.
    auto const inputs = variance(Rational { 4057, 600 });
    auto const node = formula::rounded_sqrt<unit::Gram, DecimalPlaces { 3 }, RoundingMode::Ceiling>(var<MassSquared>);
    auto const outcome = formula::checked_evaluate<Spread>(node, inputs);
    REQUIRE(outcome.has_value());
    CHECK(outcome->measurement().value() == Rational { 2601, 1000 });
}

TEST_CASE("the exact algorithm scales only the remainder, so a large denominator keeps its headroom", "[rounded_root]")
{
    // `detail::rounded_square_root` directly, on a radicand already in the
    // unit squared. 123456789/9973 at 7 places: scaling the whole numerator by
    // 10^14 would overflow, and splitting it first scales only the remainder,
    // which is below the denominator, so it fits. The root is 111.26141503...,
    // pinned to the seventh place, so a split that dropped or double-counted
    // the remainder shows.
    STATIC_REQUIRE(
        formula::detail::rounded_square_root(Rational { 123'456'789, 9973 }, DecimalPlaces { 7 }, RoundingMode::Floor)
            .value()
        == Rational { 22'252'283, 200'000 });
}
