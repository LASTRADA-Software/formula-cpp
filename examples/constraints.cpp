// SPDX-License-Identifier: Apache-2.0
//
// Constraints: a rule a standard states purely to validate a result, not to
// compute one -- "the specimen shall be rejected below 30 MPa", not "compute
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
// Standard references, exactly as every other example in this repository is.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdio>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Strength: formula::Quantity<Strength, "f", "measured compressive strength", unit::Megapascal>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "measured specimen diameter", unit::Millimetre>
{
};

// "reject the specimen below 30 MPa" -- constraint.hpp's own illustrative
// example, given a concrete citation here.
constexpr auto minimumStrength =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" },
                        formula::Citation { .title = "Minimum compressive strength",
                                            .reference = "Example Standard 7:2020",
                                            .section = "5.1" });

constexpr auto maximumDiameter =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 100 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" });

// Divides a measured value by zero while checking, so the predicate can
// never resolve at all -- Invalid, distinct from NotChecked: this one broke
// while checking, rather than never having its input measured in the first
// place.
constexpr auto dividesByZero =
    formula::constraint((var<Strength> / formula::number(formula::Rational { 0 }))
                             > formula::constant<unit::Megapascal>(formula::Rational { 1 }),
                        formula::Verdict { "specimen result is unusable" });

[[nodiscard]] constexpr auto strengthOf(long long megapascals)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { megapascals } });
}

// Neither quantity measured -- the case this whole example exists to show.
[[nodiscard]] constexpr auto nothingMeasured()
{
    return formula::environment(formula::Measured<Strength>::absent(), formula::Measured<Diameter>::absent());
}

// Strength measured, diameter never measured -- so checking a set spanning
// both quantities resolves one and leaves the other not checked.
[[nodiscard]] constexpr auto strengthOnly(long long megapascals)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { megapascals } },
                                formula::Measured<Diameter>::absent());
}

[[nodiscard]] std::string_view describe(formula::ConstraintOutcomeKind kind)
{
    switch (kind)
    {
        case formula::ConstraintOutcomeKind::Satisfied:
            return "satisfied";
        case formula::ConstraintOutcomeKind::Violated:
            return "violated";
        case formula::ConstraintOutcomeKind::NotChecked:
            return "not checked";
        case formula::ConstraintOutcomeKind::Invalid:
            return "invalid";
    }
    return "unknown";
}

// Checks @p subject against @p environment through a fresh RecordingSink and
// renders the one-step trace it produced -- the same shape
// examples/rounding_and_conditionals.cpp uses for a Node's own trace, just
// built by hand here because Constraint::check takes a sink parameter
// directly rather than going through explain(), which only accepts a Node.
template <typename P, typename Env>
[[nodiscard]] std::string tracedCheck(formula::Constraint<P> const& subject, Env const& environment)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    [[maybe_unused]] auto const outcome = formula::check(subject, environment, sink);
    return formula::render_trace(trace, { .maxSteps = 5 });
}

} // namespace

int main()
{
    // ---- 1. A constraint renders as its rule, never its verdict -----------
    std::printf("rendered: %s\n", formula::render(minimumStrength).c_str());
    std::printf("rendered (LaTeX): %s\n", formula::render<formula::Dialect::LaTeX>(minimumStrength).c_str());

    // ---- 2. document() walks it for its citation and symbol table ---------
    //
    // A Constraint is deliberately not a Node (constraint.hpp's file comment
    // explains why), so it cannot be wrapped by documented() -- but
    // document() itself has its own overload for Constraint, alongside the
    // one for Node, so a standalone constraint documents exactly the way a
    // formula does:
    formula::Documentation const documentation = formula::document(minimumStrength);
    formula::Citation const& citation = documentation.citations.front();
    std::printf("documented: %s\n", documentation.formula.c_str());
    std::printf("cited: %.*s, %.*s, %.*s\n",
                static_cast<int>(citation.title.size()),
                citation.title.data(),
                static_cast<int>(citation.reference.size()),
                citation.reference.data(),
                static_cast<int>(citation.section.size()),
                citation.section.data());
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::printf("symbol: %.*s = %.*s [%s]\n",
                    static_cast<int>(entry.symbol.size()),
                    entry.symbol.data(),
                    static_cast<int>(entry.description.size()),
                    entry.description.data(),
                    std::string { formula::view(entry.unit.symbolText) }.c_str());

    // ---- 3. The four outcomes, checked one at a time -----------------------
    constexpr auto satisfied = formula::check(minimumStrength, strengthOf(45));
    constexpr auto violated = formula::check(minimumStrength, strengthOf(20));
    constexpr auto notChecked = formula::check(minimumStrength, nothingMeasured());
    constexpr auto invalid = formula::check(dividesByZero, strengthOf(0));

    std::string_view const satisfiedWord = describe(satisfied.kind());
    std::string_view const violatedWord = describe(violated.kind());
    std::string_view const notCheckedWord = describe(notChecked.kind());
    std::string_view const invalidWord = describe(invalid.kind());
    std::string_view const violatedVerdict = violated.verdict()->label;
    std::string_view const invalidReason = formula::describe(*invalid.error());

    std::printf("45 MPa: %.*s\n", static_cast<int>(satisfiedWord.size()), satisfiedWord.data());
    std::printf("20 MPa: %.*s (%.*s)\n",
                static_cast<int>(violatedWord.size()),
                violatedWord.data(),
                static_cast<int>(violatedVerdict.size()),
                violatedVerdict.data());
    std::printf("no strength measured: %.*s\n", static_cast<int>(notCheckedWord.size()), notCheckedWord.data());
    std::printf("divides by zero: %.*s (%.*s)\n",
                static_cast<int>(invalidWord.size()),
                invalidWord.data(),
                static_cast<int>(invalidReason.size()),
                invalidReason.data());

    // The safety property this whole phase exists for, stated as code rather
    // than only as a printed word: an unresolved check is neither satisfied
    // nor violated -- it is its own, honest, third thing.
    bool const notCheckedIsHonest = notChecked.is_not_checked() && !notChecked.is_satisfied() && !notChecked.is_violated();

    // ---- 4. Each outcome as its own trace step ------------------------------
    std::string const satisfiedTrace = tracedCheck(minimumStrength, strengthOf(45));
    std::string const violatedTrace = tracedCheck(minimumStrength, strengthOf(20));
    std::string const notCheckedTrace = tracedCheck(minimumStrength, nothingMeasured());
    std::string const invalidTrace = tracedCheck(dividesByZero, strengthOf(0));
    std::printf("%s", satisfiedTrace.c_str());
    std::printf("%s", violatedTrace.c_str());
    std::printf("%s", notCheckedTrace.c_str());
    std::printf("%s", invalidTrace.c_str());

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
        formula::check_all(formula::constraints(minimumStrength, maximumDiameter), strengthOnly(20));
    std::string_view const setWord0 = describe(setOutcomes[0].kind());
    std::string_view const setWord1 = describe(setOutcomes[1].kind());
    std::printf("set[0] (minimumStrength): %.*s\n", static_cast<int>(setWord0.size()), setWord0.data());
    std::printf("set[1] (maximumDiameter): %.*s\n", static_cast<int>(setWord1.size()), setWord1.data());

    bool const setCheckedBothWithoutShortCircuit = setOutcomes[0].is_violated() && setOutcomes[1].is_not_checked();

    // ---- Every claim printed above, verified in code ------------------------
    bool const renderedCorrectly = formula::render(minimumStrength) == "require f >= 30 MPa"
                                   && formula::render<formula::Dialect::LaTeX>(minimumStrength)
                                          == "\\text{require } f \\geq 30 MPa";
    bool const documentedCorrectly = documentation.formula == "require f >= 30 MPa" && documentation.symbols.size() == 1
                                     && citation.title == "Minimum compressive strength"
                                     && citation.reference == "Example Standard 7:2020";
    bool const fourOutcomesCorrect = satisfied.is_satisfied() && violated.is_violated() && notChecked.is_not_checked()
                                     && invalid.is_invalid();

    bool const allChecksPassed = renderedCorrectly && documentedCorrectly && fourOutcomesCorrect
                                 && notCheckedIsHonest && setCheckedBothWithoutShortCircuit;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
