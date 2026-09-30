// SPDX-License-Identifier: Apache-2.0
//
// Rounding, conditionals, and the numeric escape hatch.
//
// Three additions to the node vocabulary, in one program:
//
//  - rounded<DecimalRounding>() and rounded_to_digits<SignificantRounding>(),
//    each rounding named once (a unit, a count, a tie rule), round at a
//    stated POSITION in a formula, not only on the printed result -- so
//    rounding an intermediate value and rounding only at the end are two
//    different formulas, and in general two different answers, even though
//    both start from the same measured input.
//  - when(predicate, then, else) selects between two formulas of the same
//    dimension by a numeric threshold, evaluating only the branch it takes.
//  - numeric_value_of<Unit, Justification>() is the traced escape hatch for a
//    rule that is genuinely stated over a bare number read in one particular
//    unit -- not a shortcut around a dimensional mismatch. See
//    docs/rounding-and-conditionals.md for why reaching for it should feel
//    wrong every time except that one.
//
// Every formula and citation here is invented -- generic physics with
// fictional Example Standard references, exactly as every other example in
// this repository is.

#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::RoundingMode;
using formula::SignificantDigits;
using formula::var;
using namespace formula::literals;

using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f", "material strength", unit::Megapascal>;
using CorrectionFactor = formula::Quantity<struct CorrectionFactorTag, "k", "empirical correction factor", unit::One>;

// ---- The roundings the method states, each named once -----------------------
constexpr formula::DecimalRounding wholeMillimetre { unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero };
constexpr formula::DecimalRounding tenthMillimetre { unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero };
constexpr formula::SignificantRounding twoDigitsOfMillimetre {
    unit::Millimetre, SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero
};

// ---- 1: intermediate vs. final rounding, same formula shape, same input ---
//
// A method may say "round the diameter to the nearest millimetre before
// doubling it" -- a coarse instrument that only ever reads whole millimetres
// -- or "double the diameter, then round the result to one decimal place".
// Both are legitimate specifications, and they are not the same formula.
constexpr auto coarseInput = formula::rounded<wholeMillimetre>(var<Diameter>);
constexpr auto roundThenDouble = coarseInput + coarseInput;
constexpr auto doubleThenRound = formula::rounded<tenthMillimetre>(var<Diameter> + var<Diameter>);

// ---- 2: rounding to significant digits, rather than decimal places --------
constexpr auto toTwoSignificantDigits = formula::rounded_to_digits<twoDigitsOfMillimetre>(var<Diameter>);

// ---- 3: a numeric threshold selects between two formulas -------------------
//
// A method that reports a large specimen to the nearest millimetre and a
// small one to one decimal place: the threshold is itself part of the
// formula, not an if/else the caller has to remember to apply consistently.
constexpr auto sizeAdjustedDiameter = formula::yields<Diameter>(formula::when(
    var<Diameter> > formula::constant<unit::Millimetre>(17.3_r), coarseInput, formula::rounded<tenthMillimetre>(var<Diameter>)));

// ---- 4: the traced escape hatch --------------------------------------------
//
// An invented empirical rule whose coefficient only works when it is read
// against the strength's numeric value in megapascals -- a rule that cannot
// be stated over the Strength quantity itself without lying about what makes
// it work in the first place.
constexpr auto empiricalCorrection = formula::yields<CorrectionFactor>(
    formula::numeric_value_of<unit::Megapascal,
                              "Example Standard 9:2020 states this empirical coefficient over the numeric "
                              "value of strength in MPa">(var<Strength>)
        * 0.0213_r
    - 1.043_r);

constexpr auto diameter12_50 = formula::environment(formula::Measured<Diameter> { 12.5_r });
constexpr auto diameter12_34 = formula::environment(formula::Measured<Diameter> { 12.34_r }); // at or below the threshold
constexpr auto diameter25_40 = formula::environment(formula::Measured<Diameter> { 25.4_r });  // above the threshold
constexpr auto strength70 = formula::environment(formula::Measured<Strength> { 70 });
} // namespace

int main()
{
    // ---- 1. Same formula shape, same input, two different answers --------
    std::println("rendered: {}", formula::render(doubleThenRound));

    constexpr auto early = formula::checked_evaluate<Diameter>(roundThenDouble, diameter12_50);
    constexpr auto late = formula::checked_evaluate<Diameter>(doubleThenRound, diameter12_50);
    static_assert(early.has_value() && late.has_value());
    std::println("{:<36}= {}", "12.50 mm, round to 0 dp then double", *early);
    std::println("{:<36}= {}", "12.50 mm, double then round to 1 dp", *late);

    // ---- 2. Rounding to significant digits, not decimal places ------------
    constexpr auto sigFigResult = formula::checked_evaluate<Diameter>(toTwoSignificantDigits, diameter12_34);
    static_assert(sigFigResult.has_value());
    std::println("{:<36}= {}", "12.34 mm to 2 significant digits", *sigFigResult);

    // ---- 3. A numeric threshold selects between two formulas --------------
    std::println("rendered: {}", formula::render(sizeAdjustedDiameter));

    constexpr auto smallResult = formula::checked_evaluate(sizeAdjustedDiameter, diameter12_34);
    constexpr auto largeResult = formula::checked_evaluate(sizeAdjustedDiameter, diameter25_40);
    static_assert(smallResult.has_value() && largeResult.has_value());
    std::println("{:<36}= {}", "12.34 mm, size-adjusted rounding", *smallResult);
    std::println("{:<36}= {}", "25.40 mm, size-adjusted rounding", *largeResult);

    // The trace names which branch a when() took -- here, the "then" branch,
    // because 25.40 mm is above the 17.3 mm threshold.
    auto const explainedLarge = formula::explain(sizeAdjustedDiameter, diameter25_40);
    std::print("{}", formula::render_trace(explainedLarge.trace, { .maxSteps = 10 }));

    // ---- 4. The traced escape hatch ----------------------------------------
    std::println("rendered: {}", formula::render(empiricalCorrection));

    constexpr auto correction = formula::checked_evaluate(empiricalCorrection, strength70);
    static_assert(correction.has_value());
    std::println("empirical correction factor at 70 MPa = {}", *correction);

    auto const explainedCorrection = formula::explain(empiricalCorrection, strength70);
    std::print("{}", formula::render_trace(explainedCorrection.trace, { .maxSteps = 10 }));

    // Every number printed above is checked here; nothing is printed that
    // this bool does not also cover.
    bool const intermediateAndFinalRoundingDiffer =
        formula::number_of(early) == 26_r && formula::number_of(late) == 25_r
        && formula::number_of(early) != formula::number_of(late);
    bool const significantDigitsCorrect = formula::number_of(sigFigResult) == 12_r;
    bool const conditionalPickedTheRightBranch =
        formula::number_of(smallResult) == 12.3_r && formula::number_of(largeResult) == 25_r;
    bool const escapeHatchCorrect = formula::number_of(correction) == 0.448_r;

    bool const allChecksPassed = intermediateAndFinalRoundingDiffer && significantDigitsCorrect
                                 && conditionalPickedTheRightBranch && escapeHatchCorrect;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
