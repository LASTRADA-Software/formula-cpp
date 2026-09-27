// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "opaque_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
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

namespace
{
// A consumer's own node kind, evaluated through the two-parameter extension
// point (`sink.hpp`): it records no step, so a failure it relays through an
// opaque call cannot be told apart from the call's own.
struct UntracedFailure: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};

template <typename Rep = formula::Rational, typename Env>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(UntracedFailure const&, Env const&) noexcept
{
    return std::unexpected { formula::ArithmeticError::DomainError };
}

// The crossed-over vocabulary pattern (`vocabulary_tests.cpp`): each word is
// the other quantity's, so a symbol written past the vocabulary shows.
inline constexpr auto crossed = formula::vocabulary(formula::renames<Reading>("q"), formula::renames<Divisor>("r"));

std::size_t occurrences(std::string const& haystack, std::string_view needle)
{
    std::size_t found = 0;
    for (std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
        ++found;
    return found;
}

formula::Trace<> traced_span(auto const& environment)
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"span">(span_call), environment, formula::RecordingSink { recorded, crossed });
    return recorded;
}
} // namespace

TEST_CASE("an opaque step names the operation, its citation, every output, and that its inside is not shown",
          "[opaque][trace]")
{
    formula::Trace<> const recorded = traced_span(readings);
    // The input in the sink's vocabulary (q, not r); the outputs in the
    // input's unit, grams; the output step naming its output.
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. q = 127 g; 103 g; 191 g; 139 g\n"
             "2. series span(#1) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] "
             "[Spread of readings, Example Standard 12, 4.2]\n"
             "3. span of #2 = 88 g\n");
    REQUIRE(recorded.steps.size() == 3);
    CHECK(recorded.steps[1].kind == formula::StepKind::OpaqueOperation);
    CHECK(recorded.steps[2].kind == formula::StepKind::OpaqueOutput);
    CHECK(recorded.steps[1].operands == std::vector<std::size_t> { 0 });
    CHECK(recorded.steps[2].operands == std::vector<std::size_t> { 1 });
    formula::OpaqueStepData<> const* const callRow = formula::opaque_data(recorded, 1);
    REQUIRE(callRow != nullptr);
    CHECK(callRow->operationName == "series span");
    CHECK(callRow->failure == formula::OpaqueFailure::None);
    REQUIRE(callRow->outputs.size() == 3);
    CHECK(callRow->outputs[2].name == "span");
    CHECK(callRow->outputs[2].value == rat(11, 125));
    REQUIRE(formula::opaque_output_data(recorded, 2) != nullptr);
    CHECK(formula::opaque_output_data(recorded, 2)->outputIndex == 2);
    CHECK(formula::opaque_data(recorded, 2) == nullptr);
}

TEST_CASE("the inside-not-shown marker depends on the kind alone", "[opaque][trace]")
{
    formula::Step<> bare {};
    bare.kind = formula::StepKind::OpaqueOperation; // no name, no outputs, no citation, no row
    formula::Trace<> recorded {};
    recorded.steps.push_back(bare);
    CHECK(formula::render_trace(recorded, { .maxSteps = 5 })
          == "1. opaque() = (not measured) [inside not shown] (no citation given)\n");
}

TEST_CASE("an uncited opaque call says so, in the trace and on the page", "[opaque][trace][document]")
{
    constexpr auto uncited = formula::opaque_output<"span">(formula::opaque<SeriesSpan>({}, formula::series<Reading, 4>));
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(uncited, readings, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 20 });
    CHECK(text.find("span = 88 g [inside not shown] (no citation given)\n") != std::string::npos);

    formula::Documentation const page = formula::document(uncited);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "series span");
    CHECK(page.opaqueOperations[0].citation == formula::Citation {});
    CHECK(page.citations.empty()); // an uncited call adds no empty citation
}

TEST_CASE("an opaque step tells its own failure from a relayed one", "[opaque][trace]")
{
    // The operation's own: every reading equal.
    constexpr auto relative = formula::opaque_output<"ratio">(
        formula::opaque<RelativeSpread>({ .reference = "Example Standard 12" }, formula::series<Reading, 4>));
    constexpr auto flat = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) },
                                                                                 formula::Measured<Reading> { rat(139) }));
    formula::Trace<> own {};
    (void) formula::detail::dispatch<formula::Rational>(relative, flat, formula::RecordingSink { own });
    REQUIRE(formula::opaque_data(own, 1) != nullptr);
    CHECK(formula::opaque_data(own, 1)->failure == formula::OpaqueFailure::Own);
    CHECK(formula::render_trace(own, { .maxSteps = 10 })
          == "1. r = 139 g; 139 g; 139 g; 139 g\n"
             "2. relative spread(#1) = argument outside the domain of the operation [inside not shown] "
             "[the operation itself failed, not any input] [Example Standard 12]\n"
             "3. ratio of #2 = argument outside the domain of the operation\n");

    // Relayed: a zero divisor fails the input series at its first element.
    constexpr auto divided = formula::opaque_output<"span">(formula::opaque<SeriesSpan>(
        { .reference = "Example Standard 12" }, formula::series<Reading, 4> / formula::var<Divisor>));
    auto const zero = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                             formula::Measured<Reading> { rat(103) },
                                                                             formula::Measured<Reading> { rat(191) },
                                                                             formula::Measured<Reading> { rat(139) }),
                                           formula::Measured<Divisor> { rat(0) });
    formula::Trace<> relayed {};
    (void) formula::detail::dispatch<formula::Rational>(divided, zero, formula::RecordingSink { relayed, crossed });
    std::string const text = formula::render_trace(relayed, { .maxSteps = 20 });
    REQUIRE(formula::opaque_data(relayed, 3) != nullptr);
    CHECK(formula::opaque_data(relayed, 3)->failure == formula::OpaqueFailure::Propagated);
    CHECK(relayed.steps[3].failedElement == std::optional<std::size_t> { 0 });
    CHECK(text.find("series span(#3) = division by zero [inside not shown] [carried up from #3, at element 1] "
                    "[Example Standard 12]\n")
          != std::string::npos);
    // One failing input step, one relay by the call and one by its output:
    // the division's own line, the call's and the output's -- counted by hand,
    // since a REJECT cannot refuse a second copy of the same text.
    CHECK(occurrences(text, "division by zero") == 3);
    CHECK(occurrences(text, "[the operation itself failed") == 0);
}

TEST_CASE("a relayed failure no input step shows is undetermined, not guessed", "[opaque][trace]")
{
    constexpr auto shifted = formula::opaque_output<"shifted">(formula::opaque<ShiftedLowest>(
        { .reference = "Example Standard 12" }, formula::series<Reading, 4>, UntracedFailure {}));
    formula::Trace<> recorded {};
    auto const outcome =
        formula::detail::dispatch<formula::Rational>(shifted, readings, formula::RecordingSink { recorded });
    REQUIRE(!outcome.has_value());
    REQUIRE(formula::opaque_data(recorded, 1) != nullptr);
    CHECK(formula::opaque_data(recorded, 1)->failure == formula::OpaqueFailure::Undetermined);
    CHECK(formula::render_trace(recorded, { .maxSteps = 10 }).find("[this operation or an input: an input recorded no step]")
          != std::string::npos);
}

TEST_CASE("an opaque step's outputs share the render budget and say how many were cut", "[opaque][trace]")
{
    formula::Trace<> const recorded = traced_span(readings);
    // Six units: the series line and its four readings take five, the call's
    // line the sixth, and it has none left for its outputs.
    CHECK(formula::render_trace(recorded, { .maxSteps = 6 })
          == "1. q = 127 g; 103 g; 191 g; 139 g\n"
             "2. series span(#1) = ... 3 more [inside not shown] [Spread of readings, Example Standard 12, 4.2]\n"
             "... 1 further step not shown\n");
    // Eight: the call shows two outputs and says one more.
    CHECK(formula::render_trace(recorded, { .maxSteps = 8 })
          == "1. q = 127 g; 103 g; 191 g; 139 g\n"
             "2. series span(#1) = lowest = 103 g; highest = 191 g; ... 1 more [inside not shown] "
             "[Spread of readings, Example Standard 12, 4.2]\n"
             "... 1 further step not shown\n");
}

TEST_CASE("an opaque output renders as a call to the named operation in every dialect", "[opaque][render]")
{
    constexpr auto spanOutput = formula::opaque_output<"span">(span_call);
    CHECK(formula::render(spanOutput) == "series span(r(i)).span");
    CHECK(formula::render<formula::Dialect::Markdown>(spanOutput) == "series span(`r(i)`).span");
    CHECK(formula::render<formula::Dialect::LaTeX>(spanOutput) == "\\text{series span}({r}_{i})_{\\text{span}}");
    // The inputs follow the vocabulary; the operation's name does not.
    CHECK(formula::render(spanOutput, crossed) == "series span(q(i)).span");
    // In arithmetic, a call is one operand and needs no bracket.
    CHECK(formula::render(spanOutput / rat(2)) == "series span(r(i)).span / 2");
    // No Markdown link syntax (the render_tests.cpp guard).
    std::string const markdown = formula::render<formula::Dialect::Markdown>(spanOutput);
    CHECK(markdown.find("](") == std::string::npos);
    CHECK(markdown.find('[') == std::string::npos);
}

TEST_CASE("a curve input renders as its points and its values", "[opaque][render]")
{
    constexpr formula::BreakpointTable<3> openings { formula::breakpoint(103),
                                                     formula::breakpoint(127),
                                                     formula::breakpoint(163) };
    constexpr auto slopeOutput = formula::opaque_output<"slope">(formula::opaque<EndToEndSlope>(
        { .reference = "Example Standard 12" },
        formula::curve(formula::domain<unit::Millimetre, openings>, formula::series<Load, 3>)));
    CHECK(formula::render(slopeOutput) == "end to end slope(domain(103, 127, 163 mm), F_h(i)).slope");
}

TEST_CASE("a page lists each opaque call once, with its citation", "[opaque][document]")
{
    constexpr auto both = formula::opaque_output<"lowest">(span_call) + formula::opaque_output<"span">(span_call);
    formula::Documentation const page = formula::document(both, crossed);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "series span");
    CHECK(page.opaqueOperations[0].outputs == std::vector<std::string_view> { "lowest", "highest", "span" });
    CHECK(page.opaqueOperations[0].citation.reference == "Example Standard 12");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].section == "4.2");
    // The input's row, in the page's vocabulary.
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "q");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
}

TEST_CASE("opaque trace data lives in side tables, and a Step is no larger for it", "[opaque][trace]")
{
    // Measured at the branch point, d09657e: 1008 bytes on cl 19.51 (MSVC STL,
    // release) and on g++ 13.3 and clang++ 20.1.8 (libstdc++), 1048 on cl 19.51
    // debug, whose checked iterators make each of Step's five vectors 32 bytes
    // rather than 24. Less those five vectors it is 888 in every one. A field
    // added to Step for an opaque step, or later a retry's, fails this. Not
    // measured on libc++.
    STATIC_REQUIRE(sizeof(formula::Step<formula::Rational>) - 5 * sizeof(std::vector<std::size_t>) == 888);
}