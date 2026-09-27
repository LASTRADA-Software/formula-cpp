// SPDX-License-Identifier: Apache-2.0
//
// The join: an overlaid method that reads from another record, evaluated
// through a context and a renaming vocabulary, traced, documented, checked,
// and across two translation units (`record_cross_tu.hpp`). Each part was
// verified on its own in earlier tasks; this file verifies that they compose.
#include "record_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

using namespace record_cross_tu;

TEST_CASE("an overlaid method reads from another record, with the overlay reaching inside", "[record-join]")
{
    // 6 MPa over the reference's 57 268 N / (139 mm x 103 mm) x 0.97 =
    // 3.88 MPa is 1.5464, which the overlay's rule rounds to 1.55. The wrong
    // answers: no overlay, 6 / 4 = 1.5 under the method's own 0.1; the
    // overlay outside the scope only, keeping the reference's typed-in 1,
    // 1.50; read from this record, 6 / (90 000 N / (197 mm x 103 mm) x 0.97)
    // = 6 / 4.3024 = 1.39.
    constexpr auto value = formula::evaluate_method<Cube>(overlaid, context());
    STATIC_REQUIRE(value.has_value());
    STATIC_REQUIRE(**value == formula::Rational { 155, 100 });
    STATIC_REQUIRE(**formula::evaluate_method<Cube>(baseMethod, context()) == formula::Rational { 3, 2 });
}

TEST_CASE("the joined trace names the variant, the lineage, the fixed constant's record and the page's words",
          "[record-join]")
{
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cube>(overlaid, context(), formula::RecordingSink { recorded, north });

    std::size_t lineageSteps = 0;
    bool sawCube = false;
    bool sawFixedInside = false;
    for (formula::Step<> const& step: recorded.steps)
    {
        if (step.kind == formula::StepKind::LineageChecked)
            ++lineageSteps;
        if (step.kind == formula::StepKind::VariantSelected)
            sawCube = step.variantTag == "Cube";
        if (step.kind == formula::StepKind::OverriddenConstant)
        {
            REQUIRE(formula::origin_of(recorded, step).has_value());
            sawFixedInside = formula::origin_of(recorded, step)->role() == "Reference" && step.inputSource == formula::ValueSource::ManuallyEntered;
        }
    }
    CHECK(lineageSteps == 1);
    CHECK(sawCube);
    CHECK(sawFixedInside);

    std::string const text = trace();
    INFO(text);
    CHECK(text.find("same MaterialBatch as this record: 4411 and 4411, satisfied") != std::string::npos);
    CHECK(text.find("P = 57268 N, from record Reference (sample 23, test 3)") != std::string::npos);
    // The overlay's constant replaced the reference's typed-in shape factor,
    // and the line says so (the task 4 review's L3).
    CHECK(text.find("k = 97/100, from record Reference (sample 23, test 3) [fixed by ") != std::string::npos);
    CHECK(text.find(", replacing a value entered by hand]") != std::string::npos);
    CHECK(text.find("k_s") == std::string::npos);
    CHECK(text.find("F =") == std::string::npos);

    std::string const renderedPage = page();
    INFO(renderedPage);
    CHECK(renderedPage.find("of Reference") != std::string::npos);
    CHECK(renderedPage.find("P") != std::string::npos);
    CHECK(renderedPage.find("k") != std::string::npos);
    CHECK(renderedPage.find("k_s") == std::string::npos);
}

TEST_CASE("the joined page documents the fixed constant once, as read from no record", "[record-join]")
{
    // The fixed value is the overlay's, read from no record, so its row has
    // no record even though the constant is used only inside the scope (the
    // task 7 review's L5).
    auto const documented = formula::document(std::get<0>(overlaid.variantSet.cases).expression, north);
    std::size_t fixedRows = 0;
    for (formula::SymbolEntry const& row: documented.symbols)
        if (row.symbol == "k")
        {
            ++fixedRows;
            CHECK(row.record.empty());
            CHECK(row.fixedValue == formula::Rational { 97, 100 });
        }
    CHECK(fixedRows == 1);
}

TEST_CASE("check_method through a context reads the constraint's value from the other record", "[record-join]")
{
    // 57 268 N there is below the 70 000 N the constraint asks for, and this
    // record's 90 000 N is above it: only a read from the reference violates.
    formula::Trace<> recorded {};
    auto const verdicts = formula::check_method(overlaid, context(), formula::RecordingSink { recorded });
    CHECK(verdicts[0].is_violated());

    bool constraintOperandIsForeign = false;
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::Constraint && !step.operands.empty())
        {
            formula::Step<> const& firstOperand = recorded.steps[step.operands[0]];
            constraintOperandIsForeign = firstOperand.kind == formula::StepKind::RecordScope
                                         && formula::origin_of(recorded, firstOperand).has_value() && formula::origin_of(recorded, firstOperand)->role() == "Reference";
        }
    CHECK(constraintOperandIsForeign);
}

TEST_CASE("the joined method crosses translation units by its type", "[record-join]")
{
    // The other unit's function takes the method, so this call links only if
    // both units see one type (see `record_cross_tu.hpp`); the value then
    // catches a split that changes the result.
    auto const theirs = record_join_evaluate_in_other_tu(overlaid);
    REQUIRE(theirs.has_value());
    CHECK(**theirs == formula::Rational { 155, 100 });
}

namespace
{
struct EdgeRatio: formula::Quantity<EdgeRatio, "r", "edge ratio", unit::One>
{
};

/// A shape factor and an edge ratio each read both here and inside the
/// scope, so that each overlay operation must reach both places.
inline constexpr auto bothPlaces = formula::method(
    formula::variants(
        formula::variant<Cube>(var<ShapeFactor> * var<EdgeRatio>
                               * formula::from_record<Reference>(var<ShapeFactor> * var<EdgeRatio> * var<Force>))),
    formula::rounding_rule<unit::Newton, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto fixShape = formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, annex));
inline constexpr auto deriveRatio = formula::overlay(formula::add_derived<EdgeRatio>(var<EdgeX> / var<EdgeY>, annex));
} // namespace

TEST_CASE("a constant and a derived quantity reach inside a scope in either order, to the same method", "[record-join]")
{
    // Judged against what is produced: the two orders give the same method
    // type, the same page and the same value. 0.97 x 197/103 here (197 / 103
    // mm), times 0.97 x 139/103 x 57 268 N = 74 965.48 N there (139 / 103
    // mm), is 139 079.16 N, rounded to 139 079 N. Leaving either quantity
    // unsubstituted inside the scope would read the reference's own shape
    // factor, or ask its environment for an edge ratio it does not hold;
    // deriving the ratio from this record's edges inside the scope would give
    // 197 112 N.
    constexpr auto fixedFirst = formula::apply(deriveRatio, formula::apply(fixShape, bothPlaces));
    constexpr auto derivedFirst = formula::apply(fixShape, formula::apply(deriveRatio, bothPlaces));
    STATIC_REQUIRE(std::is_same_v<decltype(fixedFirst), decltype(derivedFirst)>);
    STATIC_REQUIRE(formula::evaluate_method<Cube>(fixedFirst, context())
                   == formula::evaluate_method<Cube>(derivedFirst, context()));
    STATIC_REQUIRE(**formula::evaluate_method<Cube>(fixedFirst, context()) == formula::Rational { 139'079 });
    CHECK(formula::render(std::get<0>(fixedFirst.variantSet.cases).expression)
          == formula::render(std::get<0>(derivedFirst.variantSet.cases).expression));
}

namespace
{
/// A shape factor fixed by an overlay, over a formula that reads it here,
/// with no scope: the typed-in clause is the overlay's, not the scope's.
inline constexpr auto fixedHere = formula::apply(
    formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, annex)),
    formula::method(
        formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force>)),
        formula::rounding_rule<unit::Newton, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints()));

/// The overlaid constant's step, and the whole trace rendered, over @p env.
template <typename Env>
std::pair<formula::Step<>, std::string> fixed_step_over(Env const& env)
{
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cube>(fixedHere, env, formula::RecordingSink { recorded });
    formula::Step<> fixed {};
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::OverriddenConstant)
            fixed = step;
    return { fixed, formula::render_trace(recorded, { .maxSteps = 10 }) };
}
} // namespace

TEST_CASE("an overlay's constant that replaced a typed-in value says so", "[record-join]")
{
    // Kills: the overlay's evaluator not asking where the environment's value
    // came from; the trace not keeping the answer for an OverriddenConstant;
    // the renderer ignoring it.
    auto const [fixed, text] =
        fixed_step_over(formula::environment(formula::Measured<Force> { formula::Rational { 1'000 } },
                                             formula::entered(formula::Measured<ShapeFactor> { formula::Rational { 1 } })));
    INFO(text);
    CHECK(fixed.kind == formula::StepKind::OverriddenConstant);
    CHECK(fixed.inputSource == formula::ValueSource::ManuallyEntered);
    CHECK(text.find("k_s = 97/100 [fixed by jurisdiction overlay: Example Standard 14:2022 NA, NA.1, replacing a value "
                    "entered by hand]")
          != std::string::npos);
}

TEST_CASE("an overlay's constant that replaced a measured value, or none, says only who fixed it", "[record-join]")
{
    // Kills: the clause keyed on any known source rather than a typed-in one,
    // which would say "entered by hand" of a measurement.
    auto const [overMeasured, measuredText] =
        fixed_step_over(formula::environment(formula::Measured<Force> { formula::Rational { 1'000 } },
                                             formula::Measured<ShapeFactor> { formula::Rational { 1 } }));
    INFO(measuredText);
    CHECK(overMeasured.inputSource == formula::ValueSource::Measured);
    CHECK(measuredText.find("k_s = 97/100 [fixed by jurisdiction overlay: Example Standard 14:2022 NA, NA.1]")
          != std::string::npos);
    CHECK(measuredText.find("replacing") == std::string::npos);

    // No value held: nothing was replaced, and the step says it does not know
    // of one rather than inventing a source. Without the `provides` guard
    // this would not compile, asking the environment for a value it lacks.
    auto const [overNothing, nothingText] =
        fixed_step_over(formula::environment(formula::Measured<Force> { formula::Rational { 1'000 } }));
    INFO(nothingText);
    CHECK(overNothing.kind == formula::StepKind::OverriddenConstant);
    CHECK_FALSE(overNothing.inputSource.has_value());
    CHECK(nothingText.find("replacing") == std::string::npos);
}

TEST_CASE("one constant fixed here and inside a scope traces each step with its own record and source",
          "[record-join]")
{
    // The task 8 review's L1. This record's shape factor was measured and the
    // reference's typed in, so a step stamped with the other's origin, or the
    // two sources swapped, reads differently.
    constexpr auto fixedBoth = formula::apply(deriveRatio, formula::apply(fixShape, bothPlaces));
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cube>(fixedBoth, context(), formula::RecordingSink { recorded });

    std::size_t fixedSteps = 0;
    for (formula::Step<> const& step: recorded.steps)
    {
        if (step.kind != formula::StepKind::OverriddenConstant)
            continue;
        ++fixedSteps;
        if (formula::origin_of(recorded, step).has_value())
        {
            CHECK(formula::origin_of(recorded, step)->role() == "Reference");
            CHECK(step.inputSource == formula::ValueSource::ManuallyEntered);
        }
        else
            CHECK(step.inputSource == formula::ValueSource::Measured);
    }
    CHECK(fixedSteps == 2);
}

TEST_CASE("an overlay's constant that replaced an entry typed in empty says so, here and inside a scope",
          "[record-join]")
{
    // The task 8 review's M1: no value was replaced, and the line says what
    // the entry held, in the words a typed-in empty input has.
    auto const [overEmpty, emptyText] =
        fixed_step_over(formula::environment(formula::Measured<Force> { formula::Rational { 1'000 } },
                                             formula::entered(formula::Measured<ShapeFactor>::absent())));
    INFO(emptyText);
    CHECK(overEmpty.inputSource == formula::ValueSource::ManuallyEntered);
    CHECK(overEmpty.replacedEntryEmpty);
    CHECK(emptyText.find("k_s = 97/100 [fixed by jurisdiction overlay: Example Standard 14:2022 NA, NA.1, replacing a "
                         "value entered by hand as empty]")
          != std::string::npos);

    constexpr auto thereEmpty =
        formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                             formula::Measured<EdgeX> { formula::Rational { 139 } },
                             formula::Measured<EdgeY> { formula::Rational { 103 } },
                             formula::entered(formula::Measured<ShapeFactor>::absent()));
    constexpr auto emptyThere = formula::record_context(
        formula::record<formula::ThisRecord>(
            formula::record_key(formula::sample_id(17), formula::test_id(5)), here, formula::lineage<MaterialBatch>(4411)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), thereEmpty,
                                   formula::lineage<MaterialBatch>(4411)));
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cube>(overlaid, emptyThere, formula::RecordingSink { recorded, north });
    std::string const scopedText = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(scopedText);
    CHECK(scopedText.find("k = 97/100, from record Reference (sample 23, test 3) [fixed by jurisdiction overlay: "
                          "Example Standard 14:2022 NA, NA.1, replacing a value entered by hand as empty]")
          != std::string::npos);

    // A filled typed-in entry keeps the plain clause: the flag is not set.
    auto const [overFilled, filledText] =
        fixed_step_over(formula::environment(formula::Measured<Force> { formula::Rational { 1'000 } },
                                             formula::entered(formula::Measured<ShapeFactor> { formula::Rational { 1 } })));
    CHECK_FALSE(overFilled.replacedEntryEmpty);
    CHECK(filledText.find("as empty") == std::string::npos);
}

namespace
{
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
{
};

/// This record's five retained masses total 806 g; the reference's 703 g.
/// The ratio, 806/703, is neither 1 nor its own inverse, so a sum read from
/// the wrong record -- or the two swapped -- gives another value.
inline constexpr auto screensHere = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 130 } }, formula::Measured<Retained> { formula::Rational { 210 } },
    formula::Measured<Retained> { formula::Rational { 97 } }, formula::Measured<Retained> { formula::Rational { 340 } },
    formula::Measured<Retained> { formula::Rational { 29 } }));
inline constexpr auto screensThere = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 113 } }, formula::Measured<Retained> { formula::Rational { 197 } },
    formula::Measured<Retained> { formula::Rational { 89 } }, formula::Measured<Retained> { formula::Rational { 263 } },
    formula::Measured<Retained> { formula::Rational { 41 } }));
inline constexpr auto screensContext = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), screensThere));

inline constexpr auto retainedRatio =
    formula::sum(formula::series<Retained, 5>) / formula::from_record<Reference>(formula::sum(formula::series<Retained, 5>));
} // namespace

TEST_CASE("a series formula through a context reads as through this record's environment", "[record-join]")
{
    // Task 1 step 2's inheritance probe, over phase 12's accessors: the
    // context is this record's environment, `get_series` included.
    constexpr auto viaContext =
        formula::checked_evaluate_si<formula::Rational>(formula::sum(formula::series<Retained, 5>), screensContext);
    constexpr auto viaEnvironment =
        formula::checked_evaluate_si<formula::Rational>(formula::sum(formula::series<Retained, 5>), screensHere);
    STATIC_REQUIRE(viaContext == viaEnvironment);
    STATIC_REQUIRE(**viaContext == formula::Rational { 806, 1000 }); // 806 g, in kilograms
}

TEST_CASE("a series read from another record is stamped with its origin", "[record-join]")
{
    constexpr auto ratio = formula::checked_evaluate_si<formula::Rational>(retainedRatio, screensContext);
    STATIC_REQUIRE(**ratio == formula::Rational { 806, 703 });

    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_si<formula::Rational>(retainedRatio, screensContext,
                                                           formula::RecordingSink { recorded });
    std::size_t seriesSteps = 0;
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::SeriesVariable)
        {
            ++seriesSteps;
            bool const isReferences = step.elements.size() == 5 && step.elements[0] == formula::Rational { 113, 1000 };
            if (isReferences)
            {
                REQUIRE(formula::origin_of(recorded, step).has_value());
                CHECK(formula::origin_of(recorded, step)->role() == "Reference");
            }
            else
                CHECK_FALSE(formula::origin_of(recorded, step).has_value());
        }
    CHECK(seriesSteps == 2);

    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("m_r = 113 g; 197 g; 89 g; 263 g; 41 g, from record Reference (sample 23, test 3)")
          != std::string::npos);
    CHECK(text.find("m_r = 130 g; 210 g; 97 g; 340 g; 29 g\n") != std::string::npos);
}

namespace
{
struct Size: formula::Quantity<Size, "d", "particle size", unit::Metre>
{
};
struct Count: formula::Quantity<Count, "n", "particles in a class", unit::One>
{
};

// Phase 12's invented classes and sizes: three significant digits, none a
// preferred number or a sieve size.
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 127, 1), formula::band(127, 1, 197, 1),
                                                     formula::band(197, 1, 331, 1) };
inline constexpr auto particlesThere = formula::environment(formula::MeasuredObservations<Size, 7>(
    formula::Rational { 103 }, formula::Rational { 127 }, formula::Rational { 163 }, formula::Rational { 277 },
    formula::Rational { 113 }, formula::Rational { 197 }, formula::Rational { 241 }));
inline constexpr auto particlesContext = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), particlesThere));
} // namespace

TEST_CASE("raw observations read from another record are stamped with its origin", "[record-join]")
{
    // The observations path phase 12 added records its own step; read inside
    // a scope, that step carries the record and its line names it.
    constexpr auto countedThere = formula::from_record<Reference>(
        formula::sum(formula::binned<unit::Metre, sizeClasses>(formula::observations<Size, 7>)));
    constexpr auto total = formula::checked_evaluate_si<formula::Rational>(countedThere, particlesContext);
    STATIC_REQUIRE(**total == formula::Rational { 7 });

    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_si<formula::Rational>(countedThere, particlesContext,
                                                           formula::RecordingSink { recorded });
    std::size_t observationSteps = 0;
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::ObservationsVariable)
        {
            ++observationSteps;
            REQUIRE(formula::origin_of(recorded, step).has_value());
            CHECK(formula::origin_of(recorded, step)->role() == "Reference");
        }
    CHECK(observationSteps == 1);
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("d = 103 m; 127 m; 163 m; 277 m; 113 m; 197 m; 241 m, from record Reference (sample 23, test 3)")
          != std::string::npos);
}

TEST_CASE("a series typed in on another record says where it was read from, and that it was typed in",
          "[record-join]")
{
    // Task 10 step 5. The environment records whether a series was typed in,
    // and the series variable's evaluator reports it, as a single value's
    // does: the line names the record first and then the source, in the order
    // a single value's line uses.
    constexpr auto typedThere = formula::environment(formula::entered(formula::measured_series<Retained>(
        formula::Measured<Retained> { formula::Rational { 113 } }, formula::Measured<Retained> { formula::Rational { 197 } },
        formula::Measured<Retained> { formula::Rational { 89 } }, formula::Measured<Retained> { formula::Rational { 263 } },
        formula::Measured<Retained> { formula::Rational { 41 } })));
    STATIC_REQUIRE(decltype(typedThere)::is_entered_series<Retained>);
    constexpr auto typedContext = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), typedThere));

    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_si<formula::Rational>(retainedRatio, typedContext, formula::RecordingSink { recorded });
    std::size_t seriesSteps = 0;
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::SeriesVariable)
        {
            ++seriesSteps;
            formula::ValueSource const expected =
                formula::origin_of(recorded, step).has_value() ? formula::ValueSource::ManuallyEntered : formula::ValueSource::Measured;
            CHECK(step.inputSource == expected);
        }
    CHECK(seriesSteps == 2);
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("m_r = 113 g; 197 g; 89 g; 263 g; 41 g, from record Reference (sample 23, test 3), entered by hand\n")
          != std::string::npos);
    // This record's series was measured, and says nothing.
    CHECK(text.find("m_r = 130 g; 210 g; 97 g; 340 g; 29 g\n") != std::string::npos);
}

TEST_CASE("a series typed in on this record says it was typed in, with no record named", "[record-join]")
{
    constexpr auto typedHere = formula::environment(formula::entered(formula::measured_series<Retained>(
        formula::Measured<Retained> { formula::Rational { 130 } }, formula::Measured<Retained> { formula::Rational { 210 } },
        formula::Measured<Retained> { formula::Rational { 97 } }, formula::Measured<Retained> { formula::Rational { 340 } },
        formula::Measured<Retained> { formula::Rational { 29 } })));
    constexpr auto typedContext = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), typedHere),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), screensThere));

    // Through the context and through the plain environment alike.
    formula::Trace<> viaContext {};
    (void) formula::checked_evaluate_si<formula::Rational>(retainedRatio, typedContext,
                                                           formula::RecordingSink { viaContext });
    formula::Trace<> viaEnvironment {};
    (void) formula::checked_evaluate_si<formula::Rational>(formula::sum(formula::series<Retained, 5>), typedHere,
                                                           formula::RecordingSink { viaEnvironment });
    for (formula::Trace<> const* recorded: { &viaContext, &viaEnvironment })
    {
        std::string const text = formula::render_trace(*recorded, { .maxSteps = 40 });
        INFO(text);
        CHECK(text.find("m_r = 130 g; 210 g; 97 g; 340 g; 29 g, entered by hand\n") != std::string::npos);
    }
    // The reference's series was measured, and says only where it was read from.
    std::string const contextText = formula::render_trace(viaContext, { .maxSteps = 40 });
    CHECK(contextText.find("m_r = 113 g; 197 g; 89 g; 263 g; 41 g, from record Reference (sample 23, test 3)\n")
          != std::string::npos);
}

namespace
{
struct Opening: formula::Quantity<Opening, "d", "screen opening", unit::Millimetre>
{
};

// Invented openings: three significant digits, none a preferred number or a
// sieve size.
inline constexpr formula::BreakpointTable<5> openings { formula::breakpoint(103), formula::breakpoint(127),
                                                        formula::breakpoint(163), formula::breakpoint(197),
                                                        formula::breakpoint(241) };

/// The reference's retained masses along the openings, read at 139 mm: a
/// third of the way from 127 mm (197 g) to 163 mm (89 g), 161 g.
inline constexpr auto curveThere = formula::environment(
    formula::measured_series<Retained>(
        formula::Measured<Retained> { formula::Rational { 113 } }, formula::Measured<Retained> { formula::Rational { 197 } },
        formula::Measured<Retained> { formula::Rational { 89 } }, formula::Measured<Retained> { formula::Rational { 263 } },
        formula::Measured<Retained> { formula::Rational { 41 } }),
    formula::Measured<Opening> { formula::Rational { 139 } });
inline constexpr auto curveContext = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), curveThere));
} // namespace

TEST_CASE("a curve read inside a read from another record is stamped with that record", "[record-join]")
{
    // The curve's own line names no record, as no computed line does, so the
    // stamp shows only in `Step::record` -- the field a reader of the
    // structured trace relies on.
    constexpr auto alongThere = formula::from_record<Reference>(formula::interpolate_at(
        formula::curve(formula::domain<unit::Millimetre, openings>, formula::series<Retained, 5>), formula::var<Opening>));
    constexpr auto read = formula::checked_evaluate_si<formula::Rational>(alongThere, curveContext);
    STATIC_REQUIRE(**read == formula::Rational { 161, 1000 }); // 161 g, in kilograms

    formula::Trace<> recorded {};
    (void) formula::checked_evaluate_si<formula::Rational>(alongThere, curveContext, formula::RecordingSink { recorded });
    std::size_t curveSteps = 0;
    for (formula::Step<> const& step: recorded.steps)
        if (step.kind == formula::StepKind::CurvePairing || step.kind == formula::StepKind::CurveSplice)
        {
            ++curveSteps;
            REQUIRE(formula::origin_of(recorded, step).has_value());
            CHECK(formula::origin_of(recorded, step)->role() == "Reference");
        }
    CHECK(curveSteps == 1);
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("m_r = 113 g; 197 g; 89 g; 263 g; 41 g, from record Reference (sample 23, test 3)\n")
          != std::string::npos);
}

namespace
{
/// The trace of @p read over a context whose reference, holding @p Env, has
/// not been tested yet.
template <typename Env, typename Read>
std::string unbound_reference_trace(Read const& read)
{
    auto const notYetTested = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screensHere),
        formula::Record<Reference, Env>::unbound());
    formula::Trace<> recorded {};
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(read, notYetTested, formula::RecordingSink { recorded });
    CHECK(evaluated.has_value());
    CHECK_FALSE(evaluated->has_value());
    return formula::render_trace(recorded, { .maxSteps = 40 });
}
} // namespace

TEST_CASE("a reference holding a series, typed in or not, or observations, can be not yet tested", "[record-join]")
{
    // A screen analysis not done yet is as ordinary as a strength not
    // measured yet: the context's type is the same either way, and the read
    // is absent, with the trace saying no record was bound -- never a key.
    constexpr auto readSum = formula::from_record<Reference>(formula::sum(formula::series<Retained, 5>));
    using MeasuredScreens = std::remove_cv_t<decltype(screensThere)>;
    using TypedScreens = decltype(formula::environment(formula::entered(formula::measured_series<Retained>(
        formula::Measured<Retained> { formula::Rational { 113 } }, formula::Measured<Retained> { formula::Rational { 197 } },
        formula::Measured<Retained> { formula::Rational { 89 } }, formula::Measured<Retained> { formula::Rational { 263 } },
        formula::Measured<Retained> { formula::Rational { 41 } }))));
    for (std::string const& text: { unbound_reference_trace<MeasuredScreens>(readSum),
                                    unbound_reference_trace<TypedScreens>(readSum),
                                    unbound_reference_trace<std::remove_cv_t<decltype(particlesThere)>>(
                                        formula::from_record<Reference>(formula::sum(
                                            formula::binned<unit::Metre, sizeClasses>(formula::observations<Size, 7>)))) })
    {
        INFO(text);
        CHECK(text.find("from record Reference (no record bound) = (not measured)\n") != std::string::npos);
        CHECK(text.find("sample 23") == std::string::npos);
    }
}

TEST_CASE("an unbound record holds every kind of entry, each absent", "[record-join]")
{
    // One environment of every entry kind the library has: a measured and a
    // typed-in single value, a measured and a typed-in series, and raw
    // observations. The unbound record keeps the type and invents no value.
    constexpr auto everyKind = formula::environment(
        formula::Measured<Force> { formula::Rational { 1'000 } },
        formula::entered(formula::Measured<ShapeFactor> { formula::Rational { 1 } }),
        formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 113 } },
                                           formula::Measured<Retained> { formula::Rational { 197 } }),
        formula::entered(formula::measured_series<Opening>(formula::Measured<Opening> { formula::Rational { 127 } },
                                                           formula::Measured<Opening> { formula::Rational { 163 } })),
        formula::MeasuredObservations<Size, 3>(formula::Rational { 103 }, formula::Rational { 241 }));
    using EveryKind = std::remove_cv_t<decltype(everyKind)>;
    constexpr auto notYetTested = formula::Record<Reference, EveryKind>::unbound();
    STATIC_REQUIRE(std::is_same_v<std::remove_cvref_t<decltype(notYetTested.environment())>, EveryKind>);
    STATIC_REQUIRE(!notYetTested.is_bound());
    STATIC_REQUIRE(notYetTested.environment().get<Force>().is_absent());
    STATIC_REQUIRE(notYetTested.environment().get<ShapeFactor>().is_absent());
    STATIC_REQUIRE(notYetTested.environment().get_series<Retained, 2>()
                   == formula::MeasuredSeries<Retained, 2> { std::array<formula::Measured<Retained>, 2> {} });
    STATIC_REQUIRE(notYetTested.environment().get_series<Opening, 2>()
                   == formula::MeasuredSeries<Opening, 2> { std::array<formula::Measured<Opening>, 2> {} });
    STATIC_REQUIRE(notYetTested.environment().get_observations<Size, 3>() == formula::MeasuredObservations<Size, 3> {});
    // The kinds a person typed in stay typed in: the type says so, as a bound
    // record's does.
    STATIC_REQUIRE(EveryKind::is_entered<ShapeFactor>);
    STATIC_REQUIRE(EveryKind::is_entered_series<Opening>);
}
