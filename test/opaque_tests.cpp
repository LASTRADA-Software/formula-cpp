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
#include <utility>
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

    // With the shift present: 103 + 163 = 266 g. The inputs arrive in the
    // declared order, series then value.
    auto const shifted = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                                formula::Measured<Reading> { rat(103) },
                                                                                formula::Measured<Reading> { rat(191) },
                                                                                formula::Measured<Reading> { rat(139) }),
                                              formula::Measured<Shift> { rat(163) });
    auto const raised = formula::checked_evaluate<Lowest>(formula::opaque_output<"shifted">(shiftedCall), shifted);
    REQUIRE(raised.has_value());
    CHECK(raised->measurement().value() == rat(266));
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
                                              formula::entered(formula::Measured<Spread> { rat(197) }));
    auto const outcome = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), typedIn);
    REQUIRE(outcome.has_value());
    CHECK(outcome->measurement().value() == rat(197));
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

// Converts to any field type, so that brace-initialising an aggregate with N
// of them compiles exactly when it has at least N fields. Outside the
// anonymous namespace: its conversion is declared and never defined, which
// clang's -Wundefined-internal refuses for an internal-linkage function.
namespace field_probe
{
struct AnyField
{
    template <typename T>
    operator T() const;
};

template <typename T, std::size_t... I>
consteval bool brace_init_with(std::index_sequence<I...>)
{
    return requires { T { (void(I), AnyField {})... }; };
}

template <typename T, std::size_t N = 0>
consteval std::size_t field_count()
{
    if constexpr (brace_init_with<T>(std::make_index_sequence<N + 1> {}))
        return field_count<T, N + 1>();
    else
        return N;
}
} // namespace field_probe

TEST_CASE("opaque trace data lives in side tables, and a Step has no more fields for it", "[opaque][trace]")
{
    // Counted, not measured: Step has 45 fields at the branch point, 9d3cdd4.
    // A byte-sized field -- an enum or a flag, the likeliest slip for an
    // opaque step's failure or a retry's -- can land in padding and leave
    // sizeof unchanged (it did, on g++-14 and on libc++); it cannot leave the
    // count unchanged. Any field added to Step, on any library, fails this.
    STATIC_REQUIRE(field_probe::field_count<formula::Step<formula::Rational>>() == 45);
}
namespace
{
// Two outputs of two dimensions: the total of a series, a mass, and the ratio
// of its highest element to its lowest, a pure number. An output that took
// another's dimension -- or the first output's -- is told apart by both the
// type and the value.
struct TotalAndRatio
{
    static constexpr std::string_view name = "total and ratio";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "total", "ratio" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], formula::dim::Scalar };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> values) noexcept
    {
        Rep total = values[0];
        Rep least = values[0];
        Rep most = values[0];
        for (std::size_t at = 1; at < values.size(); ++at)
        {
            std::expected<Rep, formula::ArithmeticError> const added = formula::RepTraits<Rep>::add(total, values[at]);
            if (!added.has_value())
                return std::unexpected { added.error() };
            total = *added;
            if (values[at] < least)
                least = values[at];
            if (most < values[at])
                most = values[at];
        }
        std::expected<Rep, formula::ArithmeticError> const ratio = formula::RepTraits<Rep>::divide(most, least);
        if (!ratio.has_value())
            return std::unexpected { ratio.error() };
        return std::array { total, *ratio };
    }
};

constexpr auto totalAndRatio =
    formula::opaque<TotalAndRatio>({ .reference = "Example Standard 12", .section = "4.6" }, formula::series<Reading, 4>);

struct Spot: formula::Quantity<Spot, "d_s", "an invented measured opening", unit::Millimetre>
{
};

struct EndPoint
{
    static constexpr std::string_view name = "end point";
    static constexpr std::array shapes { formula::InputShape::Curve };
    static constexpr std::array<std::string_view, 1> outputs { "last" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[1] };
    }

    static inline int calls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> points, std::span<Rep const> pointValues) noexcept
    {
        if !consteval
        {
            ++calls;
        }
        (void) points;
        return std::array { pointValues.back() };
    }
};
} // namespace

TEST_CASE("each output has its own declared dimension, not the first output's", "[opaque]")
{
    using Total = decltype(formula::opaque_output<"total">(totalAndRatio));
    using Ratio = decltype(formula::opaque_output<"ratio">(totalAndRatio));
    STATIC_REQUIRE(Total::dimension == formula::dim::Mass);
    STATIC_REQUIRE(Ratio::dimension == formula::dim::Scalar);
    // 127 + 103 + 191 + 139 = 560 g; 191/103.
    constexpr auto total = formula::checked_evaluate<Lowest>(formula::opaque_output<"total">(totalAndRatio), readings);
    STATIC_REQUIRE(total->measurement().value() == rat(560));
    constexpr auto ratio = formula::checked_evaluate<Level>(formula::opaque_output<"ratio">(totalAndRatio), readings);
    STATIC_REQUIRE(ratio->measurement().value() == rat(191, 103));
}

TEST_CASE("an absent curve point makes the call absent, and compute is never called", "[opaque]")
{
    // A domain that is itself measured: point 2 of 3 absent. The curve keeps
    // it absent (a gap does not stop the other points ascending), and the call
    // is then absent -- compute never sees a point that is not there.
    EndPoint::calls = 0;
    constexpr auto lastCall = formula::opaque<EndPoint>({ .reference = "Example Standard 12" },
                                                        formula::curve(formula::series<Spot, 3>, formula::series<Load, 3>));
    auto const gap = formula::environment(formula::measured_series<Spot>(formula::Measured<Spot> { rat(103) },
                                                                         formula::Measured<Spot>::absent(),
                                                                         formula::Measured<Spot> { rat(163) }),
                                          formula::measured_series<Load>(formula::Measured<Load> { rat(139) },
                                                                         formula::Measured<Load> { rat(163) },
                                                                         formula::Measured<Load> { rat(241) }));
    auto const called = formula::detail::evaluate_call<formula::Rational>(lastCall, gap, formula::NullSink {});
    REQUIRE(called.has_value());
    CHECK(!called->has_value());
    CHECK(EndPoint::calls == 0);

    // The control: every point present, and compute is called.
    auto const whole = formula::environment(formula::measured_series<Spot>(formula::Measured<Spot> { rat(103) },
                                                                           formula::Measured<Spot> { rat(127) },
                                                                           formula::Measured<Spot> { rat(163) }),
                                            formula::measured_series<Load>(formula::Measured<Load> { rat(139) },
                                                                           formula::Measured<Load> { rat(163) },
                                                                           formula::Measured<Load> { rat(241) }));
    auto const answered = formula::detail::evaluate_call<formula::Rational>(lastCall, whole, formula::NullSink {});
    REQUIRE(answered.has_value());
    REQUIRE(answered->has_value());
    CHECK((**answered)[0] == rat(241));
    CHECK(EndPoint::calls == 1);
}

namespace
{
struct Warmth: formula::Quantity<Warmth, "t_w", "an invented temperature", unit::Celsius>
{
};
struct Share: formula::Quantity<Share, "s_r", "an invented share", unit::Percent>
{
};

// Forwards every node and series hook to a RecordingSink, and nothing of an
// opaque call: a sink written before opaque operations existed.
struct WithoutOpaqueHooks
{
    formula::RecordingSink<formula::Rational> inner;

    template <formula::Node N>
    void entered(N const& node)
    {
        inner.entered(node);
    }
    template <formula::Node N, typename V>
    void produced(N const& node, V const& value)
    {
        inner.produced(node, value);
    }
    template <typename S>
    void series_entered(S const& node)
    {
        inner.series_entered(node);
    }
    template <typename S, typename E>
    void series_produced(S const& node, E const& evaluated)
    {
        inner.series_produced(node, evaluated);
    }
};
} // namespace

TEST_CASE("an opaque output of a Celsius input reads in kelvin, not in the input's offset unit", "[opaque][trace]")
{
    // 19.7, 43.1 and 31.3 degC: the span is 23.4 K. Shown in degC it would read
    // as a reading of 23.4 degC, 273.15 off; borrowed without its offset it
    // would be a difference printed as a temperature.
    constexpr auto warmSpan = formula::opaque_output<"span">(
        formula::opaque<SeriesSpan>({ .reference = "Example Standard 12" }, formula::series<Warmth, 3>));
    constexpr auto warm = formula::environment(formula::measured_series<Warmth>(formula::Measured<Warmth> { rat(197, 10) },
                                                                                formula::Measured<Warmth> { rat(431, 10) },
                                                                                formula::Measured<Warmth> { rat(313, 10) }));
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(warmSpan, warm, formula::RecordingSink { recorded });
    REQUIRE(formula::opaque_data(recorded, 1) != nullptr);
    CHECK(formula::opaque_data(recorded, 1)->outputs[2].unit.offsetNumerator == 0);
    CHECK(recorded.steps[2].unit.offsetNumerator == 0);
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. t_w = 197/10 °C; 431/10 °C; 313/10 °C\n"
             "2. series span(#1) = lowest = 5857/20 K; highest = 1265/4 K; span = 117/5 K [inside not shown] "
             "[Example Standard 12]\n"
             "3. span of #2 = 117/5 K\n");
}

TEST_CASE("a dimensionless opaque output does not borrow a dimensionless input's unit", "[opaque][trace]")
{
    // Shares of 12.7, 10.3, 19.1 and 13.9 %: their ratio, 191/103, is a pure
    // number and no percentage.
    constexpr auto shareRatio = formula::opaque_output<"ratio">(
        formula::opaque<RelativeSpread>({ .reference = "Example Standard 12" }, formula::series<Share, 4>));
    constexpr auto shares = formula::environment(formula::measured_series<Share>(formula::Measured<Share> { rat(127, 10) },
                                                                                 formula::Measured<Share> { rat(103, 10) },
                                                                                 formula::Measured<Share> { rat(191, 10) },
                                                                                 formula::Measured<Share> { rat(139, 10) }));
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(shareRatio, shares, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 20 });
    CHECK(text
          == "1. s_r = 127/10 %; 103/10 %; 191/10 %; 139/10 %\n"
             "2. relative spread(#1) = ratio = 191/103 [inside not shown] [Example Standard 12]\n"
             "3. ratio of #2 = 191/103\n");
}

TEST_CASE("an opaque call over a curve reads its output in the coherent unit, spelt out", "[opaque][trace]")
{
    constexpr formula::BreakpointTable<3> openings { formula::breakpoint(103),
                                                     formula::breakpoint(127),
                                                     formula::breakpoint(163) };
    constexpr auto slopeOutput = formula::opaque_output<"slope">(formula::opaque<EndToEndSlope>(
        { .reference = "Example Standard 12", .section = "4.5" },
        formula::curve(formula::domain<unit::Millimetre, openings>, formula::series<Load, 3>)));
    constexpr auto held = formula::environment(formula::measured_series<Load>(
        formula::Measured<Load> { rat(139) }, formula::Measured<Load> { rat(163) }, formula::Measured<Load> { rat(241) }));
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(slopeOutput, held, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. 103 mm; 127 mm; 163 mm\n"
             "2. F_h = 139 N; 163 N; 241 N\n"
             "3. curve(#1, #2) = 103 mm: 139 N; 127 mm: 163 N; 163 mm: 241 N\n"
             "4. end to end slope(#3) = slope = 17/10 N/mm [inside not shown] [Example Standard 12, 4.5]\n"
             "5. slope of #4 = 17/10 N/mm\n");
}

TEST_CASE("an opaque output recorded without its call's step still says the inside is not shown", "[opaque][trace]")
{
    // The sink hears nothing of the call, so the output's step sits straight
    // over the input's: without the marker it would read as the readings
    // passed on unchanged.
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"span">(span_call), readings, WithoutOpaqueHooks { formula::RecordingSink { recorded } });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. r = 127 g; 103 g; 191 g; 139 g\n"
             "2. output of #1 = 11/125 kg [inside not shown]\n");

    // And a step built by hand, over nothing at all.
    formula::Step<> bare {};
    bare.kind = formula::StepKind::OpaqueOutput;
    formula::Trace<> handBuilt {};
    handBuilt.steps.push_back(bare);
    CHECK(formula::render_trace(handBuilt, { .maxSteps = 5 })
          == "1. an opaque output = (not measured) [inside not shown]\n");
}

TEST_CASE("a relayed failure names the one input that failed, not every input", "[opaque][trace]")
{
    constexpr auto shifted = formula::opaque_output<"shifted">(formula::opaque<ShiftedLowest>(
        { .reference = "Example Standard 12" }, formula::series<Reading, 4>, formula::var<Shift> / formula::var<Divisor>));
    auto const zero = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                                             formula::Measured<Reading> { rat(103) },
                                                                             formula::Measured<Reading> { rat(191) },
                                                                             formula::Measured<Reading> { rat(139) }),
                                           formula::Measured<Shift> { rat(163) },
                                           formula::Measured<Divisor> { rat(0) });
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(shifted, zero, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. r = 127 g; 103 g; 191 g; 139 g\n"
             "2. r_0 = 163 g\n"
             "3. q = 0\n"
             "4. #2 / #3 = division by zero\n"
             "5. shifted lowest(#1, #4) = division by zero [inside not shown] [carried up from #4] "
             "[Example Standard 12]\n"
             "6. shifted of #5 = division by zero\n");
}

TEST_CASE("an opaque call built as an aggregate, citing nothing, says so", "[opaque][trace]")
{
    using Readings = std::remove_cv_t<decltype(formula::series<Reading, 4>)>;
    constexpr auto aggregateSpan = formula::opaque_output<"span">(
        formula::OpaqueCall<SeriesSpan, Readings> { std::tuple<Readings> { formula::series<Reading, 4> }, {} });
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(aggregateSpan, readings, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. r = 127 g; 103 g; 191 g; 139 g\n"
             "2. series span(#1) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] "
             "(no citation given)\n"
             "3. span of #2 = 88 g\n");
}

TEST_CASE("a page lists one opaque operation twice when two calls cite different standards", "[opaque][document]")
{
    constexpr auto elsewhere =
        formula::opaque<SeriesSpan>({ .reference = "Example Standard 3" }, formula::series<Reading, 4>);
    constexpr auto both = formula::opaque_output<"span">(span_call) + formula::opaque_output<"lowest">(elsewhere);
    formula::Documentation const page = formula::document(both);
    REQUIRE(page.opaqueOperations.size() == 2);
    CHECK(page.opaqueOperations[0].citation.reference == "Example Standard 12");
    CHECK(page.opaqueOperations[1].citation.reference == "Example Standard 3");
    CHECK(page.citations.size() == 2);
    CHECK(page.opaqueOperations[0].outputDimensions
          == std::vector<formula::Dimension> { formula::dim::Mass, formula::dim::Mass, formula::dim::Mass });
}

TEST_CASE("the coherent unit is spelt from its base units", "[opaque][trace]")
{
    CHECK(formula::detail::coherent_unit_text(formula::dim::Scalar).empty());
    CHECK(formula::detail::coherent_unit_text(formula::dim::Mass) == "kg");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Velocity) == "m/s");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Frequency) == "1/s");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Density) == "kg/m^3");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Pressure) == "kg/(m s^2)");
    CHECK(formula::detail::coherent_unit_text(formula::Dimension { .length = formula::exponent(1, 2) }) == "m^(1/2)");
}

TEST_CASE("an output's marker is judged by its operand step's kind, not by a row", "[opaque][trace]")
{
    // A call's step and its output's, built by hand with no side tables: the
    // call's line says the inside is not shown, so the output's does not
    // repeat it -- and says it again when its operand is some other step.
    formula::Step<> call {};
    call.kind = formula::StepKind::OpaqueOperation;
    formula::Step<> output {};
    output.kind = formula::StepKind::OpaqueOutput;
    output.operands = { 0 };
    formula::Trace<> overCall {};
    overCall.steps = { call, output };
    CHECK(formula::render_trace(overCall, { .maxSteps = 5 })
          == "1. opaque() = (not measured) [inside not shown] (no citation given)\n"
             "2. output of #1 = (not measured)\n");

    formula::Step<> reading {};
    reading.kind = formula::StepKind::Constant;
    formula::Trace<> overReading {};
    overReading.steps = { reading, output };
    CHECK(formula::render_trace(overReading, { .maxSteps = 5 })
              .ends_with("2. output of #1 = (not measured) [inside not shown]\n"));
}

namespace
{
struct Gap: formula::Quantity<Gap, "g_p", "an invented gap", unit::Millimetre>
{
};
struct Viscosity: formula::Quantity<Viscosity, "eta_i", "an invented viscosity", unit::MillipascalSecond>
{
};
struct Portion: formula::Quantity<Portion, "p_s", "an invented portion", unit::Percent>
{
};

// The first input over the second, in whatever dimensions they have.
struct FirstOverSecond
{
    static constexpr std::string_view name = "first over second";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "quotient" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] / declared[1] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep over, Rep under) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const quotient = formula::RepTraits<Rep>::divide(over, under);
        if (!quotient.has_value())
            return std::unexpected { quotient.error() };
        return std::array { *quotient };
    }
};
} // namespace

TEST_CASE("a borrowed quotient brackets a denominator of more than one unit word", "[opaque][trace]")
{
    // 1.27 mm over 2.41 mPa.s: 127/241 mm/(mPa.s). Unbracketed, mm/mPa.s would
    // read (mm/mPa) s, another dimension.
    constexpr auto fluidity =
        formula::opaque_output<"quotient">(formula::opaque<FirstOverSecond>({}, formula::var<Gap>, formula::var<Viscosity>));
    constexpr auto specimen =
        formula::environment(formula::Measured<Gap> { rat(127, 100) }, formula::Measured<Viscosity> { rat(241, 100) });
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(fluidity, specimen, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 10 }).ends_with("4. quotient of #3 = 127/241 mm/(mPa.s)\n"));
}

TEST_CASE("a quotient never borrows a dimensionless unit", "[opaque][trace]")
{
    // 12.7 % over 1.03 mm is a per-length, in no percentage: 12700/103 1/m in
    // the coherent unit, never 1270/103 %/mm.
    constexpr auto perGap =
        formula::opaque_output<"quotient">(formula::opaque<FirstOverSecond>({}, formula::var<Portion>, formula::var<Gap>));
    constexpr auto specimen =
        formula::environment(formula::Measured<Portion> { rat(127, 10) }, formula::Measured<Gap> { rat(103, 100) });
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(perGap, specimen, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 10 }).ends_with("4. quotient of #3 = 12700/103 1/m\n"));
}