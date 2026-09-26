// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};

/// An environment of a consumer's own: it answers `get<Q>()`, which is all
/// the variable evaluator reads, and declares no `is_entered`. The evaluator
/// must still compile against it, and record no source it was never told.
struct ConsumerEnvironment
{
    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        return formula::Measured<Q> { formula::Rational { 139 } };
    }
};
} // namespace

TEST_CASE("the trace says which input was typed in and which was measured", "[record-trace]")
{
    constexpr auto environment =
        formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                             formula::entered(formula::Measured<EdgeX> { formula::Rational { 139 } }),
                             formula::Measured<EdgeY> { formula::Rational { 103 } });
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<Force> / (var<EdgeX> * var<EdgeY>), environment, sink);

    // Programmatic first: the step, not the line, is the record.
    CHECK(trace.steps[0].inputSource == formula::ValueSource::Measured);
    CHECK(trace.steps[1].inputSource == formula::ValueSource::ManuallyEntered);
    // The third input, measured after a typed-in one, is not taken for typed
    // in; the computed steps read no input and carry no source; and nothing
    // is left pending once the walk is over.
    CHECK(trace.steps[2].inputSource == formula::ValueSource::Measured);
    CHECK(!trace.steps[3].inputSource.has_value());
    CHECK(!trace.steps[4].inputSource.has_value());
    CHECK(!trace.pendingInputSource.has_value());

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    CHECK(text.find("x_m = 139 mm, entered by hand\n") != std::string::npos);
    CHECK(text.find("F = 85902 N\n") != std::string::npos); // measured: no suffix
}

TEST_CASE("an environment without is_entered records no input source", "[record-trace]")
{
    // Compiling at all is half the test: the evaluator asks for the source
    // only when the environment can answer. The other half is that it then
    // records none, rather than guessing Measured; and that computed steps
    // never carry one.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(var<EdgeX> * var<EdgeY>, ConsumerEnvironment {}, sink);
    REQUIRE(evaluated.has_value());
    REQUIRE(trace.steps.size() == 3);
    CHECK(!trace.steps[0].inputSource.has_value());
    CHECK(!trace.steps[1].inputSource.has_value());
    CHECK(!trace.steps[2].inputSource.has_value());
    CHECK(!trace.pendingInputSource.has_value());
}

TEST_CASE("a source stated by hand outside a variable's own recording is not recorded", "[record-trace]")
{
    // `input_source` is public, because the evaluator is not the sink's
    // friend. Called before a walk, it must not reach the first variable
    // recorded: here that variable is read from an environment that cannot
    // say where its value came from, so any source on its step would be the
    // one stated by hand.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    sink.input_source(var<EdgeX>, formula::ValueSource::ManuallyEntered);
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(var<EdgeX>, ConsumerEnvironment {}, sink);
    REQUIRE(evaluated.has_value());
    REQUIRE(trace.steps.size() == 1);
    CHECK(!trace.steps[0].inputSource.has_value());
}
