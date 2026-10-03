// SPDX-License-Identifier: Apache-2.0
//
// Generates docs/gallery.md: a page produced by RUNNING this library over a
// handful of invented formulas, rather than written by hand, so the page
// cannot drift from the code that documents itself the moment a symbol or a
// unit changes.
//
// docs/gallery.md is generated and checked in -- a reader browsing GitHub
// sees the page without building anything. After changing a formula below,
// regenerate it with:
//
//     <build-dir>/tools/gallery/formula-cpp-gallery docs/gallery.md
//
// `gallery.is-current` (tools/gallery/CMakeLists.txt,
// cmake/CheckGalleryIsCurrent.cmake) runs that same comparison in CI and
// fails, naming this exact command, when the checked-in page has drifted from
// what this program produces.
//
// Every formula and citation below is INVENTED for this library's own
// documentation -- generic physics with fictional `Example Standard`
// citations, matching every test and example elsewhere in this repository.
// No real standard is named or transcribed anywhere in this file.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdint>
#include <cstdio>
#include <expected>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::var;

// Two composed units this gallery needs that unit.hpp does not name: a
// density (mass over volume) and a flow rate (volume over time). Declared
// locally, exactly as a downstream formula library would declare a unit of
// its own -- neither is common enough across the library's own tests to earn
// a place in unit.hpp.
inline constexpr formula::Unit KilogramPerCubicMetre { .dimension = formula::dim::Density,
                                                       .symbolText = formula::symbol("kg/m3"),
                                                       .decimals = 1 };
inline constexpr formula::Unit LitrePerSecond { .dimension = formula::dim::Volume / formula::dim::Time,
                                                .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1000,
                                                .symbolText = formula::symbol("l/s"),
                                                .decimals = 2 };

// ---- 1: a density ----------------------------------------------------------

using SpecimenMass = formula::Quantity<struct SpecimenMassTag, "m", "specimen mass", unit::Kilogram>;
using SpecimenVolume = formula::Quantity<struct SpecimenVolumeTag, "V", "specimen volume", unit::CubicMetre>;
using BulkDensity = formula::Quantity<struct BulkDensityTag, "rho", "bulk density", KilogramPerCubicMetre>;

constexpr auto density =
    formula::documented(var<SpecimenMass> / var<SpecimenVolume>,
                        { .title = "Bulk density of a compacted specimen",
                          .reference = "Example Standard 1:2020",
                          .section = "4.2",
                          .equation = "(3)",
                          .text = "Bulk density is the specimen's mass divided by its volume, both measured "
                                  "under standard conditions." });

// ---- 2: a circular area -----------------------------------------------------

using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using CircularArea = formula::Quantity<struct CircularAreaTag, "A", "cross-sectional area", unit::SquareMetre>;

// No equation number this time, deliberately: the symbol table and the
// citation block below both show only the fields a formula actually has.
constexpr auto circularArea =
    formula::documented(formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 },
                        { .title = "Circular cross-sectional area",
                          .reference = "Example Standard 2:2020",
                          .section = "5.1",
                          .text = "The area of a circular cross-section computed from its diameter." });

// ---- 2a: a constraint, alongside the circular area it validates ------------
//
// Not every relationship a standard states computes something -- some exist
// purely to validate a measured input before it ever reaches a formula like
// circularArea above. A specimen wider than the die diameter cannot be
// tested at all, so the method rejects it outright rather than reporting an
// area for it.
constexpr auto maximumDiameter =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 139 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" },
                        { .title = "Maximum specimen diameter",
                          .reference = "Example Standard 6:2020",
                          .section = "4.1",
                          .text = "A specimen wider than the die diameter cannot be tested and is rejected "
                                  "outright." });

// ---- 3: a flow rate ----------------------------------------------------------

using DischargedVolume = formula::Quantity<struct DischargedVolumeTag, "V", "volume discharged", unit::Litre>;
using ElapsedTime = formula::Quantity<struct ElapsedTimeTag, "t", "elapsed time", unit::Second>;
using FlowRate = formula::Quantity<struct FlowRateTag, "Q", "volumetric flow rate", LitrePerSecond>;

// Sparser still: no section, no equation, just a reference and the prose.
constexpr auto flowRate =
    formula::documented(var<DischargedVolume> / var<ElapsedTime>,
                        { .title = "Volumetric flow rate",
                          .reference = "Example Standard 3:2020",
                          .text = "Flow rate is the volume discharged divided by the time taken to discharge it." });

// ---- 4: a water/cement ratio, evaluated below too ----------------------------

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

constexpr auto waterCementRatio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio",
                          .reference = "Example Standard 4:2020",
                          .section = "6.3",
                          .equation = "(2)",
                          .text = "Ratio of the effective water content to the cement content of a batch." });

// ---- 5: a conditional, evaluated below for its worked derivation ------------

// A density read directly off a specimen, distinct from `BulkDensity` above
// (which is a formula's *result*, not a variable a formula can read): this
// conditional's predicate and both its branches need a leaf they can read
// from the environment.
using MeasuredDensity =
    formula::Quantity<struct MeasuredDensityTag, "rho_m", "measured bulk density", KilogramPerCubicMetre>;
using AdjustedBulkDensity =
    formula::Quantity<struct AdjustedBulkDensityTag, "rho_adj", "compaction-adjusted bulk density", KilogramPerCubicMetre>;

// A numeric threshold selects between two formulas, not an if/else a caller
// has to remember to apply: a specimen compacted below the reference density
// is corrected upward by a fixed factor, and one at or above it is reported
// exactly as measured.
constexpr auto compactionAdjustedDensity = formula::documented(
    formula::when(var<MeasuredDensity> < formula::constant<KilogramPerCubicMetre>(formula::Rational { 1737 }),
                  var<MeasuredDensity>* formula::Rational { 1127, 1000 },
                  var<MeasuredDensity>),
    { .title = "Compaction-adjusted bulk density",
      .reference = "Example Standard 5:2020",
      .section = "4.5",
      .text = "A specimen compacted below the reference density is corrected upward by a fixed factor; "
              "one at or above it is reported as measured." });

// ---- 6: three lookup tables, one per kind ----------------------------------
//
// A method's own algebra sometimes needs a number no formula computes -- the
// method's procedure publishes it as a table instead. Three kinds, and they
// are not interchangeable: a banded table buckets a measurement into an
// interval, an exact table is keyed on a category, and an interpolating table
// computes a value between two rows that appears in no row at all.
//
// The tables are stated in megapascals and in plain dimensionless factors.
// They were chosen while `render<Dialect::LaTeX>` still wrote a unit's symbol
// unescaped, when a percent-valued row failed to typeset: `%` is TeX's
// comment character. The library now escapes it (`detail::latex_math_words`),
// so a table in percent typesets too; these were left as they are.

using CrushingStrength = formula::Quantity<struct CrushingStrengthTag, "f", "measured crushing strength", unit::Megapascal>;
using CuringAge = formula::Quantity<struct CuringAgeTag, "t", "curing age at test", unit::Hour>;
using SizeAllowance = formula::Quantity<struct SizeAllowanceTag, "k_d", "size allowance", unit::Megapascal>;
using CorrectedStrength =
    formula::Quantity<struct CorrectedStrengthTag, "f_c", "size- and age-corrected strength", unit::Megapascal>;

// Three bands over the diameter `Diameter` already declares, each stated as a
// numerator/denominator pair for its low (inclusive) and high (EXCLUSIVE)
// bound. A gap or an overlap anywhere here is a compile error naming both
// offending bands, so this table cannot reach the page mis-bucketing anything.
inline constexpr formula::BandTable<3> GallerySizeBands {
    formula::band(0, 1, 103, 1),   // 0 to under 103 mm
    formula::band(103, 1, 163, 1), // 103 to under 163 mm
    formula::band(163, 1, 197, 1), // 163 to under 197 mm -- 197 mm itself is in NO band
};

// A key enumeration wants a name of its own per translation unit: two files
// declaring a same-named internal-linkage enumeration, used as a `KeyTable`
// non-type template parameter with equal values, silently mislink on clang
// (`lookup.hpp` measures the mechanism). Hence `GalleryMould` and not `Mould`.
//
// **Numbered explicitly, and not contiguously, on purpose.** An exact lookup
// renders a key by its enumerator's name -- `key Cylinder` -- recovered at
// compile time (`enumerator.hpp`), and falls back to the underlying value only
// for a key that names no row. Numbered anyway so that a renderer printing a
// number where a name belongs would put 3, 7 or 11 on the page, which cannot
// pass for anything, rather than a plausible 0, 1, 2.
enum class GalleryMould : std::uint8_t
{
    Cube = 3,
    Cylinder = 7,
    Prism = 11,
};

inline constexpr formula::KeyTable<GalleryMould, 3> GalleryMouldKeys {
    GalleryMould::Cube,
    GalleryMould::Cylinder,
    GalleryMould::Prism,
};

// Breakpoints, not bands: each is one key the curve states a value AT, and the
// value between two of them is computed rather than stored. The domain is
// closed at both ends -- 31 h and 197 h are both hits -- because a breakpoint
// is a row and not a boundary between rows.
inline constexpr formula::BreakpointTable<3> GalleryAgeCurve {
    formula::breakpoint(31),
    formula::breakpoint(83),
    formula::breakpoint(197),
};

// The structure (the bands, the keys, the breakpoints, and the two units) is
// the method and lives in each node's type; the contents -- the number each row
// gives -- are a registered table's data and arrive at runtime.
constexpr auto sizeAllowanceTable = formula::banded_lookup<unit::Millimetre, GallerySizeBands, unit::Megapascal>(
    var<Diameter>, { formula::Rational { 237, 100 }, formula::Rational { 113, 100 }, formula::Rational { 41, 100 } });

constexpr auto mouldFactorTable = formula::exact_lookup<GalleryMouldKeys, unit::One>(
    GalleryMould::Cylinder,
    { formula::Rational { 1061, 1000 }, formula::Rational { 863, 1000 }, formula::Rational { 781, 1000 } });

constexpr auto maturityFactorTable = formula::interpolating_lookup<unit::Hour, GalleryAgeCurve, unit::One>(
    var<CuringAge>, { formula::Rational { 613, 1000 }, formula::Rational { 857, 1000 }, formula::Rational { 1031, 1000 } });

constexpr auto sizeAllowance =
    formula::documented(sizeAllowanceTable,
                        { .title = "Size allowance by specimen diameter",
                          .reference = "Example Standard 7:2020",
                          .section = "8.2",
                          .text = "The allowance deducted from a measured crushing strength, selected by the band "
                                  "the specimen's diameter falls in. A diameter in no band is not a value: the "
                                  "method defined no allowance there and this library reports that rather than "
                                  "inventing one." });

constexpr auto mouldFactor =
    formula::documented(mouldFactorTable,
                        { .title = "Mould factor by specimen mould",
                          .reference = "Example Standard 7:2020",
                          .section = "8.3",
                          .text = "A category key names a row directly, and renders under its enumerator's name. "
                                  "An author whose published table words a row differently spells it once, for "
                                  "the whole enumeration, through formula::EnumeratorName." });

constexpr auto maturityFactor =
    formula::documented(maturityFactorTable,
                        { .title = "Maturity factor by curing age",
                          .reference = "Example Standard 7:2020",
                          .section = "8.4",
                          .equation = "(7)",
                          .text = "A curve stated at three ages. A specimen tested between two of them gets the "
                                  "value those two rows imply at that age -- a number appearing in no row of the "
                                  "table. A specimen younger or older than the curve gets nothing at all: there "
                                  "is no extrapolation." });

// All three at once, wrapped exactly once so the section below has exactly one
// citation, the same way every other formula on this page does.
constexpr auto correctedStrength = formula::documented(
    (var<CrushingStrength> - sizeAllowanceTable) * mouldFactorTable * maturityFactorTable,
    { .title = "Size- and age-corrected crushing strength",
      .reference = "Example Standard 7:2020",
      .section = "8.5",
      .equation = "(8)",
      .text = "The measured strength less its size allowance, scaled by the mould factor and by the maturity "
              "factor -- one banded, one exact and one interpolating table inside a single expression." });

// ---- A method, and the method a jurisdiction's overlay yields --------------
//
// Two specimen shapes, one rounding rule, one acceptance check. The overlay
// replaces the acceptance check with two of its own, fixes the shape factor the
// base method reads from the specimen -- inside the new checks too, because the
// constant is listed after them -- and reports in its own unit, to two decimals
// of N/mm2. Each change is said in the trace, with what the jurisdiction cited.

struct Cube
{
};
struct Cylinder
{
};

using FailureLoad = formula::Quantity<struct FailureLoadTag, "F", "maximum load at failure", unit::Newton>;
using LoadedEdge = formula::Quantity<struct LoadedEdgeTag, "a", "loaded edge", unit::Millimetre>;
using ShapeFactor = formula::Quantity<struct ShapeFactorTag, "k_s", "shape factor", unit::One>;

constexpr auto cubeStrengthMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<FailureLoad> / formula::pow<2>(var<LoadedEdge>)),
                      formula::variant<Cylinder>(formula::constant<unit::One>(formula::Rational { 4 }) * var<FailureLoad>
                                                 / (formula::pi * formula::pow<2>(var<Diameter>)))),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(
        formula::constraint(var<FailureLoad> >= formula::constant<unit::Kilonewton>(formula::Rational { 163 }),
                            formula::Verdict { "the load at failure is below 163 kN" })));

constexpr formula::Citation galleryAcceptanceAnnex { .title = "Acceptance",
                                                     .reference = "Example Standard 7:2020 NA",
                                                     .section = "NA.6" };
constexpr formula::Citation galleryAnnex { .title = "Shape factor",
                                           .reference = "Example Standard 7:2020 NA",
                                           .section = "NA.2" };
constexpr formula::Citation galleryRoundingAnnex { .reference = "Example Standard 7:2020 NA", .section = "NA.4" };

constexpr auto galleryOverlay = formula::overlay(
    formula::with_constraints(
        formula::constraints(
            formula::constraint(var<FailureLoad> >= formula::constant<unit::Kilonewton>(formula::Rational { 277 }),
                                formula::Verdict { "the load at failure is below 277 kN" }),
            formula::constraint(var<ShapeFactor> <= formula::number(formula::Rational { 913, 1000 }),
                                formula::Verdict { "the shape factor exceeds 913/1000" })),
        galleryAcceptanceAnnex),
    formula::with_constant<ShapeFactor>(formula::Rational { 887, 1000 }, galleryAnnex),
    formula::with_rounding<unit::NewtonPerSquareMillimetre,
                           formula::DecimalPlaces { 2 },
                           formula::RoundingMode::HalfAwayFromZero>(galleryRoundingAnnex));

constexpr auto overlaidStrengthMethod = formula::apply(galleryOverlay, cubeStrengthMethod);

// ---- A screen analysis: a series, a grading curve, and particles binned ----
//
// Invented screens of 103, 127, 163, 197 and 241 m: three significant digits,
// none a preferred number, and not a sieve size or designation in any unit.
using RetainedMass = formula::Quantity<struct RetainedMassTag, "m_r", "mass retained on a screen", unit::Gram>;
using DryMass = formula::Quantity<struct DryMassTag, "m_t", "total dry mass", unit::Gram>;
using PassingShare = formula::Quantity<struct PassingShareTag, "p", "percentage passing a screen", unit::Percent>;
using ParticleSize = formula::Quantity<struct ParticleSizeTag, "s", "particle size", unit::Metre>;
using ClassShare = formula::Quantity<struct ClassShareTag, "n", "share of the particles in a class", unit::One>;

inline constexpr formula::BreakpointTable<5> galleryScreens { formula::breakpoint(103),
                                                              formula::breakpoint(127),
                                                              formula::breakpoint(163),
                                                              formula::breakpoint(197),
                                                              formula::breakpoint(241) };

constexpr auto passingEachScreen =
    formula::constant<unit::Percent>(formula::Rational { 100 })
    - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<RetainedMass, 5>) / var<DryMass>;

constexpr auto passingAtOpening =
    formula::interpolate_at(formula::curve(formula::domain<unit::Metre, galleryScreens>, passingEachScreen),
                            formula::constant<unit::Metre>(formula::Rational { 173 }));

inline constexpr formula::BandTable<3> gallerySizeClasses { formula::band(0, 1, 127, 1),
                                                            formula::band(127, 1, 197, 1),
                                                            formula::band(197, 1, 331, 1) };

constexpr auto countedParticles = formula::binned<unit::Metre, gallerySizeClasses>(formula::observations<ParticleSize, 8>);
constexpr auto classShares = countedParticles / formula::sum(countedParticles);

// ---- 10: statistics, outliers and precision -----------------------------------
//
// Six determinations of one mass, their spread reported exactly, their mean
// after outliers are rejected, and a precision check of two determinations
// against a limit that depends on their own level.

using DeterminedMass = formula::Quantity<struct DeterminedMassTag, "m", "mass of a determination", unit::Gram>;
using MassSpread = formula::Quantity<struct MassSpreadTag, "s", "spread of the determinations", unit::Gram>;
using FirstResult = formula::Quantity<struct FirstResultTag, "x_A", "first determination", unit::Gram>;
using SecondResult = formula::Quantity<struct SecondResultTag, "x_B", "second determination", unit::Gram>;

constexpr auto massMean = formula::sample_mean(formula::series<DeterminedMass, 6>);

constexpr auto massSpread = formula::documented(
    formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        formula::sample_variance(formula::series<DeterminedMass, 6>)),
    { .title = "Spread of repeated determinations",
      .reference = "Example Standard 5:2022",
      .section = "7.2",
      .text = "The square root of the sample variance, rounded exactly to 0.01 g: never a rounded "
              "floating-point root." });

using InitialCount = formula::Quantity<struct InitialCountTag, "N_0", "count before treatment", unit::One>;
using SurvivingCount = formula::Quantity<struct SurvivingCountTag, "N", "count after treatment", unit::One>;

constexpr auto logReduction =
    formula::documented(formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                            var<InitialCount> / var<SurvivingCount>),
                        { .title = "Logarithmic reduction",
                          .reference = "Example Standard 8:2023",
                          .section = "6.1",
                          .text = "The decimal logarithm of the count before over the count after, rounded exactly to 0.01: "
                                  "the decimal the true logarithm rounds to, never a rounded floating-point one." });

constexpr formula::Verdict repeatTheTest { "discard the determinations and repeat the test" };
constexpr formula::Citation outlierRule { .title = "Outliers", .reference = "Example Standard 5:2022", .section = "7.4" };

template <typename AtMostT>
[[nodiscard]] constexpr auto massWithoutOutliers()
{
    return formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, AtMostT, formula::KeepAtLeast<4>>(
            formula::series<DeterminedMass, 6>,
            formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<DeterminedMass>),
            repeatTheTest,
            outlierRule);
}

constexpr auto meanWithoutOutliers = formula::documented(
    formula::sample_mean(massWithoutOutliers<formula::AtMost<2>>()),
    { .title = "Mean after rejecting outliers",
      .reference = "Example Standard 5:2022",
      .section = "7.4",
      .text = "A determination more than 6 % of the mean from it is rejected, and the mean is taken again, "
              "until nothing more is rejected; a third rejection, or fewer than four left, is the author's "
              "verdict." });

constexpr auto repeatabilityCheck = formula::constraint(
    formula::abs(var<FirstResult> - var<SecondResult>) <= formula::precision_limit<formula::PrecisionKind::Repeatability>(
        (var<FirstResult> + var<SecondResult>) / formula::Rational { 2 },
        formula::constant<unit::Gram>(formula::Rational { 1, 10 })
            + formula::Rational { 1, 50 } * formula::precision_level<FirstResult>),
    formula::Verdict { "repeat the determinations" },
    { .title = "Repeatability of two determinations",
      .reference = "Example Standard 5:2022",
      .section = "8.1",
      .text = "The two determinations agree when they differ by no more than r = 0.1 g + level / 50, the "
              "level being their mean." });

// ---- Another record: a reference specimen, and a gated read -----------------
//
// A role names which record a formula reads from; which sample plays it is
// bound at run time. The curing batch is an attribute the author declares --
// the library compares the keys each record states, and nothing else.

struct ReferenceSpecimen
{
};
struct CuringBatch
{
};

/// This specimen's crushing strength over the reference specimen's, computed
/// from the reference's own load and edge.
constexpr auto relativeStrength =
    var<CrushingStrength> / formula::from_record<ReferenceSpecimen>(var<FailureLoad> / (var<LoadedEdge> * var<LoadedEdge>) );

/// The same, read only when both specimens were cured in one batch.
constexpr auto gatedRelativeStrength =
    var<CrushingStrength>
    / formula::from_record<ReferenceSpecimen>(var<FailureLoad> / (var<LoadedEdge> * var<LoadedEdge>),
                                              formula::same_lineage<CuringBatch>());

// ---- An opaque operation and a retry ---------------------------------------------
//
// A straight line fitted by least squares, whose sums the method names but
// does not spell out, and an estimate repeated until it settles, at most four
// times.

using SettlementTime = formula::Quantity<struct SettlementTimeTag, "t", "time of a settlement reading", unit::Second>;
using Settlement = formula::Quantity<struct SettlementTag, "L", "settlement read", unit::Millimetre>;
using SettlementRate = formula::Quantity<struct SettlementRateTag, "v", "rate of settlement", unit::MillimetrePerMinute>;
using IteratedEstimate = formula::Quantity<struct IteratedEstimateTag, "w", "an invented iterated estimate", unit::Gram>;

constexpr auto settlementSlope = formula::opaque_output<"slope">(
    formula::linear_least_squares(formula::curve(formula::series<SettlementTime, 4>, formula::series<Settlement, 4>),
                                  { .title = "Rate of settlement", .reference = "Example Standard 12", .section = "5.1" }));

constexpr auto settledEstimate = formula::retry<IteratedEstimate, 4, formula::FirstJudged::AtFirstAttempt>(
    formula::starting_from(formula::constant<unit::Gram>(formula::Rational { 0 })),
    formula::constant<unit::Gram>(formula::Rational { 152, 25 })
        + formula::previous_attempt<IteratedEstimate> / formula::Rational { 2 },
    formula::previous_attempt<IteratedEstimate> - formula::this_attempt<IteratedEstimate>
        >= formula::constant<unit::Gram>(formula::Rational { -19, 25 }),
    formula::Verdict { "repeat the determination" },
    { .title = "Settled estimate", .reference = "Example Standard 12", .section = "6" });

/// An exact rational as text: `4`, or `3/5` when it is not whole.
///
/// `formula::fraction_text` (`number_text.hpp`) spells the same text; this
/// helper predates it.
[[nodiscard]] std::string exact_text(formula::Rational value)
{
    if (value.denominator() == 1)
        return std::to_string(value.numerator());
    return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
}

/// A unit's symbol for a table cell, or a word for the one unit that has none.
[[nodiscard]] std::string unit_cell(formula::Unit unitOfValue)
{
    std::string_view const symbolText = formula::view(unitOfValue.symbolText);
    return symbolText.empty() ? std::string { "dimensionless" } : std::string { symbolText };
}

/// Writes the symbol table for a section, or nothing when there are no symbols
/// to put in it.
///
/// **The empty case is reachable, which is why it is handled rather than
/// asserted away.** An exact lookup has no operand at all -- its row is chosen
/// by a category key, which is data on the node and not a sub-expression
/// (`lookup.hpp`) -- so a formula that is nothing but an exact lookup reads no
/// quantity and contributes no `SymbolEntry`. Emitting the header and the
/// separator with no rows under them publishes an empty table, which tells a
/// reader the section has a symbol table and then shows them none.
///
/// One function called from both section writers rather than the same six
/// lines twice, for the reason this file's own `write_constraint` comment
/// gives: a constraint should document exactly the way a formula does, and two
/// copies of a rule are how two surfaces that must agree begin to disagree.
void write_symbol_table(std::ofstream& out, std::vector<formula::SymbolEntry> const& symbols)
{
    if (symbols.empty())
        return;

    out << "| Symbol | Description | Unit |\n";
    out << "| --- | --- | --- |\n";
    for (formula::SymbolEntry const& row: symbols)
        out << "| " << row.symbol << " | " << row.description << " | " << unit_cell(row.unit) << " |\n";
    out << "\n";
}

/// Writes one formula's section: its citation's title as a heading, the plain
/// and LaTeX renderings, its symbol table, and whichever citation fields are
/// non-empty.
///
/// Three separate `document<D>()` calls, not one reused across dialects: the
/// symbol table asks for `Dialect::Markdown` and the display form asks for
/// `Dialect::LaTeX`, each the dialect it is actually for, rather than
/// assuming today's `collect()` ignores `D` for the symbol table -- an
/// assumption a later phase could quietly invalidate.
template <formula::Node N>
void write_formula(std::ofstream& out, N const& node)
{
    formula::Documentation const plain = formula::document(node);
    formula::Documentation const markdown = formula::document<formula::Dialect::Markdown>(node);
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(node);

    // Exactly one documented() wrap per formula in this file, so exactly one
    // citation comes back.
    formula::Citation const& citation = plain.citations.front();

    out << "## " << citation.title << "\n\n";

    out << "```\n" << plain.formula << "\n```\n\n";

    out << "$$\n" << latex.formula << "\n$$\n\n";

    write_symbol_table(out, markdown.symbols);

    bool const hasBibliographicFields =
        !citation.reference.empty() || !citation.section.empty() || !citation.equation.empty();
    if (!citation.reference.empty())
        out << "- Reference: " << citation.reference << "\n";
    if (!citation.section.empty())
        out << "- Section: " << citation.section << "\n";
    if (!citation.equation.empty())
        out << "- Equation: " << citation.equation << "\n";
    if (hasBibliographicFields)
        out << "\n";

    if (!citation.text.empty())
        out << citation.text << "\n\n";
}

/// Writes one constraint's section: its citation's title as a heading, the
/// plain and LaTeX renderings of its rule -- never its verdict, which
/// belongs to the trace, not the rule (docs/constraints.md explains why) --
/// and its symbol table.
///
/// Not `write_formula` above only because a `Constraint` is not a `Node`
/// (`constraint.hpp`'s file comment) and so cannot be passed to the
/// `Node`-constrained `document<D>()` overload; it goes through
/// `document()`'s other overload, for `Constraint`, instead, added
/// alongside the `Node` one in `document.hpp` for exactly this reason.
/// Everything downstream of that call is identical to `write_formula`'s own
/// shape, including the symbol table -- a constraint documents exactly the
/// way a formula does, and this page should not read as though it doesn't.
///
/// Returns false, with a message on stderr, rather than assuming a citation
/// the way `write_formula` above assumes one: `write_formula`'s own
/// `citations.front()` leans on every `Node` here being wrapped by
/// `documented(inner, citation)`, whose `citation` parameter has no
/// default, so a wrapped node always contributes one citation regardless of
/// what it says. `constraint(predicate, verdict, citation = {})`
/// (`constraint.hpp`) has no such backstop -- its citation is optional by
/// design, used uncited in this project's own guide, example and tests --
/// so nothing here can lean on the type system the way `write_formula` can;
/// "every gallery constraint is cited" is a convention this file happens to
/// follow today, not a guarantee. `.front()` on an empty vector is
/// undefined behaviour, and on an MSVC debug build that means an assertion
/// dialog, not a clean crash -- exactly the modal-dialog failure mode
/// `examples/CMakeLists.txt`'s own comment warns about, and this tool is
/// run from `ctest` (`gallery.is-current`) just as an example is. Checking
/// first and failing loudly costs one `if`.
template <formula::Predicate P>
[[nodiscard]] bool write_constraint(std::ofstream& out, formula::Constraint<P> const& node)
{
    formula::Documentation const plain = formula::document(node);
    formula::Documentation const markdown = formula::document<formula::Dialect::Markdown>(node);
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(node);

    if (plain.citations.empty())
    {
        std::println(stderr, "formula-cpp-gallery: a gallery constraint must be cited, and this one is not");
        return false;
    }
    formula::Citation const& citation = plain.citations.front();

    out << "## " << citation.title << "\n\n";

    out << "```\n" << plain.formula << "\n```\n\n";

    out << "$$\n" << latex.formula << "\n$$\n\n";

    write_symbol_table(out, markdown.symbols);

    bool const hasBibliographicFields =
        !citation.reference.empty() || !citation.section.empty() || !citation.equation.empty();
    if (!citation.reference.empty())
        out << "- Reference: " << citation.reference << "\n";
    if (!citation.section.empty())
        out << "- Section: " << citation.section << "\n";
    if (!citation.equation.empty())
        out << "- Equation: " << citation.equation << "\n";
    if (hasBibliographicFields)
        out << "\n";

    if (!citation.text.empty())
        out << citation.text << "\n\n";

    return true;
}

} // namespace

/// A worked section shows the expression it is working, not only the trace
/// of having worked it. A reader who lands on one directly -- which is what
/// a deep link into this page does -- otherwise has to reconstruct the
/// formula from its own numbered steps.
///
/// Rendered, never hand-typed. A literal spelling here would be a second
/// source for the formula, free to drift from what `render()` actually
/// produces -- and a page that exists to prove its snippets came from a real
/// run is the worst place in the repository to keep one.
template <typename N>
void write_worked_formula(std::ofstream& out, N const& node)
{
    out << "```\n" << formula::render(node) << "\n```\n\n";
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::println(stderr, "usage: formula-cpp-gallery <output-file>");
        return 1;
    }

    std::ofstream out { argv[1], std::ios::trunc };
    if (!out)
    {
        std::println(stderr, "formula-cpp-gallery: could not open '{}' for writing", argv[1]);
        return 1;
    }

    out << "# Gallery\n\n";
    out << "This page is generated by running `tools/gallery/main.cpp`, which builds each "
           "formula below with the library's own operators and asks the library to render "
           "and evaluate it. **Do not edit it by hand** -- change the generator and "
           "regenerate instead; `gallery.is-current` fails CI when the two disagree.\n\n";
    out << "Every formula and citation on this page is invented -- generic physics with "
           "fictional `Example Standard` citations, exactly as every test and example "
           "elsewhere in this repository is. See [the home page](index.md).\n\n";

    write_formula(out, density);
    write_formula(out, circularArea);
    if (!write_constraint(out, maximumDiameter))
        return 1;
    write_formula(out, flowRate);
    write_formula(out, waterCementRatio);
    write_formula(out, compactionAdjustedDensity);
    write_formula(out, sizeAllowance);
    write_formula(out, mouldFactor);
    write_formula(out, maturityFactor);
    write_formula(out, correctedStrength);
    write_formula(out, massSpread);
    write_formula(out, logReduction);
    write_formula(out, meanWithoutOutliers);
    if (!write_constraint(out, repeatabilityCheck))
        return 1;

    // ---- A worked evaluation, so the page proves the numbers as well as the text ----

    out << "## Worked evaluation: water/cement ratio\n\n";
    out << "`V_w` = 180 l, `V_c` = 300 l:\n\n";

    auto const inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                             formula::Measured<CementVolume> { formula::Rational { 300 } });
    auto const outcome = formula::checked_evaluate<WaterCementRatio>(waterCementRatio, inputs);
    if (!outcome.has_value() || !outcome->is_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked evaluation did not produce a value");
        return 1;
    }
    formula::Rational const result = outcome->measurement().value();

    write_worked_formula(out, waterCementRatio);

    out << "```\n";
    out << "with V_w = 180 l and V_c = 300 l: " << exact_text(result) << " = " << result.to_double() << "\n";
    out << "```\n\n";

    // ---- A worked derivation, so the page shows how a number was reached, not only what it is ----

    out << "## Worked derivation: bulk density\n\n";
    out << "`m` = 1200 kg, `V` = 0.5 m3, `formula::explain()` and `formula::render_trace()`:\n\n";

    write_worked_formula(out, density);

    auto const densityInputs = formula::environment(formula::Measured<SpecimenMass> { formula::Rational { 1200 } },
                                                    formula::Measured<SpecimenVolume> { formula::Rational { 1, 2 } });
    formula::Explained<BulkDensity> const explained = formula::explain<BulkDensity>(density, densityInputs);
    if (!explained.outcome.is_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked derivation did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explained.trace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A worked conditional, so the page shows a when() naming the branch it took ----

    out << "## Worked derivation: compaction-adjusted bulk density\n\n";
    out << "`rho_m` = 1523 kg/m3 -- below the 1737 kg/m3 reference density, so the predicate holds "
           "and the correction factor is applied:\n\n";

    write_worked_formula(out, compactionAdjustedDensity);

    auto const compactionInputs = formula::environment(formula::Measured<MeasuredDensity> { formula::Rational { 1523 } });
    formula::Explained<AdjustedBulkDensity> const explainedCompaction =
        formula::explain<AdjustedBulkDensity>(compactionAdjustedDensity, compactionInputs);
    if (!explainedCompaction.outcome.is_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked conditional did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explainedCompaction.trace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A worked constraint, so the page shows a verdict as its own trace step ----

    out << "## Worked derivation: maximum specimen diameter, alongside the circular area it validates\n\n";
    out << "`d` = 173 mm -- above the 139 mm tolerance, so the constraint is violated and its verdict "
           "appears in the trace, `formula::check()` and `formula::render_trace()`:\n\n";

    // Both, because the heading promises both: the constraint that was
    // checked, and the formula it guards.
    write_worked_formula(out, maximumDiameter);
    write_worked_formula(out, circularArea);

    auto const oversizedSpecimen = formula::environment(formula::Measured<Diameter> { formula::Rational { 173 } });
    formula::Trace<> constraintTrace {};
    formula::RecordingSink<> constraintSink { constraintTrace };
    formula::ConstraintOutcome const diameterOutcome = formula::check(maximumDiameter, oversizedSpecimen, constraintSink);
    if (!diameterOutcome.is_violated())
    {
        std::println(stderr, "formula-cpp-gallery: the worked constraint did not violate as expected");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(constraintTrace, { .maxSteps = 5 });
    out << "```\n\n";

    // ---- Three lookup tables in one derivation, so the page shows each kind naming the row it used ----

    out << "## Worked derivation: size- and age-corrected crushing strength\n\n";
    // The key is written out of the enumerator rather than typed, so this
    // sentence cannot drift from the row the lookup below actually selects,
    // nor from the spelling render() and the trace give it.
    out << "`f` = 33 MPa, `d` = 127 mm, `t` = 57 h, mould `key " << formula::enumerator_name<GalleryMould::Cylinder>()
        << "`. Each table names the row it answered "
           "from: the banded one its interval, the interpolating one the two rows it drew on. The exact "
           "lookup adds nothing there -- its key is already the subject of its own line.\n\n";

    write_worked_formula(out, correctedStrength);

    auto const correctedInputs = formula::environment(formula::Measured<CrushingStrength> { formula::Rational { 33 } },
                                                      formula::Measured<Diameter> { formula::Rational { 127 } },
                                                      formula::Measured<CuringAge> { formula::Rational { 57 } });
    formula::Explained<CorrectedStrength> const explainedCorrected =
        formula::explain<CorrectedStrength>(correctedStrength, correctedInputs);
    if (!explainedCorrected.outcome.is_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked lookup derivation did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explainedCorrected.trace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A lookup that found nothing, because a miss is not a number ----
    //
    // Traced through a RecordingSink rather than `explain()`: `explain()` goes
    // through the THROWING `evaluate()`, so a miss -- an ordinary outcome for a
    // lookup, not a defect -- would throw instead of handing back the
    // derivation that says why it missed.

    out << "## Worked derivation: a lookup that found nothing\n\n";
    out << "The same size-allowance table at `d` = 241 mm. The table's last band stops below 197 mm, so "
           "241 mm falls in no band -- and a miss is not a value: not zero, not the nearest band, not the "
           "last one. The bracketed clause is what keeps the line from being read as a failure relayed up "
           "from somewhere below it.\n\n";

    write_worked_formula(out, sizeAllowance);

    auto const uncoveredSpecimen = formula::environment(formula::Measured<Diameter> { formula::Rational { 241 } });
    formula::Trace<> missTrace {};
    formula::RecordingSink<> missSink { missTrace };
    std::expected<formula::Outcome<SizeAllowance>, formula::ArithmeticError> const missed =
        formula::checked_evaluate<SizeAllowance>(sizeAllowance, uncoveredSpecimen, missSink);
    if (missed.has_value() || missed.error() != formula::ArithmeticError::DomainError)
    {
        std::println(stderr, "formula-cpp-gallery: the uncovered diameter did not report a domain error");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(missTrace, { .maxSteps = 5 });
    out << "```\n\n";

    // ---- A method's selected variant, and the same method overlaid ----
    //
    // Traced through a RecordingSink: a method is evaluated by
    // `evaluate_method`, which answers in the coherent unit and names, in the
    // trace, the variant it selected and whose rounding rule it applied.

    out << "## Worked derivation: a method's selected variant, and the same method overlaid\n\n";
    out << "`F` = 226 kN, `a` = 150 mm, `k_s` = 1043/1000, the cube variant selected by tag. The method rounds by "
           "its own rule, and the trace says which variant ran and whose rule rounded it:\n\n";

    write_worked_formula(out, std::get<0>(cubeStrengthMethod.variantSet.cases).expression);

    auto const cubeSpecimen = formula::environment(formula::Measured<FailureLoad> { formula::Rational { 226'000 } },
                                                   formula::Measured<LoadedEdge> { formula::Rational { 150 } },
                                                   formula::Measured<Diameter> { formula::Rational { 127 } },
                                                   formula::Measured<ShapeFactor> { formula::Rational { 1043, 1000 } });
    formula::Trace<> methodTrace {};
    auto const baseStrength =
        formula::evaluate_method<Cube>(cubeStrengthMethod, cubeSpecimen, formula::RecordingSink<> { methodTrace });
    if (!baseStrength.has_value() || !baseStrength->has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked method did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(methodTrace, { .maxSteps = 20 });
    out << "```\n\n";

    out << "The same specimen under a jurisdiction's overlay, which fixes the shape factor and reports in "
           "N/mm2 to two decimals:\n\n";

    write_worked_formula(out, std::get<0>(overlaidStrengthMethod.variantSet.cases).expression);

    formula::Trace<> overlaidTrace {};
    auto const overlaidStrength =
        formula::evaluate_method<Cube>(overlaidStrengthMethod, cubeSpecimen, formula::RecordingSink<> { overlaidTrace });
    if (!overlaidStrength.has_value() || !overlaidStrength->has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked overlaid method did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(overlaidTrace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- Whose acceptance checks ----
    //
    // `check_method` checks every constraint the method holds, and the trace
    // says beside each verdict whether the method or a jurisdiction's overlay
    // supplied it.

    out << "## Worked acceptance: the method's own checks, and a jurisdiction's\n\n";
    out << "The same specimen checked by the method's own acceptance check:\n\n";

    formula::Trace<> ownAcceptance {};
    auto const ownOutcomes =
        formula::check_method(cubeStrengthMethod, cubeSpecimen, formula::RecordingSink<> { ownAcceptance });
    if (ownOutcomes.size() != 1 || !ownOutcomes[0].is_satisfied())
    {
        std::println(stderr, "formula-cpp-gallery: the method's own check did not hold");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(ownAcceptance, { .maxSteps = 20 });
    out << "```\n\n";

    out << "And by the overlay's two checks in its place. The overlay lists the shape factor's constant after "
           "its checks, so the constant reaches inside them:\n\n";

    formula::Trace<> overlaidAcceptance {};
    auto const overlaidOutcomes =
        formula::check_method(overlaidStrengthMethod, cubeSpecimen, formula::RecordingSink<> { overlaidAcceptance });
    if (overlaidOutcomes.size() != 2 || !overlaidOutcomes[0].is_violated() || !overlaidOutcomes[1].is_satisfied())
    {
        std::println(stderr, "formula-cpp-gallery: the overlay's checks did not answer as expected");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(overlaidAcceptance, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A series, element by element ----
    //
    // A series is evaluated by `checked_evaluate_series`: one trace step per
    // operation, every element on its line, the total read once.

    out << "## Worked derivation: the percentage passing each screen\n\n";
    out << "`m_r` = 130, 210, 95, 340 and 28 g retained on five screens, `m_t` = 1250 g. The series is marked "
           "`(i)` in the formula, and each step of its derivation carries every element:\n\n";

    write_worked_formula(out, passingEachScreen);

    auto const screenAnalysis = formula::environment(
        formula::measured_series<RetainedMass>(formula::Measured<RetainedMass> { formula::Rational { 130 } },
                                               formula::Measured<RetainedMass> { formula::Rational { 210 } },
                                               formula::Measured<RetainedMass> { formula::Rational { 95 } },
                                               formula::Measured<RetainedMass> { formula::Rational { 340 } },
                                               formula::Measured<RetainedMass> { formula::Rational { 28 } }),
        formula::Measured<DryMass> { formula::Rational { 1250 } });
    formula::Trace<> seriesTrace {};
    auto const passingValues = formula::checked_evaluate_series<PassingShare>(
        passingEachScreen, screenAnalysis, formula::RecordingSink<> { seriesTrace });
    if (!passingValues.has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked series did not evaluate");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(seriesTrace, { .maxSteps = 40 });
    out << "```\n\n";

    // ---- A grading curve read between two screens ----

    out << "## Worked derivation: a grading curve read between two screens\n\n";
    out << "The same percentages paired with the declared screens as a curve, and read at 173 m. The last step "
           "names the two screens the answer lay between. Its value is read off computed percentages -- 100 % less "
           "a ratio of two masses, in two units with no one unit to borrow -- so it reads in the coherent unit, a "
           "plain fraction: 6927/10625 is about 65.2 %.\n\n";

    write_worked_formula(out, passingAtOpening);

    formula::Trace<> curveTrace {};
    auto const readOff =
        formula::checked_evaluate<PassingShare>(passingAtOpening, screenAnalysis, formula::RecordingSink<> { curveTrace });
    if (!readOff.has_value() || !readOff->is_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked curve did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(curveTrace, { .maxSteps = 60 });
    out << "```\n\n";

    // ---- Particles binned, and one in no class ----

    out << "## Worked derivation: particles counted into classes, and one in no class\n\n";
    out << "Seven particles, counted into three half-open classes and divided by their total. 127 and 197 m "
           "sit on class boundaries and count in the upper class. The formula names the binning twice -- once "
           "counted, once summed -- and each is evaluated where it stands, so the observations are read, and "
           "binned, twice:\n\n";

    write_worked_formula(out, classShares);

    auto const sieved = formula::environment(formula::MeasuredObservations<ParticleSize, 8>(formula::Rational { 103 },
                                                                                            formula::Rational { 127 },
                                                                                            formula::Rational { 163 },
                                                                                            formula::Rational { 277 },
                                                                                            formula::Rational { 113 },
                                                                                            formula::Rational { 197 },
                                                                                            formula::Rational { 241 }));
    formula::Trace<> binningTrace {};
    auto const shared =
        formula::checked_evaluate_series<ClassShare>(classShares, sieved, formula::RecordingSink<> { binningTrace });
    if (!shared.has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked binning did not evaluate");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(binningTrace, { .maxSteps = 40 });
    out << "```\n\n";

    out << "The fourth particle measured 331 m instead: the last class's high bound, in no class. A miss is "
           "not dropped, and the step names the observation. The division then relays the failure without a "
           "position. Its divisor was never reached, so the line names the counts it evaluated and says so "
           "in the divisor's place, `#2 / (not evaluated)`:\n\n";

    auto const oversized = formula::environment(formula::MeasuredObservations<ParticleSize, 8>(formula::Rational { 103 },
                                                                                               formula::Rational { 127 },
                                                                                               formula::Rational { 163 },
                                                                                               formula::Rational { 331 },
                                                                                               formula::Rational { 113 },
                                                                                               formula::Rational { 197 },
                                                                                               formula::Rational { 241 }));
    formula::Trace<> binningMissTrace {};
    auto const missedShares =
        formula::checked_evaluate_series<ClassShare>(classShares, oversized, formula::RecordingSink<> { binningMissTrace });
    if (missedShares.has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the oversized particle did not miss");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(binningMissTrace, { .maxSteps = 40 });
    out << "```\n\n";

    // ---- Statistics, outliers and precision ----

    out << "## Worked statistics: a mean with its spread, and the mean after rejecting outliers\n\n";
    out << "Six determinations, 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g. Their mean:\n\n";

    auto const sixDeterminations = formula::environment(
        formula::measured_series<DeterminedMass>(formula::Measured<DeterminedMass> { formula::Rational { 402, 10 } },
                                                 formula::Measured<DeterminedMass> { formula::Rational { 398, 10 } },
                                                 formula::Measured<DeterminedMass> { formula::Rational { 405, 10 } },
                                                 formula::Measured<DeterminedMass> { formula::Rational { 44 } },
                                                 formula::Measured<DeterminedMass> { formula::Rational { 40 } },
                                                 formula::Measured<DeterminedMass> { formula::Rational { 433, 10 } }));

    write_worked_formula(out, massMean);
    formula::Trace<> meanTrace {};
    auto const meanValue =
        formula::checked_evaluate<DeterminedMass>(massMean, sixDeterminations, formula::RecordingSink<> { meanTrace });
    if (!meanValue.has_value() || meanValue->measurement().value() != formula::Rational { 413, 10 })
    {
        std::println(stderr, "formula-cpp-gallery: the worked mean did not come to 41.3 g");
        return 1;
    }
    out << "```\n" << formula::render_trace(meanTrace, { .maxSteps = 20 }) << "```\n\n";

    out << "Their spread, reported exactly:\n\n";
    write_worked_formula(out, massSpread);
    formula::Trace<> spreadTrace {};
    auto const spreadValue =
        formula::checked_evaluate<MassSpread>(massSpread, sixDeterminations, formula::RecordingSink<> { spreadTrace });
    if (!spreadValue.has_value() || spreadValue->measurement().value() != formula::Rational { 37, 20 })
    {
        std::println(stderr, "formula-cpp-gallery: the worked spread did not come to 1.85 g");
        return 1;
    }
    out << "```\n" << formula::render_trace(spreadTrace, { .maxSteps = 20 }) << "```\n\n";

    out << "Their mean after rejecting outliers: 44.0 g goes in pass 1, 43.3 g in pass 2, and pass 3 "
           "settles:\n\n";
    write_worked_formula(out, meanWithoutOutliers);
    formula::Trace<> settledTrace {};
    auto const settledMean = formula::checked_evaluate<DeterminedMass>(
        meanWithoutOutliers, sixDeterminations, formula::RecordingSink<> { settledTrace });
    if (!settledMean.has_value() || settledMean->measurement().value() != formula::Rational { 321, 8 })
    {
        std::println(stderr, "formula-cpp-gallery: the worked rejection did not settle at 321/8 g");
        return 1;
    }
    out << "```\n" << formula::render_trace(settledTrace, { .maxSteps = 30 }) << "```\n\n";

    out << "The same rule allowed one rejection: the second is one too many, and the author's verdict "
           "stands in place of a mean:\n\n";
    constexpr auto meanAtMostOne = formula::sample_mean(massWithoutOutliers<formula::AtMost<1>>());
    write_worked_formula(out, meanAtMostOne);
    formula::Trace<> abortedTrace {};
    auto const abortedMean = formula::checked_evaluate<DeterminedMass>(
        meanAtMostOne, sixDeterminations, formula::RecordingSink<> { abortedTrace });
    if (abortedMean.has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the worked rejection did not abort");
        return 1;
    }
    out << "```\n" << formula::render_trace(abortedTrace, { .maxSteps = 30 }) << "```\n\n";

    out << "## Worked precision check: two determinations at their own level\n\n";
    out << "`x_A` = 40.0 g and `x_B` = 40.905 g, 0.905 g apart. The limit is evaluated at the level it "
           "checks -- their mean, 40.4525 g -- in two declared passes:\n\n";
    write_worked_formula(out, repeatabilityCheck);
    auto const twoDeterminations =
        formula::environment(formula::Measured<FirstResult> { formula::Rational { 40 } },
                             formula::Measured<SecondResult> { formula::Rational { 40905, 1000 } });
    formula::Trace<> precisionTrace {};
    formula::ConstraintOutcome const agreed =
        formula::check(repeatabilityCheck, twoDeterminations, formula::RecordingSink<> { precisionTrace });
    if (!agreed.is_satisfied())
    {
        std::println(stderr, "formula-cpp-gallery: the worked precision check did not hold");
        return 1;
    }
    out << "```\n" << formula::render_trace(precisionTrace, { .maxSteps = 30 }) << "```\n\n";

    // ---- Another record ----
    //
    // A context holds this specimen's record and the reference's, by role, and
    // is this specimen's environment. Every step read from the reference says
    // so, with both of its keys.

    out << "## Worked derivation: a strength relative to a reference specimen\n\n";
    out << "This specimen's `f` = 36 MPa over the reference specimen's strength, computed from the "
           "reference's own `F` = 579 630 N and `a` = 139 mm. Every step read from the reference says so, "
           "with its sample and test:\n\n";

    write_worked_formula(out, relativeStrength);

    auto const thisSpecimen = formula::environment(formula::Measured<CrushingStrength> { formula::Rational { 36 } });
    auto const referenceSpecimen = formula::environment(formula::Measured<FailureLoad> { formula::Rational { 579'630 } },
                                                        formula::Measured<LoadedEdge> { formula::Rational { 139 } });
    auto const sameBatch = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)),
                                             thisSpecimen,
                                             formula::lineage<CuringBatch>(4411)),
        formula::record<ReferenceSpecimen>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                           referenceSpecimen,
                                           formula::lineage<CuringBatch>(4411)));

    formula::Trace<> relativeTrace {};
    auto const relative = formula::checked_evaluate_si<formula::Rational>(
        relativeStrength, sameBatch, formula::RecordingSink<> { relativeTrace });
    if (!relative.has_value() || !relative->has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the relative strength did not produce a value");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(relativeTrace, { .maxSteps = 20 });
    out << "```\n\n";

    out << "The same read, gated on the two specimens sharing a curing batch. The reference was cured in "
           "another batch, so the read is refused before the reference's values are used, and the trace "
           "says which attribute refused it, with both keys. The requirement does not change the formula "
           "on the page; the trace records every attribute it compared:\n\n";

    write_worked_formula(out, gatedRelativeStrength);

    auto const otherBatch = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)),
                                             thisSpecimen,
                                             formula::lineage<CuringBatch>(4411)),
        formula::record<ReferenceSpecimen>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                           referenceSpecimen,
                                           formula::lineage<CuringBatch>(4412)));

    formula::Trace<> gatedTrace {};
    auto const gated = formula::checked_evaluate_si<formula::Rational>(
        gatedRelativeStrength, otherBatch, formula::RecordingSink<> { gatedTrace });
    if (gated.has_value() || gated.error() != formula::ArithmeticError::DomainError)
    {
        std::println(stderr, "formula-cpp-gallery: the gated read was not refused");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(gatedTrace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- An opaque operation, and a retry ----
    //
    // A fit's inside is not an expression tree the method states, so its step
    // lists what it produced and says the inside is not shown. A retry's every
    // attempt is traced, then how it ended.

    out << "## Worked derivation: a straight line fitted by least squares\n\n";
    out << "Settlement read at `t` = 1, 2, 4 and 7 s: `L` = 10.2, 10.9, 12.1 and 14.3 mm. The fit is an "
           "opaque operation: its step lists the intercept and the slope it produced, exactly, and says "
           "its inside is not shown:\n\n";

    write_worked_formula(out, settlementSlope);

    auto const settlementReadings = formula::environment(
        formula::measured_series<SettlementTime>(formula::Measured<SettlementTime> { formula::Rational { 1 } },
                                                 formula::Measured<SettlementTime> { formula::Rational { 2 } },
                                                 formula::Measured<SettlementTime> { formula::Rational { 4 } },
                                                 formula::Measured<SettlementTime> { formula::Rational { 7 } }),
        formula::measured_series<Settlement>(formula::Measured<Settlement> { formula::Rational { 102, 10 } },
                                             formula::Measured<Settlement> { formula::Rational { 109, 10 } },
                                             formula::Measured<Settlement> { formula::Rational { 121, 10 } },
                                             formula::Measured<Settlement> { formula::Rational { 143, 10 } }));
    formula::Trace<> fitTrace {};
    auto const fitted = formula::checked_evaluate_si<formula::Rational>(
        settlementSlope, settlementReadings, formula::RecordingSink<> { fitTrace });
    if (!fitted.has_value() || !fitted->has_value())
    {
        std::println(stderr, "formula-cpp-gallery: the least-squares fit did not produce a slope");
        return 1;
    }
    out << "```\n" << formula::render_trace(fitTrace, { .maxSteps = 20 }) << "```\n\n";

    out << "## Worked retry: an estimate repeated until it settles\n\n";
    out << "Each attempt halves the previous estimate and adds 6.08 g, from 0 g, and is accepted once it rose "
           "by at most 0.76 g; after four attempts without that, the method's verdict. It settles at the "
           "fourth:\n\n";

    write_worked_formula(out, settledEstimate);

    auto const settling = formula::explain_retry(settledEstimate, formula::environment());
    if (!settling.outcome.has_value() || settling.outcome->end() != formula::RetryEnd::Accepted)
    {
        std::println(stderr, "formula-cpp-gallery: the retry was not accepted");
        return 1;
    }
    out << "```\n" << formula::render_trace(settling.trace, { .maxSteps = 60 }) << "```\n\n";

    out.flush();
    return out ? 0 : 1;
}
