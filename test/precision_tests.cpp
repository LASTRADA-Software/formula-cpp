// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational::make(numerator, denominator).value();
}

struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", unit::Gram>
{
};
/// What a precision limit evaluates to, in grams.
struct Tolerance: formula::Quantity<Tolerance, "r_g", "precision limit", unit::Gram>
{
};

// Fixture P, invented: 40.0 g and 40.905 g, so d = 0.905 g, and
// r(level) = 0.1 g + level / 50. The level is the mean, 40.4525 g, which gives
// r = 0.90905 g: satisfied. The level taken as x_A (40 g), the level rounded
// to 0 dp (40 g), and an unbound level read as 0 each give r <= 0.9 g: violated.
inline constexpr auto pairP =
    formula::environment(formula::Measured<ResultA> { rat(40) }, formula::Measured<ResultB> { rat(40905, 1000) });

inline constexpr auto meanOfPair = (var<ResultA> + var<ResultB>) / rat(2);

/// r(level) = 0.1 g + level / 50, the limit expression of fixture P.
inline constexpr auto limitOfLevel =
    formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<ResultA>;

template <formula::PrecisionKind K, typename Level>
[[nodiscard]] constexpr auto agreementAt(Level levelExpression)
{
    return formula::constraint(formula::abs(var<ResultA> - var<ResultB>)
                                   <= formula::precision_limit<K>(levelExpression, limitOfLevel),
                               formula::Verdict { "repeat the determinations" });
}

// Fixture Q, invented: level bands [0, 20), [20, 60), [60, 100) g give
// r = 0.5, 0.9, 1.4 g.
inline constexpr formula::BandTable<3> LevelBands { formula::band(0, 1, 20, 1),
                                                    formula::band(20, 1, 60, 1),
                                                    formula::band(60, 1, 100, 1) };
} // namespace

TEST_CASE("a precision level told without its entry is dropped, never read off an empty stack", "[precision][trace]")
{
    // The hook is public: a consumer's own node, or a second sink that
    // cleared the marks, can tell it with nothing entered. Reading back()
    // of the empty stack would be undefined behaviour; the step is dropped.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    sink.precision_level_produced(formula::PrecisionKind::Repeatability,
                                  unit::Gram,
                                  formula::Evaluated<Rational> { std::optional { Rational { 40 } } });
    CHECK(trace.steps.empty());
    CHECK(trace.marks.empty());
    CHECK(trace.precisionRecords.empty());
}

TEST_CASE("a precision limit that depends on the level is evaluated at the level of the results it checks", "[precision]")
{
    constexpr auto agree = agreementAt<formula::PrecisionKind::Repeatability>(meanOfPair);
    STATIC_REQUIRE(formula::check(agree, pairP).is_satisfied());

    // The limit's own value: 0.90905 g, the one number that separates the
    // mean from the first result (0.9 g) and from an unbound level (0.1 g).
    constexpr auto r = formula::precision_limit<formula::PrecisionKind::Repeatability>(meanOfPair, limitOfLevel);
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(r, pairP)->measurement().value() == rat(90905, 100000));
    STATIC_REQUIRE(decltype(r)::dimension == formula::dim::Mass);
}

TEST_CASE("rounding the level before it enters the limit is the author's, and it can change the verdict", "[precision]")
{
    // The mean rounded to 0 dp is 40 g, so r = 0.9 g < 0.905 g: violated.
    constexpr auto agree = agreementAt<formula::PrecisionKind::Repeatability>(
        formula::rounded<unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(meanOfPair));
    STATIC_REQUIRE(formula::check(agree, pairP).is_violated());
}

TEST_CASE("a constant per level read from a table uses the band the level falls in (fixture Q)", "[precision]")
{
    constexpr auto r = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        meanOfPair,
        formula::banded_lookup<unit::Gram, LevelBands, unit::Gram>(formula::precision_level<ResultA>,
                                                                   { rat(1, 2), rat(9, 10), rat(7, 5) }));
    constexpr auto agree =
        formula::constraint(formula::abs(var<ResultA> - var<ResultB>) <= r, formula::Verdict { "repeat" });
    // 19.6 g and 20.5 g: the level 20.05 g falls in band 2, r = 0.9 g, and
    // d = 0.9 g, so the closed limit is satisfied. The level taken as x_A,
    // 19.6 g, would select band 1 and 0.5 g.
    constexpr auto pairQ =
        formula::environment(formula::Measured<ResultA> { rat(196, 10) }, formula::Measured<ResultB> { rat(205, 10) });
    STATIC_REQUIRE(formula::check(agree, pairQ).is_satisfied());
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(r, pairQ)->measurement().value() == rat(9, 10));
    // Fixture P's pair also falls in band 2, where d = 0.905 g exceeds 0.9 g.
    STATIC_REQUIRE(formula::check(agree, pairP).is_violated());
}

TEST_CASE("a reproducibility limit is the same arithmetic under a different name", "[precision]")
{
    constexpr auto reproducibility =
        formula::precision_limit<formula::PrecisionKind::Reproducibility>(meanOfPair, limitOfLevel);
    STATIC_REQUIRE(decltype(reproducibility)::kind == formula::PrecisionKind::Reproducibility);
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(reproducibility, pairP)->measurement().value()
                   == rat(90905, 100000));
    STATIC_REQUIRE(formula::check(agreementAt<formula::PrecisionKind::Reproducibility>(meanOfPair), pairP).is_satisfied());
}

TEST_CASE("a nested precision limit reads its own level, not the outer one", "[precision]")
{
    // The outer limit binds x_A (40 g); the inner binds x_B (40.905 g) and
    // reads its level; the outer then reads its own. 40.905 + 40 = 80.905 g.
    // An inner limit reading the outer level would give 80 g, and an outer
    // one reading the inner level 81.81 g.
    constexpr auto inner =
        formula::precision_limit<formula::PrecisionKind::Repeatability>(var<ResultB>, formula::precision_level<ResultA>);
    constexpr auto outer = formula::precision_limit<formula::PrecisionKind::Reproducibility>(
        var<ResultA>, inner + formula::precision_level<ResultA>);
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(outer, pairP)->measurement().value() == rat(80905, 1000));
}

TEST_CASE("an absent result makes the level, the limit and the check absent or not checked", "[precision]")
{
    constexpr auto halfPair =
        formula::environment(formula::Measured<ResultA> { rat(40) }, formula::Measured<ResultB>::absent());
    constexpr auto r = formula::precision_limit<formula::PrecisionKind::Repeatability>(meanOfPair, limitOfLevel);
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(r, halfPair)->is_empty());
    // NotChecked, never Satisfied: an unknown spread is not an agreeing one.
    STATIC_REQUIRE(
        formula::check(agreementAt<formula::PrecisionKind::Repeatability>(meanOfPair), halfPair).is_not_checked());
}

TEST_CASE("abs is the absolute value, and keeps the dimension", "[precision]")
{
    // x_A - x_B is negative here, so returning the operand unchanged gives
    // -0.905 g.
    constexpr auto spread = formula::abs(var<ResultA> - var<ResultB>);
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(spread, pairP)->measurement().value() == rat(905, 1000));
    STATIC_REQUIRE(
        formula::checked_evaluate<Tolerance>(formula::abs(var<ResultB> - var<ResultA>), pairP)->measurement().value()
        == rat(905, 1000));
    STATIC_REQUIRE(decltype(spread)::dimension == formula::dim::Mass);
    constexpr auto halfPair =
        formula::environment(formula::Measured<ResultA> { rat(40) }, formula::Measured<ResultB>::absent());
    STATIC_REQUIRE(formula::checked_evaluate<Tolerance>(spread, halfPair)->is_empty());
}

TEST_CASE("a precision limit evaluates at runtime too", "[precision]")
{
    // The cases above are evaluated by the compiler; these run.
    auto const agree = agreementAt<formula::PrecisionKind::Repeatability>(meanOfPair);
    CHECK(formula::check(agree, pairP).is_satisfied());
    auto const r = formula::precision_limit<formula::PrecisionKind::Repeatability>(meanOfPair, limitOfLevel);
    auto const evaluated = formula::checked_evaluate<Tolerance>(r, pairP);
    REQUIRE(evaluated.has_value());
    CHECK(evaluated->measurement().value() == rat(90905, 100000));
}

TEST_CASE("the two passes of a precision limit are two steps, and the limit names the level it was evaluated at",
          "[precision][trace-render]")
{
    // Fixture P: pass 1 is the mean, 16181/400 g = 40.4525 g, shown in the
    // unit of the quantity the placeholder names; the placeholder says which
    // limit bound it; pass 2 names its level step and its limit's value.
    formula::Trace<> trace {};
    (void) formula::check(
        agreementAt<formula::PrecisionKind::Repeatability>(meanOfPair), pairP, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. x_A = 40 g\n"
             "2. x_B = 8181/200 g\n"
             "3. #1 - #2 = -181/200000\n"
             "4. abs(#3) = 181/200000\n"
             "5. x_A = 40 g\n"
             "6. x_B = 8181/200 g\n"
             "7. #5 + #6 = 16181/200000\n"
             "8. 2\n"
             "9. #7 / #8 = 16181/400000\n"
             "10. level (pass 1 of 2) = #9 = 16181/400 g\n"
             "11. 1/10 g\n"
             "12. 1/50\n"
             "13. level = 16181/400 g [bound by #16]\n"
             "14. #12 * #13 = 16181/20000000\n"
             "15. #11 + #14 = 18181/20000000\n"
             "16. r at level #10 (pass 2 of 2) = #15 = 18181/20000000\n"
             "17. require #4 <= #16 [satisfied]\n");

    // The records behind those lines, one per precision step, keyed by step.
    REQUIRE(trace.precisionRecords.size() == 3);
    CHECK(trace.precisionRecords[0].step == 9);
    CHECK(trace.precisionRecords[0].role == formula::detail::PrecisionStepRole::LevelPass);
    CHECK(trace.precisionRecords[1].step == 12);
    CHECK(trace.precisionRecords[1].role == formula::detail::PrecisionStepRole::Placeholder);
    CHECK(trace.precisionRecords[1].levelStep == 9);
    CHECK(trace.precisionRecords[2].step == 15);
    CHECK(trace.precisionRecords[2].kind == formula::PrecisionKind::Repeatability);
    CHECK(trace.precisionRecords[2].limitStep == 15);
    CHECK(trace.precisionBindings.empty());
}

TEST_CASE("a nested precision limit's trace says which level each limit was evaluated at", "[precision][trace-render]")
{
    constexpr auto inner =
        formula::precision_limit<formula::PrecisionKind::Repeatability>(var<ResultB>, formula::precision_level<ResultA>);
    constexpr auto outer = formula::precision_limit<formula::PrecisionKind::Reproducibility>(
        var<ResultA>, inner + formula::precision_level<ResultA>);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Tolerance>(outer, pairP, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. x_A = 40 g\n"
             "2. level (pass 1 of 2) = #1 = 40 g\n"
             "3. x_B = 8181/200 g\n"
             "4. level (pass 1 of 2) = #3 = 8181/200 g\n"
             "5. level = 8181/200 g [bound by #6]\n"
             "6. r at level #4 (pass 2 of 2) = #5 = 8181/200000\n"
             "7. level = 40 g [bound by #9]\n"
             "8. #6 + #7 = 16181/200000\n"
             "9. R at level #2 (pass 2 of 2) = #8 = 16181/200000\n");
}

TEST_CASE("a precision step with no record says so rather than guess", "[precision][trace-render]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Tolerance>(
        formula::precision_limit<formula::PrecisionKind::Repeatability>(meanOfPair, limitOfLevel),
        pairP,
        formula::RecordingSink<> { trace });
    trace.precisionRecords.clear();
    std::string const text = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(text.find("level (its record is missing)") != std::string::npos);
    CHECK(text.find("precision limit (its record is missing)") != std::string::npos);
}

TEST_CASE("a precision limit renders both its passes, in words a reader can check", "[precision][render]")
{
    constexpr auto agree = agreementAt<formula::PrecisionKind::Repeatability>(meanOfPair);
    CHECK(formula::render(agree) == "require abs(x_A - x_B) <= r(1/10 g + 1/50 * level; level = (x_A + x_B) / 2)");
    CHECK(formula::render<formula::Dialect::Markdown>(agree)
          == "require abs(`x_A` - `x_B`) <= r(1/10 g + 1/50 * level; level = (`x_A` + `x_B`) / 2)");
    CHECK(formula::render<formula::Dialect::LaTeX>(agree)
          == "\\text{require } \\left|x_A - x_B\\right| \\leq r\\left(1/10\\,\\mathrm{g} + 1/50 \\cdot "
             "\\text{level}\\right)\\Big|_{\\text{level} = \\frac{x_A + x_B}{2}}");
    CHECK(formula::render(formula::precision_limit<formula::PrecisionKind::Reproducibility>(meanOfPair, limitOfLevel))
          == "R(1/10 g + 1/50 * level; level = (x_A + x_B) / 2)");
    // The author's rounding of the level is on the page, where it can be read.
    CHECK(
        formula::render(formula::precision_limit<formula::PrecisionKind::Repeatability>(
            formula::rounded<unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(meanOfPair),
            limitOfLevel))
        == "r(1/10 g + 1/50 * level; level = round((x_A + x_B) / 2, to 0 dp of g))");
}

TEST_CASE("a precision limit documents its results, and never the level as an input", "[precision][document]")
{
    // The placeholder names Tolerance, which nothing reads: it names the
    // level's unit, and the page must not list it as something measured.
    formula::Documentation const documentation =
        formula::document(formula::precision_limit<formula::PrecisionKind::Repeatability>(
            meanOfPair, rat(1, 50) * formula::precision_level<Tolerance>));
    CHECK(documentation.formula == "r(1/50 * level; level = (x_A + x_B) / 2)");
    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "x_A" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "x_B" });
}
