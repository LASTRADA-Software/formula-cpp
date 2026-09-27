// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/opaque.hpp>

#include "opaque_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The shared fixture's quantities (see the plan): invented readings in grams.
struct Reading: formula::Quantity<Reading, "r", "an invented reading", unit::Gram>
{
};
struct Lowest: formula::Quantity<Lowest, "r_lo", "the lowest reading", unit::Gram>
{
};
struct Spread: formula::Quantity<Spread, "r_sp", "the spread of the readings", unit::Gram>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", unit::One>
{
};
struct Shift: formula::Quantity<Shift, "r_0", "an invented shift", unit::Gram>
{
};

// A consumer's operation. Declared here, in the test, on purpose: the point
// is that a consumer can write one and it is evaluated without any library
// change.
struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 3> outputs { "lowest", "highest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 3>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0], declared[0] };
    }

    // Counts calls, so a test can tell "absent because compute was never
    // called" from "absent because compute coped" (Review Focus 2).
    static inline int calls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 3>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        if !consteval
        {
            ++calls;
        }
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const spread = formula::RepTraits<Rep>::subtract(most, least);
        if (!spread.has_value())
            return std::unexpected { spread.error() };
        return std::array { least, most, *spread };
    }
};

// A second consumer operation, whose own compute fails: a series whose
// elements are all equal has no spread to scale by, and says so.
struct RelativeSpread
{
    static constexpr std::string_view name = "relative spread";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "ratio" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1>) noexcept
    {
        return std::array { formula::dim::Scalar };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
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
        if (!(least < most))
            return std::unexpected { formula::ArithmeticError::DomainError };
        std::expected<Rep, formula::ArithmeticError> const ratio = formula::RepTraits<Rep>::divide(most, least);
        if (!ratio.has_value())
            return std::unexpected { ratio.error() };
        return std::array { *ratio };
    }
};

// Two inputs of two shapes, a series and then one value: the lowest reading
// raised by a shift of the same dimension. Called counts calls as SeriesSpan
// does.
struct ShiftedLowest
{
    static constexpr std::string_view name = "shifted lowest";
    static constexpr std::array shapes { formula::InputShape::Series, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "shifted" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        if (!(declared[0] == declared[1]))
            return std::nullopt;
        return std::array { declared[0] };
    }

    static inline int calls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> readings,
                                                                                         Rep raisedBy) noexcept
    {
        if !consteval
        {
            ++calls;
        }
        Rep least = readings[0];
        for (Rep const& each: readings)
            if (each < least)
                least = each;
        std::expected<Rep, formula::ArithmeticError> const shifted = formula::RepTraits<Rep>::add(least, raisedBy);
        if (!shifted.has_value())
            return std::unexpected { shifted.error() };
        return std::array { *shifted };
    }
};

// A curve input: the rise of the values from the first point to the last,
// per unit of the points -- two dimensions in, the second over the first out.
struct EndToEndSlope
{
    static constexpr std::string_view name = "end to end slope";
    static constexpr std::array shapes { formula::InputShape::Curve };
    static constexpr std::array<std::string_view, 1> outputs { "slope" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[1] / declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> points, std::span<Rep const> pointValues) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const rise =
            formula::RepTraits<Rep>::subtract(pointValues.back(), pointValues.front());
        if (!rise.has_value())
            return std::unexpected { rise.error() };
        std::expected<Rep, formula::ArithmeticError> const run =
            formula::RepTraits<Rep>::subtract(points.back(), points.front());
        if (!run.has_value())
            return std::unexpected { run.error() };
        std::expected<Rep, formula::ArithmeticError> const slope = formula::RepTraits<Rep>::divide(*rise, *run);
        if (!slope.has_value())
            return std::unexpected { slope.error() };
        return std::array { *slope };
    }
};

// A shift stated in tonnes, so that reading it into kilograms can overflow.
struct Huge: formula::Quantity<Huge, "r_h", "an invented huge shift", unit::Tonne>
{
};
struct HugeShifted
{
    static constexpr std::string_view name = "shifted lowest";
    static constexpr std::array shapes { formula::InputShape::Series, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "shifted" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> readingsIn,
                                                                                         Rep raisedBy) noexcept
    {
        (void) readingsIn;
        return std::array { raisedBy };
    }
};

struct Level: formula::Quantity<Level, "r_l", "an invented level", unit::One>
{
};
struct Load: formula::Quantity<Load, "F_h", "an invented load held", unit::Newton>
{
};
struct Rise: formula::Quantity<Rise, "k_r", "an invented rise per millimetre", unit::NewtonPerMillimetre>
{
};

// The shared fixture's readings: 127, 103, 191, 139 g. Every element differs
// and the lowest and highest are neither first nor last, so min/max and
// first/last give different answers.
constexpr auto readings = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                 formula::Measured<Reading> { rat(103) },
                                                                                 formula::Measured<Reading> { rat(191) },
                                                                                 formula::Measured<Reading> { rat(139) }));

constexpr auto span_call = formula::opaque<SeriesSpan>(
    { .title = "Spread of readings", .reference = "Example Standard 12", .section = "4.2" }, formula::series<Reading, 4>);
} // namespace

TEST_CASE("an opaque operation's outputs are nodes, each in its declared dimension", "[opaque]")
{
    // min/max, not first/last: first/last would give 127 and 139. Coherent SI
    // in, declared unit out: a unit slip would be off by a factor of 1000.
    constexpr auto lowest = formula::checked_evaluate<Lowest>(formula::opaque_output<"lowest">(span_call), readings);
    STATIC_REQUIRE(lowest->measurement().value() == rat(103));
    constexpr auto highest = formula::checked_evaluate<Lowest>(formula::opaque_output<"highest">(span_call), readings);
    STATIC_REQUIRE(highest->measurement().value() == rat(191));
    constexpr auto spread = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), readings);
    STATIC_REQUIRE(spread->measurement().value() == rat(88));
    STATIC_REQUIRE(spread->source() == formula::ValueSource::Derived);

    // The output is chosen by name: "span" is the third output, zero-based 2.
    STATIC_REQUIRE(decltype(formula::opaque_output<"span">(span_call))::index == 2);
    STATIC_REQUIRE(decltype(formula::opaque_output<"span">(span_call))::output == "span");
    STATIC_REQUIRE(decltype(formula::opaque_output<"span">(span_call))::dimension == formula::dim::Mass);
    STATIC_REQUIRE(!decltype(span_call)::refused);
}

TEST_CASE("an output enters ordinary arithmetic", "[opaque]")
{
    // span / 2 + lowest = 44 + 103 = 147 g: the output is a Node like any
    // other. Taking highest for lowest would give 235, first/last 133.
    constexpr auto mid = formula::opaque_output<"span">(span_call) / rat(2) + formula::opaque_output<"lowest">(span_call);
    STATIC_REQUIRE(formula::checked_evaluate<Lowest>(mid, readings)->measurement().value() == rat(147));
}

TEST_CASE("the operation receives values in coherent SI and its outputs are in coherent SI", "[opaque]")
{
    constexpr auto called = formula::detail::evaluate_call<formula::Rational>(span_call, readings, formula::NullSink {});
    STATIC_REQUIRE(called.has_value());
    STATIC_REQUIRE(called->has_value());
    // Grams arrive as kilograms: 103 g is 103/1000 kg, 88 g is 11/125 kg.
    STATIC_REQUIRE((**called)[0] == rat(103, 1000));
    STATIC_REQUIRE((**called)[1] == rat(191, 1000));
    STATIC_REQUIRE((**called)[2] == rat(11, 125));
}

TEST_CASE("one absent element makes the whole call absent, and compute is never called", "[opaque]")
{
    SeriesSpan::calls = 0;
    auto const gap = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                            formula::Measured<Reading>::absent(),
                                                                            formula::Measured<Reading> { rat(191) },
                                                                            formula::Measured<Reading> { rat(139) }));
    auto const outcome = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), gap);
    REQUIRE(outcome.has_value());
    CHECK(outcome->is_empty());
    CHECK(SeriesSpan::calls == 0); // not "compute skipped the gap and said 191 - 127"

    // The control: the same call with every element present does call it,
    // so the count above can tell the two apart.
    auto const whole = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), readings);
    REQUIRE(whole.has_value());
    CHECK(whole->measurement().value() == rat(88));
    CHECK(SeriesSpan::calls == 1);
}

TEST_CASE("an absent single input makes the call absent, and compute is never called", "[opaque]")
{
    ShiftedLowest::calls = 0;
    constexpr auto shiftedCall = formula::opaque<ShiftedLowest>(
        { .reference = "Example Standard 12", .section = "4.4" }, formula::series<Reading, 4>, formula::var<Shift>);
    auto const noShift = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                formula::Measured<Reading> { rat(103) },
                                                                                formula::Measured<Reading> { rat(191) },
                                                                                formula::Measured<Reading> { rat(139) }),
                                              formula::Measured<Shift>::absent());
    auto const outcome = formula::checked_evaluate<Lowest>(formula::opaque_output<"shifted">(shiftedCall), noShift);
    REQUIRE(outcome.has_value());
    CHECK(outcome->is_empty());
    CHECK(ShiftedLowest::calls == 0);

    // With the shift present: 103 + 17 = 120 g. The inputs arrive in the
    // declared order, series then value.
    auto const shifted = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                formula::Measured<Reading> { rat(103) },
                                                                                formula::Measured<Reading> { rat(191) },
                                                                                formula::Measured<Reading> { rat(139) }),
                                              formula::Measured<Shift> { rat(17) });
    auto const raised = formula::checked_evaluate<Lowest>(formula::opaque_output<"shifted">(shiftedCall), shifted);
    REQUIRE(raised.has_value());
    CHECK(raised->measurement().value() == rat(120));
    CHECK(ShiftedLowest::calls == 1);
}

TEST_CASE("a failing series input fails the call, and the error is the input's, not the operation's", "[opaque]")
{
    // series<Reading, 4> / var<Divisor> with the divisor zero: the elementwise
    // division fails at element 0, and the call relays DivisionByZero with
    // that element, as Propagated -- compute is never called.
    SeriesSpan::calls = 0;
    constexpr auto divided = formula::opaque<SeriesSpan>({ .reference = "Example Standard 12" },
                                                         formula::series<Reading, 4> / formula::var<Divisor>);
    auto const zero = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                             formula::Measured<Reading> { rat(103) },
                                                                             formula::Measured<Reading> { rat(191) },
                                                                             formula::Measured<Reading> { rat(139) }),
                                           formula::Measured<Divisor> { rat(0) });
    auto const called = formula::detail::evaluate_call<formula::Rational>(divided, zero, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::DivisionByZero);
    CHECK(called.error().origin == formula::OpaqueFailure::Propagated);
    CHECK(called.error().element == std::optional<std::size_t> { 0 });
    CHECK(SeriesSpan::calls == 0);

    auto const outcome = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(divided), zero);
    REQUIRE(!outcome.has_value());
    CHECK(outcome.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("an input's failure is not hidden behind another input's absence", "[opaque]")
{
    // The series is absent at one element and the shift fails to convert: the
    // failure wins, as BinaryNode decides it, since absence is only judged
    // once every input has been asked.
    constexpr auto hugeCall = formula::opaque<HugeShifted>(
        { .reference = "Example Standard 12" }, formula::series<Reading, 4>, formula::var<Huge>);
    // 2^62 t is 2^65 kg: its conversion to the coherent unit overflows.
    auto const overflowing = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                    formula::Measured<Reading>::absent(),
                                                                                    formula::Measured<Reading> { rat(191) },
                                                                                    formula::Measured<Reading> { rat(139) }),
                                                  formula::Measured<Huge> { rat(std::int64_t { 1 } << 62) });
    auto const called = formula::detail::evaluate_call<formula::Rational>(hugeCall, overflowing, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::Overflow);
    CHECK(called.error().origin == formula::OpaqueFailure::Propagated);
    CHECK(!called.error().element.has_value());
}

TEST_CASE("compute's own failure is relayed as its error, and marked as the operation's own", "[opaque]")
{
    // Every element equal: RelativeSpread has no spread to scale by and
    // returns DomainError itself.
    constexpr auto relative =
        formula::opaque<RelativeSpread>({ .reference = "Example Standard 12" }, formula::series<Reading, 4>);
    constexpr auto flat = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) }));
    constexpr auto called = formula::detail::evaluate_call<formula::Rational>(relative, flat, formula::NullSink {});
    STATIC_REQUIRE(!called.has_value());
    STATIC_REQUIRE(called.error().error == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(called.error().origin == formula::OpaqueFailure::Own);
    STATIC_REQUIRE(!called.error().element.has_value());
    constexpr auto outcome = formula::checked_evaluate<Level>(formula::opaque_output<"ratio">(relative), flat);
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::ArithmeticError::DomainError);

    // The control: the fixture's readings are not all equal, 191/103.
    constexpr auto ratio = formula::checked_evaluate<Level>(formula::opaque_output<"ratio">(relative), readings);
    STATIC_REQUIRE(ratio->measurement().value() == rat(191, 103));
}

TEST_CASE("a curve input arrives as its points and then its values", "[opaque]")
{
    // Points 103, 127, 163 mm and values 139, 163, 241 N: (241 - 139) N over
    // (163 - 103) mm is 17/10 N/mm. Values first and points second would give
    // 10/17; one point late would give 13/4.
    constexpr formula::BreakpointTable<3> openings { formula::breakpoint(103),
                                                     formula::breakpoint(127),
                                                     formula::breakpoint(163) };
    constexpr auto slopeCall = formula::opaque<EndToEndSlope>(
        { .reference = "Example Standard 12", .section = "4.5" },
        formula::curve(formula::domain<unit::Millimetre, openings>, formula::series<Load, 3>));
    constexpr auto held = formula::environment(formula::measured_series<Load>(
        formula::Measured<Load> { rat(139) }, formula::Measured<Load> { rat(163) }, formula::Measured<Load> { rat(241) }));
    constexpr auto outcome = formula::checked_evaluate<Rise>(formula::opaque_output<"slope">(slopeCall), held);
    STATIC_REQUIRE(outcome->measurement().value() == rat(17, 10));
    STATIC_REQUIRE(decltype(formula::opaque_output<"slope">(slopeCall))::dimension == formula::dim::ForcePerLength);

    // An absent value anywhere in the curve makes the call absent.
    constexpr auto gap = formula::environment(formula::measured_series<Load>(
        formula::Measured<Load> { rat(139) }, formula::Measured<Load>::absent(), formula::Measured<Load> { rat(241) }));
    STATIC_REQUIRE(formula::checked_evaluate<Rise>(formula::opaque_output<"slope">(slopeCall), gap)->is_empty());
}

TEST_CASE("an opaque output evaluates in double as well as exactly", "[opaque]")
{
    // 88 g is 0.088 kg in the coherent unit.
    auto const inDouble = formula::checked_evaluate_si<double>(formula::opaque_output<"span">(span_call), readings);
    REQUIRE(inDouble.has_value());
    REQUIRE(inDouble->has_value());
    CHECK(std::abs(**inDouble - 0.088) < 1e-12);
}

TEST_CASE("a result entered by a person replaces an opaque output, which is not evaluated", "[opaque]")
{
    SeriesSpan::calls = 0;
    auto const typedIn = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                formula::Measured<Reading> { rat(103) },
                                                                                formula::Measured<Reading> { rat(191) },
                                                                                formula::Measured<Reading> { rat(139) }),
                                              formula::entered(formula::Measured<Spread> { rat(97) }));
    auto const outcome = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), typedIn);
    REQUIRE(outcome.has_value());
    CHECK(outcome->measurement().value() == rat(97));
    CHECK(outcome->is_overridden());
    CHECK(SeriesSpan::calls == 0);
}

TEST_CASE("an opaque call and its outputs survive a default-constructibility probe", "[opaque]")
{
    // std::tuple asks whether its members are default-constructible; a `{}`
    // initialiser on an expression member turned that question into a hard
    // error on clang, g++-14 and libc++ in phase 11 (defect class 4). Here
    // it must simply compile, and answer.
    using Output = decltype(formula::opaque_output<"span">(span_call));
    STATIC_REQUIRE(std::is_default_constructible_v<std::tuple<Output>> == std::is_default_constructible_v<Output>);
    STATIC_REQUIRE(std::is_default_constructible_v<std::tuple<decltype(span_call)>>
                   == std::is_default_constructible_v<decltype(span_call)>);
    STATIC_REQUIRE(formula::Node<Output>);
    STATIC_REQUIRE(!formula::Node<decltype(span_call)>);
    STATIC_REQUIRE(!formula::SeriesNode<decltype(span_call)>);
}

TEST_CASE("an opaque output declared in a header is one formula in two translation units", "[opaque]")
{
    // 163, 127, 197 g: the highest is 197, neither the first nor the last.
    auto const here = formula::checked_evaluate<opaque_cross_tu::Reading>(opaque_cross_tu::highest, opaque_cross_tu::inputs);
    auto const there = highest_in_other_tu(opaque_cross_tu::highest);
    REQUIRE(here.has_value());
    REQUIRE(there.has_value());
    CHECK(here->measurement().value() == rat(197));
    CHECK(there->measurement().value() == rat(197));
}
