// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

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
struct Ratio: formula::Quantity<Ratio, "r", "a ratio", unit::One>
{
};

struct Reference
{
};
struct PriorTest
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 103 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 103 } });
// The cross-test fixture: the same sample as this record, another test.
constexpr auto prior = formula::environment(formula::Measured<Ratio> { formula::Rational { 1, 4 } });

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
    // records none on either variable, rather than guessing Measured. Whether
    // a computed step or the pending slot can carry a source is asked in the
    // first test, where a source is actually recorded; here none ever is, so
    // those checks could not fail.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(var<EdgeX> * var<EdgeY>, ConsumerEnvironment {}, sink);
    REQUIRE(evaluated.has_value());
    REQUIRE(trace.steps.size() == 3);
    CHECK(!trace.steps[0].inputSource.has_value());
    CHECK(!trace.steps[1].inputSource.has_value());
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

TEST_CASE("an input typed in but left empty says it was entered by hand as empty", "[record-trace]")
{
    // Absent is "(not measured)" for a measured input. For a typed-in one that
    // would be false -- it was never going to be measured -- and ", entered by
    // hand" beside it would contradict it. So an empty typed-in input reads
    // "(entered by hand as empty)". The step records where the entry came
    // from even though it holds no value: the absent path reports the source
    // as the present path does.
    constexpr auto blank = formula::environment(formula::entered(formula::Measured<EdgeX>::absent()),
                                                formula::Measured<EdgeY>::absent());
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<EdgeX> * var<EdgeY>, blank, sink);

    CHECK(trace.steps[0].inputSource == formula::ValueSource::ManuallyEntered);
    CHECK(trace.steps[1].inputSource == formula::ValueSource::Measured);
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    CHECK(text.find("1. x_m = (entered by hand as empty)\n") != std::string::npos);
    CHECK(text.find("2. y_m = (not measured)\n") != std::string::npos);
    CHECK(text.find(", entered by hand") == std::string::npos);
}

TEST_CASE("every value read from another record says which record", "[record-trace]")
{
    // The reference's force was copied in by hand from its certificate.
    constexpr auto thereEntered =
        formula::environment(formula::entered(formula::Measured<Force> { formula::Rational { 57'268 } }));
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), thereEntered),
        formula::record<PriorTest>(formula::record_key(formula::sample_id(17), formula::test_id(3)), prior));

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(
        var<Force> / formula::from_record<Reference>(var<Force>) + formula::from_record<PriorTest>(var<Ratio>),
        context, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(text.find("1. F = 85902 N\n") != std::string::npos); // this record: no origin at all
    CHECK(text.find("F = 57268 N, from record Reference (sample 23, test 3), entered by hand\n") != std::string::npos);
    // Same sample, another test: the test key is what tells the two apart.
    CHECK(text.find("from record PriorTest (sample 17, test 3)") != std::string::npos);
    CHECK(text.find("from record Reference (sample 23, test 3) = ") != std::string::npos); // the scope's own line
    CHECK(text.find('[') == std::string::npos); // the gallery puts traces in Markdown
    INFO(text);
    CHECK(text
          == "1. F = 85902 N\n"
             "2. F = 57268 N, from record Reference (sample 23, test 3), entered by hand\n"
             "3. #2 from record Reference (sample 23, test 3) = 57268 N\n"
             "4. #1 / #3 = 3/2\n"
             "5. r = 1/4, from record PriorTest (sample 17, test 3)\n"
             "6. #5 from record PriorTest (sample 17, test 3) = 1/4\n"
             "7. #4 + #6 = 7/4\n");
}

TEST_CASE("a scope over a record not yet made says no record is bound", "[record-trace]")
{
    // The trace half: the scope's own line says so, it has no
    // operands -- nothing was read -- and no line names a sample.
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::Record<Reference, std::remove_cv_t<decltype(there)>>::unbound());
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<Force> / formula::from_record<Reference>(var<Force>),
                                                           context, sink);

    REQUIRE(trace.steps.size() == 3);
    formula::Step<> const& scope = trace.steps[1];
    CHECK(scope.kind == formula::StepKind::RecordScope);
    CHECK(scope.operands.empty());
    REQUIRE(formula::origin_of(trace, scope).has_value());
    CHECK(!formula::origin_of(trace, scope)->is_bound());
    CHECK(!formula::origin_of(trace, scope)->key().has_value());
    CHECK(formula::origin_of(trace, scope)->role() == "Reference");
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    CHECK(text.find("2. from record Reference (no record bound) = (not measured)\n") != std::string::npos);
    CHECK(text.find("sample") == std::string::npos);
}

TEST_CASE("every step inside a scope carries its origin, and none outside it does", "[record-trace]")
{
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    // One local variable before the scope and one after it, so that a stamp
    // leaking either way is seen.
    (void) formula::checked_evaluate_si<formula::Rational>(
        var<EdgeX> * formula::from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>)) * var<EdgeY>, context,
        sink);

    // 0: x_m (here); 1-5: F, x_m, y_m, x*y, F/(x*y) (Reference); 6: the scope;
    // 7: x_m * scope; 8: y_m (here); 9: the product.
    REQUIRE(trace.steps.size() == 10);
    for (std::size_t inside = 1; inside <= 6; ++inside)
    {
        INFO("step " << inside);
        REQUIRE(formula::origin_of(trace, trace.steps[inside]).has_value());
        CHECK(formula::origin_of(trace, trace.steps[inside])->role() == "Reference");
        CHECK(formula::origin_of(trace, trace.steps[inside])->key()
              == formula::record_key(formula::sample_id(23), formula::test_id(3)));
    }
    CHECK(trace.steps[6].kind == formula::StepKind::RecordScope);
    for (std::size_t outside: { std::size_t { 0 }, std::size_t { 7 }, std::size_t { 8 }, std::size_t { 9 } })
    {
        INFO("step " << outside);
        CHECK(!formula::origin_of(trace, trace.steps[outside]).has_value());
    }
    CHECK(trace.recordStack.empty());
}

namespace
{
/// A sink of a consumer's own, defining only the two hooks every sink must.
struct CountingSink
{
    int* nodes;

    template <formula::Node N>
    void entered(N const&) const
    {
    }
    template <formula::Node N>
    void produced(N const&, formula::Evaluated<formula::Rational> const&) const
    {
        ++*nodes;
    }
};
} // namespace

TEST_CASE("a sink of a consumer's own, with only entered and produced, evaluates a scope", "[record-trace]")
{
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    int nodes = 0;
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(
        var<Force> / formula::from_record<Reference>(var<Force>), context, CountingSink { &nodes });
    REQUIRE(evaluated.has_value());
    CHECK(**evaluated == formula::Rational { 3, 2 });
    CHECK(nodes == 4); // F here, F there, the scope, the division
}

namespace
{
inline constexpr formula::BandTable<2> EdgeBands { formula::band(0, 1, 127, 1), formula::band(127, 1, 197, 1) };
} // namespace

TEST_CASE("a constant, a lookup and a conditional inside a scope are stamped too", "[record-trace]")
{
    // Every kind of step inside the scope says which record it belongs to,
    // not only the variables: a consumer grouping steps by record must find
    // the whole computation over the reference there.
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    constexpr auto inside = formula::from_record<Reference>(
        formula::when(var<Force> > formula::constant<unit::Newton>(formula::Rational { 1 }),
                      formula::constant<unit::Newton>(formula::Rational { 2 }),
                      formula::constant<unit::Newton>(formula::Rational { 3 }))
        * formula::banded_lookup<unit::Millimetre, EdgeBands, unit::One>(
            var<EdgeX>, { formula::Rational { 1 }, formula::Rational { 2 } }));
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(inside, context, sink);
    REQUIRE(evaluated.has_value());

    bool sawConstant = false;
    bool sawLookup = false;
    bool sawConditional = false;
    for (formula::Step<> const& recorded: trace.steps)
    {
        INFO("step kind " << static_cast<int>(recorded.kind));
        REQUIRE(formula::origin_of(trace, recorded).has_value());
        CHECK(formula::origin_of(trace, recorded)->role() == "Reference");
        sawConstant = sawConstant || recorded.kind == formula::StepKind::Constant;
        sawLookup = sawLookup || recorded.kind == formula::StepKind::BandedLookup;
        sawConditional = sawConditional || recorded.kind == formula::StepKind::Conditional;
    }
    CHECK(sawConstant);
    CHECK(sawLookup);
    CHECK(sawConditional);
}

TEST_CASE("a scope reported without its origin records none, and says so", "[record-trace]")
{
    // A consumer's own evaluator may report a scope node through a
    // RecordingSink without calling record_entered. There is then no origin
    // to stamp and none to pop: the recording must neither pop an empty stack
    // -- undefined behaviour, and an abort under a checked standard library
    // -- nor invent an origin, and the line says only that the value came
    // from another record.
    constexpr auto scope = formula::from_record<Reference>(formula::constant<unit::Newton>(formula::Rational { 5 }));
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    sink.entered(scope);
    (void) formula::checked_evaluate_si<formula::Rational>(scope.operand, here, sink);
    sink.produced(scope,
                  formula::Evaluated<formula::Rational> { std::optional<formula::Rational> { formula::Rational { 5 } } });

    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::RecordScope);
    CHECK(!formula::origin_of(trace, trace.steps[1]).has_value());
    CHECK(trace.recordStack.empty());
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    CHECK(text.find("2. #1 from another record = ") != std::string::npos);
}

TEST_CASE("a step costs what it cost before records were traced", "[record-trace]")
{
    // 1608 bytes, measured on cl and clang-cl (release, /MD) and on g++ 14 and
    // clang++ 20 with libstdc++, all 64-bit. It was 1320 until a unit symbol
    // held 32 bytes and a unit gained an ASCII key: a `Symbol` grew from 16
    // bytes to 32, so a `Dimension`'s four named-base symbols grew by 64 bytes,
    // from 152 to 216, and a `Unit` by 112, from 248 to 360 -- 64 in its
    // dimension, 16 in its symbol and 32 for its new key. One dimension and two
    // units make 288 bytes more per step. The `BoundsEnd` flags that replaced a
    // unit's one `bool` of declared bounds are a byte each and fit the padding
    // that `bool` left, so `Bounds` stays 40 bytes. Before that it was 1296,
    // until `Rational` held its numerator and denominator in 128 bits: a
    // `Rational` grew from 16 bytes to 32, and the one a step holds inline, its
    // `std::optional` value, from 24 to 40 -- 16 bytes more -- and
    // `lookupKeyHigh`, a count's top 64 bits, adds 8 more after
    // `lookupKeyIsSigned`. A step's band, segment and covered range keep their
    // declared 64-bit numerators and denominators, and its other values live
    // in its vectors, so neither grew. Before that it was
    // 1008, until a `Dimension` could carry named base dimensions and a step's
    // one dimension and two units grew by 96 bytes each. Records' per-step
    // facts -- the source, whether a replaced entry was empty, and the record's
    // number -- still fit in padding `Step` already had; each origin and each
    // lineage comparison lives once, in the trace's side tables. A checked
    // standard library's containers are larger, and so is every step there, so
    // only an unchecked 64-bit build pins the number. The optional value's 40
    // bytes depend on no container, so they are pinned on every build.
    static_assert(sizeof(std::optional<formula::Rational>) == 40);
#if (defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0) || defined(_GLIBCXX_DEBUG)
    SUCCEED("a checked standard library's containers change every step's size, so nothing is pinned here");
#else
    STATIC_REQUIRE((sizeof(void*) != 8 || sizeof(formula::Step<formula::Rational>) == 1608));
#endif
}
