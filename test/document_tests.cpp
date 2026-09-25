// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
// Shares Diameter's symbol on purpose, with a different description and a
// different unit, so a formula using both proves deduplication keys on the
// quantity, not on the rendered letter.
struct ExcavationDepth: formula::Quantity<ExcavationDepth, "d", "excavation depth", formula::unit::Metre>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", formula::unit::Megapascal>
{
};
// Two distinct quantities appearing nowhere else in this file, so a
// constraint's symbol-table test can tell which side of its predicate
// contributed a given row.
struct ReplicateA: formula::Quantity<ReplicateA, "R_a", "first replicate reading", formula::unit::Megapascal>
{
};
struct ReplicateB: formula::Quantity<ReplicateB, "R_b", "second replicate reading", formula::unit::Megapascal>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Invented, as every citation in this repository must be.
constexpr auto ratio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", .section = "5.4.2" });

constexpr auto perCent = formula::documented(
    ratio * rat(100),
    { .title = "Water/cement ratio, per cent", .reference = "Example Standard 1:2020", .section = "5.4.3" });

// A two-variable predicate -- var<ReplicateA> against var<ReplicateB>, not a
// variable against an inert constant -- so a symbol-table test can tell
// whether each side was walked. Invented, as every citation and verdict in
// this repository must be.
constexpr auto replicateAgreement =
    formula::constraint(var<ReplicateA> > var<ReplicateB>,
                        formula::Verdict { "repeat the test" },
                        { .title = "Replicate agreement", .reference = "Example Standard 1:2020", .section = "6.2" });

// The two-argument call -- constraint(predicate, verdict), citation left at
// its default -- is an ordinary, supported way to declare a constraint, used
// by this project's own guide, example and several tests. So a constraint
// with no citation at all is not a hypothetical input.
constexpr auto uncitedAgreement = formula::constraint(var<ReplicateA> > var<ReplicateB>,
                                                     formula::Verdict { "repeat the test" });

// Cited by clause number alone, with no title -- an ordinary input for a
// rule that has a section but no name of its own. Deliberately a field
// other than `title`: it distinguishes collect()'s actual guard, "every
// field of the citation is blank", from a guard that only happened to
// check `title`, which this fixture would not catch.
constexpr auto sectionOnlyAgreement =
    formula::constraint(var<ReplicateA> > var<ReplicateB>, formula::Verdict { "repeat the test" },
                        { .section = "9.4" });

} // namespace

TEST_CASE("document: the documentation carries the rendered formula", "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    CHECK(documentation.formula == "V_w / V_c");
}

TEST_CASE("document: the rendered formula follows the requested dialect", "[document]")
{
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(ratio);

    CHECK(latex.formula == "\\frac{V_w}{V_c}");
}

TEST_CASE("document: the rendered formula follows the Markdown dialect too", "[document]")
{
    // Dialect::LaTeX above and Dialect::Plain (the default, exercised by the
    // very first test in this file) were the only dialects document<D>() was
    // ever asked to produce -- Dialect::Markdown was untested here even
    // though render<Markdown> exists and tools/gallery/main.cpp never
    // exercises it (it only reads .symbols, which is dialect-independent).
    formula::Documentation const markdown = formula::document<formula::Dialect::Markdown>(ratio);

    CHECK(markdown.formula == "`V_w` / `V_c`");
}

TEST_CASE("document: citations come back outermost first", "[document]")
{
    formula::Documentation const documentation = formula::document(perCent);

    REQUIRE(documentation.citations.size() == 2);
    CHECK(documentation.citations[0].title == std::string_view { "Water/cement ratio, per cent" });
    CHECK(documentation.citations[0].section == std::string_view { "5.4.3" });
    CHECK(documentation.citations[1].title == std::string_view { "Water/cement ratio" });
    CHECK(documentation.citations[1].section == std::string_view { "5.4.2" });
}

TEST_CASE("document: an undocumented formula yields an empty citation list", "[document]")
{
    formula::Documentation const documentation = formula::document(var<WaterVolume> + var<CementVolume>);

    CHECK(documentation.citations.empty());
    CHECK(documentation.formula == "V_w + V_c");
}

TEST_CASE("document: the symbol table carries each variable's symbol, description and unit", "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[0].description == std::string_view { "effective water content" });
    CHECK(documentation.symbols[0].unit == formula::unit::Litre);
}

TEST_CASE("document: symbols come back in first-appearance order", "[document]")
{
    // Alphabetical order would put A before d in a formula that reads
    // pi * d^2 / 4, which is not how anyone reads it.
    formula::Documentation const documentation =
        formula::document(formula::pi * formula::pow<2>(var<Diameter>) / var<WaterVolume>);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a quantity used twice appears once", "[document]")
{
    formula::Documentation const documentation = formula::document(var<WaterVolume> + var<WaterVolume> * rat(2));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: two entries for the same quantity compare equal", "[document]")
{
    constexpr formula::SymbolEntry left { .symbol = "d",
                                          .description = "specimen diameter",
                                          .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry right { .symbol = "d",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry other { .symbol = "D",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("document: a constant contributes no symbol", "[document]")
{
    formula::Documentation const documentation =
        formula::document(var<Diameter> * formula::constant<formula::unit::Millimetre>(rat(2)));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}

TEST_CASE("document: two quantities that share a symbol both get a row", "[document]")
{
    // Diameter is millimetres, ExcavationDepth is metres: two unrelated
    // quantities that happen to render the same letter. Collapsing them would
    // silently attribute one's description and unit to the other's uses.
    formula::Documentation const documentation = formula::document(var<Diameter> + var<ExcavationDepth>);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[0].description == std::string_view { "specimen diameter" });
    CHECK(documentation.symbols[0].unit == formula::unit::Millimetre);
    CHECK(documentation.symbols[1].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].description == std::string_view { "excavation depth" });
    CHECK(documentation.symbols[1].unit == formula::unit::Metre);
}

TEST_CASE("document: a negated variable still appears in the symbol table", "[document]")
{
    formula::Documentation const documentation = formula::document(-var<Diameter>);

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}

TEST_CASE("document: a variable under a root still appears in the symbol table", "[document]")
{
    formula::Documentation const documentation = formula::document(formula::sqrt(var<WaterVolume>));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a variable inside a RoundNode still appears in the symbol table, "
          "even nested under a BinaryNode",
          "[document]")
{
    // The RoundNode sits as the right operand of a BinaryNode, not at the
    // root -- exactly the shape the collect() forward declarations exist
    // for. An overload that is only *defined*, and never forward declared,
    // compiles for a formula where RoundNode sits at the top and fails to
    // find an overload here, where BinaryNode's collect() must recurse into
    // it before RoundNode's own collect() has been declared.
    constexpr auto node =
        var<CementVolume>
        + formula::rounded<formula::unit::Litre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<WaterVolume>);
    formula::Documentation const documentation = formula::document(node);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_c" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a variable inside a RoundSignificantNode still appears in the symbol table", "[document]")
{
    constexpr auto node = formula::rounded_to_digits<formula::unit::Millimetre,
                                                      formula::SignificantDigits { 3 },
                                                      formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);
    formula::Documentation const documentation = formula::document(node);

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}

TEST_CASE("document: a variable read through numeric_value_of still appears in the symbol table", "[document]")
{
    // Invented, as every justification in this repository is.
    constexpr auto node = formula::numeric_value_of<formula::unit::Litre,
                                                     "Example Standard 1:2020 states this coefficient over the "
                                                     "numeric value in litres">(var<WaterVolume>);
    formula::Documentation const documentation = formula::document(node);

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a WhenNode's predicate contributes to the symbol table", "[document]")
{
    // Strength appears only in the predicate -- not in either branch -- so
    // this fails if collect(WhenNode) walks the branches but forgets the
    // predicate, or if PredicateNode's own collect() forgets one of its
    // sides.
    constexpr auto node =
        formula::when(var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50)), var<Diameter>, var<ExcavationDepth>);
    formula::Documentation const documentation = formula::document(node);

    REQUIRE(documentation.symbols.size() == 3);
    CHECK(documentation.symbols[0].symbol == std::string_view { "f" });
    CHECK(documentation.symbols[0].description == std::string_view { "measured strength" });
}

TEST_CASE("document: a WhenNode's predicate's right-hand side contributes to the symbol table", "[document]")
{
    // Every other predicate in this file compares a variable against an
    // inert constant, so nothing ever reached PredicateNode::collect()'s
    // walk of node.rhs -- dropping that one line left the entire suite
    // green. A predicate comparing two quantities closes that gap, and is
    // also the more realistic shape: a formula guarded on one measurement
    // exceeding another, not on a fixed number.
    constexpr auto node = formula::when(var<WaterVolume> > var<CementVolume>, var<Diameter>, var<ExcavationDepth>);
    formula::Documentation const documentation = formula::document(node);

    // First-appearance order: predicate lhs, predicate rhs, thenBranch,
    // elseBranch.
    REQUIRE(documentation.symbols.size() == 4);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_c" });
}

TEST_CASE("document: a WhenNode documents both branches, not just the one that would be taken", "[document]")
{
    // This is the opposite of evaluation, deliberately: checked_evaluate_si
    // for WhenNode dispatches only the selected branch, but document() has no
    // input to select with, and a formula's documentation must not depend on
    // which branch some particular evaluation happened to take. If
    // collect(WhenNode) only walked thenBranch (mirroring evaluation),
    // ExcavationDepth would be missing below.
    constexpr auto node =
        formula::when(var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50)), var<Diameter>, var<ExcavationDepth>);
    formula::Documentation const documentation = formula::document(node);

    REQUIRE(documentation.symbols.size() == 3);
    // First-appearance order: predicate, then thenBranch, then elseBranch.
    CHECK(documentation.symbols[1].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].description == std::string_view { "specimen diameter" });
    CHECK(documentation.symbols[2].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[2].description == std::string_view { "excavation depth" });
}

TEST_CASE("document: a constraint's citation reaches the documentation", "[document]")
{
    // A Constraint is deliberately not a Node (constraint.hpp's file
    // comment) and so cannot be wrapped by documented() -- it carries its
    // own Citation instead. document() itself DOES accept a Constraint
    // directly, through the overload below the Node one in document.hpp;
    // going through formula::document(...) here, exactly as every other
    // test in this file does, is what proves that public path reachable
    // rather than only proving detail::collect's internals work.
    formula::Documentation const documentation = formula::document(replicateAgreement);

    // Proves the render() half of this overload too, not only the walk:
    // document(Constraint<P> const&) dispatches render<D>(node) to
    // render.hpp's own Constraint overload exactly as the Node overload
    // does for everything else.
    CHECK(documentation.formula == "require R_a > R_b");
    REQUIRE(documentation.citations.size() == 1);
    CHECK(documentation.citations[0].title == std::string_view { "Replicate agreement" });
    CHECK(documentation.citations[0].section == std::string_view { "6.2" });
}

TEST_CASE("document: an uncited constraint contributes no citation row", "[document]")
{
    // constraint(predicate, verdict, citation = {}) makes the citation
    // optional, so collect(Walk&, Constraint<P> const&) must check before
    // pushing node.citation onto the list -- pushing unconditionally would
    // turn documentation.citations.empty() from "this formula cites
    // nothing" into "this formula cites nothing, unless it read an uncited
    // constraint", and would render a bare, five-blank-field citation entry
    // on a generated page. The test above proves the cited direction still
    // contributes exactly one row; this is the other direction.
    formula::Documentation const documentation = formula::document(uncitedAgreement);

    CHECK(documentation.citations.empty());
}

TEST_CASE("document: a constraint cited by only one field still contributes a citation row", "[document]")
{
    // The all-empty and all-populated cases above do not pin *why* the
    // guard in collect(Walk&, Constraint<P> const&) is correct -- a guard
    // written as `!node.citation.title.empty()` passes both of those tests
    // too, and title is not what the guard actually checks. This is the
    // case that tells them apart: a citation with title blank and section
    // filled in must still contribute a row, because it is not blank --
    // only title is.
    formula::Documentation const documentation = formula::document(sectionOnlyAgreement);

    REQUIRE(documentation.citations.size() == 1);
    CHECK(documentation.citations[0].title.empty());
    CHECK(documentation.citations[0].section == std::string_view { "9.4" });
}

TEST_CASE("document: a constraint predicate's left-hand side reaches the symbol table", "[document]")
{
    formula::Documentation const documentation = formula::document(replicateAgreement);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "R_a" });
    CHECK(documentation.symbols[0].description == std::string_view { "first replicate reading" });
}

TEST_CASE("document: a constraint predicate's right-hand side reaches the symbol table, as a separate case",
          "[document]")
{
    // A second, independent case from the one above -- not a second CHECK in
    // the same test -- because the task-7 review of phase 8 found
    // collect(walk, node.rhs) completely uncovered: every predicate in this
    // file put its variable on the left and a constant on the right, so
    // deleting that line left the whole suite green. This is the same
    // two-variable predicate as the left-hand test, but the assertion below
    // targets the right side specifically.
    formula::Documentation const documentation = formula::document(replicateAgreement);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[1].symbol == std::string_view { "R_b" });
    CHECK(documentation.symbols[1].description == std::string_view { "second replicate reading" });
}

// ------------------------------------------------------- phase 10: lookups

namespace
{
using formula::band;
using formula::banded_lookup;
using formula::BandTable;
using formula::breakpoint;
using formula::BreakpointTable;
using formula::exact_lookup;
using formula::interpolating_lookup;
using formula::KeyTable;
namespace unit = formula::unit;

// Symbols chosen so that none of them occurs inside the words a lookup renders
// around them. `find` cannot tell a variable's symbol from the same characters
// sitting inside some other word, and `d` -- the symbol this file's Diameter
// already uses -- is inside `under`, which every banded row prints. Both halves
// of the cross-surface test at the end of this file are built on `find`: the
// presence half would report a variable in a formula that names none, and the
// order half would compare the position of a letter in `under` against the
// position of a real symbol. Symbols that cannot collide are what make either
// answer mean anything.
struct PlateThickness: formula::Quantity<PlateThickness, "t_p", "plate thickness", formula::unit::Millimetre>
{
};
struct CoreLength: formula::Quantity<CoreLength, "L_c", "core length", formula::unit::Millimetre>
{
};
struct GaugeLength: formula::Quantity<GaugeLength, "L_g", "gauge length", formula::unit::Millimetre>
{
};
// Read by no fixture in this file. The cross-surface test needs one such
// quantity: without it, "the formula shows a symbol if and only if the table
// has a row for it" collapses into "every candidate is present", which a
// symbol table that simply listed everything would also satisfy.
struct AbsentReading: formula::Quantity<AbsentReading, "Z_q", "a reading no formula here takes", formula::unit::Megapascal>
{
};

/// The three tables below are declared as non-degenerately as
/// `render_tests.cpp`'s are: unequal band widths and breakpoint spacings, a
/// bound and a breakpoint declared unreduced, no bound or key equal to its own
/// row's index, keys out of numeric order, one negative value on the curve.
///
/// **None of that can reach `detail::collect`**, which never looks at a table
/// at all -- so this is not a claim that a degenerate table would let a defect
/// in this file's subject survive. They are written this way because the
/// cross-surface test at the end reads the *rendered* formula, and because a
/// fixture that was degenerate on an axis nobody had yet named has let a
/// mutation survive in every task of this phase.
inline constexpr BandTable<3> LayerBands {
    band(3, 2, 7, 1),   // 3/2 to under 7 mm
    band(7, 1, 34, 2),  // 7 to under 17 mm -- 34/2 declared, so reduction shows
    band(17, 1, 40, 1), // 17 to under 40 mm
};

enum class ApparatusType : std::uint8_t
{
    Bench = 4,
    Frame = 6,
    Rig = 9,
};

inline constexpr KeyTable<ApparatusType, 3> ApparatusKeys {
    ApparatusType::Rig,   // key 9
    ApparatusType::Bench, // key 4
    ApparatusType::Frame, // key 6
};

inline constexpr BreakpointTable<3> ProfilePoints {
    breakpoint(3),
    breakpoint(22, 4), // 11/2 -- declared unreduced, and in the middle
    breakpoint(24),
};

/// Key unit `mm` (a symbol), result unit `One` (none) -- and an operand that is
/// **not** a bare variable, so the walk has to pass through a node before it
/// reaches one. `profileLookup` below takes the other end of that axis.
[[nodiscard]] constexpr auto bandedLookup()
{
    return banded_lookup<unit::Millimetre, LayerBands, unit::One>(var<PlateThickness> * rat(2),
                                                                  { rat(23, 25), rat(6, 5), rat(27, 20) });
}

/// No operand at all -- the whole of what distinguishes this kind for a
/// documentation walk.
[[nodiscard]] constexpr auto apparatusLookup()
{
    return exact_lookup<ApparatusKeys, unit::Megapascal>(ApparatusType::Rig, { rat(8, 5), rat(3, 4), rat(21, 10) });
}

/// A bare variable as the operand, against `bandedLookup`'s compound one.
[[nodiscard]] constexpr auto profileLookup()
{
    return interpolating_lookup<unit::Millimetre, ProfilePoints, unit::Megapascal>(var<CoreLength>,
                                                                                   { rat(4, 5), rat(-9, 10), rat(13, 10) });
}

// Invented, as every citation in this repository must be.
constexpr formula::Citation layerSource { .title = "Layer correction table",
                                          .reference = "Example Standard 2:2021",
                                          .section = "7.3" };
constexpr formula::Citation thicknessSource { .title = "Plate thickness, as measured",
                                              .reference = "Example Standard 2:2021",
                                              .section = "4.1" };
constexpr formula::Citation apparatusSource { .title = "Apparatus correction table",
                                              .reference = "Example Standard 2:2021",
                                              .section = "7.4" };
constexpr formula::Citation profileSource { .title = "Profile curve",
                                            .reference = "Example Standard 2:2021",
                                            .section = "7.5" };
constexpr formula::Citation coreSource { .title = "Core length, as measured",
                                         .reference = "Example Standard 2:2021",
                                         .section = "4.2" };

/// A cited table entered with a cited measurement -- the shape a published
/// method actually has, and the reason the table is the part of a method that
/// carries a source. The inner citation is reachable only through the lookup's
/// own operand.
[[nodiscard]] constexpr auto citedBandedLookup()
{
    return formula::documented(
        banded_lookup<unit::Millimetre, LayerBands, unit::One>(formula::documented(var<PlateThickness>, thicknessSource),
                                                               { rat(23, 25), rat(6, 5), rat(27, 20) }),
        layerSource);
}

[[nodiscard]] constexpr auto citedApparatusLookup()
{
    return formula::documented(apparatusLookup(), apparatusSource);
}

[[nodiscard]] constexpr auto citedProfileLookup()
{
    return formula::documented(
        interpolating_lookup<unit::Millimetre, ProfilePoints, unit::Megapascal>(
            formula::documented(var<CoreLength>, coreSource), { rat(4, 5), rat(-9, 10), rat(13, 10) }),
        profileSource);
}

/// True when @p documentation carries a symbol-table row spelled @p symbol.
[[nodiscard]] bool hasSymbolRow(formula::Documentation const& documentation, std::string_view symbol)
{
    for (formula::SymbolEntry const& entry: documentation.symbols)
        if (entry.symbol == symbol)
            return true;
    return false;
}

/// One quantity's half of `formulaAndSymbolsAgree` below: the rendered formula
/// shows this quantity's symbol if and only if the symbol table has a row for
/// it. Counts the ones it found in @p shown.
template <typename Q>
void quantityAgrees(formula::Documentation const& documentation, std::size_t& shown)
{
    constexpr std::string_view symbol = formula::Describe<Q>::symbol;
    bool const inFormula = documentation.formula.find(symbol) != std::string::npos;
    INFO("symbol " << symbol << " against: " << documentation.formula);
    CHECK(inFormula == hasSymbolRow(documentation, symbol));
    if (inFormula)
        ++shown;
}

/// The two halves of a `Documentation` are filled by two different walks over
/// the same tree -- `render<D>()` fills `.formula`, `detail::collect` fills
/// `.symbols` -- and `document()` never compares them with each other. A node
/// kind taught to one walk and not to the other is therefore invisible to any
/// test that asserts each half on its own, which is the shape of the defect
/// phase 8 published: two surfaces, each internally consistent, disagreeing.
///
/// So this asserts the relation instead of either half. For every quantity in
/// @p Qs the rendered formula shows its symbol **if and only if** the symbol
/// table carries a row for it, and the table carries no row beyond those -- so
/// neither a variable the renderer shows and the walk missed, nor a row for
/// something that is not a variable at all (a key unit, a band, a key), can
/// pass.
///
/// The rows also come back in the order the formula reads, which is what
/// `Documentation::symbols` promises in so many words -- "in the order they
/// first appear when the formula is read left to right". Every other order
/// assertion in this file checks that promise against an order its author
/// wrote down, which agrees with the formula only as long as both were got
/// right; this checks it against the formula.
///
/// **Neither surface is located by the other.** Both are probed with a symbol
/// read from the quantity's own `Describe`, which is a third place; a test that
/// found the symbol by searching the formula, or by reading it back off a row,
/// would agree with itself whatever the two walks said.
///
/// @p Qs must name every quantity the tree reads and at least one it does not.
/// No two of them may share a rendered symbol, and none may occur inside
/// another or inside the words a node renders around them -- `find` would then
/// be answering about the wrong occurrence, in both halves.
template <typename... Qs>
void formulaAndSymbolsAgree(auto const& node)
{
    formula::Documentation const documentation = formula::document(node);

    std::size_t shown = 0;
    (quantityAgrees<Qs>(documentation, shown), ...);

    INFO("symbol table against: " << documentation.formula);
    CHECK(documentation.symbols.size() == shown);

    std::size_t previous = 0;
    bool firstRow = true;
    for (formula::SymbolEntry const& entry: documentation.symbols)
    {
        std::size_t const at = documentation.formula.find(entry.symbol);
        INFO("row " << entry.symbol << " found at " << at);
        REQUIRE(at != std::string::npos);
        CHECK((firstRow || at > previous));
        previous = at;
        firstRow = false;
    }
}
} // namespace

TEST_CASE("document: a banded lookup's operand reaches the symbol table", "[document][lookup]")
{
    // The lookup sits on the RIGHT of a binary node and its operand is a
    // compound expression; the interpolating case below puts its lookup on the
    // LEFT with a bare variable as the operand. The pair covers both ends of
    // both axes rather than one end of each.
    formula::Documentation const documentation = formula::document(var<GaugeLength> * bandedLookup());

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "L_g" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "t_p" });
    CHECK(documentation.symbols[1].description == std::string_view { "plate thickness" });
    CHECK(documentation.symbols[1].unit == formula::unit::Millimetre);
}

TEST_CASE("document: an exact lookup names no variable and contributes no row", "[document][lookup]")
{
    // `lookup(key 9, ...)` occupies the subject position of the rendered
    // formula exactly where the other two kinds put their operand, so a reader
    // could take it for a variable. It is not one: an exact lookup has no
    // operand at all, and a key is a discriminator with no symbol and no unit.
    // The only row here is the one contributed from outside the lookup.
    formula::Documentation const documentation = formula::document(var<GaugeLength> * apparatusLookup());

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "L_g" });
}

TEST_CASE("document: an interpolating lookup's operand reaches the symbol table", "[document][lookup]")
{
    formula::Documentation const documentation = formula::document(profileLookup() * var<GaugeLength>);

    // First-appearance order with the lookup on the LEFT, so its operand's
    // quantity must come first. A walk that reached the operand but contributed
    // its row after the rest of the tree's would pass the banded case above and
    // fail here.
    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "L_c" });
    CHECK(documentation.symbols[0].description == std::string_view { "core length" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "L_g" });
}

TEST_CASE("document: a citation on a banded lookup reaches the documentation with its operand's", "[document][lookup]")
{
    formula::Documentation const documentation = formula::document(citedBandedLookup());

    // Outermost first, as for every other node kind: the table's own source
    // before the source of the measurement it is entered with. The inner
    // citation is reachable ONLY through the lookup's operand, so it is the
    // citation list -- not only the symbol table -- that fails if the banded
    // overload stops walking that operand.
    REQUIRE(documentation.citations.size() == 2);
    CHECK(documentation.citations[0].title == std::string_view { "Layer correction table" });
    CHECK(documentation.citations[0].section == std::string_view { "7.3" });
    CHECK(documentation.citations[1].title == std::string_view { "Plate thickness, as measured" });
    CHECK(documentation.citations[1].section == std::string_view { "4.1" });

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "t_p" });
}

TEST_CASE("document: a citation on an exact lookup reaches the documentation", "[document][lookup]")
{
    // No nested citation here, and that asymmetry belongs to the node kind
    // rather than to this test: an exact lookup has no operand, so there is no
    // sub-expression that could carry a second source. A discriminator is not a
    // measurement and cites nothing of its own.
    formula::Documentation const documentation = formula::document(citedApparatusLookup());

    REQUIRE(documentation.citations.size() == 1);
    CHECK(documentation.citations[0].title == std::string_view { "Apparatus correction table" });
    CHECK(documentation.citations[0].section == std::string_view { "7.4" });
    CHECK(documentation.symbols.empty());
}

TEST_CASE("document: a citation on an interpolating lookup reaches the documentation with its operand's",
          "[document][lookup]")
{
    formula::Documentation const documentation = formula::document(citedProfileLookup());

    REQUIRE(documentation.citations.size() == 2);
    CHECK(documentation.citations[0].title == std::string_view { "Profile curve" });
    CHECK(documentation.citations[0].section == std::string_view { "7.5" });
    CHECK(documentation.citations[1].title == std::string_view { "Core length, as measured" });
    CHECK(documentation.citations[1].section == std::string_view { "4.2" });

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "L_c" });
}

TEST_CASE("document: the rendered formula and the symbol table agree on every lookup kind", "[document][lookup]")
{
    formulaAndSymbolsAgree<PlateThickness, GaugeLength, AbsentReading>(var<GaugeLength> * bandedLookup());
    formulaAndSymbolsAgree<PlateThickness, GaugeLength, AbsentReading>(var<GaugeLength> * apparatusLookup());
    formulaAndSymbolsAgree<CoreLength, GaugeLength, AbsentReading>(profileLookup() * var<GaugeLength>);
}
