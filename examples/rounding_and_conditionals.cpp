// SPDX-License-Identifier: Apache-2.0
//
// Rounding, conditionals, and the numeric escape hatch.
//
// Three additions to the node vocabulary, in one program:
//
//  - rounded<Unit, DecimalPlaces, RoundingMode>() and
//    rounded_to_digits<Unit, SignificantDigits, RoundingMode>() round at a
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

#include <cstdio>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::RoundingMode;
using formula::SignificantDigits;
using formula::var;

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f", "material strength", unit::Megapascal>
{
};
struct CorrectionFactor: formula::Quantity<CorrectionFactor, "k", "empirical correction factor", unit::One>
{
};

// ---- 1: intermediate vs. final rounding, same formula shape, same input ---
//
// A method may say "round the diameter to the nearest millimetre before
// doubling it" -- a coarse instrument that only ever reads whole millimetres
// -- or "double the diameter, then round the result to one decimal place".
// Both are legitimate specifications, and they are not the same formula.
constexpr auto coarseInput =
    formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
constexpr auto roundThenDouble = coarseInput + coarseInput;
constexpr auto doubleThenRound =
    formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter> + var<Diameter>);

// ---- 2: rounding to significant digits, rather than decimal places --------
constexpr auto toTwoSignificantDigits =
    formula::rounded_to_digits<unit::Millimetre, SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);

// ---- 3: a numeric threshold selects between two formulas -------------------
//
// A method that reports a large specimen to the nearest millimetre and a
// small one to one decimal place: the threshold is itself part of the
// formula, not an if/else the caller has to remember to apply consistently.
constexpr auto sizeAdjustedDiameter =
    formula::when(var<Diameter> > formula::constant<unit::Millimetre>(formula::Rational { 20 }),
                  formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>),
                  formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter>));

// ---- 4: the traced escape hatch --------------------------------------------
//
// An invented empirical rule whose coefficient only works when it is read
// against the strength's numeric value in megapascals -- a rule that cannot
// be stated over the Strength quantity itself without lying about what makes
// it work in the first place.
constexpr auto empiricalCorrection =
    formula::numeric_value_of<unit::Megapascal,
                              "Example Standard 9:2020 states this empirical coefficient over the numeric "
                              "value of strength in MPa">(var<Strength>)
        * formula::Rational { 2, 100 }
    - formula::Rational { 1 };

[[nodiscard]] constexpr auto millimetres(long long hundredths)
{
    return formula::environment(formula::Measured<Diameter> { formula::Rational { hundredths, 100 } });
}
} // namespace

int main()
{
    // ---- 1. Same formula shape, same input, two different answers --------
    std::printf("rendered: %s\n", formula::render(doubleThenRound).c_str());

    constexpr auto measured = millimetres(1250); // 12.50 mm
    constexpr auto early = formula::checked_evaluate<Diameter>(roundThenDouble, measured);
    constexpr auto late = formula::checked_evaluate<Diameter>(doubleThenRound, measured);
    std::printf("%-36s= %f mm\n", "12.50 mm, round to 0 dp then double", early->measurement().value().to_double());
    std::printf("%-36s= %f mm\n", "12.50 mm, double then round to 1 dp", late->measurement().value().to_double());

    // ---- 2. Rounding to significant digits, not decimal places ------------
    constexpr auto sigFigInput = millimetres(1234); // 12.34 mm
    constexpr auto sigFigResult = formula::checked_evaluate<Diameter>(toTwoSignificantDigits, sigFigInput);
    std::printf("%-36s= %f mm\n", "12.34 mm to 2 significant digits", sigFigResult->measurement().value().to_double());

    // ---- 3. A numeric threshold selects between two formulas --------------
    std::printf("rendered: %s\n", formula::render(sizeAdjustedDiameter).c_str());

    constexpr auto smallSpecimen = millimetres(1234); // 12.34 mm -- at or below the threshold
    constexpr auto largeSpecimen = millimetres(2540); // 25.40 mm -- above the threshold
    constexpr auto smallResult = formula::checked_evaluate<Diameter>(sizeAdjustedDiameter, smallSpecimen);
    constexpr auto largeResult = formula::checked_evaluate<Diameter>(sizeAdjustedDiameter, largeSpecimen);
    std::printf("%-36s= %f mm\n", "12.34 mm, size-adjusted rounding", smallResult->measurement().value().to_double());
    std::printf("%-36s= %f mm\n", "25.40 mm, size-adjusted rounding", largeResult->measurement().value().to_double());

    // The trace names which branch a when() took -- here, the "then" branch,
    // because 25.40 mm is above the 20 mm threshold.
    formula::Explained<Diameter> const explainedLarge = formula::explain<Diameter>(sizeAdjustedDiameter, largeSpecimen);
    std::string const conditionalTrace = formula::render_trace(explainedLarge.trace, { .maxSteps = 10 });
    std::printf("%s", conditionalTrace.c_str());

    // ---- 4. The traced escape hatch ----------------------------------------
    std::printf("rendered: %s\n", formula::render(empiricalCorrection).c_str());

    constexpr auto strengthKnown = formula::environment(formula::Measured<Strength> { formula::Rational { 70 } });
    constexpr auto correction = formula::checked_evaluate<CorrectionFactor>(empiricalCorrection, strengthKnown);
    std::printf("empirical correction factor at 70 MPa = %f\n", correction->measurement().value().to_double());

    formula::Explained<CorrectionFactor> const explainedCorrection =
        formula::explain<CorrectionFactor>(empiricalCorrection, strengthKnown);
    std::string const escapeTrace = formula::render_trace(explainedCorrection.trace, { .maxSteps = 10 });
    std::printf("%s", escapeTrace.c_str());

    // Every number printed above is checked here; nothing is printed that
    // this bool does not also cover.
    bool const intermediateAndFinalRoundingDiffer = early.has_value() && early->is_value() && late.has_value()
                                                    && late->is_value()
                                                    && early->measurement().value() == formula::Rational { 26 }
                                                    && late->measurement().value() == formula::Rational { 25 }
                                                    && early->measurement().value() != late->measurement().value();
    bool const significantDigitsCorrect = sigFigResult.has_value() && sigFigResult->is_value()
                                          && sigFigResult->measurement().value() == formula::Rational { 12 };
    bool const conditionalPickedTheRightBranch = smallResult.has_value() && smallResult->is_value()
                                                 && largeResult.has_value() && largeResult->is_value()
                                                 && smallResult->measurement().value() == formula::Rational { 123, 10 }
                                                 && largeResult->measurement().value() == formula::Rational { 25 };
    bool const escapeHatchCorrect =
        correction.has_value() && correction->is_value() && correction->measurement().value() == formula::Rational { 2, 5 };

    bool const allChecksPassed = intermediateAndFinalRoundingDiffer && significantDigitsCorrect
                                 && conditionalPickedTheRightBranch && escapeHatchCorrect;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
