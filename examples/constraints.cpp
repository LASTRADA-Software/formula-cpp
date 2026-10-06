// SPDX-License-Identifier: Apache-2.0
//
// Constraints: a rule a standard states purely to validate a result, not to
// compute one -- "the specimen shall be rejected below 27.3 MPa", not "compute
// the strength". formula::constraint() pairs a predicate with what to do when
// it does not hold; formula::check() reports one of FOUR outcomes, not two:
// Satisfied, Violated, NotChecked, Invalid.
//
// The one this example exists to show is NotChecked. An input a standard
// requires can go unmeasured, and a constraint whose predicate never
// resolved must never be reported as Satisfied -- that would be a record
// claiming a specimen was verified when nothing verified it. Checking a SET
// of constraints together never short-circuits either, unlike when()'s
// branch selection -- see the comment at step 5 below for why the two
// differ on purpose rather than being inconsistent.
//
// Every citation here is invented -- generic physics with fictional Example
// Standard references, as every citation of a standard in this repository is.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

using Strength = formula::Quantity<struct StrengthTag, "f", "measured compressive strength", unit::Megapascal>;
using Diameter = formula::Quantity<struct DiameterTag, "d", "measured specimen diameter", unit::Millimetre>;

// "reject the specimen below 27.3 MPa" -- an invented threshold, given an
// invented citation here.
constexpr auto minimumStrength = formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(27.3_r),
                                                     formula::Verdict { "reject the specimen" },
                                                     formula::Citation { .title = "Minimum compressive strength",
                                                                         .reference = "Example Standard 7:2020",
                                                                         .section = "5.1" });

constexpr auto maximumDiameter = formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(139),
                                                     formula::Verdict { "specimen exceeds diameter tolerance" });

// Divides a measured value by zero while checking, so the predicate can
// never resolve at all -- Invalid, distinct from NotChecked: this one broke
// while checking, rather than never having its input measured in the first
// place.
constexpr auto dividesByZero =
    formula::constraint((var<Strength> / formula::number(0)) > formula::constant<unit::Megapascal>(1),
                        formula::Verdict { "specimen result is unusable" });

constexpr auto strength45 = formula::environment(formula::Measured<Strength> { 45 });
constexpr auto strength20 = formula::environment(formula::Measured<Strength> { 20 });
constexpr auto strength0 = formula::environment(formula::Measured<Strength> { 0 });

// Neither quantity measured -- the case this whole example exists to show.
constexpr auto nothingMeasured =
    formula::environment(formula::Measured<Strength>::absent(), formula::Measured<Diameter>::absent());

// Strength measured, diameter never measured -- so checking a set spanning
// both quantities resolves one and leaves the other not checked.
constexpr auto strength20Only =
    formula::environment(formula::Measured<Strength> { 20 }, formula::Measured<Diameter>::absent());

} // namespace

int main()
{
    // ---- 1. A constraint renders as its rule, never its verdict -----------
    std::println("rendered: {}", formula::render(minimumStrength));
    std::println("rendered (LaTeX): {}", formula::render<formula::Dialect::LaTeX>(minimumStrength));

    // ---- 2. document() walks it for its citation and symbol table ---------
    //
    // A Constraint is deliberately not a Node (constraint.hpp's file comment
    // explains why), so it cannot be wrapped by documented() -- but
    // document() itself has its own overload for Constraint, alongside the
    // one for Node, so a standalone constraint documents exactly the way a
    // formula does:
    formula::Documentation const documentation = formula::document(minimumStrength);
    formula::Citation const& citation = documentation.citations.front();
    std::println("documented: {}", documentation.formula);
    std::println("cited: {}, {}, {}", citation.title, citation.reference, citation.section);
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::println("symbol: {} = {} [{}]", entry.symbol, entry.description, entry.unit);

    // ---- 3. The four outcomes, checked one at a time -----------------------
    constexpr auto satisfied = formula::check(minimumStrength, strength45);
    constexpr auto violated = formula::check(minimumStrength, strength20);
    constexpr auto notChecked = formula::check(minimumStrength, nothingMeasured);
    constexpr auto invalid = formula::check(dividesByZero, strength0);

    // Each outcome is the one it was built to be, or the build stops here --
    // before the verdict and the error below are read.
    static_assert(satisfied.is_satisfied() && violated.is_violated() && violated.verdict().has_value()
                  && notChecked.is_not_checked() && invalid.is_invalid() && invalid.error().has_value());

    std::println("45 MPa: {}", satisfied.kind());
    std::println("20 MPa: {} ({})", violated.kind(), violated.verdict()->label);
    std::println("no strength measured: {}", notChecked.kind());
    std::println("divides by zero: {} ({})", invalid.kind(), *invalid.error());

    // The safety property constraints exist for, stated as code rather
    // than only as a printed word: an unresolved check is neither satisfied
    // nor violated -- it is its own, honest, third thing.
    bool const notCheckedIsHonest = notChecked.is_not_checked() && !notChecked.is_satisfied() && !notChecked.is_violated();

    // ---- 4. Each outcome as its own trace step ------------------------------
    //
    // explain_check() is check() with a recording sink: the outcome, and the
    // trace it recorded.
    std::print("{}", formula::render_trace(formula::explain_check(minimumStrength, strength45).trace, { .maxSteps = 5 }));
    std::print("{}", formula::render_trace(formula::explain_check(minimumStrength, strength20).trace, { .maxSteps = 5 }));
    std::print("{}", formula::render_trace(formula::explain_check(minimumStrength, nothingMeasured).trace, { .maxSteps = 5 }));
    std::print("{}", formula::render_trace(formula::explain_check(dividesByZero, strength0).trace, { .maxSteps = 5 }));

    // ---- 5. Checking a SET of constraints never short-circuits -------------
    //
    // Strength (20 MPa) violates minimumStrength; diameter was never
    // measured, so maximumDiameter cannot resolve. Both outcomes are
    // reported below -- neither suppresses the other -- unlike when(), which
    // evaluates only the branch it takes because the other branch could
    // raise an arithmetic error that has nothing to do with the answer.
    // Every constraint here IS about the answer, so skipping one to save
    // work would discard a check the caller actually asked for; a specimen
    // can fail two checks at once, and a report naming only the first sends
    // someone back for a second round of testing they should not have
    // needed.
    constexpr auto setOutcomes =
        formula::check_all(formula::constraints(minimumStrength, maximumDiameter), strength20Only);
    std::println("set[0] (minimumStrength): {}", setOutcomes[0].kind());
    std::println("set[1] (maximumDiameter): {}", setOutcomes[1].kind());

    bool const setCheckedBothWithoutShortCircuit = setOutcomes[0].is_violated() && setOutcomes[1].is_not_checked();

    // ---- Every claim printed above, verified in code ------------------------
    bool const renderedCorrectly =
        formula::render(minimumStrength) == "require f >= 273/10 MPa"
        && formula::render<formula::Dialect::LaTeX>(minimumStrength) == "\\text{require } f \\geq 273/10\\,\\mathrm{MPa}";
    bool const documentedCorrectly = documentation.formula == "require f >= 273/10 MPa" && documentation.symbols.size() == 1
                                     && citation.title == "Minimum compressive strength"
                                     && citation.reference == "Example Standard 7:2020";
    bool const fourOutcomesCorrect = satisfied.is_satisfied() && violated.is_violated() && notChecked.is_not_checked()
                                     && invalid.is_invalid();

    bool const allChecksPassed = renderedCorrectly && documentedCorrectly && fourOutcomesCorrect
                                 && notCheckedIsHonest && setCheckedBothWithoutShortCircuit;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
