// SPDX-License-Identifier: Apache-2.0
//
// Displaying numbers: a decimal wherever it is the exact value, a rounding
// only where one is asked for, and std::format for Rational and Measured.
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
//
// Every number here is invented.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdint>
#include <cstdio>
#include <format>
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

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

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
    (var<WetMass> - var<DryMass>) / (var<DryMass> - formula::constant<unit::Gram>(rat(51, 2)));

inline constexpr auto specimen =
    formula::environment(formula::Measured<WetMass> { rat(787, 5) }, formula::Measured<DryMass> { rat(144) });

// ---- 2. A value in a unit nobody declared, and a comparison ------------------------
// The mean of three weighings: their sum times a typed 1/3, which has no exact decimal.
inline constexpr auto dishMass = formula::sum(formula::series<DishWeighing, 3>) * formula::number(rat(1, 3));

inline constexpr auto weighings = formula::environment(
    formula::measured_series<DishWeighing>(formula::Measured<DishWeighing> { rat(421, 100) },
                                           formula::Measured<DishWeighing> { rat(423, 100) },
                                           formula::Measured<DishWeighing> { rat(426, 100) }));

inline constexpr formula::Envelope<2> atMostTwelve {
    formula::LimitRow { formula::unbounded, formula::limit(rat(12)) },
    formula::LimitRow { formula::unbounded, formula::limit(rat(12)) },
};
inline constexpr auto moistureLimit = formula::conformity<unit::Percent>(
    formula::series<MoistureContent, 2>, atMostTwelve, formula::Verdict { "dry the specimen again" });

inline constexpr auto twoSpecimens = formula::environment(formula::measured_series<MoistureContent>(
    formula::Measured<MoistureContent> { rat(67, 6) }, formula::Measured<MoistureContent> { rat(289, 24) }));

// ---- 3. The formula's text ---------------------------------------------------------
// A tare typed as a whole 24 g, to set a formula's text beside a trace's.
inline constexpr auto wholeTare = var<DryMass> - formula::constant<unit::Gram>(rat(24));

// The dish's mean without a weighing further than a typed 1/30 of the pass's
// mean from it: a rejection, whose limit a documentation page states.
inline constexpr auto dishMean = formula::sample_mean(
    formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
        formula::series<DishWeighing, 3>,
        formula::deviation_from_mean(rat(1, 30) * formula::pass_mean<DishWeighing>),
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

/// Prints @p label and @p spelled, a number `number_text` or `decimal_text` wrote.
void print_spelled(char const* label, formula::NumberText const& spelled)
{
    std::printf("%s%.*s\n", label, static_cast<int>(spelled.view().size()), spelled.view().data());
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

    // ---- 1. A trace in every style --------------------------------------------------
    std::printf("== 1. A trace in every style ==\n\n");

    formula::Trace<> trace {};
    auto const moisture =
        formula::checked_evaluate<MoistureContent>(moistureContent, specimen, formula::RecordingSink<> { trace });
    check(moisture.has_value() && moisture->measurement().value() == rat(2680, 237), "the moisture content is 2680/237 %");

    NumberStyle const exactStyle = NumberStyle::exact_decimal();
    NumberStyle const roundedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
    NumberStyle const paddedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven, DecimalPadding::Padded);
    std::string const fractions = formula::render_trace(trace, { .maxSteps = 20 });
    std::string const exactDecimals = formula::render_trace(trace, { .maxSteps = 20, .numbers = exactStyle });
    std::string const rounded = formula::render_trace(trace, { .maxSteps = 20, .numbers = roundedStyle });
    std::string const padded = formula::render_trace(trace, { .maxSteps = 20, .numbers = paddedStyle });
    std::printf("-- fractions, the default --\n%s\n", fractions.c_str());
    std::printf("-- exact decimals --\n%s\n", exactDecimals.c_str());
    std::printf("-- rounded where no decimal ends --\n%s\n", rounded.c_str());
    std::printf("-- rounded and padded --\n%s\n", padded.c_str());
    check(exactDecimals.find("#3 / #6 = 134/1185\n") != std::string::npos, "no exact decimal, so a fraction");
    check(rounded.find("#3 / #6 = \xe2\x89\x88" "0.113\n") != std::string::npos, "rounded, and marked");
    check(padded.find("m_d = 144.0 g\n") != std::string::npos, "padded to the gram's one decimal");

    // ---- 2. A unit nobody declared, and a comparison ---------------------------------
    std::printf("== 2. A unit nobody declared, and a comparison ==\n\n");

    formula::Trace<> dishTrace {};
    auto const dish = formula::checked_evaluate<DishMass>(dishMass, weighings, formula::RecordingSink<> { dishTrace });
    check(dish.has_value(), "the dish's mass is a value");
    std::string const dishTraceText = formula::render_trace(dishTrace, { .maxSteps = 20, .numbers = paddedStyle });
    std::printf("%s\n", dishTraceText.c_str());
    formula::NumberText const dishText = formula::number_text(dish->measurement(), roundedStyle);
    print_spelled("the dish's mass in its declared grams: ", dishText);
    std::printf("\n");
    check(dishTraceText.find("t = 4.21 g; 4.23 g; 4.26 g\n") != std::string::npos, "padding cuts no decimal short");
    check(dishTraceText.find("3. 1/3\n") != std::string::npos, "a typed number is never rounded");
    check(dishText == "\xe2\x89\x88" "4.2 g", "the declared result rounds at the gram's one decimal");

    formula::Trace<> limitTrace {};
    (void) formula::check_conformity(moistureLimit, twoSpecimens, formula::RecordingSink<> { limitTrace });
    std::string const limitText = formula::render_trace(limitTrace, { .maxSteps = 20, .numbers = roundedStyle });
    std::printf("%s\n", limitText.c_str());
    check(limitText.find("67/6 % (at most 12 %)") != std::string::npos, "a compared value is stated exactly");

    // ---- 3. The formula's text ----------------------------------------------------------
    std::printf("== 3. The formula's text ==\n\n");

    formula::RenderOptions const decimals { .numbers = NumberStyle::exact_decimal() };
    std::string const defaultText = formula::render(moistureContent);
    std::string const decimalText = formula::render(moistureContent, formula::DefaultVocabulary {}, decimals);
    std::string const latexText =
        formula::render<formula::Dialect::LaTeX>(moistureContent, formula::DefaultVocabulary {}, decimals);
    formula::Documentation const page = formula::document(moistureContent, formula::DefaultVocabulary {}, decimals);
    std::printf("default:        %s\n", defaultText.c_str());
    std::printf("exact decimals: %s\n", decimalText.c_str());
    std::printf("LaTeX:          %s\n", latexText.c_str());
    std::printf("document():     %s\n\n", page.formula.c_str());
    check(decimalText == "(m_w - m_d) / (m_d - 25.5 g)", "the typed tare as the decimal it is");

    formula::RenderOptions const rounding { .numbers = roundedStyle };
    std::string const dishFormula = formula::render(dishMass, formula::DefaultVocabulary {}, rounding);
    formula::Documentation const dishPage = formula::document(dishMean, formula::DefaultVocabulary {}, rounding);
    std::printf("formula, rounded style:           %s\n", dishFormula.c_str());
    std::printf("rejection's limit, rounded style: %s\n\n", dishPage.rejections.front().limit.c_str());
    check(dishFormula.find("1/3") != std::string::npos, "a typed number is never rounded in a formula's text");
    check(dishPage.rejections.front().limit.find("1/30") != std::string::npos, "nor in a documentation page's limit");

    NumberStyle const paddedDecimals = NumberStyle::exact_decimal(DecimalPadding::Padded);
    formula::Trace<> tareTrace {};
    (void) formula::checked_evaluate<DryMass>(wholeTare, specimen, formula::RecordingSink<> { tareTrace });
    std::string const tareFormula = formula::render(wholeTare, formula::DefaultVocabulary {}, { .numbers = paddedDecimals });
    std::string const tareTraceText = formula::render_trace(tareTrace, { .maxSteps = 20, .numbers = paddedDecimals });
    std::printf("formula, padded style: %s\n", tareFormula.c_str());
    std::printf("trace, padded style:\n%s\n", tareTraceText.c_str());
    check(tareFormula == "m_d - 24 g" && tareTraceText.find("2. 24.0 g\n") != std::string::npos,
          "a formula states the typed 24 g, a trace pads it");
    check(tareTraceText.find("3. #1 - #2 = 0.12\n") != std::string::npos, "a unit nobody declared is not padded");

    // ---- 4. number_text and decimal_text ------------------------------------------------
    std::printf("== 4. number_text and decimal_text ==\n\n");

    formula::Measured<MoistureContent> const w = moisture->measurement();
    formula::Measured<MoistureContent> const notMeasured = formula::Measured<MoistureContent>::absent();
    formula::NumberText const measuredText = formula::number_text(w, roundedStyle);
    formula::NumberText const absentText = formula::number_text(notMeasured, roundedStyle);
    formula::NumberText const twoPlaces =
        formula::decimal_text(w.value(), formula::DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    print_spelled("measured:     ", measuredText);
    print_spelled("not measured: ", absentText);
    print_spelled("two places:   ", twoPlaces);
    std::printf("\n");
    check(measuredText == "\xe2\x89\x88" "11.3 %" && absentText == "(not measured)" && twoPlaces == "11.31",
          "number_text and decimal_text spell the moisture content");

    // ---- 5. std::format -------------------------------------------------------------------
    std::printf("== 5. std::format ==\n\n");

    formula::Measured<WetMass> const wetMass { rat(787, 5) };
    formula::Measured<OvenTemperature> const oven { rat(583, 10) };
    formula::Measured<GrainSize> const grain { rat(217) };
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
        std::printf("%s\n", reference_line(row).c_str());
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
        std::printf("\nstd::vformat(\"{:.2}\", ...) throws std::format_error:\n%s\n\n", refusal.what());
        check(std::string_view { refusal.what() }.starts_with("formula: "), "the refusal starts formula: ");
    }

    std::printf("all checks passed: %s\n", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
