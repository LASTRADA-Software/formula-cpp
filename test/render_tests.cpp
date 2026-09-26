// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/vocabulary.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", formula::unit::SquareMetre>
{
};
struct Edge: formula::Quantity<Edge, "a", "cube edge", formula::unit::Metre>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", formula::unit::Megapascal>
{
};

/// A gram squared, for a variance of masses in grams.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};
struct OtherVariance: formula::Quantity<OtherVariance, "t2", "variance of a second series", GramSquared>
{
};

struct Determinations: formula::Quantity<Determinations, "n_d", "number of determinations", formula::unit::One>
{
};

/// The shared fixtures' critical-value table. **Invented, and deliberately
/// unrealistic -- no published table holds values like these.** No row for 7.
inline constexpr formula::SampleSizeTable<5> DeviationSizes { 3, 4, 5, 6, 8 };

/// The deviation table's critical value, read at the number of determinations.
inline constexpr auto criticalLimit =
    formula::critical_value<DeviationSizes, formula::unit::One>(formula::var<Determinations>,
                                                                { formula::Rational { 10 },
                                                                  formula::Rational { 30 },
                                                                  formula::Rational { 20 },
                                                                  formula::Rational { 50 },
                                                                  formula::Rational { 40 } });

/// The root of the variance, to 0.01 g -- the spelling every dialect below pins.
inline constexpr auto roundedSpread =
    formula::rounded_sqrt<formula::unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        formula::var<MassVariance>);

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

/// A precision limit over a diameter, for the Markdown guards.
inline constexpr auto precisionOfDiameter = formula::precision_limit<formula::PrecisionKind::Repeatability>(
    (formula::var<Diameter> + formula::var<Diameter>) / rat(2), rat(1, 50) * formula::precision_level<Diameter>);

using formula::Dialect;
using formula::var;

} // namespace

TEST_CASE("render: a variable renders as its own symbol", "[render]")
{
    CHECK(formula::render<Dialect::Plain>(var<WaterVolume>) == "V_w");
    CHECK(formula::render<Dialect::Plain>(var<Diameter>) == "d");
}

TEST_CASE("render: the default dialect is plain", "[render]")
{
    CHECK(formula::render(var<WaterVolume> / var<CementVolume>) == "V_w / V_c");
}

TEST_CASE("render: the four operators render as themselves", "[render]")
{
    CHECK(formula::render(var<WaterVolume> + var<CementVolume>) == "V_w + V_c");
    CHECK(formula::render(var<WaterVolume> - var<CementVolume>) == "V_w - V_c");
    CHECK(formula::render(var<WaterVolume> * var<CementVolume>) == "V_w * V_c");
    CHECK(formula::render(var<WaterVolume> / var<CementVolume>) == "V_w / V_c");
}

TEST_CASE("render: a sum inside a quotient keeps its brackets", "[render]")
{
    CHECK(formula::render((var<WaterVolume> + var<CementVolume>) / var<CementVolume>) == "(V_w + V_c) / V_c");
}

TEST_CASE("render: subtraction and division bracket their right operand", "[render]")
{
    // a - (b - c) is not (a - b) - c, so the brackets are not decoration.
    CHECK(formula::render(var<WaterVolume> - (var<CementVolume> - var<WaterVolume>) ) == "V_w - (V_c - V_w)");
    CHECK(formula::render(var<WaterVolume> / (var<CementVolume> * var<CementVolume>) ) == "V_w / (V_c * V_c)");
}

TEST_CASE("render: equal precedence on the left needs no brackets", "[render]")
{
    CHECK(formula::render((var<WaterVolume> - var<CementVolume>) -var<WaterVolume>) == "V_w - V_c - V_w");
    CHECK(formula::render((var<WaterVolume> / var<CementVolume>) *rat(100)) == "V_w / V_c * 100");
}

TEST_CASE("render: a product inside a sum needs no brackets", "[render]")
{
    CHECK(formula::render(var<WaterVolume> + var<CementVolume> * rat(2)) == "V_w + V_c * 2");
}

TEST_CASE("render: negation brackets a sum but not a variable", "[render]")
{
    CHECK(formula::render(-var<WaterVolume>) == "-V_w");
    CHECK(formula::render(-(var<WaterVolume> + var<CementVolume>) ) == "-(V_w + V_c)");
}

TEST_CASE("render: a constant renders with its unit", "[render]")
{
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(139))) == "139 mm");
}

TEST_CASE("render: a dimensionless constant renders bare", "[render]")
{
    CHECK(formula::render(formula::number(rat(4))) == "4");
    CHECK(formula::render(formula::number(rat(1, 4))) == "1/4");
}

TEST_CASE("render: a power renders its exponent", "[render]")
{
    CHECK(formula::render(formula::pow<2>(var<Diameter>)) == "d^2");
    CHECK(formula::render(formula::pow<-1>(var<Diameter>)) == "d^-1");
    // A sum raised to a power must keep its brackets.
    CHECK(formula::render(formula::pow<2>(var<WaterVolume> + var<CementVolume>)) == "(V_w + V_c)^2");
}

TEST_CASE("render: a square root and a general root render differently", "[render]")
{
    CHECK(formula::render(formula::sqrt(var<Area>)) == "sqrt(A)");
    CHECK(formula::render(formula::cbrt(var<Area>)) == "root3(A)");
    CHECK(formula::render(formula::root<4>(var<Area>)) == "root4(A)");
}

TEST_CASE("render: pi renders per dialect", "[render]")
{
    CHECK(formula::render<Dialect::Plain>(formula::pi) == "pi");
    CHECK(formula::render<Dialect::LaTeX>(formula::pi) == "\\pi");
}

TEST_CASE("render: LaTeX renders a quotient as a fraction", "[render]")
{
    CHECK(formula::render<Dialect::LaTeX>(var<WaterVolume> / var<CementVolume>) == "\\frac{V_w}{V_c}");
    // A fraction brackets nothing: \frac already groups both sides.
    CHECK(formula::render<Dialect::LaTeX>((var<WaterVolume> + var<CementVolume>) / var<CementVolume>)
          == "\\frac{V_w + V_c}{V_c}");
}

TEST_CASE("render: LaTeX spells multiplication, powers and roots its own way", "[render]")
{
    CHECK(formula::render<Dialect::LaTeX>(var<WaterVolume> * var<CementVolume>) == "V_w \\cdot V_c");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(var<Diameter>)) == "d^{2}");
    CHECK(formula::render<Dialect::LaTeX>(formula::sqrt(var<Area>)) == "\\sqrt{A}");
    CHECK(formula::render<Dialect::LaTeX>(formula::root<4>(var<Area>)) == "\\sqrt[4]{A}");
}

TEST_CASE("render: the Markdown dialect emphasises the symbols", "[render]")
{
    // A symbol containing an underscore would otherwise be read as emphasis by
    // a Markdown renderer, which is exactly why this dialect exists.
    CHECK(formula::render<Dialect::Markdown>(var<WaterVolume> / var<CementVolume>) == "`V_w` / `V_c`");
}

TEST_CASE("render: the Markdown dialect covers every node kind, not only the variable it was built for", "[render]")
{
    // The test above is the only place Dialect::Markdown was exercised at
    // all, and it covers VarNode and BinaryNode(Divide) together. Every
    // other node kind's render_node<Markdown> falls back to the same text
    // Dialect::Plain produces (render.hpp has no Markdown-specific branch
    // for any of them), which is correct -- but that correctness was
    // untested, so a change that broke it would not have been caught. One
    // assertion per node kind pins the current, correct behaviour.
    constexpr auto citedDiameter = formula::documented(var<Diameter>, { .title = "Diameter, cited" });

    CHECK(formula::render<Dialect::Markdown>(var<Diameter>) == "`d`"); // VarNode
    CHECK(formula::render<Dialect::Markdown>(formula::constant<formula::unit::Millimetre>(rat(139)))
          == "139 mm");                                                                                 // ConstantNode
    CHECK(formula::render<Dialect::Markdown>(-var<Diameter>) == "-`d`");                                // UnaryNode
    CHECK(formula::render<Dialect::Markdown>(var<WaterVolume> + var<CementVolume>) == "`V_w` + `V_c`"); // BinaryNode
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(var<Diameter>)) == "`d`^2");               // PowerNode
    CHECK(formula::render<Dialect::Markdown>(formula::sqrt(var<Area>)) == "sqrt(`A`)");                 // RootNode
    CHECK(formula::render<Dialect::Markdown>(formula::pi) == "pi");                                     // PiNode
    CHECK(formula::render<Dialect::Markdown>(citedDiameter) == "`d`");                                  // DocumentedNode
    CHECK(formula::render<Dialect::Markdown>(roundedSpread) == "round(sqrt(`s2`), to 2 dp of g)");      // RoundedRootNode
    CHECK(formula::render<Dialect::Markdown>(criticalLimit) == "critical(`n_d`, at 3, 4, 5, 6, 8)"); // SampleSizeLookupNode
    CHECK(formula::render<Dialect::Markdown>(formula::abs(var<Diameter> - var<Diameter>))
          == "abs(`d` - `d`)"); // AbsoluteValueNode
    CHECK(formula::render<Dialect::Markdown>(precisionOfDiameter)
          == "r(1/50 * level; level = (`d` + `d`) / 2)"); // PrecisionLimitNode, PrecisionLevelNode
}

TEST_CASE("render: a critical value prints every declared size and none of the values", "[render]")
{
    CHECK(formula::render(criticalLimit) == "critical(n_d, at 3, 4, 5, 6, 8)");
    CHECK(formula::render<Dialect::LaTeX>(criticalLimit)
          == "\\operatorname{critical}(n_d,\\allowbreak \\mathrm{at\\ }3,\\allowbreak 4,\\allowbreak 5,\\allowbreak "
             "6,\\allowbreak 8)");
    // The values are data: 50 and 40 appear nowhere in the formula's text.
    CHECK(formula::render(criticalLimit).find("50") == std::string::npos);
    CHECK(formula::render(criticalLimit).find("40") == std::string::npos);
    // A call, so an atom to whatever holds it.
    CHECK(formula::render(criticalLimit * var<Determinations>) == "critical(n_d, at 3, 4, 5, 6, 8) * n_d");
    // A table of no sizes says so, as an empty lookup does.
    constexpr auto empty =
        formula::critical_value<formula::SampleSizeTable<0> {}, formula::unit::One>(var<Determinations>, {});
    CHECK(formula::render(empty) == "critical(n_d, no rows)");
}

TEST_CASE("render: a rounded square root reads as a rounding of a root, in every dialect", "[render]")
{
    CHECK(formula::render(roundedSpread) == "round(sqrt(s2), to 2 dp of g)");
    CHECK(formula::render<Dialect::LaTeX>(roundedSpread) == "\\operatorname{round}_{2\\,\\mathrm{g}}(\\sqrt{s2})");

    // The mode is the trace's, as it is for `rounded`: these two differ only
    // in it, and render alike.
    CHECK(formula::render(
              formula::rounded_sqrt<formula::unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::Floor>(
                  var<MassVariance>))
          == formula::render(roundedSpread));

    // The call's own parentheses group a compound radicand, and the call is
    // an atom to whatever holds it: no bracket either side.
    constexpr auto pooled =
        formula::rounded_sqrt<formula::unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::Ceiling>(
            (var<MassVariance> + var<OtherVariance>) / rat(2));
    CHECK(formula::render(pooled) == "round(sqrt((s2 + t2) / 2), to 3 dp of g)");
    CHECK(formula::render(pooled * rat(2)) == "round(sqrt((s2 + t2) / 2), to 3 dp of g) * 2");
    CHECK(formula::render<Dialect::LaTeX>(pooled) == "\\operatorname{round}_{3\\,\\mathrm{g}}(\\sqrt{\\frac{s2 + t2}{2}})");
}

TEST_CASE("render: a citation does not appear in the rendered formula", "[render]")
{
    constexpr auto documented = formula::documented(var<WaterVolume> / var<CementVolume>, { .title = "Water/cement ratio" });

    CHECK(formula::render(documented) == "V_w / V_c");
    // And wrapping does not change how the result is bracketed in a larger tree.
    CHECK(formula::render(documented * rat(100)) == "V_w / V_c * 100");
}

TEST_CASE("render: a documented sum inside a quotient keeps its brackets", "[render]")
{
    constexpr auto documented = formula::documented(var<WaterVolume> + var<CementVolume>, { .title = "Total volume" });

    CHECK(formula::render(documented / var<Diameter>) == "(V_w + V_c) / d");
}

TEST_CASE("render: a documented negative constant as the base of a power keeps its bracket", "[render]")
{
    // The two cases above wrap a VarNode and a sum -- both already Atom and
    // Additive at the *type* level, so both would pass even if the runtime
    // precedence_of(DocumentedNode) overload did not exist: the generic
    // fallback to the type-level PrecedenceOf trait gives the right answer
    // for them regardless. A ConstantNode is the one node kind whose
    // bracketing is decided by data the type does not carry (its sign), so
    // it is the one wrapper case that actually exercises the runtime
    // overload -- and the one this test pins.
    constexpr auto documented = formula::documented(formula::number(rat(-5)), { .title = "An invented negative constant" });

    CHECK(formula::render(formula::pow<2>(documented)) == "(-5)^2");
}

TEST_CASE("render: a deep tree renders without losing a bracket", "[render]")
{
    constexpr auto circularArea = formula::pi * formula::pow<2>(var<Diameter>) / rat(4);

    CHECK(formula::render(circularArea) == "pi * d^2 / 4");
    CHECK(formula::render<Dialect::LaTeX>(circularArea) == "\\frac{\\pi \\cdot d^{2}}{4}");
}

TEST_CASE("render: a negative constant as the base of a power keeps its bracket", "[render]")
{
    // A trait alone cannot answer this: ConstantNode is Precedence::Atom by
    // type, but a negative number's rendered text opens with a "-" that reads
    // like a unary minus. Without the bracket, "-5^2" means "-(5^2)" to a
    // reader, while the tree means (-5)^2 -- these evaluate to different
    // numbers, so this is not a cosmetic bracket.
    CHECK(formula::render(formula::pow<2>(formula::number(rat(-5)))) == "(-5)^2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(formula::number(rat(-5)))) == "(-5)^{2}");
}

TEST_CASE("render: a positive constant as the base of a power needs no bracket", "[render]")
{
    // Confirms the fix is keyed on sign, not a blanket bracket around every
    // constant that sits at the base of a power.
    CHECK(formula::render(formula::pow<2>(formula::number(rat(5)))) == "5^2");
}

TEST_CASE("render: a negative constant as a factor stays unbracketed", "[render]")
{
    // Multiplication cannot misread the leading "-" the way a power's base
    // can (there is no "-5 * d" reading other than the one intended), so a
    // negative constant here is left exactly as before.
    CHECK(formula::render(formula::number(rat(-5)) * var<Diameter>) == "-5 * d");
}

TEST_CASE("render: a unit-bearing constant as the base of a power keeps its bracket", "[render]")
{
    // A constant with a unit symbol renders as two tokens ("139 mm"), not
    // one, so without a bracket "139 mm^2" would read as "139 * mm^2" =
    // 139 mm^2, while the tree means (139 mm)^2 = 19321 mm^2 -- the same
    // class of defect as a negative constant's leading "-", for the other
    // piece of runtime data the type does not carry.
    CHECK(formula::render(formula::pow<2>(formula::constant<formula::unit::Millimetre>(rat(139)))) == "(139 mm)^2");
}

TEST_CASE("render: a unit-bearing constant as a term or factor stays unbracketed", "[render]")
{
    // Neither an additive nor a multiplicative context can misread "139 mm"
    // the way a power's base can, so a unit-bearing constant here is left
    // exactly as before. Addition requires both sides to share a dimension,
    // so the addend is also stated in millimetres rather than as a bare
    // dimensionless rational.
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(139))
                          + formula::constant<formula::unit::Millimetre>(rat(3)))
          == "139 mm + 3 mm");
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(139)) * rat(2)) == "139 mm * 2");
}

TEST_CASE("render: a dimensionless constant as the base of a power needs no bracket", "[render]")
{
    // Confirms the unit-symbol fix is keyed on the unit having a symbol, not
    // a blanket bracket around every constant at the base of a power: `One`
    // has no symbol, so a constant of it renders as a single token.
    CHECK(formula::render(formula::pow<2>(formula::constant<formula::unit::One>(rat(5)))) == "5^2");
}

// ------------------------------------------------------- phase 8: rounding

TEST_CASE("render: a decimal-places rounding node renders as round(..., to N dp of unit)", "[render][rounding]")
{
    // The granularity is a comma-separated second argument, operand first --
    // see the comment on RoundNode's render_node for why: a trailing suffix
    // with nothing between it and the operand (round(... to 1 dp of mm),
    // fixed in review round 1) let it misattach to a WhenNode operand's else
    // branch, and a `[...]` prefix right against the operand's own
    // parentheses (round[to 1 dp of mm](...), the round-1 fix itself) read as
    // a CommonMark link in Markdown, fixed in review round 3.
    constexpr auto rounded =
        formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);

    CHECK(formula::render<Dialect::Plain>(rounded) == "round(d, to 1 dp of mm)");
    CHECK(formula::render<Dialect::Markdown>(rounded) == "round(`d`, to 1 dp of mm)");
    CHECK(formula::render<Dialect::LaTeX>(rounded) == "\\operatorname{round}_{1\\,\\mathrm{mm}}(d)");
    // The RoundingMode (HalfAwayFromZero here) does not appear anywhere above
    // -- see the comment on RoundNode's render_node for why that is a
    // decision, not an oversight.
}

TEST_CASE("render: a decimal-places rounding node inside a power and inside a product keeps no extra bracket",
          "[render][rounding]")
{
    // A rounding node reads as a function call -- like sqrt(...) -- so it is
    // already fully delimited by its own parentheses and never needs a
    // bracket added around it, in either context.
    constexpr auto rounded =
        formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);

    CHECK(formula::render(formula::pow<2>(rounded)) == "round(d, to 1 dp of mm)^2");
    CHECK(formula::render(rounded * rat(2)) == "round(d, to 1 dp of mm) * 2");
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(rounded)) == "round(`d`, to 1 dp of mm)^2");
    CHECK(formula::render<Dialect::Markdown>(rounded * rat(2)) == "round(`d`, to 1 dp of mm) * 2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(rounded)) == "\\operatorname{round}_{1\\,\\mathrm{mm}}(d)^{2}");
    CHECK(formula::render<Dialect::LaTeX>(rounded * rat(2)) == "\\operatorname{round}_{1\\,\\mathrm{mm}}(d) \\cdot 2");
}

TEST_CASE("render: a significant-digits rounding node renders as round(..., to N sf of unit)", "[render][rounding]")
{
    constexpr auto rounded = formula::rounded_to_digits<formula::unit::Millimetre,
                                                        formula::SignificantDigits { 2 },
                                                        formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render<Dialect::Plain>(rounded) == "round(d, to 2 sf of mm)");
    CHECK(formula::render<Dialect::Markdown>(rounded) == "round(`d`, to 2 sf of mm)");
    CHECK(formula::render<Dialect::LaTeX>(rounded) == "\\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{mm}}(d)");
}

TEST_CASE("render: a significant-digits rounding node inside a power and inside a product keeps no extra bracket",
          "[render][rounding]")
{
    constexpr auto rounded = formula::rounded_to_digits<formula::unit::Millimetre,
                                                        formula::SignificantDigits { 2 },
                                                        formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render(formula::pow<2>(rounded)) == "round(d, to 2 sf of mm)^2");
    CHECK(formula::render(rounded * rat(2)) == "round(d, to 2 sf of mm) * 2");
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(rounded)) == "round(`d`, to 2 sf of mm)^2");
    CHECK(formula::render<Dialect::Markdown>(rounded * rat(2)) == "round(`d`, to 2 sf of mm) * 2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(rounded))
          == "\\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{mm}}(d)^{2}");
    CHECK(formula::render<Dialect::LaTeX>(rounded * rat(2))
          == "\\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{mm}}(d) \\cdot 2");
}

// --------------------------------------------------- phase 8: predicates

TEST_CASE("render: a predicate renders as lhs comparison rhs", "[render][predicate]")
{
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));

    CHECK(formula::render<Dialect::Plain>(overThreshold) == "f > 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(overThreshold) == "`f` > 473/10 MPa");
    CHECK(formula::render<Dialect::LaTeX>(overThreshold) == "f > 473/10\\,\\mathrm{MPa}");
    // The default dialect for a Predicate is plain, exactly as for a Node.
    CHECK(formula::render(overThreshold) == "f > 473/10 MPa");
}

TEST_CASE("render: every comparison spells correctly, and three of them get a LaTeX-specific symbol", "[render][predicate]")
{
    // <, > and == read the same in every dialect; <=, >= and != get the
    // mathematical spelling in LaTeX, the same way BinaryNode's "*" becomes
    // "\cdot" there.
    constexpr auto threshold = formula::constant<formula::unit::Megapascal>(rat(473, 10));

    CHECK(formula::render(var<Strength> < threshold) == "f < 473/10 MPa");
    CHECK(formula::render(var<Strength> <= threshold) == "f <= 473/10 MPa");
    CHECK(formula::render(var<Strength> > threshold) == "f > 473/10 MPa");
    CHECK(formula::render(var<Strength> >= threshold) == "f >= 473/10 MPa");
    CHECK(formula::render(var<Strength> == threshold) == "f == 473/10 MPa");
    CHECK(formula::render(var<Strength> != threshold) == "f != 473/10 MPa");

    CHECK(formula::render<Dialect::LaTeX>(var<Strength> < threshold) == "f < 473/10\\,\\mathrm{MPa}");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> <= threshold) == "f \\leq 473/10\\,\\mathrm{MPa}");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> > threshold) == "f > 473/10\\,\\mathrm{MPa}");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> >= threshold) == "f \\geq 473/10\\,\\mathrm{MPa}");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> == threshold) == "f = 473/10\\,\\mathrm{MPa}");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> != threshold) == "f \\neq 473/10\\,\\mathrm{MPa}");

    // Markdown only backtick-quotes the variable; the comparison symbol
    // itself is unaffected, the same way it is unaffected for BinaryNode.
    CHECK(formula::render<Dialect::Markdown>(var<Strength> < threshold) == "`f` < 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(var<Strength> <= threshold) == "`f` <= 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(var<Strength> > threshold) == "`f` > 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(var<Strength> >= threshold) == "`f` >= 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(var<Strength> == threshold) == "`f` == 473/10 MPa");
    CHECK(formula::render<Dialect::Markdown>(var<Strength> != threshold) == "`f` != 473/10 MPa");
}

TEST_CASE("render: a conditional nested as a predicate's operand keeps its bracket", "[render][predicate]")
{
    // A PredicateNode's own operands are ordinary Node expressions, and a
    // WhenNode is one of those -- so the same hazard a power or a product
    // operand has applies here too: rendered without a bracket, "if f > 473/10
    // MPa then f else f * 2 > 137/10 MPa" would read as the comparison applying
    // to the else branch alone, not to the whole conditional.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * rat(2));
    constexpr auto guarded = chosen > formula::constant<formula::unit::Megapascal>(rat(137, 10));

    CHECK(formula::render(guarded) == "(if f > 473/10 MPa then f else f * 2) > 137/10 MPa");
}

// --------------------------------------------------- phase 8: conditionals

TEST_CASE("render: a conditional renders as if/then/else, and as a LaTeX cases block", "[render][conditional]")
{
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render<Dialect::Plain>(chosen) == "if f > 473/10 MPa then f * 2 else f * 4");
    CHECK(formula::render<Dialect::Markdown>(chosen) == "if `f` > 473/10 MPa then `f` * 2 else `f` * 4");
    CHECK(formula::render<Dialect::LaTeX>(chosen)
          == "\\begin{cases} f \\cdot 2 & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 4 & \\text{otherwise} "
             "\\end{cases}");
    // RoundingMode is not the only thing this phase deliberately keeps out of
    // the rendered text -- WhenNode has no state to omit, but note that its
    // predicate's operands are plain quantities and constants here on
    // purpose: the brackets a nested conditional needs are covered below,
    // not in this standalone case.
}

TEST_CASE("render: a conditional inside a power keeps its bracket", "[render][conditional]")
{
    // This is the case phase 6's two rendering bugs generalise to: a node
    // whose *text* binds looser than arithmetic must bracket as the base of
    // a power, exactly as a negative or unit-bearing constant does.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render(formula::pow<2>(chosen)) == "(if f > 473/10 MPa then f * 2 else f * 4)^2");
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(chosen))
          == "(if `f` > 473/10 MPa then `f` * 2 else `f` * 4)^2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(chosen))
          == "(\\begin{cases} f \\cdot 2 & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 4 & \\text{otherwise} "
             "\\end{cases})^{2}");
}

TEST_CASE("render: a conditional inside a product keeps its bracket", "[render][conditional]")
{
    // The exact scenario named in the task: when(p, a, b) * 2 must not read
    // as when(p, a, b * 2), which is a different formula.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render(chosen * rat(2)) == "(if f > 473/10 MPa then f * 2 else f * 4) * 2");
    CHECK(formula::render<Dialect::Markdown>(chosen * rat(2)) == "(if `f` > 473/10 MPa then `f` * 2 else `f` * 4) * 2");
    CHECK(formula::render<Dialect::LaTeX>(chosen * rat(2))
          == "(\\begin{cases} f \\cdot 2 & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 4 & \\text{otherwise} "
             "\\end{cases}) "
             "\\cdot 2");
}

// ------------------------------------------------- phase 8: numeric_value_of

TEST_CASE("render: a numeric-value escape hatch renders as numeric(..., in unit)", "[render][escape]")
{
    // The unit is a comma-separated second argument, operand first -- see the
    // comment on NumericValueNode's render_node for why: a trailing suffix
    // with nothing between it and the operand (numeric(... in MPa), fixed in
    // review round 1) let it misattach to a WhenNode operand's else branch,
    // and a `[...]` prefix right against the operand's own parentheses
    // (numeric[in MPa](...), the round-1 fix itself) read as a CommonMark
    // link in Markdown, fixed in review round 3.
    constexpr auto numeric =
        formula::numeric_value_of<formula::unit::Megapascal, "empirical fit is only valid stated in MPa">(var<Strength>);

    CHECK(formula::render<Dialect::Plain>(numeric) == "numeric(f, in MPa)");
    CHECK(formula::render<Dialect::Markdown>(numeric) == "numeric(`f`, in MPa)");
    CHECK(formula::render<Dialect::LaTeX>(numeric) == "\\{f/\\mathrm{MPa}\\}");
    // The justification string does not appear above -- see the comment on
    // NumericValueNode's render_node for why: it is an audit trail for the
    // TRACE, not part of the formula's stated arithmetic. It is not in
    // document() either -- Documentation has no field for it and collect()
    // records none; render_trace writes it as a bracketed clause on the step
    // (test/trace_render_tests.cpp), and that is the only surface it has.
}

TEST_CASE("render: a numeric-value escape hatch inside a power and inside a product keeps no extra bracket",
          "[render][escape]")
{
    constexpr auto numeric =
        formula::numeric_value_of<formula::unit::Megapascal, "empirical fit is only valid stated in MPa">(var<Strength>);

    CHECK(formula::render(formula::pow<2>(numeric)) == "numeric(f, in MPa)^2");
    CHECK(formula::render(numeric * rat(2)) == "numeric(f, in MPa) * 2");
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(numeric)) == "numeric(`f`, in MPa)^2");
    CHECK(formula::render<Dialect::Markdown>(numeric * rat(2)) == "numeric(`f`, in MPa) * 2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(numeric)) == "\\{f/\\mathrm{MPa}\\}^{2}");
    CHECK(formula::render<Dialect::LaTeX>(numeric * rat(2)) == "\\{f/\\mathrm{MPa}\\} \\cdot 2");
}

// ----------------------------- phase 8 fix round 1: nesting a new kind
// inside another new kind, in every dialect. This is the exact axis review
// round 1 found untested -- and where the trailing-suffix bug (findings 1-2
// of that review) was hiding.

TEST_CASE("render: a rounding node wrapping a conditional keeps the granularity from misattaching to a branch",
          "[render][rounding][conditional]")
{
    // Before review round 1's fix, this rendered in Plain as "round(if f > 473/10
    // MPa then d * 2 else d * 3 to 1 dp of mm)" -- a reader parses "d * 3 to
    // 1 dp of mm" as one phrase, rounding the else branch alone. The
    // granularity is now a comma-separated second argument, so nothing can
    // trail into the operand from a branch with no closing delimiter of its
    // own to misattach.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosenLength = formula::when(overThreshold, var<Diameter> * rat(2), var<Diameter> * rat(3));
    constexpr auto rounded =
        formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            chosenLength);

    CHECK(formula::render<Dialect::Plain>(rounded) == "round(if f > 473/10 MPa then d * 2 else d * 3, to 1 dp of mm)");
    CHECK(formula::render<Dialect::Markdown>(rounded)
          == "round(if `f` > 473/10 MPa then `d` * 2 else `d` * 3, to 1 dp of mm)");
    CHECK(formula::render<Dialect::LaTeX>(rounded)
          == "\\operatorname{round}_{1\\,\\mathrm{mm}}(\\begin{cases} d \\cdot 2 & \\text{if } f > 473/10\\,\\mathrm{MPa} "
             "\\\\ "
             "d \\cdot 3 & "
             "\\text{otherwise} \\end{cases})");
}

TEST_CASE("render: a numeric-value escape hatch wrapping a conditional keeps the unit from misattaching to a branch",
          "[render][escape][conditional]")
{
    // The exact shape of must-fix finding 2 in review round 1: before the
    // fix, this rendered in Plain as "numeric(if f > 473/10 MPa then f * 2 else f
    // * 4 in MPa)", reading as if "in MPa" (and therefore the whole escape
    // hatch) applied to the else branch alone.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength> * rat(2), var<Strength> * rat(4));
    constexpr auto numeric = formula::numeric_value_of<formula::unit::Megapascal, "nested-conditional coverage">(chosen);

    CHECK(formula::render<Dialect::Plain>(numeric) == "numeric(if f > 473/10 MPa then f * 2 else f * 4, in MPa)");
    CHECK(formula::render<Dialect::Markdown>(numeric) == "numeric(if `f` > 473/10 MPa then `f` * 2 else `f` * 4, in MPa)");
    CHECK(formula::render<Dialect::LaTeX>(numeric)
          == "\\{\\begin{cases} f \\cdot 2 & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 4 & \\text{otherwise} "
             "\\end{cases}/\\mathrm{MPa}\\}");
}

TEST_CASE("render: a numeric-value escape hatch wrapping a rounding node needs no extra bracket",
          "[render][escape][rounding]")
{
    constexpr auto rounded =
        formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    constexpr auto numeric = formula::numeric_value_of<formula::unit::Millimetre, "nested-rounding coverage">(rounded);

    CHECK(formula::render<Dialect::Plain>(numeric) == "numeric(round(d, to 1 dp of mm), in mm)");
    CHECK(formula::render<Dialect::Markdown>(numeric) == "numeric(round(`d`, to 1 dp of mm), in mm)");
    CHECK(formula::render<Dialect::LaTeX>(numeric) == "\\{\\operatorname{round}_{1\\,\\mathrm{mm}}(d)/\\mathrm{mm}\\}");
}

TEST_CASE("render: a predicate comparing two rounded operands needs no extra bracket", "[render][predicate][rounding]")
{
    constexpr auto rounded =
        formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    constexpr auto guarded = rounded > formula::constant<formula::unit::Millimetre>(rat(537, 100));

    CHECK(formula::render<Dialect::Plain>(guarded) == "round(d, to 1 dp of mm) > 537/100 mm");
    CHECK(formula::render<Dialect::Markdown>(guarded) == "round(`d`, to 1 dp of mm) > 537/100 mm");
    CHECK(formula::render<Dialect::LaTeX>(guarded)
          == "\\operatorname{round}_{1\\,\\mathrm{mm}}(d) > 537/100\\,\\mathrm{mm}");
}

TEST_CASE("render: a conditional nested inside another conditional's branches is unambiguous without brackets",
          "[render][conditional]")
{
    // Every when() renders with a mandatory else, unlike an "if" whose else
    // is optional -- so nested if-then-else has no dangling-else problem:
    // nearest-else-binds-nearest-if recovers the tree correctly at any
    // nesting depth, in every dialect. That is true regardless of the
    // bracket asserted below.
    //
    // The bracket itself (Plain/Markdown, then-position only) is for the
    // reader, not the parser -- see the comment on WhenNode's render_node.
    // Unambiguous under nearest-else-binds-nearest-if does not mean a person
    // does not have to count "else"s against "then"s to find where a nested
    // conditional in the *then* position stops; the *else* position needs no
    // such help, because "if a then x else if b then y else z" is the
    // ordinary else-if chain and already reads fine unbracketed.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto inner = formula::when(overThreshold, var<Strength>, var<Strength> * rat(3));

    constexpr auto nestedInThen = formula::when(overThreshold, inner, var<Strength> * rat(5));
    constexpr auto nestedInElse = formula::when(overThreshold, var<Strength> * rat(5), inner);
    constexpr auto nestedInBoth = formula::when(overThreshold, inner, inner);

    CHECK(formula::render<Dialect::Plain>(nestedInThen)
          == "if f > 473/10 MPa then (if f > 473/10 MPa then f else f * 3) else f * 5");
    CHECK(formula::render<Dialect::Plain>(nestedInElse)
          == "if f > 473/10 MPa then f * 5 else if f > 473/10 MPa then f else f * 3");
    CHECK(formula::render<Dialect::Plain>(nestedInBoth)
          == "if f > 473/10 MPa then (if f > 473/10 MPa then f else f * 3) else if f > 473/10 MPa then f else f * 3");

    CHECK(formula::render<Dialect::Markdown>(nestedInThen)
          == "if `f` > 473/10 MPa then (if `f` > 473/10 MPa then `f` else `f` * 3) else `f` * 5");
    CHECK(formula::render<Dialect::Markdown>(nestedInElse)
          == "if `f` > 473/10 MPa then `f` * 5 else if `f` > 473/10 MPa then `f` else `f` * 3");
    CHECK(formula::render<Dialect::Markdown>(nestedInBoth)
          == "if `f` > 473/10 MPa then (if `f` > 473/10 MPa then `f` else `f` * 3) else if `f` > 473/10 MPa then `f` else "
             "`f` * 3");

    CHECK(formula::render<Dialect::LaTeX>(nestedInThen)
          == "\\begin{cases} \\begin{cases} f & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 3 & \\text{otherwise} "
             "\\end{cases} & "
             "\\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 5 & \\text{otherwise} \\end{cases}");
    CHECK(formula::render<Dialect::LaTeX>(nestedInElse)
          == "\\begin{cases} f \\cdot 5 & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ \\begin{cases} f & \\text{if } f > "
             "473/10\\,\\mathrm{MPa} \\\\ f "
             "\\cdot 3 & \\text{otherwise} \\end{cases} & \\text{otherwise} \\end{cases}");
    CHECK(formula::render<Dialect::LaTeX>(nestedInBoth)
          == "\\begin{cases} \\begin{cases} f & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f \\cdot 3 & \\text{otherwise} "
             "\\end{cases} & "
             "\\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ \\begin{cases} f & \\text{if } f > 473/10\\,\\mathrm{MPa} \\\\ f "
             "\\cdot 3 "
             "& \\text{otherwise} "
             "\\end{cases} & \\text{otherwise} \\end{cases}");
}

// --------------------------------------------------- phase 9: constraints

TEST_CASE("render: a constraint renders as its rule, never its verdict", "[render][constraint]")
{
    constexpr formula::Verdict rejectSpecimen { .label = "reject the specimen" };
    constexpr auto rule = formula::constraint(var<WaterVolume> <= var<CementVolume>, rejectSpecimen);

    CHECK(formula::render<Dialect::Plain>(rule) == "require V_w <= V_c");
    CHECK(formula::render<Dialect::Markdown>(rule) == "require `V_w` <= `V_c`");
    CHECK(formula::render<Dialect::LaTeX>(rule) == "\\text{require } V_w \\leq V_c");
    // The default dialect for a Constraint is plain, exactly as for a Node
    // and for a Predicate.
    CHECK(formula::render(rule) == "require V_w <= V_c");

    // The verdict's own label appears nowhere above, in any dialect -- see
    // render_node(Constraint...)'s comment for why that is the decision,
    // not an oversight: it is what checking concludes, not part of the rule
    // a standard asks a reader to check.
    std::string const label { rejectSpecimen.label };
    CHECK(formula::render<Dialect::Plain>(rule).find(label) == std::string::npos);
    CHECK(formula::render<Dialect::Markdown>(rule).find(label) == std::string::npos);
    CHECK(formula::render<Dialect::LaTeX>(rule).find(label) == std::string::npos);
}

TEST_CASE("render: a constraint keeps the predicate's own side order, tested in both arrangements",
          "[render][constraint]")
{
    // The Global Constraint about testing both sides of a two-sided
    // operation: a predicate has two sides, so build the same two variables
    // into it both ways and confirm the rendered order actually follows the
    // tree, rather than a bug that always printed (say) the alphabetically
    // first symbol regardless of which side it was declared on.
    constexpr formula::Verdict rejectSpecimen { .label = "reject the specimen" };
    constexpr auto waterFirst = formula::constraint(var<WaterVolume> <= var<CementVolume>, rejectSpecimen);
    constexpr auto cementFirst = formula::constraint(var<CementVolume> <= var<WaterVolume>, rejectSpecimen);

    CHECK(formula::render(waterFirst) == "require V_w <= V_c");
    CHECK(formula::render(cementFirst) == "require V_c <= V_w");
}

TEST_CASE("render: a constraint's predicate brackets a nested conditional exactly as it would bare",
          "[render][constraint]")
{
    // Constraint's render_node forwards to the same render<D>(predicate)
    // PredicateNode itself uses, so a WhenNode on either side still brackets
    // for the identical reason it does for a bare predicate (see "render: a
    // conditional nested as a predicate's operand keeps its bracket" above)
    // -- this is not re-derived, only confirmed still true through the new
    // entry point.
    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * rat(2));
    constexpr auto guarded = chosen > formula::constant<formula::unit::Megapascal>(rat(137, 10));
    constexpr formula::Verdict rejectSpecimen { .label = "reject the specimen" };
    constexpr auto rule = formula::constraint(guarded, rejectSpecimen);

    CHECK(formula::render(rule) == "require (if f > 473/10 MPa then f else f * 2) > 137/10 MPa");
}

// ------------------------------------------------------- phase 10: lookups

namespace
{
using formula::band;
using formula::BandTable;
using formula::banded_lookup;
using formula::breakpoint;
using formula::BreakpointTable;
using formula::exact_lookup;
using formula::interpolating_lookup;
using formula::KeyTable;
namespace unit = formula::unit;

/// Three bands whose every axis is deliberately non-degenerate, because a
/// mutation survives whenever *any* axis of a fixture is degenerate and not
/// only the one that caught the last defect:
///
///  - the widths are 33/50, 174/25 and 2097/100 -- unequal, so a renderer
///    computing a band's extent from the first pair rather than per band is
///    visible;
///  - no bound equals its own row's index, so a bound cannot be confused with
///    an index;
///  - no bound repeats across rows other than where two bands genuinely share
///    a boundary, and the shared boundaries (277/100 and 973/100) differ from each other,
///    so "always print the first band's bounds" is visible on every row;
///  - the corrections are 863/1000, 1381/1000 and 1043/1000: three distinct values, none of
///    them equal to any index, any bound or any other correction;
///  - **no bound has denominator 1.** A first revision of this fixture
///    declared every bound as a whole number, and with that fixture
///    `declared_number_text` could be cut down to `return
///    std::to_string(numerator);` -- throwing the denominator away outright --
///    and the entire suite still passed. A table states a bound like 2.77 mm
///    as readily as a whole number, so the fixture now does too.
///  - and one bound is declared **unreduced** (`554/200`), because reducing
///    through `Rational::make` is a decision this renderer makes and a bound
///    already in lowest terms cannot tell whether it was made.
inline constexpr BandTable<3> SizeBands {
    band(211, 100, 554, 200), // 211/100 to under 277/100 mm -- 554/200 declared, so reduction shows
    band(277, 100, 973, 100), // 277/100 to under 973/100 mm
    band(973, 100, 307, 10),  // 973/100 to under 307/10 mm
};

/// The underlying type is fixed and the enumerators are numbered by hand, both
/// deliberately. A key renders as its name, and as its underlying value only
/// when it names no row -- `Beam`, which the table below leaves out, is that
/// case. Numbered by hand because a renderer printing a row's *index* rather
/// than its key's value there is invisible against an enumeration left to
/// default, where the two coincide; and declared here **out of numeric order**
/// (3, 7, 5) so that a renderer sorting the rows, or reading them off the
/// enumeration rather than off the table, is visible too. An exact table has no
/// order, so an out-of-order table is not malformed -- it is just a table.
///
/// **Named for this file rather than generically, and that is a rule not a
/// preference.** Two translation units whose anonymous-namespace key
/// enumerations share a name and whose tables share their element values fail
/// to link under clang, with a dangling relocation and no diagnostic naming the
/// key type. This file spelled its `SpecimenShape` and got away with it only
/// because its values happened to differ from another file's. See
/// `trace_render_tests.cpp`'s `RenderedShape` for the measured mechanism.
enum class MouldShape : std::uint8_t
{
    Cube = 3,
    Prism = 5,
    Cylinder = 7,
    Beam = 11,
};

inline constexpr KeyTable<MouldShape, 3> ShapeKeys {
    MouldShape::Cube,     // key 3
    MouldShape::Cylinder, // key 7
    MouldShape::Prism,    // key 5
};

/// Three breakpoints, spaced unequally (249/50 then 1193/100), with no key equal to
/// its own index, with the middle key **not** a whole number and declared
/// **unreduced** -- every reason `SizeBands` gives, and the last two for the
/// same measured reason: a curve whose every key was an integer could not tell
/// a renderer that keeps the denominator from one that discards it.
inline constexpr BreakpointTable<3> CurvePoints {
    breakpoint(239, 100),
    breakpoint(1474, 200), // 737/100 -- declared unreduced, and in the middle
    breakpoint(193, 10),
};

inline constexpr BandTable<0> NoBands {};
inline constexpr KeyTable<MouldShape, 0> NoShapes {};
inline constexpr BreakpointTable<0> NoPoints {};

/// The other degenerate shape: a table whose only row is simultaneously its
/// first and its last, and which needs no separator between rows at all.
inline constexpr BandTable<1> OneBand { band(211, 100, 554, 200) };
inline constexpr KeyTable<MouldShape, 1> OneShape { MouldShape::Cylinder };
inline constexpr BreakpointTable<1> OnePoint { breakpoint(1474, 200) };

/// Key unit `mm` (a symbol), result unit `One` (no symbol) -- so this fixture
/// exercises the unit-bearing side of a row and the bare-number side at once.
[[nodiscard]] constexpr auto bandedLookup()
{
    return banded_lookup<unit::Millimetre, SizeBands, unit::One>(var<Diameter>,
                                                                 { rat(863, 1000), rat(1381, 1000), rat(1043, 1000) });
}

/// No key unit at all (an exact lookup has none) and a result unit that does
/// have a symbol, so the two fixtures above and below cover both sides.
/// `Cylinder` is the **middle** row, the position a defect is hardest to see
/// from either end.
[[nodiscard]] constexpr auto shapeLookup(MouldShape shape = MouldShape::Cylinder)
{
    // The middle correction is a whole number, so that `number_text`'s
    // whole-number branch is reached through a lookup and not only through a
    // constant -- and 43 is not the middle row's index, its key, or any bound.
    return exact_lookup<ShapeKeys, unit::Megapascal>(shape, { rat(2791, 1000), rat(43), rat(1373, 1000) });
}

/// Both units bear a symbol here, which neither fixture above does.
[[nodiscard]] constexpr auto curveLookup()
{
    // The middle value is negative -- a correction a published curve may well
    // subtract -- so a renderer that took a magnitude anywhere is visible, and
    // `number_text`'s sign handling is reached through a lookup.
    return interpolating_lookup<unit::Millimetre, CurvePoints, unit::Megapascal>(
        var<Diameter>, { rat(873, 1000), rat(-1139, 1000), rat(1217, 1000) });
}

/// The fields of a rendered call's argument list, split on @p separator.
///
/// **Located by position and by nothing else.** The cross-dialect test below
/// exists to establish that two dialects name a band the *same way*, and a
/// test that found the band by searching for the band's own text would pass
/// whatever the two dialects said -- it would be comparing each of them to a
/// literal in the test, not to each other.
///
/// The separator is the caller's to state because it is a property of the
/// dialect, not of the field: LaTeX writes `,\allowbreak ` where the other two
/// write `, ` (see `detail::lookup_separator`). Passing it in keeps the split
/// structural -- "the third field", whatever a field happens to say -- rather
/// than letting one dialect's punctuation leak into a shared helper.
///
/// Only valid for a rendering whose own operand contains neither the separator
/// nor brackets of its own, which is why every caller below uses a bare
/// variable as the operand.
[[nodiscard]] std::vector<std::string> callFields(std::string const& text, std::string const& separator)
{
    std::size_t const open = text.find('(');
    std::size_t const close = text.rfind(')');
    REQUIRE(open != std::string::npos);
    REQUIRE(close != std::string::npos);
    REQUIRE(open < close);

    std::string const inside = text.substr(open + 1, close - open - 1);
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (std::size_t at = inside.find(separator); at != std::string::npos; at = inside.find(separator, start))
    {
        fields.push_back(inside.substr(start, at - start));
        start = at + separator.size();
    }
    fields.push_back(inside.substr(start));
    return fields;
}
} // namespace

TEST_CASE("render: all three lookup kinds are Nodes, so they need no entry point of their own", "[render][lookup]")
{
    // `render<D>(N const&)` is constrained on `Node`, and `Node` is exactly
    // `std::derived_from<..., NodeBase>` (expression.hpp) -- which all three
    // lookup nodes are. A `Constraint` needed its own overload because it is
    // not a `Node`; these do not, and this is the assertion rather than the
    // assumption the brief asked for.
    STATIC_REQUIRE(formula::Node<decltype(bandedLookup())>);
    STATIC_REQUIRE(formula::Node<decltype(shapeLookup())>);
    STATIC_REQUIRE(formula::Node<decltype(curveLookup())>);
}

TEST_CASE("render: a banded lookup renders its operand and one field per band", "[render][lookup]")
{
    CHECK(formula::render<Dialect::Plain>(bandedLookup())
          == "lookup(d, 211/100 to under 277/100 mm gives 863/1000, 277/100 to under 973/100 mm gives 1381/1000, 973/100 to "
             "under 307/10 mm gives 1043/1000)");
    // Markdown differs from plain in exactly one thing -- the backticks the
    // variable already had. The bands are words and numbers, so nothing in
    // them is Markdown's business.
    CHECK(formula::render<Dialect::Markdown>(bandedLookup())
          == "lookup(`d`, 211/100 to under 277/100 mm gives 863/1000, 277/100 to under 973/100 mm gives 1381/1000, 973/100 "
             "to under 307/10 mm gives 1043/1000)");
    CHECK(formula::render<Dialect::LaTeX>(bandedLookup())
          == "\\operatorname{lookup}(d,\\allowbreak \\mathrm{211/100\\ to\\ under\\ 277/100\\ mm\\ gives\\ 863/1000},"
             "\\allowbreak \\mathrm{277/100\\ to\\ under\\ 973/100\\ mm\\ gives\\ 1381/1000},\\allowbreak "
             "\\mathrm{973/100\\ to\\ under\\ 307/10\\ mm\\ gives\\ 1043/1000})");
    // The default dialect is plain, exactly as for every other node kind.
    CHECK(formula::render(bandedLookup())
          == "lookup(d, 211/100 to under 277/100 mm gives 863/1000, 277/100 to under 973/100 mm gives 1381/1000, 973/100 to "
             "under 307/10 mm gives 1043/1000)");
}

TEST_CASE("render: an exact lookup renders the key it selects with and one field per row", "[render][lookup]")
{
    CHECK(formula::render<Dialect::Plain>(shapeLookup())
          == "lookup(key Cylinder, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
    // Nothing here is a variable, so Markdown has nothing to backtick and the
    // two dialects coincide. That is a fact about this node kind, not an
    // accident: an exact lookup has no operand.
    CHECK(formula::render<Dialect::Markdown>(shapeLookup())
          == "lookup(key Cylinder, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
    // `key Cylinder` is words, not mathematics, so LaTeX sets the subject
    // upright as the rows are -- unlike the other two kinds, whose subject is
    // a real sub-expression.
    CHECK(formula::render<Dialect::LaTeX>(shapeLookup())
          == "\\operatorname{lookup}(\\mathrm{key\\ Cylinder},\\allowbreak \\mathrm{key\\ Cube\\ gives\\ 2791/1000\\ MPa},"
             "\\allowbreak \\mathrm{key\\ Cylinder\\ gives\\ 43\\ MPa},\\allowbreak \\mathrm{key\\ Prism\\ gives\\ "
             "1373/1000\\ MPa})");
    CHECK(formula::render(shapeLookup())
          == "lookup(key Cylinder, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
}

TEST_CASE("render: an exact lookup's subject is the key it holds, not a row of its table", "[render][lookup]")
{
    // The table's rows never move; only the subject does. A renderer that read
    // the subject off `Keys[0]`, or off the middle row -- which is what the
    // fixture's own default selects, so that mistake would pass the test above
    // -- is caught here and nowhere else.
    CHECK(formula::render(shapeLookup(MouldShape::Cube))
          == "lookup(key Cube, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
    CHECK(formula::render(shapeLookup(MouldShape::Prism))
          == "lookup(key Prism, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
}

TEST_CASE("render: an interpolating lookup renders its operand and one field per breakpoint", "[render][lookup]")
{
    CHECK(formula::render<Dialect::Plain>(curveLookup())
          == "interpolate(d, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)");
    CHECK(formula::render<Dialect::Markdown>(curveLookup())
          == "interpolate(`d`, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)");
    CHECK(formula::render<Dialect::LaTeX>(curveLookup())
          == "\\operatorname{interpolate}(d,\\allowbreak \\mathrm{at\\ 239/100\\ mm\\ gives\\ 873/1000\\ MPa},"
             "\\allowbreak \\mathrm{at\\ 737/100\\ mm\\ gives\\ -1139/1000\\ MPa},\\allowbreak "
             "\\mathrm{at\\ 193/10\\ mm\\ gives\\ 1217/1000\\ MPa})");
    CHECK(formula::render(curveLookup())
          == "interpolate(d, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)");
}

TEST_CASE("render: a band's excluded top and a breakpoint's included one are spelled differently", "[render][lookup]")
{
    // `lookup.hpp` pins the two *behaviours* against each other on one shared
    // number: a band's high bound is exclusive, so that number falls off the
    // top of a table ending there, while a breakpoint IS a row, so it hits it
    // exactly. This pins the two *spellings* against each other on one shared
    // number -- 293/10 mm here -- so that
    // harmonising them in either direction fails here rather than in a
    // consumer reading a published page.
    //
    // Both tables are built over the same bounds so that nothing but the
    // spelling can differ.
    static constexpr BandTable<2> topBands { band(973, 100, 209, 10), band(209, 10, 293, 10) };
    static constexpr BreakpointTable<2> topPoints { breakpoint(209, 10), breakpoint(293, 10) };

    std::string const banded =
        formula::render(banded_lookup<unit::Millimetre, topBands, unit::One>(var<Diameter>,
                                                                             { rat(1127, 1000), rat(863, 1000) }));
    std::string const curve = formula::render(interpolating_lookup<unit::Millimetre, topPoints, unit::One>(
        var<Diameter>, { rat(1127, 1000), rat(863, 1000) }));

    // The band says, in words, that 293/10 is not in it.
    CHECK(banded.find("209/10 to under 293/10 mm") != std::string::npos);
    // The breakpoint says the table states a value AT 293/10 -- a point, not
    // an interval, so there is nothing for it to exclude.
    CHECK(curve.find("at 293/10 mm") != std::string::npos);

    // And neither borrows the other's spelling. A curve that excluded anything
    // would be claiming its own last row is unreachable; a band rendered as a
    // point would drop the exclusion the whole table is built on.
    CHECK(curve.find("under") == std::string::npos);
    CHECK(banded.find("at 293/10 mm") == std::string::npos);
}

TEST_CASE("render: a declared bound keeps its denominator and is reduced, in every kind", "[render][lookup]")
{
    // A first revision of these fixtures declared every bound and every
    // breakpoint as a whole number. With that, `detail::declared_number_text`
    // could be cut down to `return std::to_string(numerator);` -- discarding
    // the denominator outright -- and the whole suite still passed. The code
    // was right; the fixture was degenerate on an axis nobody had looked at.
    //
    // So: a bound that is not whole (277/100), one declared unreduced (554/200,
    // which must come back as 277/100 and not as 554/200), and the same on the
    // breakpoint side (1474/200 -> 737/100). Asserted here as its own case as well as inside the
    // full-text cases above, because a reader of a failure should be told
    // which property broke.
    std::string const banded = formula::render(bandedLookup());
    CHECK(banded.find("211/100 to under 277/100 mm") != std::string::npos);
    CHECK(banded.find("277/100 to under 973/100 mm") != std::string::npos);
    CHECK(banded.find("554/200") == std::string::npos);

    std::string const curve = formula::render(curveLookup());
    CHECK(curve.find("at 737/100 mm") != std::string::npos);
    CHECK(curve.find("1474/200") == std::string::npos);

    // And the two branches of `number_text` a lookup can reach: a whole-number
    // correction and a negative one, neither of which the first revision of
    // these fixtures had either.
    CHECK(formula::render(shapeLookup()).find("gives 43 MPa") != std::string::npos);
    CHECK(curve.find("gives -1139/1000 MPa") != std::string::npos);
}

TEST_CASE("render: every lookup kind nests inside a product and a power without a bracket of its own",
          "[render][lookup]")
{
    // Each kind emits its own parentheses, which group whatever it holds --
    // so `Atom` (the PrecedenceOf primary template) is already the right
    // answer and no override is needed, exactly as for `round`. What that
    // claim is worth is what these cases measure.
    CHECK(formula::render(var<Strength> * bandedLookup())
          == "f * lookup(d, 211/100 to under 277/100 mm gives 863/1000, 277/100 to under 973/100 mm gives 1381/1000, "
             "973/100 to under 307/10 mm gives 1043/1000)");
    CHECK(formula::render(formula::pow<2>(bandedLookup()))
          == "lookup(d, 211/100 to under 277/100 mm gives 863/1000, 277/100 to under 973/100 mm gives 1381/1000, 973/100 to "
             "under 307/10 mm gives 1043/1000)^2");

    CHECK(formula::render(var<Strength> * shapeLookup())
          == "f * lookup(key Cylinder, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 "
             "MPa)");
    CHECK(formula::render(formula::pow<2>(shapeLookup()))
          == "lookup(key Cylinder, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 "
             "MPa)^2");

    CHECK(formula::render(var<Strength> * curveLookup())
          == "f * interpolate(d, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)");
    CHECK(formula::render(formula::pow<2>(curveLookup()))
          == "interpolate(d, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)^2");

    // Markdown and LaTeX too, since the bracketing decision is dialect-wide
    // and a power is the one context where a two-token atom went wrong before
    // (see `precedence_of(ConstantNode)`).
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(curveLookup()))
          == "interpolate(`d`, at 239/100 mm gives 873/1000 MPa, at 737/100 mm gives -1139/1000 MPa, at 193/10 mm gives "
             "1217/1000 MPa)^2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(shapeLookup()))
          == "\\operatorname{lookup}(\\mathrm{key\\ Cylinder},\\allowbreak \\mathrm{key\\ Cube\\ gives\\ 2791/1000\\ MPa},"
             "\\allowbreak \\mathrm{key\\ Cylinder\\ gives\\ 43\\ MPa},\\allowbreak \\mathrm{key\\ Prism\\ gives\\ "
             "1373/1000\\ MPa})^{2}");
}

TEST_CASE("render: a table with no rows says so, and a table with one row renders it", "[render][lookup]")
{
    // All three empty tables are valid and all three always miss (`band.hpp`,
    // `lookup.hpp`). `lookup(d)` would show a reader a complete-looking call
    // with the whole table silently absent, which is the same class of lie as
    // the operand a published page dropped in phase 8.
    CHECK(formula::render(banded_lookup<unit::Millimetre, NoBands, unit::One>(var<Diameter>, {}))
          == "lookup(d, no rows)");
    CHECK(formula::render(exact_lookup<NoShapes, unit::One>(MouldShape::Beam, {}))
          == "lookup(key 11, no rows)");
    CHECK(formula::render(interpolating_lookup<unit::Millimetre, NoPoints, unit::One>(var<Diameter>, {}))
          == "interpolate(d, no rows)");

    CHECK(formula::render<Dialect::LaTeX>(banded_lookup<unit::Millimetre, NoBands, unit::One>(var<Diameter>, {}))
          == "\\operatorname{lookup}(d,\\allowbreak \\mathrm{no\\ rows})");

    // One row is the other degenerate shape: the only row is simultaneously
    // the first and the last, and there is no separator between rows for a
    // renderer to get wrong -- so a table of one is the case that tells a
    // "say so when empty" branch from a "say so when fewer than two" one.
    CHECK(formula::render(banded_lookup<unit::Millimetre, OneBand, unit::One>(var<Diameter>, { rat(863, 1000) }))
          == "lookup(d, 211/100 to under 277/100 mm gives 863/1000)");
    CHECK(formula::render(exact_lookup<OneShape, unit::One>(MouldShape::Cylinder, { rat(863, 1000) }))
          == "lookup(key Cylinder, key Cylinder gives 863/1000)");
    CHECK(formula::render(interpolating_lookup<unit::Millimetre, OnePoint, unit::One>(var<Diameter>, { rat(863, 1000) }))
          == "interpolate(d, at 737/100 mm gives 863/1000)");
}

TEST_CASE("render: a lookup states the expression, never that the expression found something", "[render][lookup]")
{
    // `Beam` is a perfectly good `MouldShape` that this table has no row
    // for: evaluating it is `ArithmeticError::DomainError`. The rendered text
    // is unchanged by that, and says nothing that implies a value was found --
    // it shows the reader the key and the rows and lets them see there is no
    // match, which is exactly what a rendered formula is for.
    //
    // `key 11`, not `key Beam`: a key is named only among the keys its own
    // table declares (`detail::key_name`), and this one is not among them.
    // The value is the fallback, and 11 is neither Beam's index in its
    // enumeration nor any row's index. Kills a renderer that names the key
    // from anywhere but the table, and one that falls back to an index.
    CHECK(formula::render(shapeLookup(MouldShape::Beam))
          == "lookup(key 11, key Cube gives 2791/1000 MPa, key Cylinder gives 43 MPa, key Prism gives 1373/1000 MPa)");
}

TEST_CASE("render: a row declared under a value that names no enumerator shows that value", "[render][lookup]")
{
    // `static_cast<MouldShape>(9)` is a legal key -- a `KeyTable` holds values
    // of the enumeration, not only its enumerators -- and it has no name. Its
    // row and a hit on it both show the value, while the named row beside it
    // keeps its name. Kills an implementation that prints an empty name
    // (`key `) or a fragment of the compiler's cast spelling (`key 9` would
    // survive that, `key 0x9` or `key true` would not).
    static constexpr KeyTable<MouldShape, 2> unnamedRow { MouldShape::Cube, static_cast<MouldShape>(9) };
    CHECK(formula::render(
              exact_lookup<unnamedRow, unit::One>(static_cast<MouldShape>(9), { rat(1127, 1000), rat(863, 1000) }))
          == "lookup(key 9, key Cube gives 1127/1000, key 9 gives 863/1000)");
}

namespace
{
/// A key enumeration with one enumerator spelled by its author and one left
/// to its own name -- and that own name has an underscore, which LaTeX will
/// not accept bare even inside `\text{...}`. The author's spelling holds a
/// bracket pair, an asterisk pair and a percent sign, every one of which
/// means something to Markdown or to LaTeX.
enum class MouldFinish : std::uint8_t
{
    Hollow_Core = 2,
    Polished = 4,
};

inline constexpr KeyTable<MouldFinish, 2> FinishKeys { MouldFinish::Polished, MouldFinish::Hollow_Core };

[[nodiscard]] constexpr auto finishLookup(MouldFinish finish)
{
    return exact_lookup<FinishKeys, unit::One>(finish, { rat(1127, 1000), rat(863, 1000) });
}
} // namespace

template <>
struct formula::EnumeratorName<MouldFinish>
{
    static constexpr std::string_view of(MouldFinish finish) noexcept
    {
        return finish == MouldFinish::Polished ? "polished *A* 100% & oiled" : "";
    }
};

TEST_CASE("render: an exact lookup shows the author's own spelling of a key when there is one", "[render][lookup]")
{
    // The customized row and the subject both take the author's spelling,
    // and the uncustomized row still takes its reflected name. Kills a
    // renderer that ignores `EnumeratorName`, and one that applies it to the
    // subject but not to the rows or the other way round.
    CHECK(formula::render<Dialect::Plain>(finishLookup(MouldFinish::Polished))
          == "lookup(key polished *A* 100% & oiled, key polished *A* 100% & oiled gives 1127/1000, key Hollow_Core gives "
             "863/1000)");
}

TEST_CASE("render: a key's name is shown literally in every dialect, whatever characters it holds",
          "[render][lookup][markdown][latex]")
{
    // The one piece of text in a rendering this library did not write. In
    // Markdown, `*A*` unescaped is emphasis, `_` can open
    // emphasis and `&` can start an entity; in LaTeX, `_` and `&` are errors
    // and `%` silently eats the rest of the line. So each dialect escapes the
    // name: Markdown the way an author writing in it by hand would, LaTeX
    // inside the row's `\mathrm{...}`, where a space is `\ ` as well.
    // Kills a renderer that puts the name in unescaped, and one that escapes
    // for the wrong dialect.
    //
    // Both measured rather than reasoned: the LaTeX string typesets with
    // tectonic 0.17.0 under `\usepackage[OT1]{fontenc}` and with MathJax
    // 3.2.2 under the site's configuration, and shows the name as written in
    // both; the Markdown string does the same through python-markdown and
    // pandoc's CommonMark and GFM readers.
    CHECK(formula::render<Dialect::Markdown>(finishLookup(MouldFinish::Hollow_Core))
          == "lookup(key Hollow\\_Core, key polished \\*A\\* 100% &amp; oiled gives 1127/1000, key Hollow\\_Core gives "
             "863/1000)");
    CHECK(formula::render<Dialect::LaTeX>(finishLookup(MouldFinish::Hollow_Core))
          == "\\operatorname{lookup}(\\mathrm{key\\ Hollow\\_Core},\\allowbreak \\mathrm{key\\ polished\\ *A*\\ 100\\%\\ "
             "\\&\\ oiled\\ gives\\ 1127/1000},\\allowbreak \\mathrm{key\\ Hollow\\_Core\\ gives\\ 863/1000})");
    // Plain is for a terminal, where nothing is markup, so nothing is escaped.
    CHECK(formula::render<Dialect::Plain>(finishLookup(MouldFinish::Hollow_Core))
          == "lookup(key Hollow_Core, key polished *A* 100% & oiled gives 1127/1000, key Hollow_Core gives 863/1000)");
}

namespace
{
/// A key whose author's spelling holds every character either dialect
/// escapes, each once and each between two letters, so that a missing escape
/// shows as exactly one wrong character in a known place.
enum class MouldMarking : std::uint8_t
{
    Stamped = 1,
};

inline constexpr KeyTable<MouldMarking, 1> MarkingKeys { MouldMarking::Stamped };
} // namespace

template <>
struct formula::EnumeratorName<MouldMarking>
{
    static constexpr std::string_view of(MouldMarking) noexcept
    {
        // The tail is TeX's ligature pairs: `--`, `---`, `''`, `,,`.
        return "a\\b`c*d_efg<h>i&j|k~l$m^n{o}p#q%r\"s--t---u''v,,w";
    }
};

TEST_CASE("render: every character either dialect escapes in a key's name is escaped", "[render][lookup][markdown][latex]")
{
    // One fixture for every escape branch of `detail::literal_words_in_dialect`
    // (Markdown) and `detail::latex_math_words` (LaTeX): deleting any single
    // `case` from either switch fails this test, and it is the only test that
    // fails for most of them. The escaped forms were each measured -- see
    // those functions' comments.
    constexpr auto node = exact_lookup<MarkingKeys, unit::One>(MouldMarking::Stamped, { rat(1127, 1000) });

    // Markdown has no ligatures to break: the pairs pass through unchanged.
    std::string const markdown = "a\\\\b\\`c\\*d\\_efg&lt;h&gt;i&amp;j&#124;k&#126;l&#36;m^n{o}p#q%r\"s--t---u''v,,w";
    CHECK(formula::render<Dialect::Markdown>(node) == "lookup(key " + markdown + ", key " + markdown + " gives 1127/1000)");

    // Math mode has no ligatures, so the pairs need no empty group -- but
    // `'` is a prime there, so each one is set as text.
    std::string const latex = "a\\backslash{}b\\grave{}c*d\\_efg<h>i\\&j|k\\tilde{}l\\$m\\hat{}n\\{o\\}p\\#q\\%r"
                              "\\mathtt{\"}s--t---u\\text{'}\\text{'}v,,w";
    CHECK(formula::render<Dialect::LaTeX>(node)
          == "\\operatorname{lookup}(\\mathrm{key\\ " + latex + "},\\allowbreak \\mathrm{key\\ " + latex
                 + "\\ gives\\ 1127/1000})");

    std::string const plain = "a\\b`c*d_efg<h>i&j|k~l$m^n{o}p#q%r\"s--t---u''v,,w";
    CHECK(formula::render<Dialect::Plain>(node) == "lookup(key " + plain + ", key " + plain + " gives 1127/1000)");

    // A spelling can no longer hold a square bracket -- `EnumeratorName`
    // refuses one, since it could forge a trace's provenance clause -- so
    // the two bracket branches are asked of the escaping function itself.
    // Kept rather than deleted, as a second line behind that refusal: a
    // Markdown link label is the defect they prevent.
    CHECK(formula::detail::literal_words_in_dialect<Dialect::Markdown>("e[f]g") == "e\\[f\\]g");
    CHECK(formula::detail::latex_math_words("e[f]g") == "e[f]g");
}

TEST_CASE("render: a documented lookup renders as the bare lookup, like every other wrapped node", "[render][lookup]")
{
    // A citation is documentation, not arithmetic -- `document()` surfaces it.
    // Worth one case per phase that adds node kinds, because `DocumentedNode`
    // is the wrapper `documented()` puts round a table's identity, and a table
    // is the part of a method that carries a source.
    constexpr auto cited = formula::documented(bandedLookup(), { .title = "Invented Method 7, table 2" });
    CHECK(formula::render(cited) == formula::render(bandedLookup()));
    CHECK(formula::render(cited).find("Invented Method 7") == std::string::npos);
}

TEST_CASE("render: a LaTeX lookup can be broken across lines, and never breaks itself", "[render][lookup][latex]")
{
    // Each row is one atomic `\text{...}` and TeX puts no break penalty at a
    // math comma, so without `\allowbreak` a rendered lookup has NO legal
    // break point at any row count: it does not wrap, it runs off the line and
    // -- for a table of six rows -- off the paper, with the reader told
    // nothing is missing. Typeset with tectonic 0.17.0, worst overfull box
    // across 22 renderings -- these tests' 20, plus a six-row table and its
    // square -- inline in prose: 721pt before, 88pt after.
    //
    // Asserted on the emitted text rather than on a PDF, because a test that
    // needed a LaTeX toolchain could not run on this project's CI -- so this
    // pins the mechanism the measurement showed to work, and the measurement
    // itself lives in `detail::lookup_separator`'s comment.
    std::string const banded = formula::render<Dialect::LaTeX>(bandedLookup());
    std::string const exact = formula::render<Dialect::LaTeX>(shapeLookup());
    std::string const curve = formula::render<Dialect::LaTeX>(curveLookup());

    auto const breakPoints = [](std::string const& text) {
        std::size_t count = 0;
        for (std::size_t at = text.find(",\\allowbreak "); at != std::string::npos;
             at = text.find(",\\allowbreak ", at + 1))
            ++count;
        return count;
    };

    // One per field separator: three rows means three separators, the first of
    // them after the subject. A renderer that put `\allowbreak` only between
    // rows, or only once, is a different number.
    CHECK(breakPoints(banded) == 3);
    CHECK(breakPoints(exact) == 3);
    CHECK(breakPoints(curve) == 3);
    // And no bare comma is left without one.
    CHECK(banded.find(", ") == std::string::npos);

    // The other half of the display-math decision: a lookup never emits `\\`.
    // `\\` is the only thing that would break a DISPLAY, and it is a hard
    // LaTeX error in `$...$` and `\[...\]` -- so a caller who needs a wide
    // table broken in a display reaches for breqn's `dmath`, which leaves 5
    // small boxes where a plain display leaves 16, and does so *because* the
    // separator above is there: stripped out, `dmath` is no better than the
    // plain display. This library emits nothing that could break their build
    // to get there. See `detail::lookup_separator` -- and note that neither
    // that number nor any other in it is pinned here. They are properties of
    // a TeX engine, so this case pins only the emitted string; if the
    // mechanism ever stops being the right one, these assertions still pass.
    CHECK(banded.find("\\\\") == std::string::npos);
    CHECK(exact.find("\\\\") == std::string::npos);
    CHECK(curve.find("\\\\") == std::string::npos);

    // Plain and Markdown carry none of it: `\allowbreak` is a LaTeX control
    // word, and leaking one into a terminal or a web page would be the
    // dialect-blind mistake this library keeps testing for.
    CHECK(formula::render<Dialect::Plain>(bandedLookup()).find("allowbreak") == std::string::npos);
    CHECK(formula::render<Dialect::Markdown>(bandedLookup()).find("allowbreak") == std::string::npos);
}

TEST_CASE("render: the three dialects name a lookup's rows the same way, for all three kinds",
          "[render][lookup][markdown]")
{
    // THE cross-surface test. Every other case in this section asserts one
    // dialect's output against a literal, and a set of such cases cannot catch
    // two dialects drifting apart -- that is the whole lesson of phase 8,
    // where two renderers each had passing tests and each was internally
    // consistent, and a human reading a published page found the disagreement.
    //
    // So this compares the dialects **against each other**, and locates what
    // it compares by POSITION -- the field index inside the rendered call --
    // never by the text it is about to compare. A test that found the band
    // label by searching for the band label would pass whatever the two
    // dialects said.
    //
    // LaTeX's own decoration is applied to plain's field, not searched for:
    // `\mathrm{...}` round the escaped words is a dialect-wide fact about how
    // this library sets words in mathematics (the same kind of fact as
    // Markdown's backticks round a symbol), so comparing LaTeX's field to
    // plain's field *modulo that wrapper* compares the two renderers' choice
    // of words, which is the thing that can drift.
    auto const dialectsAgree = [](auto const& node, std::size_t fieldCount) {
        std::vector<std::string> const plain = callFields(formula::render<Dialect::Plain>(node), ", ");
        std::vector<std::string> const markdown = callFields(formula::render<Dialect::Markdown>(node), ", ");
        std::vector<std::string> const latex =
            callFields(formula::render<Dialect::LaTeX>(node), ",\\allowbreak ");

        REQUIRE(plain.size() == fieldCount);
        REQUIRE(markdown.size() == fieldCount);
        REQUIRE(latex.size() == fieldCount);

        // Field 0 is the subject; fields 1.. are the rows. The MIDDLE row is
        // field 2 of four, so a comparison confined to the first or the last
        // row would not be what is being made here.
        for (std::size_t field = 1; field < fieldCount; ++field)
        {
            REQUIRE(!plain[field].empty());
            CHECK(markdown[field] == plain[field]);
            CHECK(latex[field] == "\\mathrm{" + formula::detail::latex_math_words(plain[field]) + "}");
        }
    };

    dialectsAgree(bandedLookup(), 4);
    dialectsAgree(shapeLookup(), 4);
    dialectsAgree(curveLookup(), 4);
}

// --------------------------------------------- phase 8 fix round 3: guard
// against the whole class of bug review round 3 found, not just this one
// instance. `"](" `is CommonMark's inline-link syntax -- a Markdown renderer
// displays only the link's label, silently dropping whatever the destination
// held, so string equality between two Markdown-dialect strings is blind to
// this: two strings can be equal to each other and still both be wrong in
// the same way. Only checking the actual character sequence a Markdown
// parser treats specially catches it, which is what this test does instead.

namespace
{
/// True when the character at @p at is backslash-escaped: preceded by an odd
/// number of backslashes. `\[` is escaped; `\\[` is an escaped backslash
/// followed by a live `[`.
[[nodiscard]] bool isEscapedAt(std::string const& text, std::size_t at)
{
    std::size_t backslashes = 0;
    while (at > backslashes && text[at - backslashes - 1] == '\\')
        ++backslashes;
    return backslashes % 2 == 1;
}

/// Every position of @p c in @p text that is not backslash-escaped.
[[nodiscard]] std::vector<std::size_t> unescapedPositions(std::string const& text, char c)
{
    std::vector<std::size_t> positions;
    for (std::size_t at = text.find(c); at != std::string::npos; at = text.find(c, at + 1))
        if (!isEscapedAt(text, at))
            positions.push_back(at);
    return positions;
}
} // namespace

TEST_CASE("render: the Markdown guard tells an escaped character from a live one", "[render][markdown]")
{
    // The guard below forbids a live `[`, and must still let through the
    // `\[` a key's escaped name legitimately carries. Pinned on its own,
    // because a guard that accepted everything would pass every case it is
    // then run over.
    CHECK(unescapedPositions("a [b", '[') == std::vector<std::size_t> { 2 });
    CHECK(unescapedPositions("a \\[b", '[').empty());
    CHECK(unescapedPositions("a \\\\[b", '[') == std::vector<std::size_t> { 4 });
    CHECK(unescapedPositions("[", '[') == std::vector<std::size_t> { 0 });
}

namespace
{
// The per-element rounding in the Markdown guard below: three granularities,
// one of them negative.
constexpr formula::PlacesTable<3> guardPlaces { formula::DecimalPlaces { 0 },
                                                formula::DecimalPlaces { -1 },
                                                formula::DecimalPlaces { 2 } };
} // namespace

namespace
{
/// Roles for the Markdown guard below: one plainly named, and one whose
/// published name holds underscores, the one character of Markdown's
/// emphasis an identifier-like role name can hold.
struct GuardReference
{
};
struct PunctuatedRole
{
};
} // namespace

template <>
struct formula::TagName<PunctuatedRole>
{
    static constexpr std::string_view of() noexcept { return "_reference_"; }
};

TEST_CASE("render: Markdown output never contains text a CommonMark parser reinterprets, for any node kind",
          "[render][markdown]")
{
    auto const isInertInMarkdown = [](std::string const& text) {
        INFO("in: " << text);
        for (std::size_t const at: unescapedPositions(text, ']'))
            CHECK(text.compare(at, 2, "](") != 0);
        // A bare "[" alone is not risky by itself, but nothing this library
        // writes has any legitimate reason to contain one either -- so the
        // stronger check costs nothing and catches a "[...]" reference-style
        // link too, not only the inline "[...](...)" shape review round 3
        // found. A backslash-escaped `\[` is inert, and is exactly how a key's
        // author-supplied name carries one (`detail::literal_words_in_dialect`).
        CHECK(unescapedPositions(text, '[').empty());

        // Phase 13: a bare `|`. Inside a Markdown table cell it ends the
        // cell, silently -- task 1 measured a row whose formula held an
        // absolute value in bars render as one cell holding only the text
        // before the first bar (python-markdown 3.10.3, pymdown-extensions
        // 12.1). No plain or Markdown spelling in this library writes one:
        // an absolute value is `abs(...)` there, and bars are LaTeX's alone.
        CHECK(unescapedPositions(text, '|').empty());

        // Phase 10 round 2: an asterisk. A bare `*` CANNOT be forbidden the
        // way `[` is, because one node kind emits it legitimately --
        // `render_node(BinaryNode)` spells multiplication ` * ` in Plain and
        // Markdown alike, and always will.
        //
        // But every asterisk this library emits has a space on BOTH sides,
        // and that is exactly what makes it safe: CommonMark's flanking rules
        // make a `*` surrounded by whitespace neither a left- nor a
        // right-flanking delimiter run, so it can open and close nothing.
        // Measured rather than reasoned -- `a * b * c * d * e`, four
        // asterisks, through pandoc's CommonMark reader, pandoc's GFM reader
        // and python-markdown (MkDocs' engine) with `attr_list` and `smarty`:
        // all four survive, no `<em>` anywhere. An asterisk in any other
        // position does corrupt: `*x*` is dropped and emphasised by all
        // three.
        //
        // So the guard is on the property that makes the library's asterisks
        // inert, not on the character. It permits multiplication and fails on
        // any future node kind that emits `*` anywhere else. Anyone tempted to
        // relax this to "no asterisk at all": that breaks multiplication, and
        // this comment is here so you need not re-measure to find that out.
        //
        // A backslash-escaped `\*` is inert too, and is how a key's name
        // carries one, so only live asterisks are held to the rule.
        for (std::size_t const at: unescapedPositions(text, '*'))
        {
            INFO("asterisk at " << at << " in: " << text);
            CHECK(at > 0);
            CHECK(at + 1 < text.size());
            if (at > 0 && at + 1 < text.size())
            {
                CHECK(text[at - 1] == ' ');
                CHECK(text[at + 1] == ' ');
            }
        }
    };

    constexpr auto overThreshold = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(473, 10));
    constexpr auto rounded = formula::rounded<formula::unit::Millimetre,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);
    constexpr auto roundedSig = formula::rounded_to_digits<formula::unit::Millimetre,
                                                           formula::SignificantDigits { 2 },
                                                           formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);
    constexpr auto numeric = formula::numeric_value_of<formula::unit::Megapascal, "guard test coverage">(var<Strength>);
    constexpr auto chosen = formula::when(overThreshold, var<Strength> * rat(2), var<Strength> * rat(4));
    constexpr auto citedDiameter = formula::documented(var<Diameter>, { .title = "Diameter, cited" });
    constexpr formula::Verdict rejectSpecimen { .label = "reject the specimen" };
    constexpr auto rule = formula::constraint(var<WaterVolume> <= var<CementVolume>, rejectSpecimen);

    isInertInMarkdown(formula::render<Dialect::Markdown>(var<Strength>));                         // VarNode
    // ConstantNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::constant<formula::unit::Millimetre>(rat(139))));
    isInertInMarkdown(formula::render<Dialect::Markdown>(-var<Strength>));                        // UnaryNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(var<Strength> + var<Strength>));         // BinaryNode, +
    // BinaryNode, * -- the one node kind that legitimately emits an asterisk,
    // and therefore the one that decides how far the asterisk check can go.
    isInertInMarkdown(formula::render<Dialect::Markdown>(var<Strength> * var<Strength>));
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::pow<2>(var<Diameter>)));        // PowerNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::sqrt(var<Area>)));              // RootNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::pi));                           // PiNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(citedDiameter));                         // DocumentedNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(rounded));                               // RoundNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(roundedSig));                            // RoundSignificantNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(numeric));                               // NumericValueNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(roundedSpread));                         // RoundedRootNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(criticalLimit));                         // SampleSizeLookupNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::abs(var<Diameter> - var<Diameter>))); // AbsoluteValueNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(precisionOfDiameter));                         // PrecisionLimitNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(chosen));                                // WhenNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(overThreshold));                             // PredicateNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(rule));                                  // Constraint
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::series<Strength, 3>));          // SeriesVarNode
    // The elementwise nodes and a per-element constant, with a scalar
    // broadcast on either side and a series on both.
    isInertInMarkdown(formula::render<Dialect::Markdown>(-formula::series<Strength, 3>)); // ElementwiseUnaryNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::series<Strength, 3> * var<Strength>));
    isInertInMarkdown(formula::render<Dialect::Markdown>(var<Strength> - formula::series<Strength, 3>));
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::series<Strength, 3> / formula::series<Strength, 3>));
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        formula::series_constant<formula::unit::Megapascal>(rat(1), rat(-2), rat(3, 4)))); // SeriesConstantNode
    // A running total, from each end, and the sum that reduces a series to one
    // value -- alone and in a product, where LaTeX brackets it.
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Strength, 3>))); // CumulativeNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(formula::series<Strength, 3>)));
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::sum(formula::series<Strength, 3>))); // SumNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::sum(formula::series<Strength, 3>) * var<Strength>));
    // A per-element rounding: its table of granularities, which must not be
    // bracketed the way a list often is.
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        formula::rounded_elementwise<formula::unit::Megapascal, guardPlaces, formula::RoundingMode::HalfEven>(
            formula::series<Strength, 3>))); // ElementwiseRoundNode

    // Phase 10's three lookup kinds. A band is naturally written `[103, 197)`,
    // which is the exact character sequence this guard forbids -- so these
    // three lines are the reason `render.hpp` rules that a half-open interval
    // is spelled `103 to under 197` instead, and the thing that fails if anyone
    // ever "fixes" that back to the mathematician's spelling.
    isInertInMarkdown(formula::render<Dialect::Markdown>(bandedLookup()));                        // BandedLookupNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(shapeLookup()));                         // ExactLookupNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(curveLookup()));                         // InterpolatingLookupNode

    // A key whose name the author spelled with Markdown's own punctuation,
    // and one holding every character either dialect escapes: inert only
    // because the name is escaped, which is what these two lines guard.
    isInertInMarkdown(formula::render<Dialect::Markdown>(finishLookup(MouldFinish::Polished)));
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        exact_lookup<MarkingKeys, formula::unit::One>(MouldMarking::Stamped, { rat(1127, 1000) })));

    // Phase 14: a read from another record, alone and compound, and one whose
    // role's published name is underscored. A role's name is identifier-like
    // (`RequireIdentifierLikeRoleName`), so Markdown's link syntax, asterisks
    // and backticks cannot reach it; this guard does not check underscores,
    // and the name's escaping (`\_`) is pinned by `record_render_tests.cpp`.
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::from_record<GuardReference>(var<Strength>)));
    isInertInMarkdown(
        formula::render<Dialect::Markdown>(formula::from_record<GuardReference>(var<Strength> * var<Strength>)));
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::from_record<PunctuatedRole>(var<Strength>)));

    // And a formula nesting several of the above, since a guard that only
    // ever sees one node kind in isolation could still miss an interaction
    // between two -- which is exactly how review round 3's defect hid from
    // both the mutation testing and the "read it as a person would" pass in
    // fix round 1: neither ever combined a rounding/escape node with a
    // conditional operand under Dialect::Markdown and looked at the raw
    // character sequence rather than the string as a whole.
    constexpr auto deep =
        formula::numeric_value_of<formula::unit::Megapascal, "guard test coverage">(formula::when(
            overThreshold, var<Strength> * rat(2), var<Strength> * rat(4)));
    isInertInMarkdown(formula::render<Dialect::Markdown>(deep));

    // And the same for a lookup, which nests two ways at once: a conditional
    // as its operand (no closing delimiter of its own, the hazard the comma
    // form exists for) inside a rounding node inside a power.
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::pow<2>(
        banded_lookup<formula::unit::Millimetre, SizeBands, formula::unit::One>(
            formula::when(overThreshold, var<Diameter>, var<Diameter> * rat(2)),
            { rat(863, 1000), rat(1381, 1000), rat(1043, 1000) }))));
}

TEST_CASE("render: a rounding or a numeric value in a unit with no symbol adds no unit clause", "[render][rounding]")
{
    // `unit::One`'s symbol is empty, and the clause once read `to 2 dp of )`
    // and `numeric(..., in )`. A value with no unit is shown with none, as a
    // dimensionless constant is; a named unit keeps its clause.
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
    constexpr auto toPlaces =
        formula::rounded<formula::unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(ratio);
    constexpr auto toDigits = formula::rounded_to_digits<formula::unit::One,
                                                         formula::SignificantDigits { 2 },
                                                         formula::RoundingMode::HalfAwayFromZero>(ratio);
    constexpr auto bare = formula::numeric_value_of<formula::unit::One, "the fit is stated over the bare ratio">(ratio);

    CHECK(formula::render(toPlaces) == "round(V_w / V_c, to 2 dp)");
    CHECK(formula::render(toDigits) == "round(V_w / V_c, to 2 sf)");
    CHECK(formula::render(bare) == "numeric(V_w / V_c)");
    CHECK(formula::render<Dialect::LaTeX>(toPlaces) == "\\operatorname{round}_{2}(\\frac{V_w}{V_c})");
    CHECK(formula::render<Dialect::LaTeX>(toDigits) == "\\operatorname{round}_{2\\mathrm{sf}}(\\frac{V_w}{V_c})");
    CHECK(formula::render<Dialect::LaTeX>(bare) == "\\{\\frac{V_w}{V_c}\\}");

    constexpr auto inMegapascals =
        formula::numeric_value_of<formula::unit::Megapascal, "the fit is stated in MPa">(var<Strength>);
    CHECK(formula::render(inMegapascals) == "numeric(f, in MPa)");
    CHECK(formula::render<Dialect::LaTeX>(inMegapascals) == "\\{f/\\mathrm{MPa}\\}");
    CHECK(formula::render(formula::rounded<formula::unit::Millimetre,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(var<Diameter>))
          == "round(d, to 1 dp of mm)");
}

// ------------------------------------ LaTeX: a unit's specials, a key's name

namespace
{
struct Share: formula::Quantity<Share, "s", "share of the mix", formula::unit::Percent>
{
};

/// An author's unit whose symbol holds six of TeX's specials.
inline constexpr formula::Unit SpecialUnit { .dimension = formula::dim::Scalar,
                                             .symbolText = formula::symbol("a#b&c_d$e{f}g"),
                                             .decimals = 1 };
struct Special: formula::Quantity<Special, "q", "a quantity in the author's unit", SpecialUnit>
{
};

inline constexpr BandTable<2> ShareBands { band(0, 1, 413, 10), band(413, 10, 879, 10) };

/// A reflected name holding an underscore, the way an identifier does.
enum class LatexFit : std::uint8_t
{
    fit_2 = 1,
    loose = 2,
};

inline constexpr KeyTable<LatexFit, 2> LatexFitKeys { LatexFit::fit_2, LatexFit::loose };
} // namespace

TEST_CASE("render: a percent sign is escaped wherever a unit's symbol enters LaTeX", "[render][latex]")
{
    // `%` is TeX's comment character. Written bare it commented out the rest
    // of the formula: a percent-valued lookup row stopped tectonic with "File
    // ended while scanning use of \text@", and a constant `5 %` with "Missing
    // $ inserted" -- while the site's MathJax silently dropped the `%`. Each
    // clause a unit's symbol reaches, spelt exactly.
    CHECK(formula::render<Dialect::LaTeX>(var<Share> * formula::constant<formula::unit::Percent>(rat(5)))
          == "s \\cdot 5\\,\\mathrm{\\%}");
    CHECK(
        formula::render<Dialect::LaTeX>(
            formula::rounded<formula::unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
                var<Share>))
        == "\\operatorname{round}_{1\\,\\mathrm{\\%}}(s)");
    CHECK(formula::render<Dialect::LaTeX>(formula::rounded_to_digits<formula::unit::Percent,
                                                                     formula::SignificantDigits { 2 },
                                                                     formula::RoundingMode::HalfAwayFromZero>(var<Share>))
          == "\\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{\\%}}(s)");
    CHECK(formula::render<Dialect::LaTeX>(formula::numeric_value_of<formula::unit::Percent, "fit stated in %">(var<Share>))
          == "\\{s/\\mathrm{\\%}\\}");
    // A lookup's result unit, and its key unit.
    CHECK(formula::render<Dialect::LaTeX>(banded_lookup<formula::unit::Millimetre, ShareBands, formula::unit::Percent>(
              var<Diameter>, { rat(863, 10), rat(1127, 10) }))
          == "\\operatorname{lookup}(d,\\allowbreak \\mathrm{0\\ to\\ under\\ 413/10\\ mm\\ gives\\ 863/10\\ \\%},"
             "\\allowbreak \\mathrm{413/10\\ to\\ under\\ 879/10\\ mm\\ gives\\ 1127/10\\ \\%})");
    CHECK(formula::render<Dialect::LaTeX>(
              banded_lookup<formula::unit::Percent, ShareBands, formula::unit::One>(var<Share>,
                                                                                    { rat(1127, 1000), rat(863, 1000) }))
          == "\\operatorname{lookup}(s,\\allowbreak \\mathrm{0\\ to\\ under\\ 413/10\\ \\%\\ gives\\ 1127/1000},"
             "\\allowbreak \\mathrm{413/10\\ to\\ under\\ 879/10\\ \\%\\ gives\\ 863/1000})");
    // Plain and Markdown are for a terminal and a web page, where `%` is
    // nothing special: untouched there.
    CHECK(formula::render(formula::constant<formula::unit::Percent>(rat(5))) == "5 %");
    CHECK(formula::render<Dialect::Markdown>(formula::constant<formula::unit::Percent>(rat(5))) == "5 %");
}

TEST_CASE("render: every TeX special in an author's unit symbol is escaped in LaTeX", "[render][latex]")
{
    CHECK(formula::render<Dialect::LaTeX>(formula::constant<SpecialUnit>(rat(3)))
          == "3\\,\\mathrm{a\\#b\\&c\\_d\\$e\\{f\\}g}");
    CHECK(formula::render<Dialect::LaTeX>(
              formula::rounded<SpecialUnit, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
                  var<Special>))
          == "\\operatorname{round}_{1\\,\\mathrm{a\\#b\\&c\\_d\\$e\\{f\\}g}}(q)");
    // A named unit is only wrapped, and a unit with no symbol adds nothing.
    CHECK(formula::render<Dialect::LaTeX>(formula::constant<formula::unit::Millimetre>(rat(139))) == "139\\,\\mathrm{mm}");
    CHECK(formula::render<Dialect::LaTeX>(formula::constant<formula::unit::One>(rat(3))) == "3");
}

TEST_CASE("render: a lookup key's name is set in math mode, where the site's MathJax reads its escapes", "[render][latex]")
{
    // `\text{key fit\_2}` is right for a TeX engine and shows `key fit\_2`,
    // backslash and all, on the site: MathJax does not load `textmacros`, so
    // it reads `\text{...}` literally. In `\mathrm{...}` both read `\_` and
    // `\ ` the same way.
    CHECK(
        formula::render<Dialect::LaTeX>(
            exact_lookup<LatexFitKeys, formula::unit::One>(LatexFit::fit_2, { rat(1127, 1000), rat(863, 1000) }))
        == "\\operatorname{lookup}(\\mathrm{key\\ fit\\_2},\\allowbreak \\mathrm{key\\ fit\\_2\\ gives\\ 1127/1000},"
           "\\allowbreak \\mathrm{key\\ loose\\ gives\\ 863/1000})");
    CHECK(formula::detail::latex_math_words("key fit_2") == "key\\ fit\\_2");
}

// ---- A series variable, marked as a series in the formula itself (phase 12, S14) ----

namespace
{
namespace series_render
{
    struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
    {
    };
    struct Total: formula::Quantity<Total, "m_t", "total dry mass", formula::unit::Gram>
    {
    };
} // namespace series_render
} // namespace

TEST_CASE("a series variable is marked as a series in the formula itself, in every dialect", "[series][render]")
{
    // S14 as ruled; the marker task 1 chose (task-1-spike.md): LaTeX braces
    // the whole symbol then subscripts it, plain and Markdown append `(i)`,
    // Markdown inside the backticks.
    using series_render::Retained;
    constexpr auto everyone = formula::vocabulary(formula::renames<Retained>("x_m"));
    CHECK(formula::render(formula::series<Retained, 5>, everyone) == "x_m(i)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::series<Retained, 5>, everyone) == "`x_m(i)`");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::series<Retained, 5>, everyone) == "{x_m}_{i}");
    // A scalar of the same quantity is NOT marked, so the two read differently.
    CHECK(formula::render(formula::var<Retained>, everyone) == "x_m");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::var<Retained>, everyone) == "x_m");
    // The marker wraps the jurisdiction's symbol, never the declared one.
    CHECK(formula::render(formula::series<Retained, 5>) == "m_r(i)");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::series<Retained, 5>) == "{m_r}_{i}");
    // A symbol with a braced subscript still groups (typeset clean in task 1).
    constexpr auto braced = formula::vocabulary(formula::renames<Retained>("f_{c}"));
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::series<Retained, 5>, braced) == "{f_{c}}_{i}");
    // The known limit, pinned so it is a decision and not an accident: a
    // symbol that already ends in `)` reads with two parenthesised groups.
    constexpr auto parenthesised = formula::vocabulary(formula::renames<Retained>("w(t)"));
    CHECK(formula::render(formula::series<Retained, 5>, parenthesised) == "w(t)(i)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::series<Retained, 5>, parenthesised) == "`w(t)(i)`");
}

TEST_CASE("elementwise arithmetic renders as scalar arithmetic does, the series operand marked", "[series][render]")
{
    using series_render::Retained;
    constexpr auto fraction = formula::series<Retained, 5> / formula::var<series_render::Total>;
    // S14: the series variable carries the marker; the operation adds none.
    CHECK(formula::render(fraction) == "m_r(i) / m_t");
    CHECK(formula::render<formula::Dialect::Markdown>(fraction) == "`m_r(i)` / `m_t`");
    CHECK(formula::render<formula::Dialect::LaTeX>(fraction) == "\\frac{{m_r}_{i}}{m_t}");
    // Brackets exactly where the scalar operators put them: the right side of
    // a subtraction, a sum inside a product, a negated sum.
    constexpr auto s = formula::series<Retained, 5>;
    CHECK(formula::render(formula::var<series_render::Total> - (s - formula::var<series_render::Total>) )
          == "m_t - (m_r(i) - m_t)");
    CHECK(formula::render((s + s) * formula::var<series_render::Total>) == "(m_r(i) + m_r(i)) * m_t");
    CHECK(formula::render(-(s + s)) == "-(m_r(i) + m_r(i))");
    CHECK(formula::render(-s) == "-m_r(i)");
    CHECK(formula::render<formula::Dialect::LaTeX>(s * formula::var<series_render::Total>) == "{m_r}_{i} \\cdot m_t");
}

TEST_CASE("a per-element constant renders as its list of values", "[series][render]")
{
    // S14: a series constant prints its rows, which already reads as many
    // values, so it carries no index marker. Each value is spelled as a
    // constant holding it would be.
    constexpr auto factors = formula::series_constant<formula::unit::Millimetre>(rat(11), rat(29), rat(41));
    CHECK(formula::render(factors) == "values(11 mm, 29 mm, 41 mm)");
    CHECK(formula::render<formula::Dialect::Markdown>(factors) == "values(11 mm, 29 mm, 41 mm)");
    CHECK(formula::render<formula::Dialect::LaTeX>(factors)
          == "\\operatorname{values}(11\\,\\mathrm{mm},\\allowbreak 29\\,\\mathrm{mm},\\allowbreak 41\\,\\mathrm{mm})");
    constexpr auto plain = formula::series_constant<formula::unit::One>(rat(1), rat(2));
    CHECK(formula::render(plain) == "values(1, 2)");
    CHECK(formula::render(formula::series<series_render::Retained, 2> * plain) == "m_r(i) * values(1, 2)");
}

TEST_CASE("a running total renders with its direction, and a sum as a call on the series", "[series][render]")
{
    using series_render::Retained;
    constexpr auto s = formula::series<Retained, 5>;
    constexpr auto fromLast = formula::cumulative<formula::CumulativeDirection::FromLast>(s);
    constexpr auto fromFirst = formula::cumulative<formula::CumulativeDirection::FromFirst>(s);
    // The direction is part of the formula: a rendering without it would state
    // half of it, so both ends are pinned and read differently.
    CHECK(formula::render(fromLast) == "cumulative(m_r(i), from last)");
    CHECK(formula::render(fromFirst) == "cumulative(m_r(i), from first)");
    CHECK(formula::render<formula::Dialect::Markdown>(fromLast) == "cumulative(`m_r(i)`, from last)");
    CHECK(formula::render<formula::Dialect::LaTeX>(fromLast) == "\\operatorname{cumulative}_{\\text{from last}}({m_r}_{i})");
    CHECK(formula::render<formula::Dialect::LaTeX>(fromFirst)
          == "\\operatorname{cumulative}_{\\text{from first}}({m_r}_{i})");

    // The sum's operand is a series and carries the marker; its result is one
    // value and carries none.
    constexpr auto total = formula::sum(s);
    CHECK(formula::render(total) == "sum(m_r(i))");
    CHECK(formula::render<formula::Dialect::Markdown>(total) == "sum(`m_r(i)`)");
    CHECK(formula::render<formula::Dialect::LaTeX>(total) == "\\sum {m_r}_{i}");

    // In context. Plain text's call groups itself; LaTeX's large operator does
    // not, so it is bracketed wherever an additive expression would be -- and
    // a fraction groups it itself.
    constexpr auto share = s / formula::sum(s);
    CHECK(formula::render(share) == "m_r(i) / sum(m_r(i))");
    CHECK(formula::render<formula::Dialect::LaTeX>(share) == "\\frac{{m_r}_{i}}{\\sum {m_r}_{i}}");
    constexpr auto scaled = formula::sum(s) * formula::var<series_render::Total>;
    CHECK(formula::render(scaled) == "sum(m_r(i)) * m_t");
    CHECK(formula::render<formula::Dialect::LaTeX>(scaled) == "(\\sum {m_r}_{i}) \\cdot m_t");
    constexpr auto passing =
        formula::constant<formula::unit::Percent>(rat(100)) - fromLast / formula::var<series_render::Total>;
    CHECK(formula::render(passing) == "100 % - cumulative(m_r(i), from last) / m_t");
    CHECK(formula::render<formula::Dialect::Markdown>(passing) == "100 % - cumulative(`m_r(i)`, from last) / `m_t`");
    // Under a jurisdiction's symbol, in every dialect.
    constexpr auto everyone = formula::vocabulary(formula::renames<Retained>("x_m"));
    CHECK(formula::render(formula::sum(fromFirst), everyone) == "sum(cumulative(x_m(i), from first))");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::sum(fromFirst), everyone)
          == "\\sum \\operatorname{cumulative}_{\\text{from first}}({x_m}_{i})");
}

namespace
{
namespace series_rounding_render
{
    struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
    {
    };
    struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Millimetre>
    {
    };

    constexpr formula::PlacesTable<5> fivePlaces { formula::DecimalPlaces { 0 },
                                                   formula::DecimalPlaces { 0 },
                                                   formula::DecimalPlaces { 0 },
                                                   formula::DecimalPlaces { 1 },
                                                   formula::DecimalPlaces { 1 } };
    constexpr formula::PlacesTable<3> threePlaces { formula::DecimalPlaces { 0 },
                                                    formula::DecimalPlaces { -1 },
                                                    formula::DecimalPlaces { 2 } };
} // namespace series_rounding_render
} // namespace

TEST_CASE("a per-element rounding renders every granularity, in order, and no mode", "[series][render]")
{
    using series_rounding_render::Passing;
    constexpr auto passing =
        formula::rounded_elementwise<formula::unit::Percent,
                                     series_rounding_render::fivePlaces,
                                     formula::RoundingMode::HalfAwayFromZero>(formula::series<Passing, 5>);
    // The granularities in the series' order, so a table read backwards
    // (1/1/0/0/0) reads differently; the operand carries the marker.
    CHECK(formula::render(passing) == "round(p(i), to 0/0/0/1/1 dp of %)");
    CHECK(formula::render<formula::Dialect::Markdown>(passing) == "round(`p(i)`, to 0/0/0/1/1 dp of %)");
    // A formula states a granularity, not a tie rule: the mode appears in the
    // trace only, as for RoundNode. The exact strings above already hold no
    // mode and no bracket.

    // LaTeX, in millimetres -- percent's `%` is a LaTeX comment character, a
    // limit every rounding node shares (`unit.hpp`) -- with a negative place.
    constexpr auto openings =
        formula::rounded_elementwise<formula::unit::Millimetre,
                                     series_rounding_render::threePlaces,
                                     formula::RoundingMode::HalfEven>(formula::series<series_rounding_render::Opening, 3>);
    CHECK(formula::render(openings) == "round(d(i), to 0/-1/2 dp of mm)");
    CHECK(formula::render<formula::Dialect::LaTeX>(openings) == "\\operatorname{round}_{0/-1/2\\,\\mathrm{mm}}({d}_{i})");
    // In a vocabulary, and nested in a product without brackets: the call
    // groups itself.
    constexpr auto everyone = formula::vocabulary(formula::renames<Passing>("P"));
    CHECK(formula::render(passing * rat(2), everyone) == "round(P(i), to 0/0/0/1/1 dp of %) * 2");
}

TEST_CASE("a documented or replaced sum brackets in LaTeX as the bare one does", "[series][render]")
{
    using series_render::Retained;
    using series_render::Total;
    constexpr auto s = formula::series<Retained, 5>;
    constexpr formula::Citation cited { .reference = "Example Standard 1:2020", .section = "4.2" };
    // A citation is transparent to bracketing, as it is for a constant: the
    // bracket that stops `\sum x_i \cdot m_t` reading as a sum of products
    // survives the wrapper.
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::documented(formula::sum(s), cited) * formula::var<Total>)
          == "(\\sum {m_r}_{i}) \\cdot m_t");
    CHECK(formula::render(formula::documented(formula::sum(s), cited) * formula::var<Total>) == "sum(m_r(i)) * m_t");

    // A jurisdiction's replacement: the variant replaced by a sum, then
    // multiplied, brackets as the sum does.
    struct Whole
    {
    };
    constexpr auto m = formula::method(formula::variants(formula::variant<Whole>(formula::sum(s) / formula::var<Total>)),
                                       formula::rounding_rule<formula::unit::Percent,
                                                              formula::DecimalPlaces { 1 },
                                                              formula::RoundingMode::HalfAwayFromZero>(),
                                       formula::constraints());
    constexpr auto replaced =
        formula::apply(formula::overlay(formula::replace_variant<Whole>(
                           formula::sum(s / formula::var<Total>), formula::Citation { .reference = "Example Standard 4" })),
                       m);
    constexpr auto replacement = std::get<0>(replaced.variantSet.cases).expression;
    CHECK(formula::render<formula::Dialect::LaTeX>(replacement * rat(2)) == "(\\sum \\frac{{m_r}_{i}}{m_t}) \\cdot 2");
}
