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
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdio>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

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
constexpr auto here = formula::environment(formula::Measured<Strength> { formula::Rational { 30 } },
                                           formula::Measured<Force> { formula::Rational { 579'630 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 139 } });
constexpr auto there = formula::environment(formula::entered(formula::Measured<Strength> { formula::Rational { 20 } }),
                                            formula::Measured<Force> { formula::Rational { 386'420 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 139 } });

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
constexpr auto screensHere = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 163 } }, formula::Measured<Retained> { formula::Rational { 241 } },
    formula::Measured<Retained> { formula::Rational { 127 } }));
constexpr auto screensThere = formula::environment(formula::entered(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 139 } }, formula::Measured<Retained> { formula::Rational { 197 } },
    formula::Measured<Retained> { formula::Rational { 103 } })));
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

/// A value in coherent SI, exactly, with its unit: `30000000 Pa` or `3/2`;
/// `no answer` when it is absent, and `refused` when it is an error.
std::string exact(formula::Evaluated<formula::Rational> const& evaluated, std::string_view unitText)
{
    if (!evaluated.has_value())
        return "refused";
    if (!evaluated->has_value())
        return "no answer";
    formula::Rational const value = **evaluated;
    std::string text = std::to_string(value.numerator());
    if (value.denominator() != 1)
        text += "/" + std::to_string(value.denominator());
    if (!unitText.empty())
        text += " " + std::string { unitText };
    return text;
}

/// The trace of @p expression over @p context.
template <typename Expression, typename Context>
std::string traceOf(Expression const& expression, Context const& context)
{
    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_si<formula::Rational>(expression, context, formula::RecordingSink { recorded });
    return formula::render_trace(recorded, { .maxSteps = 20 });
}
} // namespace

int main()
{
    bool allPassed = true;
    auto const check = [&allPassed](bool condition, char const* what) {
        if (!condition)
        {
            std::printf("CHECK FAILED: %s\n", what);
            allPassed = false;
        }
    };

    std::printf("== 1. A role is code, a record is data ==\n\n");

    std::string const ratioText = formula::render(ratio);
    std::string const ratioValue = exact(formula::checked_evaluate_si<formula::Rational>(ratio, records), "");
    std::printf("%s = %s\n", ratioText.c_str(), ratioValue.c_str());
    check(ratioValue == "3/2", "30 MPa here over the reference's 20 MPa");

    // The context is this record's environment: anything that takes one takes
    // the context, and reads this record's values from it.
    std::string const viaContext = exact(formula::checked_evaluate_si<formula::Rational>(var<Strength>, records), "Pa");
    std::string const viaEnvironment = exact(formula::checked_evaluate_si<formula::Rational>(var<Strength>, here), "Pa");
    std::printf("f_c through the context: %s\n", viaContext.c_str());
    std::printf("f_c through this record's environment: %s\n\n", viaEnvironment.c_str());
    check(viaContext == viaEnvironment, "the context reads this record's own values");

    std::printf("== 2. Reading a value, or computing over another specimen ==\n\n");

    std::printf("%s = %s\n", formula::render(referenceStrength).c_str(),
                exact(formula::checked_evaluate_si<formula::Rational>(referenceStrength, records), "Pa").c_str());
    std::printf("%s\n\n", formula::render<formula::Dialect::LaTeX>(referenceStrength).c_str());
    check(formula::render(referenceStrength) == "(F / (x_m * y_m)) of Reference",
          "a compound read is bracketed, so the role qualifies the whole computation");

    std::string const ratioTrace = traceOf(ratio, records);
    std::printf("%s\n", ratioTrace.c_str());
    check(ratioTrace.find("f_c = 20 MPa, from record Reference (sample 23, test 3), entered by hand")
              != std::string::npos,
          "the reference's typed-in strength, and whose it is");

    formula::Documentation const page = formula::document(ratio);
    for (formula::SymbolEntry const& row: page.symbols)
        std::printf("  %.*s: %.*s, %s\n", static_cast<int>(row.symbol.size()), row.symbol.data(),
                    static_cast<int>(row.description.size()), row.description.data(),
                    row.record.empty() ? "this record" : ("record " + std::string { row.record }).c_str());
    std::printf("\n");
    check(page.symbols.size() == 2, "one row per record a quantity is read from");

    std::string const seriesTrace = traceOf(retainedRatio, screenRecords);
    std::printf("%s\n", seriesTrace.c_str());
    check(seriesTrace.find("m_r = 139 g; 197 g; 103 g, from record Reference (sample 23, test 3), entered by hand\n")
              != std::string::npos,
          "a series read from the reference names the record after its elements, and was typed in");

    std::printf("== 3. Lineage is a gate ==\n\n");

    std::string const agreed = traceOf(gated, records);
    std::printf("%s\n", agreed.c_str());
    check(agreed.find("same TestMethod as this record: 12 for this record, 12 for Reference, satisfied") != std::string::npos,
          "both attributes agree, and the value is read");

    auto const otherMethod = recordsWith(formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(13));
    auto const refused = formula::checked_explain<Strength>(gated, otherMethod);
    if (!refused.has_value())
        std::printf("%s\n", formula::render_trace(refused.error().trace, { .maxSteps = 20 }).c_str());
    check(!refused.has_value() && refused.error().error == formula::ArithmeticError::DomainError,
          "a different method refuses the read");

    auto const batchUnknown =
        recordsWith(formula::unknown_lineage<MaterialBatch>(), formula::lineage<TestMethod>(12));
    std::string const notChecked = traceOf(gated, batchUnknown);
    std::printf("%s\n", notChecked.c_str());
    check(!formula::checked_evaluate_si<formula::Rational>(gated, batchUnknown)->has_value(),
          "an unknown batch gives no answer");

    // A disagreement refuses the read even when another key is unknown: an
    // unknown key gives no answer only when nothing disagrees.
    auto const unknownAndOtherMethod =
        recordsWith(formula::unknown_lineage<MaterialBatch>(), formula::lineage<TestMethod>(13));
    auto const refusedDespiteUnknown = formula::checked_explain<Strength>(gated, unknownAndOtherMethod);
    if (!refusedDespiteUnknown.has_value())
        std::printf("%s\n", formula::render_trace(refusedDespiteUnknown.error().trace, { .maxSteps = 20 }).c_str());
    check(!refusedDespiteUnknown.has_value()
              && refusedDespiteUnknown.error().error == formula::ArithmeticError::DomainError,
          "a different method refuses the read, though the batch is unknown");

    std::printf("== 4. A record not yet made ==\n\n");

    auto const notYetTested = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                             formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
        formula::Record<Reference, decltype(there), formula::LineageEntry<MaterialBatch>,
                        formula::LineageEntry<TestMethod>>::unbound());
    std::string const unbound = traceOf(ratio, notYetTested);
    std::printf("%s\n", unbound.c_str());
    check(!formula::checked_evaluate_si<formula::Rational>(ratio, notYetTested)->has_value(),
          "no answer, never zero");

    // The same record behind the gated read: every attribute it declares is
    // unknown, so no lineage is compared, and there is no answer.
    std::string const unboundGated = traceOf(gated, notYetTested);
    std::printf("%s\n", unboundGated.c_str());
    check(!formula::checked_evaluate_si<formula::Rational>(gated, notYetTested)->has_value()
              && unboundGated.find("same ") == std::string::npos,
          "a gated read over a record not yet made checks no lineage, and gives no answer");

    auto const typedInEmpty = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                   formula::environment(formula::entered(formula::Measured<Strength>::absent()))));
    std::string const leftEmpty = traceOf(ratio, typedInEmpty);
    std::printf("%s\n", leftEmpty.c_str());
    check(leftEmpty.find("f_c = (entered by hand as empty), from record Reference") != std::string::npos,
          "an entry left empty by hand says so");

    std::printf("== 5. A role's name reads as a name ==\n\n");

    constexpr auto secondRead = formula::from_record<SecondReference>(var<Strength>);
    std::printf("%s\n%s\n\n", formula::render(secondRead).c_str(),
                formula::render<formula::Dialect::LaTeX>(secondRead).c_str());
    check(formula::render(secondRead) == "f_c of second reference", "a role spelt through TagName");

    std::printf("all checks passed: %s\n", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
