// SPDX-License-Identifier: Apache-2.0
//
// Other samples and other tests: a formula that reads a value from a record
// other than the specimen being evaluated -- a reference sample, a prior test
// of the same sample -- and says in its trace which record every value came
// from.
//
//   1. A role is code and a record is data. The formula names a role,
//      `Reference`; which sample and test plays it is decided at run time. The
//      context holding the records is this record's environment, so
//      everything that takes an environment takes it.
//   2. A scope reads one value from another record, or computes over its
//      measurements -- a series among them, reduced inside the scope. The
//      page says whose values a sub-derivation uses, and so does every line
//      of the trace -- including a reference value a person typed in.
//   3. Lineage is a gate: a read that requires the two records to share a
//      batch and a method gives a value, a refusal, or no answer, and the
//      trace says which attribute decided.
//   4. A record not yet made gives no answer, never zero.
//   5. A role's name is written into formulas, so it must read as a name.
//
// Every number here is invented, as in every other example in this
// repository; nothing here cites a standard.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- Roles: which record a formula reads from --------------------------------
//
// A role is a type the author declares, like a variant's tag. The formula
// names the role; it never names a sample. Which sample and test play the
// role is data, bound when the context is built.
struct Reference
{
};
struct SecondReference
{
};

// ---- Lineage attributes: the author's, not the library's ---------------------
//
// The library knows no attribute. It compares the keys the caller states for
// each record, and nothing else; which batch a specimen came from is the
// laboratory's data.
struct MaterialBatch
{
};
struct TestMethod
{
};

using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;
using Force = formula::Quantity<struct ForceTag, "F", "load at failure", unit::Newton>;
using EdgeX = formula::Quantity<struct EdgeXTag, "x_m", "measured edge", unit::Millimetre>;
using EdgeY = formula::Quantity<struct EdgeYTag, "y_m", "measured edge", unit::Millimetre>;
using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", unit::Gram>;
} // namespace

template <>
struct formula::TagName<SecondReference>
{
    static constexpr std::string_view of() noexcept { return "second reference"; }
};

namespace
{
// ---- 1. The records, and the context that holds them -------------------------

// This specimen, and the reference specimen, whose strength was typed in.
constexpr auto here = formula::environment(formula::Measured<Strength> { 30 },
                                           formula::Measured<Force> { 579'630 },
                                           formula::Measured<EdgeX> { 139 },
                                           formula::Measured<EdgeY> { 139 });
constexpr auto there = formula::environment(formula::entered(formula::Measured<Strength> { 20 }),
                                            formula::Measured<Force> { 386'420 },
                                            formula::Measured<EdgeX> { 139 },
                                            formula::Measured<EdgeY> { 139 });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                         formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                               formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)));

constexpr auto ratio = var<Strength> / formula::from_record<Reference>(var<Strength>);

// ---- 2. A computation over the reference specimen ----------------------------

constexpr auto referenceStrength = formula::from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>));

// The masses retained on three screens, here and on the reference, whose
// masses were typed in. A series is reduced inside the read: the read holds
// one value.
constexpr auto screensHere = formula::environment(formula::measured_series<Retained>(163, 241, 127));
constexpr auto screensThere = formula::environment(formula::entered(formula::measured_series<Retained>(139, 197, 103)));
constexpr auto screenRecords = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), screensThere));

constexpr auto retainedRatio =
    formula::sum(formula::series<Retained, 3>) / formula::from_record<Reference>(formula::sum(formula::series<Retained, 3>));

// ---- 3. The lineage gate -----------------------------------------------------

constexpr auto gated = formula::from_record<Reference>(var<Strength>, formula::same_lineage<MaterialBatch, TestMethod>());

/// The same two specimens, with the reference's lineage keys given.
template <typename Batch, typename Method>
constexpr auto recordsWith(Batch batch, Method method)
{
    return formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there, batch,
                                   method));
}
} // namespace

int main()
{
    bool allPassed = true;
    auto const check = [&allPassed](bool condition, char const* what) {
        if (!condition)
        {
            std::println("CHECK FAILED: {}", what);
            allPassed = false;
        }
    };

    std::println("== 1. A role is code, a record is data ==\n");

    // Evaluated once, with the trace of how: the value is printed here, the
    // trace in the next part.
    auto const ratioRun = formula::traced(
        [](auto recordingSink) { return formula::checked_evaluate_si<formula::Rational>(ratio, records, recordingSink); });
    if (!ratioRun.outcome)
    {
        std::println("the ratio of the strengths: {}", ratioRun.outcome.error());
        return 1;
    }
    auto const ratioValue = formula::number_of(ratioRun.outcome);
    if (!ratioValue)
    {
        std::println("the ratio of the strengths: no answer");
        return 1;
    }
    std::println("{} = {:/}", formula::render(ratio), *ratioValue);
    check(ratioValue == 3_r / 2, "30 MPa here over the reference's 20 MPa");

    // The context is this record's environment: anything that takes one takes
    // the context, and reads this record's values from it.
    constexpr auto viaContext = formula::checked_evaluate_si<formula::Rational>(var<Strength>, records);
    constexpr auto viaEnvironment = formula::checked_evaluate_si<formula::Rational>(var<Strength>, here);
    constexpr auto strengthViaContext = formula::number_of(viaContext);
    constexpr auto strengthViaEnvironment = formula::number_of(viaEnvironment);
    static_assert(viaContext.has_value() && viaEnvironment.has_value());
    static_assert(strengthViaContext.has_value() && strengthViaEnvironment.has_value());
    std::println("f_c through the context: {} Pa", *strengthViaContext);
    std::println("f_c through this record's environment: {} Pa\n", *strengthViaEnvironment);
    check(strengthViaContext == strengthViaEnvironment, "the context reads this record's own values");

    std::println("== 2. Reading a value, or computing over another specimen ==\n");

    constexpr auto referenceRead = formula::checked_evaluate_si<formula::Rational>(referenceStrength, records);
    constexpr auto referenceValue = formula::number_of(referenceRead);
    static_assert(referenceRead.has_value() && referenceValue.has_value());
    std::println("{} = {} Pa", formula::render(referenceStrength), *referenceValue);
    std::println("{}\n", formula::render<formula::Dialect::LaTeX>(referenceStrength));
    check(formula::render(referenceStrength) == "(F / (x_m * y_m)) of Reference",
          "a compound read is bracketed, so the role qualifies the whole computation");

    std::string const ratioTrace = formula::render_trace(ratioRun.trace, { .maxSteps = 20 });
    std::println("{}", ratioTrace);
    check(ratioTrace.contains("f_c = 20 MPa, from record Reference (sample 23, test 3), entered by hand"),
          "the reference's typed-in strength, and whose it is");

    formula::Documentation const page = formula::document(ratio);
    for (formula::SymbolEntry const& row: page.symbols)
    {
        if (row.record.empty())
            std::println("  {}: {}, this record", row.symbol, row.description);
        else
            std::println("  {}: {}, record {}", row.symbol, row.description, row.record);
    }
    std::println("");
    check(page.symbols.size() == 2, "one row per record a quantity is read from");

    std::string const seriesTrace =
        formula::render_trace(formula::trace_of_si(retainedRatio, screenRecords), { .maxSteps = 20 });
    std::println("{}", seriesTrace);
    check(seriesTrace.contains("m_r = 139 g; 197 g; 103 g, from record Reference (sample 23, test 3), entered by hand\n"),
          "a series read from the reference names the record after its elements, and was typed in");

    std::println("== 3. Lineage is a gate ==\n");

    std::string const agreed = formula::render_trace(formula::trace_of_si(gated, records), { .maxSteps = 20 });
    std::println("{}", agreed);
    check(agreed.contains("same TestMethod as this record: 12 for this record, 12 for Reference, satisfied"),
          "both attributes agree, and the value is read");

    auto const otherMethod = recordsWith(formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(13));
    auto const refused = formula::checked_explain<Strength>(gated, otherMethod);
    if (!refused.has_value())
        std::println("{}", formula::render_trace(refused.error().trace, { .maxSteps = 20 }));
    check(!refused.has_value() && refused.error().error == formula::ArithmeticError::DomainError,
          "a different method refuses the read");

    auto const batchUnknown = recordsWith(formula::unknown_lineage<MaterialBatch>(), formula::lineage<TestMethod>(12));
    auto const notChecked = formula::checked_explain<Strength>(gated, batchUnknown);
    if (!notChecked)
    {
        std::println("the gated read, batch unknown: {}", notChecked.error().error);
        return 1;
    }
    std::println("{}", formula::render_trace(notChecked->trace, { .maxSteps = 20 }));
    check(notChecked->outcome.is_empty(), "an unknown batch gives no answer");

    // A disagreement refuses the read even when another key is unknown: an
    // unknown key gives no answer only when nothing disagrees.
    auto const unknownAndOtherMethod =
        recordsWith(formula::unknown_lineage<MaterialBatch>(), formula::lineage<TestMethod>(13));
    auto const refusedDespiteUnknown = formula::checked_explain<Strength>(gated, unknownAndOtherMethod);
    if (!refusedDespiteUnknown.has_value())
        std::println("{}", formula::render_trace(refusedDespiteUnknown.error().trace, { .maxSteps = 20 }));
    check(!refusedDespiteUnknown.has_value()
              && refusedDespiteUnknown.error().error == formula::ArithmeticError::DomainError,
          "a different method refuses the read, though the batch is unknown");

    std::println("== 4. A record not yet made ==\n");

    auto const notYetTested = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
        formula::Record<Reference, decltype(there), formula::LineageEntry<MaterialBatch>,
                        formula::LineageEntry<TestMethod>>::unbound());
    auto const unbound = formula::traced(
        [&](auto recordingSink)
        { return formula::checked_evaluate_si<formula::Rational>(ratio, notYetTested, recordingSink); });
    std::println("{}", formula::render_trace(unbound.trace, { .maxSteps = 20 }));
    check(unbound.outcome.has_value() && !unbound.outcome->has_value(), "no answer, never zero");

    // The same record behind the gated read: every attribute it declares is
    // unknown, so no lineage is compared, and there is no answer.
    auto const unboundGated = formula::checked_explain<Strength>(gated, notYetTested);
    if (!unboundGated)
    {
        std::println("the gated read, record not yet made: {}", unboundGated.error().error);
        return 1;
    }
    std::string const unboundGatedTrace = formula::render_trace(unboundGated->trace, { .maxSteps = 20 });
    std::println("{}", unboundGatedTrace);
    check(unboundGated->outcome.is_empty() && !unboundGatedTrace.contains("same "),
          "a gated read over a record not yet made checks no lineage, and gives no answer");

    auto const typedInEmpty = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                   formula::environment(formula::entered(formula::Measured<Strength>::absent()))));
    std::string const leftEmpty = formula::render_trace(formula::trace_of_si(ratio, typedInEmpty), { .maxSteps = 20 });
    std::println("{}", leftEmpty);
    check(leftEmpty.contains("f_c = (entered by hand as empty), from record Reference"), "an entry left empty by hand says so");

    std::println("== 5. A role's name reads as a name ==\n");

    constexpr auto secondRead = formula::from_record<SecondReference>(var<Strength>);
    std::println("{}\n{}\n", formula::render(secondRead), formula::render<formula::Dialect::LaTeX>(secondRead));
    check(formula::render(secondRead) == "f_c of second reference", "a role spelt through TagName");

    std::println("all checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
