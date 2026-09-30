// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/rounding_node.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented readings in grams, and what an operation makes of them.
struct Reading: formula::Quantity<Reading, "r", "an invented reading", unit::Gram>
{
};
struct Span: formula::Quantity<Span, "r_sp", "the span of the readings", unit::Gram>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", unit::One>
{
};
// Invented dimensionless draws, and the sum of their reciprocals.
struct Draw: formula::Quantity<Draw, "z", "an invented dimensionless draw", unit::One>
{
};
struct Reciprocals: formula::Quantity<Reciprocals, "z_r", "the sum of the draws' reciprocals", unit::One>
{
};
// Two invented gains and their product.
struct Gain: formula::Quantity<Gain, "g_1", "an invented gain", unit::One>
{
};
struct Boost: formula::Quantity<Boost, "g_2", "another invented gain", unit::One>
{
};
struct Amplified: formula::Quantity<Amplified, "g_12", "the two gains' product", unit::One>
{
};

// The lowest reading and the span of a series: compute alone, so a rounded
// output of it takes compute<Rational> and rounds that exactly.
struct ReadingSpan
{
    static constexpr std::string_view name = "reading span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "lowest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const apart = formula::RepTraits<Rep>::subtract(most, least);
        if (!apart.has_value())
            return std::unexpected { apart.error() };
        return std::array { least, *apart };
    }
};

// The sum of the draws' reciprocals, with the exact hook: over ten invented
// primes its exact denominator is their product, 75 bits, which no Rational
// holds and four 32-bit limbs do. Counts which route ran.
struct ReciprocalSum
{
    static constexpr std::string_view name = "reciprocal sum";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "total" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        if (!(declared[0] == formula::dim::Scalar))
            return std::nullopt;
        return std::array { formula::dim::Scalar };
    }

    static inline int computeCalls = 0;
    static inline int exactCalls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> draws) noexcept
    {
        if !consteval
        {
            ++computeCalls;
        }
        using Traits = formula::RepTraits<Rep>;
        std::expected<Rep, formula::ArithmeticError> const zero = Traits::from(formula::Rational { 0 });
        std::expected<Rep, formula::ArithmeticError> const one = Traits::from(formula::Rational { 1 });
        if (!zero.has_value() || !one.has_value())
            return std::unexpected { formula::ArithmeticError::Overflow };
        Rep running = *zero;
        for (Rep const& each: draws)
        {
            if (!(*zero < each))
                return std::unexpected { formula::ArithmeticError::DomainError };
            std::expected<Rep, formula::ArithmeticError> const reciprocal = Traits::divide(*one, each);
            if (!reciprocal.has_value())
                return std::unexpected { reciprocal.error() };
            std::expected<Rep, formula::ArithmeticError> const added = Traits::add(running, *reciprocal);
            if (!added.has_value())
                return std::unexpected { added.error() };
            running = *added;
        }
        return std::array { running };
    }

    static constexpr std::size_t exact_limbs = 4;

    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(std::span<formula::Rational const> draws) noexcept
    {
        if !consteval
        {
            ++exactCalls;
        }
        using Wide = formula::detail::WideUnsigned<exact_limbs>;
        formula::detail::WideRatio<exact_limbs> running { false, Wide {}, Wide::from_u64(1) };
        for (formula::Rational const& each: draws)
        {
            if (each.sign() <= 0)
                return std::unexpected { formula::ArithmeticError::DomainError };
            // running + 1 / (p / q) = (running.numerator * p + running.denominator * q) / (running.denominator * p)
            Wide const top = Wide::from_u64(static_cast<std::uint64_t>(each.numerator()));
            Wide const bottom = Wide::from_u64(static_cast<std::uint64_t>(each.denominator()));
            std::optional<Wide> const kept = formula::detail::mul_checked_or_none(running.numerator, top);
            std::optional<Wide> const added = formula::detail::mul_checked_or_none(running.denominator, bottom);
            std::optional<Wide> const summed =
                kept && added ? formula::detail::add_checked_or_none(*kept, *added) : std::nullopt;
            std::optional<Wide> const under = formula::detail::mul_checked_or_none(running.denominator, top);
            if (!summed || !under)
                return std::unexpected { formula::ArithmeticError::Overflow };
            running = formula::detail::WideRatio<exact_limbs> { false, *summed, *under };
        }
        return std::array { running };
    }
};

// The same operation with a hook that is not noexcept, and one too narrow:
// neither is taken, and both fall back to compute<Rational>. The narrow one
// declares its hook at its own width, so that the width alone refuses it; and
// were it taken, its DomainError would show where the fallback's Overflow is
// expected.
struct LooseReciprocalSum: ReciprocalSum
{
    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(std::span<formula::Rational const> draws)
    {
        return ReciprocalSum::compute_exact(draws);
    }
};
struct NarrowReciprocalSum: ReciprocalSum
{
    static constexpr std::size_t exact_limbs = 2;

    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(std::span<formula::Rational const>) noexcept
    {
        return std::unexpected { formula::ArithmeticError::DomainError };
    }
};

// The product of two gains, with the exact hook: 2^40 times 2^40 is 2^80,
// which the hook holds and no Rational does.
struct WideProduct
{
    static constexpr std::string_view name = "wide product";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "product" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] * declared[1] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep multiplicand, Rep multiplier) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const product = formula::RepTraits<Rep>::multiply(multiplicand, multiplier);
        if (!product.has_value())
            return std::unexpected { product.error() };
        return std::array { *product };
    }

    static constexpr std::size_t exact_limbs = 4;

    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(formula::Rational multiplicand, formula::Rational multiplier) noexcept
    {
        auto const leftWide = formula::detail::wide_from_rational<exact_limbs>(multiplicand);
        auto const rightWide = formula::detail::wide_from_rational<exact_limbs>(multiplier);
        auto const numeratorProduct = formula::detail::mul_checked_or_none(leftWide.numerator, rightWide.numerator);
        auto const denominatorProduct = formula::detail::mul_checked_or_none(leftWide.denominator, rightWide.denominator);
        if (!numeratorProduct || !denominatorProduct)
            return std::unexpected { formula::ArithmeticError::Overflow };
        return std::array { formula::detail::WideRatio<exact_limbs> {
            leftWide.negative != rightWide.negative && !numeratorProduct->is_zero(), *numeratorProduct, *denominatorProduct } };
    }
};

constexpr formula::Citation spanClause { .title = "Span of readings", .reference = "Example Standard 7", .section = "2.3" };
constexpr formula::Citation reciprocalClause { .title = "Reciprocal sum", .reference = "Example Standard 7", .section = "2.4" };
constexpr formula::Citation productClause { .title = "Product of gains", .reference = "Example Standard 7", .section = "2.5" };

constexpr auto spanCall = formula::opaque<ReadingSpan>(spanClause, formula::series<Reading, 4>);

// 127.3, 103.26, 191.07 and 139.4 g: span 87.81 g. With 191.11 g in place of
// 191.07 g the span is 87.85 g, a tie at 1 dp.
constexpr auto readings = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(1273, 10) },
                                                                                 formula::Measured<Reading> { rat(10326, 100) },
                                                                                 formula::Measured<Reading> { rat(19107, 100) },
                                                                                 formula::Measured<Reading> { rat(1394, 10) }));
constexpr auto tiedReadings = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(1273, 10) },
                                                                                     formula::Measured<Reading> { rat(10326, 100) },
                                                                                     formula::Measured<Reading> { rat(19111, 100) },
                                                                                     formula::Measured<Reading> { rat(1394, 10) }));

// Five and ten invented primes. The five's reciprocals sum to
// 2101205901/58386114749, which Rational holds; the ten's to a fraction over
// a 75-bit denominator, which it does not.
constexpr auto fiveDraws = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                               formula::Measured<Draw> { rat(127) },
                                                                               formula::Measured<Draw> { rat(139) },
                                                                               formula::Measured<Draw> { rat(163) },
                                                                               formula::Measured<Draw> { rat(197) }));
constexpr auto tenDraws = formula::environment(formula::measured_series<Draw>(
    formula::Measured<Draw> { rat(103) }, formula::Measured<Draw> { rat(127) }, formula::Measured<Draw> { rat(139) },
    formula::Measured<Draw> { rat(163) }, formula::Measured<Draw> { rat(197) }, formula::Measured<Draw> { rat(211) },
    formula::Measured<Draw> { rat(227) }, formula::Measured<Draw> { rat(229) }, formula::Measured<Draw> { rat(233) },
    formula::Measured<Draw> { rat(239) }));
constexpr auto fiveCall = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 5>);
constexpr auto tenCall = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 10>);

template <formula::RoundingMode Mode>
void check_routes_agree_on_five_draws()
{
    auto const fused = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, Mode>(fiveCall), fiveDraws);
    auto const afterwards = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded<unit::One, formula::DecimalPlaces { 6 }, Mode>(formula::opaque_output<"total">(fiveCall)), fiveDraws);
    REQUIRE(fused.has_value());
    REQUIRE(afterwards.has_value());
    CHECK(**fused == **afterwards);
}

/// What a sink that hears the opaque hooks was told of one call.
struct ToldCall
{
    std::size_t valuesHeld;
    formula::OpaqueValues values;
    bool answered;
};

/// A sink of the test's own: it records what each opaque call told it.
struct HearsCalls
{
    std::vector<ToldCall>* told;

    template <formula::Node N>
    void entered(N const&) noexcept
    {
    }

    template <formula::Node N, typename V>
    void produced(N const&, V const&) noexcept
    {
    }

    void opaque_entered(formula::OpaqueCallInfo const&) noexcept {}

    template <std::size_t M>
    void opaque_produced(formula::OpaqueCallInfo const& callInfo, formula::OpaqueEvaluated<formula::Rational, M> const& evaluated)
    {
        told->push_back(ToldCall { M, callInfo.values, evaluated.has_value() && evaluated->has_value() });
    }
};

struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};

// One value over another: compute alone, over two single values, so that a
// calculation -- which holds single values -- can define a quantity by it.
struct RatioOfTwo
{
    static constexpr std::string_view name = "ratio of two";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "ratio" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] / declared[1] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep dividend, Rep divisor) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const quotientValue = formula::RepTraits<Rep>::divide(dividend, divisor);
        if (!quotientValue.has_value())
            return std::unexpected { quotientValue.error() };
        return std::array { *quotientValue };
    }
};

constexpr formula::Citation shareClause { .title = "Share of two factors", .reference = "Example Standard 7", .section = "3.2" };
constexpr auto roundedShare = formula::rounded_output<"ratio", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
    formula::opaque<RatioOfTwo>(shareClause, formula::var<Factor>, formula::var<Other>));
constexpr auto shareCalculation = formula::calculation(formula::define<Share>(roundedShare));
} // namespace

TEST_CASE("rounded output: a node of its output's dimension that states its rounding", "[rounded-output]")
{
    constexpr auto roundedSpan =
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall);
    using RoundedSpan = decltype(roundedSpan);
    STATIC_REQUIRE(formula::Node<RoundedSpan>);
    STATIC_REQUIRE(RoundedSpan::dimension == formula::dim::Mass);
    STATIC_REQUIRE(RoundedSpan::unit == unit::Gram);
    STATIC_REQUIRE(RoundedSpan::places == formula::DecimalPlaces { 1 });
    STATIC_REQUIRE(RoundedSpan::mode == formula::RoundingMode::HalfEven);
    STATIC_REQUIRE(RoundedSpan::index == 1);
    STATIC_REQUIRE(RoundedSpan::output == "span");
    STATIC_REQUIRE(!RoundedSpan::refused);
}

TEST_CASE("rounded output: rounds in its own unit and not in the coherent one", "[rounded-output]")
{
    // 87.81 g to 1 dp of g is 87.8 g, 0.0878 kg. Rounded in kilograms it would
    // be 0.1 kg: the unit is part of the rounding.
    constexpr auto inGrams = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        readings);
    STATIC_REQUIRE(inGrams->measurement().value() == rat(439, 5));
    constexpr auto inKilograms = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"span", unit::Kilogram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        readings);
    STATIC_REQUIRE(**inKilograms == rat(1, 10));
    // Ceiling moves off 87.81 where HalfEven does not.
    constexpr auto upwards = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::Ceiling>(spanCall),
        readings);
    STATIC_REQUIRE(upwards->measurement().value() == rat(879, 10));
}

TEST_CASE("rounded output: a tie is broken by the mode as checked_round breaks it", "[rounded-output]")
{
    // 87.85 g: 87.8 under HalfEven and HalfTowardZero, 87.9 under HalfAwayFromZero.
    constexpr auto even = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        tiedReadings);
    constexpr auto away = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(spanCall),
        tiedReadings);
    constexpr auto toward = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfTowardZero>(spanCall),
        tiedReadings);
    STATIC_REQUIRE(even->measurement().value() == rat(439, 5));
    STATIC_REQUIRE(away->measurement().value() == rat(879, 10));
    STATIC_REQUIRE(toward->measurement().value() == rat(439, 5));
}

TEST_CASE("rounded output: the exact hook answers where the exact route overflows", "[rounded-output]")
{
    STATIC_REQUIRE(formula::detail::declares_compute_exact<ReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    // Ten reciprocals: the exact total needs a 75-bit denominator.
    auto const exactRoute = formula::checked_evaluate<Reciprocals>(formula::opaque_output<"total">(tenCall), tenDraws);
    REQUIRE(!exactRoute.has_value());
    CHECK(exactRoute.error() == formula::ArithmeticError::Overflow);

    ReciprocalSum::computeCalls = 0;
    ReciprocalSum::exactCalls = 0;
    auto const rounded = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(tenCall), tenDraws);
    REQUIRE(rounded.has_value());
    CHECK(rounded->measurement().value() == rat(2319, 40000)); // 0.057975
    CHECK(ReciprocalSum::exactCalls == 1);
    CHECK(ReciprocalSum::computeCalls == 0);
    auto const upwards = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Ceiling>(tenCall), tenDraws);
    CHECK(upwards->measurement().value() == rat(7247, 125000)); // 0.057976
}

TEST_CASE("rounded output: where both routes answer it is the exact output rounded in every mode", "[rounded-output]")
{
    // 2101205901/58386114749 = 0.0359881...: 0.035988, 0.035989 upwards.
    constexpr auto fused = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), fiveDraws);
    STATIC_REQUIRE(fused->measurement().value() == rat(8997, 250000));
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfAwayFromZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfTowardZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfEven>();
    check_routes_agree_on_five_draws<formula::RoundingMode::Ceiling>();
    check_routes_agree_on_five_draws<formula::RoundingMode::Floor>();
    check_routes_agree_on_five_draws<formula::RoundingMode::TowardZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::AwayFromZero>();
}

TEST_CASE("rounded output: absent when an input is and the hook is never called", "[rounded-output]")
{
    auto const gap = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                         formula::Measured<Draw> { rat(127) },
                                                                         formula::Measured<Draw>::absent(),
                                                                         formula::Measured<Draw> { rat(163) },
                                                                         formula::Measured<Draw> { rat(197) }));
    ReciprocalSum::exactCalls = 0;
    auto const outcome = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), gap);
    REQUIRE(outcome.has_value());
    CHECK(outcome->is_empty());
    CHECK(ReciprocalSum::exactCalls == 0);
}

TEST_CASE("rounded output: an input's failure is relayed with its site and the hook is never called", "[rounded-output]")
{
    // The draws divided by a zero divisor: the division fails at element 0.
    constexpr auto divided = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 5> / formula::var<Divisor>);
    auto const byZero = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                            formula::Measured<Draw> { rat(127) },
                                                                            formula::Measured<Draw> { rat(139) },
                                                                            formula::Measured<Draw> { rat(163) },
                                                                            formula::Measured<Draw> { rat(197) }),
                                             formula::Measured<Divisor> { rat(0) });
    ReciprocalSum::exactCalls = 0;
    auto const called = formula::detail::evaluate_rounded_call<0>(divided, byZero, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::DivisionByZero);
    CHECK(called.error().origin == formula::OpaqueFailure::Propagated);
    CHECK(called.error().element == std::optional<std::size_t> { 0 });
    CHECK(ReciprocalSum::exactCalls == 0);
    auto const outcome = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(divided), byZero);
    REQUIRE(!outcome.has_value());
    CHECK(outcome.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("rounded output: the hook's own failure is the operation's", "[rounded-output]")
{
    auto const withZero = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                              formula::Measured<Draw> { rat(0) },
                                                                              formula::Measured<Draw> { rat(139) },
                                                                              formula::Measured<Draw> { rat(163) },
                                                                              formula::Measured<Draw> { rat(197) }));
    auto const called = formula::detail::evaluate_rounded_call<0>(fiveCall, withZero, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::DomainError);
    CHECK(called.error().origin == formula::OpaqueFailure::Own);
}

TEST_CASE("rounded output: a rounding that fails after the call answered is the output's alone", "[rounded-output]")
{
    constexpr auto productCall = formula::opaque<WideProduct>(productClause, formula::var<Gain>, formula::var<Boost>);
    constexpr std::int64_t twoToForty = std::int64_t { 1 } << 40;
    auto const huge = formula::environment(formula::Measured<Gain> { rat(twoToForty) }, formula::Measured<Boost> { rat(twoToForty) });
    // The exact route cannot hold 2^80 at all.
    auto const plain = formula::checked_evaluate<Amplified>(formula::opaque_output<"product">(productCall), huge);
    REQUIRE(!plain.has_value());
    CHECK(plain.error() == formula::ArithmeticError::Overflow);
    // The hook holds it -- the call answers -- but no Rational holds 2^80
    // rounded to units either: the output fails, not the call.
    auto const called = formula::detail::evaluate_rounded_call<0>(productCall, huge, formula::NullSink {});
    CHECK(called.has_value());
    auto const rounded = formula::checked_evaluate<Amplified>(
        formula::rounded_output<"product", unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(productCall), huge);
    REQUIRE(!rounded.has_value());
    CHECK(rounded.error() == formula::ArithmeticError::Overflow);
    // The control: 2^40 times 1/2^30 is 1024.
    auto const modest = formula::environment(formula::Measured<Gain> { rat(twoToForty) },
                                             formula::Measured<Boost> { rat(1, std::int64_t { 1 } << 30) });
    auto const answered = formula::checked_evaluate<Amplified>(
        formula::rounded_output<"product", unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(productCall), modest);
    REQUIRE(answered.has_value());
    CHECK(answered->measurement().value() == rat(1024));
}

TEST_CASE("rounded output: a sink hearing the call is told it holds no values", "[rounded-output]")
{
    std::vector<ToldCall> told;
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), fiveDraws,
        HearsCalls { &told });
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"total">(fiveCall), fiveDraws, HearsCalls { &told });
    REQUIRE(told.size() == 2);
    CHECK(told[0].valuesHeld == 0);
    CHECK(told[0].values == formula::OpaqueValues::RoundedWhereUsed);
    CHECK(told[0].answered);
    CHECK(told[1].valuesHeld == 1);
    CHECK(told[1].values == formula::OpaqueValues::Exact);
    CHECK(told[1].answered);
}

TEST_CASE("rounded output: a hook that is not noexcept or too narrow is not taken", "[rounded-output]")
{
    STATIC_REQUIRE(!formula::detail::declares_compute_exact<LooseReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    STATIC_REQUIRE(!formula::detail::declares_compute_exact<NarrowReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    // Each falls back to compute<Rational>, and so to its Overflow over ten
    // draws, where ReciprocalSum's own hook answers.
    auto const loose = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(
            formula::opaque<LooseReciprocalSum>(reciprocalClause, formula::series<Draw, 10>)),
        tenDraws);
    auto const narrow = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(
            formula::opaque<NarrowReciprocalSum>(reciprocalClause, formula::series<Draw, 10>)),
        tenDraws);
    REQUIRE(!loose.has_value());
    CHECK(loose.error() == formula::ArithmeticError::Overflow);
    REQUIRE(!narrow.has_value());
    CHECK(narrow.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("rounded output: the walks see the call's inputs", "[rounded-output][precision][calculation]")
{
    using RoundedShare = std::remove_cvref_t<decltype(roundedShare)>;
    STATIC_REQUIRE(formula::detail::LevelChildren<RoundedShare>::seen);
    STATIC_REQUIRE(std::is_same_v<typename formula::detail::LevelChildren<RoundedShare>::type,
                                  std::tuple<formula::VarNode<Factor>, formula::VarNode<Other>>>);
    // A calculation defines a quantity by it and reads what the call reads.
    STATIC_REQUIRE(formula::inputs_of(shareCalculation) == std::array<std::string_view, 2> { "k", "k_o" });
}

TEST_CASE("rounded output: a page lists a call once whether its output is rounded or not", "[rounded-output][document]")
{
    constexpr auto both = formula::opaque_output<"total">(fiveCall)
                          - formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall);
    formula::Documentation const page = formula::document(both);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "reciprocal sum");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.formula == "reciprocal sum(z(i)).total - round(reciprocal sum(z(i)).total, to 6 dp)");
}
