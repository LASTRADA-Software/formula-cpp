// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Reference
{
};
struct PriorTest
{
};

// Lineage attributes: the author's, never the library's.
struct MaterialBatch
{
};
struct TestMethod
{
};
struct CuringRegime
{
};

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

using Batch = formula::LineageEntry<MaterialBatch>;
using Method = formula::LineageEntry<TestMethod>;
using Regime = formula::LineageEntry<CuringRegime>;

/// This record: batch 4411, method 12, regime 7.
constexpr auto thisRecord()
{
    return formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                                formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12),
                                                formula::lineage<CuringRegime>(7));
}

/// The reference record, with the three lineage entries given.
constexpr auto referenceRecord(Batch batch, Method method, Regime regime)
{
    return formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there, batch,
                                      method, regime);
}

/// The shared fixture's context, with the reference's lineage keys set to
/// the three arguments and this record's to 4411, 12 and 7.
constexpr auto contextWith(std::uint64_t batch, std::uint64_t method, std::uint64_t regime)
{
    return formula::record_context(thisRecord(),
                                   referenceRecord(formula::lineage<MaterialBatch>(batch),
                                                   formula::lineage<TestMethod>(method),
                                                   formula::lineage<CuringRegime>(regime)));
}

/// The reference's batch is not known; the rest agree.
constexpr auto contextWithUnknownBatch()
{
    return formula::record_context(thisRecord(),
                                   referenceRecord(formula::unknown_lineage<MaterialBatch>(),
                                                   formula::lineage<TestMethod>(12), formula::lineage<CuringRegime>(7)));
}

/// The reference's batch unknown AND its method different: one attribute
/// not checked and one violated.
constexpr auto contextWithUnknownBatchAndOtherMethod()
{
    return formula::record_context(thisRecord(),
                                   referenceRecord(formula::unknown_lineage<MaterialBatch>(),
                                                   formula::lineage<TestMethod>(13), formula::lineage<CuringRegime>(7)));
}

/// Renders a trace of @p expression over @p context.
template <typename Expression, typename Context>
std::string traced(Expression const& expression, Context const& context, formula::Trace<>& trace)
{
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(expression, context, sink);
    return formula::render_trace(trace, { .maxSteps = 20 });
}
} // namespace

TEST_CASE("a lineage requirement gates the read, attribute by attribute", "[lineage]")
{
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());

    // Same everywhere: the reference's 60 000 N comes through.
    constexpr auto agreeing = formula::checked_evaluate_si<formula::Rational>(gated, contextWith(4411, 12, 7));
    STATIC_REQUIRE(**agreeing == formula::Rational { 60'000 });

    // The MIDDLE attribute differs (method 12 here, 13 there). A check of the
    // first attribute only, or of the last only, would let this through.
    constexpr auto refused = formula::checked_evaluate_si<formula::Rational>(gated, contextWith(4411, 13, 7));
    STATIC_REQUIRE(!refused.has_value());
    STATIC_REQUIRE(refused.error() == formula::ArithmeticError::DomainError);

    // Unknown batch there: no answer, not a refusal and not a pass.
    constexpr auto unknown = formula::checked_evaluate_si<formula::Rational>(gated, contextWithUnknownBatch());
    STATIC_REQUIRE(unknown.has_value());
    STATIC_REQUIRE(!unknown->has_value());
}

TEST_CASE("a violated attribute refuses the read whatever order it is checked in", "[lineage-order]")
{
    // One attribute violated and one not checked, in both orders: violated
    // wins either way. A decision taken on the first attribute alone, or on
    // the last, would give absent for one of the two orders.
    constexpr auto batchFirst =
        formula::from_record<Reference>(var<Force>, formula::same_lineage<MaterialBatch, TestMethod>());
    constexpr auto methodFirst =
        formula::from_record<Reference>(var<Force>, formula::same_lineage<TestMethod, MaterialBatch>());
    constexpr auto context = contextWithUnknownBatchAndOtherMethod();
    constexpr auto viaBatchFirst = formula::checked_evaluate_si<formula::Rational>(batchFirst, context);
    constexpr auto viaMethodFirst = formula::checked_evaluate_si<formula::Rational>(methodFirst, context);
    STATIC_REQUIRE(!viaBatchFirst.has_value());
    STATIC_REQUIRE(viaBatchFirst.error() == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(!viaMethodFirst.has_value());
    STATIC_REQUIRE(viaMethodFirst.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("every attribute checked appears in the trace, in declared order", "[lineage-trace]")
{
    // Review Focus 4: the mismatch is in attribute 2 of 3. Three attribute
    // lines, satisfied / violated / satisfied, each with both keys; they are
    // the scope's first operands; and the operand is never read.
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    formula::Trace<> trace {};
    std::string const text = traced(gated, contextWith(4411, 13, 7), trace);
    INFO(text);
    CHECK(text
          == "1. same MaterialBatch as this record: 4411 for this record, 4411 for Reference, satisfied\n"
             "2. same TestMethod as this record: 12 for this record, 13 for Reference, violated\n"
             "3. same CuringRegime as this record: 7 for this record, 7 for Reference, satisfied\n"
             "4. from record Reference (sample 23, test 3) = argument outside the domain of the operation\n");

    REQUIRE(trace.steps.size() == 4);
    formula::Step<> const& scope = trace.steps[3];
    CHECK(scope.kind == formula::StepKind::RecordScope);
    CHECK(scope.operands == std::vector<std::size_t> { 0, 1, 2 });
    formula::Step<> const& method = trace.steps[1];
    CHECK(method.kind == formula::StepKind::LineageChecked);
    REQUIRE(formula::lineage_of(trace, 1).has_value());
    CHECK(formula::lineage_of(trace, 1)->attribute() == "TestMethod");
    CHECK(formula::lineage_of(trace, 1)->is_against_this_record());
    CHECK(formula::lineage_of(trace, 1)->comparand_key() == std::uint64_t { 12 });
    CHECK(formula::lineage_of(trace, 1)->subject_key() == std::uint64_t { 13 });
    CHECK(method.outcome.is_violated());
    CHECK(trace.steps[0].outcome.is_satisfied());
    CHECK(trace.steps[2].outcome.is_satisfied());
    // Each attribute step is inside the scope, and says so.
    CHECK(formula::origin_of(trace, method).has_value());
}

TEST_CASE("every disagreeing attribute is shown, not only the first", "[lineage-trace]")
{
    // Two attributes disagree. A check that stopped at the first mismatch
    // would hide the second from the trace.
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    formula::Trace<> trace {};
    std::string const text = traced(gated, contextWith(4412, 13, 7), trace);
    INFO(text);
    CHECK(text.find("same MaterialBatch as this record: 4411 for this record, 4412 for Reference, violated") != std::string::npos);
    CHECK(text.find("same TestMethod as this record: 12 for this record, 13 for Reference, violated") != std::string::npos);
    CHECK(text.find("same CuringRegime as this record: 7 for this record, 7 for Reference, satisfied") != std::string::npos);
}

TEST_CASE("an unknown lineage key is shown as unknown and not checked", "[lineage-trace]")
{
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    formula::Trace<> trace {};
    std::string const text = traced(gated, contextWithUnknownBatch(), trace);
    INFO(text);
    CHECK(text.find("1. same MaterialBatch as this record: 4411 for this record, unknown for Reference, not checked\n") != std::string::npos);
    // Nothing was read, so the scope does not say "(not measured)" of a
    // record whose values may well have been measured.
    CHECK(text.find("4. from record Reference (sample 23, test 3) = (not read: lineage not checked)\n")
          != std::string::npos);
}

TEST_CASE("a requirement against another role compares with that record", "[lineage-trace]")
{
    // Spec section 16.8's "its two inputs": neither need be this specimen.
    // The reference and the prior test agree on the batch (4411) though this
    // record's differs (4412), so a comparison against this record would
    // refuse where one against the prior test passes.
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4412)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                                   formula::lineage<MaterialBatch>(4411)),
        formula::record<PriorTest>(formula::record_key(formula::sample_id(17), formula::test_id(3)), there,
                                   formula::lineage<MaterialBatch>(4411)));
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch>(formula::against<PriorTest>));
    formula::Trace<> trace {};
    std::string const text = traced(gated, context, trace);
    INFO(text);
    CHECK(text.find("1. same MaterialBatch as PriorTest (sample 17, test 3): 4411 for PriorTest, 4411 for Reference, "
                    "satisfied\n")
          != std::string::npos);
    CHECK(text.find("= 60000 N\n") != std::string::npos);
    REQUIRE(formula::lineage_of(trace, 0).has_value());
    CHECK(!formula::lineage_of(trace, 0)->is_against_this_record());
    CHECK(formula::lineage_of(trace, 0)->comparand() == "PriorTest");
}

TEST_CASE("a comparison with another role names the record that played it, and whose key is whose", "[lineage-trace]")
{
    // The final review's M1: the prior test's batch, 4412, is a value read
    // from the record playing PriorTest -- sample 17, test 3 -- and the line
    // says so, with each key named by whose it is rather than by its place.
    // The attribute step is inside the reference's scope and stamped with
    // it; the comparison names the other record itself.
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4411)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                                   formula::lineage<MaterialBatch>(4411)),
        formula::record<PriorTest>(formula::record_key(formula::sample_id(17), formula::test_id(3)), there,
                                   formula::lineage<MaterialBatch>(4412)));
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch>(formula::against<PriorTest>));
    formula::Trace<> trace {};
    std::string const text = traced(gated, context, trace);
    INFO(text);
    CHECK(text.find("1. same MaterialBatch as PriorTest (sample 17, test 3): 4412 for PriorTest, 4411 for Reference, "
                    "violated\n")
          != std::string::npos);
    REQUIRE(formula::lineage_of(trace, 0).has_value());
    formula::LineageCheck const compared = *formula::lineage_of(trace, 0);
    CHECK(compared.subject() == "Reference");
    CHECK(compared.comparand_record().role() == "PriorTest");
    CHECK(compared.comparand_record().key()
          == formula::record_key(formula::sample_id(17), formula::test_id(3)));
    CHECK(compared.comparand_key() == std::uint64_t { 4412 });
    CHECK(compared.subject_key() == std::uint64_t { 4411 });
    REQUIRE(formula::origin_of(trace, trace.steps[0]).has_value());
    CHECK(formula::origin_of(trace, trace.steps[0])->role() == "Reference");
}

TEST_CASE("a scope over a record not yet made checks no lineage", "[lineage-trace]")
{
    // X9: nothing was read, so nothing is compared: no attribute steps, and
    // the scope is absent -- neither violated nor satisfied.
    auto const context = formula::record_context(
        thisRecord(), formula::Record<Reference, std::remove_cv_t<decltype(there)>, Batch, Method, Regime>::unbound());
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    formula::Trace<> trace {};
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(gated, context, formula::RecordingSink { trace });
    REQUIRE(evaluated.has_value());
    CHECK(!evaluated->has_value());
    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].kind == formula::StepKind::RecordScope);
}

TEST_CASE("checked_explain returns a refused read together with its trace", "[lineage-explain]")
{
    // explain() throws on a refusal and keeps no trace. checked_explain keeps
    // the derivation that led to the refusal, so the violated attribute that
    // refused the read is there to read.
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    auto const refused = formula::checked_explain<Force>(gated, contextWith(4411, 13, 7));
    REQUIRE(!refused.has_value());
    CHECK(refused.error().error == formula::ArithmeticError::DomainError);
    std::string const text = formula::render_trace(refused.error().trace, { .maxSteps = 20 });
    INFO(text);
    CHECK(text.find("same TestMethod as this record: 12 for this record, 13 for Reference, violated") != std::string::npos);

    auto const agreed = formula::checked_explain<Force>(gated, contextWith(4411, 12, 7));
    REQUIRE(agreed.has_value());
    CHECK(agreed->outcome.measurement() == formula::Measured<Force> { formula::Rational { 60'000 } });
    CHECK(agreed->trace.steps.size() == 5);
}

TEST_CASE("a comparison with a record not yet made is not checked, and reads nothing", "[lineage-trace]")
{
    // The compared record is unbound: its batch is not known, so the
    // attribute is not checked and the read is absent -- never read anyway,
    // and never refused. The read record itself is bound and agrees with
    // this one, so only the comparison with the unbound record decides.
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4411)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                                   formula::lineage<MaterialBatch>(4411)),
        formula::Record<PriorTest, std::remove_cv_t<decltype(there)>, Batch>::unbound());
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch>(formula::against<PriorTest>));
    formula::Trace<> trace {};
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(gated, context, formula::RecordingSink { trace });
    REQUIRE(evaluated.has_value());
    CHECK(!evaluated->has_value());
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    // No record played PriorTest, and the line says so: its `unknown` is not
    // a record that states no batch.
    CHECK(text.find("1. same MaterialBatch as PriorTest (no record bound): unknown for PriorTest, 4411 for Reference, "
                    "not checked\n")
          != std::string::npos);
    REQUIRE(formula::lineage_of(trace, 0).has_value());
    CHECK(!formula::lineage_of(trace, 0)->comparand_record().is_bound());
    CHECK(text.find("F =") == std::string::npos); // the operand was never read
}

TEST_CASE("a failed checked_explain always says which error it failed with", "[lineage-explain]")
{
    // A default-constructed failure would have to claim the enumeration's
    // first error, DivisionByZero, which nothing raised.
    STATIC_REQUIRE(!std::is_default_constructible_v<formula::CheckedExplainFailure<>>);
}

namespace
{
/// A lineage attribute whose published name holds the two characters of a
/// trace line's own punctuation a `TagName` may hold: `;`, which separates
/// clauses, and `\`, the escape itself. A bracket or a control character is
/// refused in any `TagName` spelling (`RequireTagNameSpelling`, `tag.hpp`),
/// so it cannot reach a trace through a tag at all.
struct PunctuatedLot
{
};
} // namespace

template <>
struct formula::TagName<PunctuatedLot>
{
    static constexpr std::string_view of() noexcept { return "lot; sealed\\B"; }
};

TEST_CASE("a lineage attribute's name is escaped in the trace, as other author text is", "[lineage-trace]")
{
    // Phase 11's trace escape (`escaped_author_text`), applied to role and
    // attribute names through `tag_words`. A role's name is identifier-like,
    // so only an attribute's can hold these; unescaped, ";" would read as the
    // start of a new clause, and a "\" before it as escaping it.
    constexpr auto punctuated = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<PunctuatedLot>(5)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                                   formula::lineage<PunctuatedLot>(5)));
    formula::Trace<> trace {};
    std::string const text =
        traced(formula::from_record<Reference>(var<Force>, formula::same_lineage<PunctuatedLot>()), punctuated, trace);
    INFO(text);
    CHECK(text.find("1. same lot\\; sealed\\\\B as this record: 5 for this record, 5 for Reference, satisfied\n") != std::string::npos);
    CHECK(text.find("lot; sealed") == std::string::npos);
}
