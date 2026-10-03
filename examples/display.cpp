// SPDX-License-Identifier: Apache-2.0
//
// Displaying numbers: a decimal wherever it is the exact value, a rounding
// only where one is asked for, and std::format for the library's own values.
//
//   1. A trace of a soil specimen's moisture content, in the default
//      fractions, as exact decimals, rounded where no decimal ends, and padded
//      to each unit's declared decimals.
//   2. A value in a unit nobody declared, and a comparison, which no style
//      rounds.
//   3. The formula's own text: its typed numbers as decimals, never rounded
//      and never padded.
//   4. number_text and decimal_text: no <format>, no allocation, usable at
//      compile time.
//   5. std::format: every form of the spec, the width in code points, and a
//      spec refused at run time.
//   6. std::format of an outcome, a unit, a dimension and an enumeration.
//
// Every number here is invented.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <format>
#include <print>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPadding;
using formula::NumberStyle;
using formula::Rational;
using formula::RoundingMode;
using formula::var;
using namespace formula::literals;

// ---- Quantities -------------------------------------------------------------------
using WetMass = formula::Quantity<struct WetMassTag, "m_w", "wet specimen and dish", unit::Gram>;
using DryMass = formula::Quantity<struct DryMassTag, "m_d", "dried specimen and dish", unit::Gram>;
using MoistureContent = formula::Quantity<struct MoistureContentTag, "w", "moisture content", unit::Percent>;
using DishWeighing = formula::Quantity<struct DishWeighingTag, "t", "a weighing of the empty dish", unit::Gram>;
using DishMass = formula::Quantity<struct DishMassTag, "m_t", "mass of the empty dish", unit::Gram>;
using OvenTemperature = formula::Quantity<struct OvenTemperatureTag, "T", "oven temperature", unit::Celsius>;
using GrainSize = formula::Quantity<struct GrainSizeTag, "D", "grain size", unit::Micrometre>;

// ---- 1. The moisture content -----------------------------------------------------
// The water the specimen lost over its dry mass, the dish's typed 25.5 g taken off.
inline constexpr auto moistureContent =
    (var<WetMass> - var<DryMass>) / (var<DryMass> - formula::constant<unit::Gram>(25.5_r));

inline constexpr auto specimen =
    formula::environment(formula::Measured<WetMass> { 157.4_r }, formula::Measured<DryMass> { 144 });

// ---- 2. A value in a unit nobody declared, and a comparison ------------------------
// The mean of three weighings: their sum times a typed 1/3, which has no exact decimal.
inline constexpr auto dishMass = formula::sum(formula::series<DishWeighing, 3>) * formula::number(Rational { 1, 3 });

inline constexpr auto weighings = formula::environment(formula::measured_series<DishWeighing>(4.21_r, 4.23_r, 4.26_r));

inline constexpr formula::Envelope<2> atMostTwelve {
    formula::LimitRow { formula::unbounded, formula::limit(12) },
    formula::LimitRow { formula::unbounded, formula::limit(12) },
};
inline constexpr auto moistureLimit = formula::conformity<unit::Percent>(
    formula::series<MoistureContent, 2>, atMostTwelve, formula::Verdict { "dry the specimen again" });

inline constexpr auto twoSpecimens =
    formula::environment(formula::measured_series<MoistureContent>(Rational { 67, 6 }, Rational { 289, 24 }));

// ---- 3. The formula's text ---------------------------------------------------------
// A tare typed as a whole 24 g, to set a formula's text beside a trace's.
inline constexpr auto wholeTare = var<DryMass> - formula::constant<unit::Gram>(24);

// The dish's mean without a weighing further than a typed 1/30 of the pass's
// mean from it: a rejection, whose limit a documentation page states.
inline constexpr auto dishMean = formula::sample_mean(
    formula::without_outliers<formula::PerPass::MostExtreme,
                              formula::OnLimit::Keep,
                              formula::AtMost<1>,
                              formula::KeepAtLeast<2>>(
        formula::series<DishWeighing, 3>,
        formula::deviation_from_mean(Rational { 1, 30 } * formula::pass_mean<DishWeighing>),
        formula::Verdict { "weigh the dish again" }));

// ---- 4. number_text at compile time --------------------------------------------------
static_assert(formula::number_text(Rational { 3, 5 }, NumberStyle::exact_decimal(), unit::One) == "0.6");
static_assert(formula::number_text(Rational { 1, 3 }, NumberStyle::exact_decimal(), unit::One) == "1/3");

// ---- 5. std::format ----------------------------------------------------------------
/// One row of the std::format reference: the call as written, what it wrote,
/// and what it must write.
struct FormatRow
{
    std::string_view call;
    std::string written;
    std::string_view expected;
};

/// A reference row from a call: its text, as the source spells it, beside the
/// text the call writes, so the one printed can never differ from the one run.
#define FORMAT_ROW(expectedText, ...) FormatRow { #__VA_ARGS__, __VA_ARGS__, expectedText }

/// @p row as one line of the reference: the call, then its output, quoted
/// when it starts or ends with a space the eye would miss.
[[nodiscard]] std::string reference_line(FormatRow const& row)
{
    bool const quoted = !row.written.empty() && (row.written.front() == ' ' || row.written.back() == ' ');
    return std::format("{:<62} {}", row.call, quoted ? "\"" + row.written + "\"" : row.written);
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

    // ---- 1. A trace in every style --------------------------------------------------
    std::println("== 1. A trace in every style ==\n");

    auto const moisture = formula::checked_explain<MoistureContent>(moistureContent, specimen);
    if (!moisture)
    {
        std::println("moisture content: {}", moisture.error().error);
        return 1;
    }
    check(formula::number_of(moisture->outcome) == Rational { 2680, 237 }, "the moisture content is 2680/237 %");

    auto const exactStyle = NumberStyle::exact_decimal();
    auto const roundedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
    auto const paddedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven, DecimalPadding::Padded);
    std::string const fractions = formula::render_trace(moisture->trace, { .maxSteps = 20 });
    std::string const exactDecimals = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = exactStyle });
    std::string const rounded = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = roundedStyle });
    std::string const padded = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = paddedStyle });
    std::println("-- fractions, the default --\n{}", fractions);
    std::println("-- exact decimals --\n{}", exactDecimals);
    std::println("-- rounded where no decimal ends --\n{}", rounded);
    std::println("-- rounded and padded --\n{}", padded);
    check(exactDecimals.contains("#3 / #6 = 134/1185\n"), "no exact decimal, so a fraction");
    check(rounded.contains("#3 / #6 = \xe2\x89\x88" "0.113\n"), "rounded, and marked");
    check(padded.contains("m_d = 144.0 g\n"), "padded to the gram's one decimal");

    // ---- 2. A unit nobody declared, and a comparison ---------------------------------
    std::println("== 2. A unit nobody declared, and a comparison ==\n");

    auto const dish = formula::checked_explain<DishMass>(dishMass, weighings);
    if (!dish)
    {
        std::println("the dish's mass: {}", dish.error().error);
        return 1;
    }
    check(dish->outcome.is_value(), "the dish's mass is a value");
    std::string const dishTraceText = formula::render_trace(dish->trace, { .maxSteps = 20, .numbers = paddedStyle });
    std::println("{}", dishTraceText);
    formula::NumberText const dishText = formula::number_text(dish->outcome.measurement(), roundedStyle);
    std::println("the dish's mass in its declared grams: {}\n", dishText.view());
    check(dishTraceText.contains("t = 4.21 g; 4.23 g; 4.26 g\n"), "padding cuts no decimal short");
    check(dishTraceText.contains("3. 1/3\n"), "a typed number is never rounded");
    check(dishText == "\xe2\x89\x88" "4.2 g", "the declared result rounds at the gram's one decimal");

    auto const limitCheck = formula::explain_conformity(moistureLimit, twoSpecimens);
    std::string const limitText = formula::render_trace(limitCheck.trace, { .maxSteps = 20, .numbers = roundedStyle });
    std::println("{}", limitText);
    check(limitText.contains("67/6 % (at most 12 %)"), "a compared value is stated exactly");

    // ---- 3. The formula's text ----------------------------------------------------------
    std::println("== 3. The formula's text ==\n");

    formula::RenderOptions const decimals { .numbers = NumberStyle::exact_decimal() };
    std::string const defaultText = formula::render(moistureContent);
    std::string const decimalText = formula::render(moistureContent, decimals);
    std::string const latexText = formula::render<formula::Dialect::LaTeX>(moistureContent, decimals);
    formula::Documentation const page = formula::document(moistureContent, decimals);
    std::println("default:        {}", defaultText);
    std::println("exact decimals: {}", decimalText);
    std::println("LaTeX:          {}", latexText);
    std::println("document():     {}\n", page.formula);
    check(decimalText == "(m_w - m_d) / (m_d - 25.5 g)", "the typed tare as the decimal it is");

    formula::RenderOptions const rounding { .numbers = roundedStyle };
    std::string const dishFormula = formula::render(dishMass, rounding);
    formula::Documentation const dishPage = formula::document(dishMean, rounding);
    std::println("formula, rounded style:           {}", dishFormula);
    std::println("rejection's limit, rounded style: {}\n", dishPage.rejections.front().limit);
    check(dishFormula.contains("1/3"), "a typed number is never rounded in a formula's text");
    check(dishPage.rejections.front().limit.contains("1/30"), "nor in a documentation page's limit");

    auto const paddedDecimals = NumberStyle::exact_decimal(DecimalPadding::Padded);
    std::string const tareFormula = formula::render(wholeTare, { .numbers = paddedDecimals });
    std::string const tareTraceText = formula::render_trace(formula::trace_of<DryMass>(wholeTare, specimen),
                                                            { .maxSteps = 20, .numbers = paddedDecimals });
    std::println("formula, padded style: {}", tareFormula);
    std::println("trace, padded style:\n{}", tareTraceText);
    check(tareFormula == "m_d - 24 g" && tareTraceText.contains("2. 24.0 g\n"),
          "a formula states the typed 24 g, a trace pads it");
    check(tareTraceText.contains("3. #1 - #2 = 0.12 kg\n"), "a unit nobody declared is not padded");

    // ---- 4. number_text and decimal_text ------------------------------------------------
    std::println("== 4. number_text and decimal_text ==\n");

    formula::Measured<MoistureContent> const w = moisture->outcome.measurement();
    formula::Measured<MoistureContent> const notMeasured {};
    formula::NumberText const measuredText = formula::number_text(w, roundedStyle);
    formula::NumberText const absentText = formula::number_text(notMeasured, roundedStyle);
    formula::NumberText const twoPlaces =
        formula::decimal_text(w.value(), formula::DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    std::println("measured:     {}", measuredText.view());
    std::println("not measured: {}", absentText.view());
    std::println("two places:   {}\n", twoPlaces.view());
    check(measuredText == "\xe2\x89\x88" "11.3 %" && absentText == "(not measured)" && twoPlaces == "11.31",
          "number_text and decimal_text spell the moisture content");

    // ---- 5. std::format -------------------------------------------------------------------
    std::println("== 5. std::format ==\n");

    formula::Measured<WetMass> const wetMass { 157.4_r };
    formula::Measured<OvenTemperature> const oven { 58.3_r };
    formula::Measured<GrainSize> const grain { 217 };
    FormatRow const reference[] = {
        FORMAT_ROW("0.6", std::format("{}", Rational { 3, 5 })),
        FORMAT_ROW("1/3", std::format("{}", Rational { 1, 3 })),
        FORMAT_ROW("3/5", std::format("{:/}", Rational { 3, 5 })),
        FORMAT_ROW("118.26", std::format("{:.2HalfEven}", Rational { 23653, 200 })),
        FORMAT_ROW("118.27", std::format("{:.2HalfAwayFromZero}", Rational { 23653, 200 })),
        FORMAT_ROW("4.00", std::format("{:.2HalfEven}", Rational { 4 })),
        FORMAT_ROW("\xe2\x89\x88" "0.333", std::format("{:~.3HalfEven}", Rational { 1, 3 })),
        FORMAT_ROW("0.6", std::format("{:~.3HalfEven}", Rational { 3, 5 })),
        FORMAT_ROW("     0.6", std::format("{:>8}", Rational { 3, 5 })),
        FORMAT_ROW("**0.6**", std::format("{:*^7}", Rational { 3, 5 })),
        FORMAT_ROW("157.4 g", std::format("{}", wetMass)),
        FORMAT_ROW("2680/237 %", std::format("{}", w)),
        FORMAT_ROW("\xe2\x89\x88" "11.3 %", std::format("{:~HalfEven}", w)),
        FORMAT_ROW("11.31 %", std::format("{:.2HalfEven}", w)),
        FORMAT_ROW("(not measured)", std::format("{}", notMeasured)),
        FORMAT_ROW("  \xe2\x89\x88" "0.333", std::format("{:>8~.3HalfEven}", Rational { 1, 3 })),
        FORMAT_ROW(" 58.3 \xc2\xb0" "C", std::format("{:>8}", oven)),
        FORMAT_ROW("  217 \xc2\xb5" "m", std::format("{:>8}", grain)),
    };
    for (FormatRow const& row: reference)
    {
        std::println("{}", reference_line(row));
        check(row.written == row.expected, "a std::format reference row");
    }

    std::string_view const noMode = "{:.2}";
    try
    {
        (void) std::vformat(noMode, std::make_format_args(w));
        check(false, "a rounding with no mode is refused");
    }
    catch (std::format_error const& refusal)
    {
        std::println("\nstd::vformat(\"{{:.2}}\", ...) throws std::format_error:\n{}\n", refusal.what());
        check(std::string_view { refusal.what() }.starts_with("formula: "), "the refusal starts formula: ");
    }

    // ---- 6. Outcomes, units, dimensions and enumerations -------------------------------
    std::println("== 6. Outcomes, units, dimensions and enumerations ==\n");

    // A verdict as a rejection of the dish's weighings yields it when it cannot
    // settle, built directly here, and a dish nobody weighed.
    auto const reweigh = formula::Outcome<DishMass>::verdict({ "weigh the dish again" });
    auto const unweighed = formula::Outcome<DishMass>::empty();
    FormatRow const words[] = {
        FORMAT_ROW("2680/237 %", std::format("{}", moisture->outcome)),
        FORMAT_ROW("\xe2\x89\x88" "11.3 %", std::format("{:~HalfEven}", moisture->outcome)),
        FORMAT_ROW("(not measured)", std::format("{}", unweighed)),
        FORMAT_ROW("  weigh the dish again", std::format("{:22}", reweigh)),
        FORMAT_ROW("g   ", std::format("{:4}", unit::Gram)),
        FORMAT_ROW("M^1", std::format("{}", unit::Gram.dimension)),
        FORMAT_ROW("(dimensionless)", std::format("{}", unit::Percent.dimension)),
        FORMAT_ROW("derived", std::format("{}", moisture->outcome.source())),
        FORMAT_ROW("nearest, ties to even", std::format("{}", RoundingMode::HalfEven)),
    };
    for (FormatRow const& row: words)
    {
        std::println("{}", reference_line(row));
        check(row.written == row.expected, "a std::format row for an outcome, a unit, a dimension or an enumeration");
    }

    std::println("\nall checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
