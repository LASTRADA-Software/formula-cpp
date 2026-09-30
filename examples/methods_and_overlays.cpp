// SPDX-License-Identifier: Apache-2.0
//
// Methods and jurisdiction overlays: one measurement reported by more than one
// formula, chosen by what the specimen IS, and changed per jurisdiction by a
// declared overlay rather than by an `if` the library never sees.
//
//   1. A method: three variants, each tagged with the specimen shape it
//      applies to, and one rounding rule. `evaluate_method<Cube>` selects by
//      tag, rounds by the method's rule, answers in the coherent unit, and the
//      trace says which variant ran and whose rounding rule it was.
//   2. Overlays: a northern jurisdiction fixes a constant and reports in its
//      own unit; a southern one derives a quantity, replaces a variant's
//      formula wholesale, and drops a variant it does not use; an eastern one
//      makes one variant mandatory. Each change is said in the trace and on
//      the documentation page, with what the jurisdiction cited.
//   3. The jurisdiction set is closed and compiled in; which jurisdiction
//      applies to a sample is an ordinary runtime value.
//   4. A vocabulary: the two jurisdictions write the specimen's two edges with
//      each other's letters. The page and the trace both follow it.
//   5. Constraints: the method's own acceptance check, and a western
//      jurisdiction's overlay that replaces it with acceptance logic of its
//      own. `check_method` answers one outcome per constraint, and the trace
//      says beside each verdict whose check it was.
//
// Every citation here is invented -- generic physics with fictional Example
// Standard references, exactly as every other example in this repository is.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- Tags: what a variant applies to ----------------------------------------
//
// A tag is a type, never instantiated, and need not even be complete. Which
// variant applies is a property of the specimen, stated by the caller as a
// type -- never inferred from a number in the environment.
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};

// ---- Quantities ---------------------------------------------------------------
using Force = formula::Quantity<struct ForceTag, "F", "maximum load at failure", unit::Newton>;
using EdgeA = formula::Quantity<struct EdgeATag, "a", "first loaded edge", unit::Millimetre>;
using EdgeB = formula::Quantity<struct EdgeBTag, "b", "second loaded edge", unit::Millimetre>;
using Diameter = formula::Quantity<struct DiameterTag, "d", "cylinder diameter", unit::Millimetre>;
using ShapeFactor = formula::Quantity<struct ShapeFactorTag, "k_s", "shape factor", unit::One>;
} // namespace

// A tag is shown under its own name by default -- `Cube` -- and a published
// method may word a variant its own way. Declared outside the anonymous
// namespace, where the primary template's namespace encloses it.
template <>
struct formula::TagName<Cylinder>
{
    static constexpr std::string_view of() noexcept
    {
        return "cylinder 135 x 271 mm";
    }
};

namespace
{
// ---- 1. The method -------------------------------------------------------------
//
// Three variants, one per shape, in the order the method publishes them -- the
// prism first, so that when the south prunes it the other two keep their
// published positions visibly: still 2nd and 3rd of 3, not 1st and 2nd. They
// are different expressions of different types; what they must share is the
// dimension they report, and a pack whose variants disagree does not compile.
//
// The method rounds to a tenth of a megapascal, a rule named once.
inline constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                                     formula::DecimalPlaces { 1 },
                                                     formula::RoundingMode::HalfAwayFromZero };

// Kept out of clang-format's hands: it reads `var<ShapeFactor> * var<Force>`
// as a pointer declaration and writes `var<ShapeFactor>* var<Force>`, and the
// guide quotes this declaration verbatim.
// clang-format off
inline constexpr auto compressiveStrength = formula::method(
    formula::variants(formula::variant<Prism>(var<Force> / (var<EdgeA> * var<EdgeA>)),
                      formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeA> * var<EdgeB>)),
                      formula::variant<Cylinder>(formula::constant<unit::One>(4) * var<Force>
                                                 / (formula::pi * formula::pow<2>(var<Diameter>)))),
    formula::rounding_rule<tenthMpa>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(47.3_r),
                                             formula::Verdict { "the load at failure is below 47.3 kN" })));
// clang-format on

// A 163 x 103 mm cube face loaded to 89.3 kN with a measured shape factor of
// 1.043: 5.5477... MPa, so a rounding rule's granularity shows in the number --
// 5.5 MPa to one decimal, 5.55 to two.
inline constexpr auto specimen = formula::environment(formula::Measured<Force> { 89'300 },
                                                      formula::Measured<EdgeA> { 163 },
                                                      formula::Measured<EdgeB> { 103 },
                                                      formula::Measured<Diameter> { 135 },
                                                      formula::Measured<ShapeFactor> { 1.043_r });

// ---- 2. Overlays -----------------------------------------------------------------
inline constexpr formula::Citation northConstant { .title = "Shape factor",
                                                   .reference = "Example Standard 12:2021 NA",
                                                   .section = "NA.2.1" };
inline constexpr formula::Citation northRounding { .reference = "Example Standard 12:2021 NA", .section = "NA.4" };
inline constexpr formula::Citation southDefinition { .reference = "Example Standard 7:2019 A", .section = "A.3" };
inline constexpr formula::Citation southReplacement { .reference = "Example Standard 7:2019 A", .section = "A.5" };
inline constexpr formula::Citation southScope { .reference = "Example Standard 7:2019 A", .section = "A.1" };
inline constexpr formula::Citation eastScope { .reference = "Example Standard 3:2023 E", .section = "E.1" };

/// The north fixes the shape factor at 0.863 where the base method reads it
/// from the specimen, and reports in newtons per square millimetre to two
/// decimals. A method answers in the coherent unit whatever its rule; the
/// rule's unit is the unit a jurisdiction REPORTS in, and it must measure the
/// method's dimension.
inline constexpr auto north =
    formula::overlay(formula::with_constant<ShapeFactor>(0.863_r, northConstant),
                     formula::with_rounding<unit::NewtonPerSquareMillimetre,
                                            formula::DecimalPlaces { 2 },
                                            formula::RoundingMode::HalfAwayFromZero>(northRounding));

/// The south derives the shape factor from the cube's own edges, replaces the
/// cylinder formula with its own -- 4.3 MPa for this specimen, where the base
/// method's gives 6.2 MPa -- and has no prisms at all. The replacement
/// is listed first so that nothing listed after it is missed inside it.
inline constexpr auto south = formula::overlay(
    formula::replace_variant<Cylinder>(
        var<Force> / (formula::constant<unit::One>(1.127_r) * formula::pow<2>(var<Diameter>)), southReplacement),
    formula::add_derived<ShapeFactor>(var<EdgeB> / var<EdgeA>, southDefinition),
    formula::prune_variant<Prism>(southScope));

/// The east tests cubes only, so the cube variant is mandatory there.
inline constexpr auto east = formula::overlay(formula::pin_variant<Cube>(eastScope));

inline constexpr auto northern = formula::apply(north, compressiveStrength);
inline constexpr auto southern = formula::apply(south, compressiveStrength);
inline constexpr auto eastern = formula::apply(east, compressiveStrength);

// ---- 3. Which jurisdiction applies is data ----------------------------------------
enum class Jurisdiction : std::uint8_t
{
    Base,
    North,
    South,
};

/// The cube strength as the jurisdiction registered for this sample computes
/// it. Every method here is a different type, but every one answers the same
/// `Evaluated<Rational>`, so choosing among them at run time is a `switch`.
[[nodiscard]] formula::Evaluated<formula::Rational> cubeStrengthIn(Jurisdiction jurisdiction)
{
    switch (jurisdiction)
    {
        case Jurisdiction::North:
            return formula::evaluate_method<Cube>(northern, specimen);
        case Jurisdiction::South:
            return formula::evaluate_method<Cube>(southern, specimen);
        case Jurisdiction::Base:
            break;
    }
    return formula::evaluate_method<Cube>(compressiveStrength, specimen);
}

// ---- 4. Vocabularies -------------------------------------------------------------
//
// The two jurisdictions name the two loaded edges with each other's letters:
// what the north calls `a` is what the south calls `b`. A page in the wrong
// jurisdiction's words is not unfamiliar, it states the wrong formula.
inline constexpr auto northernWords = formula::vocabulary(formula::renames<EdgeA>("a"), formula::renames<EdgeB>("b"));
inline constexpr auto southernWords = formula::vocabulary(formula::renames<EdgeA>("b"), formula::renames<EdgeB>("a"));

// ---- 5. Constraints ---------------------------------------------------------------
inline constexpr formula::Citation westAcceptance { .title = "Acceptance",
                                                    .reference = "Example Standard 9:2022 B",
                                                    .section = "B.2" };
inline constexpr formula::Citation westRevision { .title = "Acceptance",
                                                  .reference = "Example Standard 9:2025 B",
                                                  .section = "B.2" };

/// The west accepts a specimen by checks of its own: two where the base method
/// has one, and neither of them the base method's. `with_constraints` replaces
/// the method's constraints wholesale -- it does not add to them.
inline constexpr auto west = formula::overlay(formula::with_constraints(
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(97.3_r),
                                             formula::Verdict { "the load at failure is below 97.3 kN" }),
                         formula::constraint(var<EdgeA> <= formula::number(1.73_r) * var<EdgeB>,
                                             formula::Verdict { "the loaded face is more than 1.73 times as long as wide" })),
    westAcceptance));

/// A later revision of the west's annex, applied on top of the west's method:
/// its one constraint is all the stacked method checks.
inline constexpr auto westRevised = formula::overlay(formula::with_constraints(
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(83.1_r),
                                             formula::Verdict { "the load at failure is below 83.1 kN" })),
    westRevision));

inline constexpr auto western = formula::apply(west, compressiveStrength);
inline constexpr auto westernRevised = formula::apply(westRevised, western);

template <std::size_t N>
void print_outcomes(char const* method, std::array<formula::ConstraintOutcome, N> const& outcomes)
{
    std::println("{}: {} constraint(s)", method, N);
    for (std::size_t index = 0; index < N; ++index)
    {
        std::print("  [{}] {}", index, outcomes[index].kind());
        if (std::optional<formula::Verdict> const verdict = outcomes[index].verdict())
            std::print(": {}", verdict->label);
        std::println("");
    }
}

/// Whose constraints @p m checks, as `constraint_origin` answers it.
template <typename M>
[[nodiscard]] std::string whoseConstraints(M const& m)
{
    formula::ConstraintOrigin const origin = formula::constraint_origin(m);
    if (origin.provenance() == formula::ConstraintProvenance::MethodOwn)
        return "the method's own";
    return "jurisdiction overlay, " + std::string { origin.source().reference } + ", "
           + std::string { origin.source().section };
}

void print_symbols(formula::Documentation const& documentation)
{
    for (formula::SymbolEntry const& row: documentation.symbols)
    {
        std::print("  {}: {}", row.symbol, row.description);
        if (row.fixedValue.has_value())
            std::print(" -- fixed at {:/}", *row.fixedValue);
        if (row.derivedAs.has_value())
            std::print(" -- derived as {}", *row.derivedAs);
        std::println("");
    }
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

    // ---- 1. Selecting a variant ---------------------------------------------------
    std::println("== 1. A method selects a variant by tag ==\n");

    auto const cube = formula::explain_method<Cube>(compressiveStrength, specimen);
    if (!cube.outcome)
    {
        std::println("cube: {}", cube.outcome.error());
        return 1;
    }
    auto const cubeStrength = formula::number_of(cube.outcome);
    if (!cubeStrength)
    {
        std::println("cube: no value");
        return 1;
    }
    std::println("cube:   {} Pa", *cubeStrength);
    std::println("\n{}", formula::render_trace(cube.trace, { .maxSteps = 30 }));
    check(cubeStrength == 5500000_r, "the cube's strength, rounded to 5.5 MPa and answered in pascals");

    auto const cylinder = formula::explain_method<Cylinder>(compressiveStrength, specimen);
    std::string const cylinderTrace = formula::render_trace(cylinder.trace, { .maxSteps = 30 });
    std::println("{}", cylinderTrace);
    check(cylinderTrace.contains("[variant cylinder 135 x 271 mm (3rd of 3), selected by tag]"),
          "the cylinder variant is named as its TagName spells it, at its published position");

    // ---- 2. Overlays ---------------------------------------------------------------
    std::println("== 2. A jurisdiction's overlay yields a method ==\n");

    auto const northCube = formula::explain_method<Cube>(northern, specimen);
    if (!northCube.outcome)
    {
        std::println("north cube: {}", northCube.outcome.error());
        return 1;
    }
    auto const northStrength = formula::number_of(northCube.outcome);
    if (!northStrength)
    {
        std::println("north cube: no value");
        return 1;
    }
    std::println("north cube: {} Pa\n\n{}", *northStrength, formula::render_trace(northCube.trace, { .maxSteps = 30 }));
    check(northStrength == 4590000_r, "the north's fixed 0.863, rounded to 4.59 N/mm2 by its own rule");

    auto const southCube = formula::explain_method<Cube>(southern, specimen);
    if (!southCube.outcome)
    {
        std::println("south cube: {}", southCube.outcome.error());
        return 1;
    }
    auto const southStrength = formula::number_of(southCube.outcome);
    if (!southStrength)
    {
        std::println("south cube: no value");
        return 1;
    }
    std::println("south cube: {} Pa\n\n{}", *southStrength, formula::render_trace(southCube.trace, { .maxSteps = 30 }));
    check(southStrength == 3400000_r, "the south's derived shape factor b / a = 103/163");

    auto const southCylinder = formula::explain_method<Cylinder>(southern, specimen);
    std::string const southCylinderTrace = formula::render_trace(southCylinder.trace, { .maxSteps = 30 });
    std::println("{}", southCylinderTrace);
    check(southCylinderTrace.contains("[replaced by jurisdiction overlay: Example Standard 7:2019 A, A.5]"),
          "the south's cylinder formula is marked as the south's");
    check(formula::number_of(southCylinder.outcome) == 4300000_r && formula::number_of(cylinder.outcome) == 6200000_r,
          "the south's replacement formula is the one that ran: 4.3 MPa, not the base method's 6.2");
    check(southCylinderTrace.contains("(3rd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example Standard "
                                      "7:2019 A, A.1]"),
          "a variant keeps its published position after one before it is pruned, and the prune is said with its "
          "citation");

    std::string const eastCube =
        formula::render_trace(formula::explain_method<Cube>(eastern, specimen).trace, { .maxSteps = 30 });
    std::println("{}", eastCube);
    check(eastCube.contains("[variant Cube (2nd of 3), selected by tag; pinned by jurisdiction overlay: Example Standard "
                            "3:2023 E, E.1]"),
          "a pinned variant is still counted in the method as published, and the pin is said with its citation");
    std::println("east: {} variant(s) left after the pin\n", std::tuple_size_v<decltype(eastern.variantSet.cases)>);

    std::println("documentation of the south's cube:");
    auto const southCubeFormula = std::get<0>(southern.variantSet.cases).expression; // the prism is pruned
    formula::Documentation const southPage = formula::document(southCubeFormula);
    std::println("  {}", southPage.formula);
    print_symbols(southPage);
    std::println("");
    check(southPage.symbols.front().derivedAs == std::optional<std::string> { "b / a" },
          "the page says how the south derives the shape factor");

    // ---- 3. A runtime choice among compiled jurisdictions ----------------------------
    std::println("== 3. Which jurisdiction applies is a runtime value ==\n");
    for (Jurisdiction const jurisdiction: { Jurisdiction::Base, Jurisdiction::North, Jurisdiction::South })
    {
        auto const chosen = cubeStrengthIn(jurisdiction);
        if (!chosen)
        {
            std::println("jurisdiction {}: {}", std::to_underlying(jurisdiction), chosen.error());
            return 1;
        }
        auto const chosenStrength = formula::number_of(chosen);
        if (!chosenStrength)
        {
            std::println("jurisdiction {}: no value", std::to_underlying(jurisdiction));
            return 1;
        }
        std::println("jurisdiction {}: {} Pa", std::to_underlying(jurisdiction), *chosenStrength);
    }
    std::println("");
    // The one deliberate second run of the north: the runtime choice must reach the same method.
    check(formula::number_of(cubeStrengthIn(Jurisdiction::North)) == northStrength, "the runtime choice reaches the north");

    // ---- 4. A vocabulary --------------------------------------------------------------
    std::println("== 4. The same formula in two jurisdictions' words ==\n");
    auto const baseCubeFormula = std::get<1>(compressiveStrength.variantSet.cases).expression;
    std::string const inNorth = formula::render(baseCubeFormula, northernWords);
    std::string const inSouth = formula::render(baseCubeFormula, southernWords);
    std::println("north: {}\nsouth: {}\n", inNorth, inSouth);
    check(inNorth == "k_s * F / (a * b)" && inSouth == "k_s * F / (b * a)", "the two edges swap letters");

    formula::Documentation const southernPage = formula::document(baseCubeFormula, southernWords);
    std::println("the southern page's symbol table:");
    print_symbols(southernPage);
    std::println("");

    auto const southernRun = formula::explain_method<Cube>(compressiveStrength, specimen, southernWords);
    std::string const southernTrace = formula::render_trace(southernRun.trace, { .maxSteps = 30 });
    std::println("{}", southernTrace);
    check(southernTrace.contains("4. b = 163 mm\n"), "the trace writes the 163 mm edge as the south does");

    // ---- 5. Constraints ----------------------------------------------------------------
    std::println("== 5. Whose acceptance logic ==\n");

    auto const baseCheck = formula::explain_check_method(compressiveStrength, specimen);
    auto const westCheck = formula::explain_check_method(western, specimen);
    print_outcomes("base", baseCheck.outcome);
    print_outcomes("west", westCheck.outcome);
    std::println("");
    check(baseCheck.outcome.size() == 1 && baseCheck.outcome[0].is_satisfied(),
          "the base method's one check: 89.3 kN >= 47.3 kN");
    check(westCheck.outcome.size() == 2 && westCheck.outcome[0].is_violated() && westCheck.outcome[1].is_satisfied(),
          "the west's two checks, each at its own index: 89.3 kN < 97.3 kN, and 163 mm <= 1.73 x 103 mm");

    std::println("base constraints: {}", whoseConstraints(compressiveStrength));
    std::println("west constraints: {}\n", whoseConstraints(western));

    std::string const baseAcceptance = formula::render_trace(baseCheck.trace, { .maxSteps = 30 });
    std::string const westAcceptanceTrace = formula::render_trace(westCheck.trace, { .maxSteps = 30 });
    std::println("{}\n{}", baseAcceptance, westAcceptanceTrace);
    check(baseAcceptance.contains("[satisfied; the method's own constraint]"), "the base method's verdict is the method's own");
    check(westAcceptanceTrace.contains("[the load at failure is below 97.3 kN; jurisdiction overlay: Acceptance, "
                                       "Example Standard 9:2022 B, B.2]"),
          "the west's verdict names the west's annex");

    auto const revisedOutcomes = formula::check_method(westernRevised, specimen);
    print_outcomes("west, revised on top", revisedOutcomes);
    std::println("revised constraints: {}\n", whoseConstraints(westernRevised));
    check(revisedOutcomes.size() == 1 && revisedOutcomes[0].is_satisfied(),
          "stacked overlays: the later overlay's one constraint holds, and the earlier two are gone");

    std::println("all checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
