// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/render.hpp>

#include <catch2/catch_test_macros.hpp>

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

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

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
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(150))) == "150 mm");
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
    CHECK(formula::render<Dialect::Markdown>(formula::constant<formula::unit::Millimetre>(rat(150)))
          == "150 mm");                                                                                 // ConstantNode
    CHECK(formula::render<Dialect::Markdown>(-var<Diameter>) == "-`d`");                                // UnaryNode
    CHECK(formula::render<Dialect::Markdown>(var<WaterVolume> + var<CementVolume>) == "`V_w` + `V_c`"); // BinaryNode
    CHECK(formula::render<Dialect::Markdown>(formula::pow<2>(var<Diameter>)) == "`d`^2");               // PowerNode
    CHECK(formula::render<Dialect::Markdown>(formula::sqrt(var<Area>)) == "sqrt(`A`)");                 // RootNode
    CHECK(formula::render<Dialect::Markdown>(formula::pi) == "pi");                                     // PiNode
    CHECK(formula::render<Dialect::Markdown>(citedDiameter) == "`d`");                                  // DocumentedNode
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
    // A constant with a unit symbol renders as two tokens ("150 mm"), not
    // one, so without a bracket "150 mm^2" would read as "150 * mm^2" =
    // 150 mm^2, while the tree means (150 mm)^2 = 22500 mm^2 -- the same
    // class of defect as a negative constant's leading "-", for the other
    // piece of runtime data the type does not carry.
    CHECK(formula::render(formula::pow<2>(formula::constant<formula::unit::Millimetre>(rat(150)))) == "(150 mm)^2");
}

TEST_CASE("render: a unit-bearing constant as a term or factor stays unbracketed", "[render]")
{
    // Neither an additive nor a multiplicative context can misread "150 mm"
    // the way a power's base can, so a unit-bearing constant here is left
    // exactly as before. Addition requires both sides to share a dimension,
    // so the addend is also stated in millimetres rather than as a bare
    // dimensionless rational.
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(150))
                          + formula::constant<formula::unit::Millimetre>(rat(3)))
          == "150 mm + 3 mm");
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(150)) * rat(2)) == "150 mm * 2");
}

TEST_CASE("render: a dimensionless constant as the base of a power needs no bracket", "[render]")
{
    // Confirms the unit-symbol fix is keyed on the unit having a symbol, not
    // a blanket bracket around every constant at the base of a power: `One`
    // has no symbol, so a constant of it renders as a single token.
    CHECK(formula::render(formula::pow<2>(formula::constant<formula::unit::One>(rat(5)))) == "5^2");
}

// ------------------------------------------------------- phase 8: rounding

TEST_CASE("render: a decimal-places rounding node renders as round(... to N dp of unit)", "[render][rounding]")
{
    constexpr auto rounded = formula::rounded<formula::unit::Millimetre,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render<Dialect::Plain>(rounded) == "round(d to 1 dp of mm)");
    CHECK(formula::render<Dialect::Markdown>(rounded) == "round(`d` to 1 dp of mm)");
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
    constexpr auto rounded = formula::rounded<formula::unit::Millimetre,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render(formula::pow<2>(rounded)) == "round(d to 1 dp of mm)^2");
    CHECK(formula::render(rounded * rat(2)) == "round(d to 1 dp of mm) * 2");
}

TEST_CASE("render: a significant-digits rounding node renders as round(... to N sf of unit)", "[render][rounding]")
{
    constexpr auto rounded = formula::rounded_to_digits<formula::unit::Millimetre,
                                                        formula::SignificantDigits { 2 },
                                                        formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render<Dialect::Plain>(rounded) == "round(d to 2 sf of mm)");
    CHECK(formula::render<Dialect::Markdown>(rounded) == "round(`d` to 2 sf of mm)");
    CHECK(formula::render<Dialect::LaTeX>(rounded) == "\\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{mm}}(d)");
}

TEST_CASE("render: a significant-digits rounding node inside a power and inside a product keeps no extra bracket",
          "[render][rounding]")
{
    constexpr auto rounded = formula::rounded_to_digits<formula::unit::Millimetre,
                                                        formula::SignificantDigits { 2 },
                                                        formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);

    CHECK(formula::render(formula::pow<2>(rounded)) == "round(d to 2 sf of mm)^2");
    CHECK(formula::render(rounded * rat(2)) == "round(d to 2 sf of mm) * 2");
}

// --------------------------------------------------- phase 8: predicates

TEST_CASE("render: a predicate renders as lhs comparison rhs", "[render][predicate]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50));

    CHECK(formula::render<Dialect::Plain>(overFifty) == "f > 50 MPa");
    CHECK(formula::render<Dialect::Markdown>(overFifty) == "`f` > 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(overFifty) == "f > 50 MPa");
    // The default dialect for a Predicate is plain, exactly as for a Node.
    CHECK(formula::render(overFifty) == "f > 50 MPa");
}

TEST_CASE("render: every comparison spells correctly, and three of them get a LaTeX-specific symbol",
          "[render][predicate]")
{
    // <, > and == read the same in every dialect; <=, >= and != get the
    // mathematical spelling in LaTeX, the same way BinaryNode's "*" becomes
    // "\cdot" there.
    constexpr auto threshold = formula::constant<formula::unit::Megapascal>(rat(50));

    CHECK(formula::render(var<Strength> < threshold) == "f < 50 MPa");
    CHECK(formula::render(var<Strength> <= threshold) == "f <= 50 MPa");
    CHECK(formula::render(var<Strength> > threshold) == "f > 50 MPa");
    CHECK(formula::render(var<Strength> >= threshold) == "f >= 50 MPa");
    CHECK(formula::render(var<Strength> == threshold) == "f == 50 MPa");
    CHECK(formula::render(var<Strength> != threshold) == "f != 50 MPa");

    CHECK(formula::render<Dialect::LaTeX>(var<Strength> < threshold) == "f < 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> <= threshold) == "f \\leq 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> > threshold) == "f > 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> >= threshold) == "f \\geq 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> == threshold) == "f = 50 MPa");
    CHECK(formula::render<Dialect::LaTeX>(var<Strength> != threshold) == "f \\neq 50 MPa");
}

TEST_CASE("render: a conditional nested as a predicate's operand keeps its bracket", "[render][predicate]")
{
    // A PredicateNode's own operands are ordinary Node expressions, and a
    // WhenNode is one of those -- so the same hazard a power or a product
    // operand has applies here too: rendered without a bracket, "if f > 50
    // MPa then f else f * 2 > 10 MPa" would read as the comparison applying
    // to the else branch alone, not to the whole conditional.
    constexpr auto overFifty = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50));
    constexpr auto chosen = formula::when(overFifty, var<Strength>, var<Strength> * rat(2));
    constexpr auto guarded = chosen > formula::constant<formula::unit::Megapascal>(rat(10));

    CHECK(formula::render(guarded) == "(if f > 50 MPa then f else f * 2) > 10 MPa");
}

// --------------------------------------------------- phase 8: conditionals

TEST_CASE("render: a conditional renders as if/then/else, and as a LaTeX cases block", "[render][conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50));
    constexpr auto chosen = formula::when(overFifty, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render<Dialect::Plain>(chosen) == "if f > 50 MPa then f * 2 else f * 4");
    CHECK(formula::render<Dialect::Markdown>(chosen) == "if `f` > 50 MPa then `f` * 2 else `f` * 4");
    CHECK(formula::render<Dialect::LaTeX>(chosen)
          == "\\begin{cases} f \\cdot 2 & \\text{if } f > 50 MPa \\\\ f \\cdot 4 & \\text{otherwise} \\end{cases}");
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
    constexpr auto overFifty = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50));
    constexpr auto chosen = formula::when(overFifty, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render(formula::pow<2>(chosen)) == "(if f > 50 MPa then f * 2 else f * 4)^2");
}

TEST_CASE("render: a conditional inside a product keeps its bracket", "[render][conditional]")
{
    // The exact scenario named in the task: when(p, a, b) * 2 must not read
    // as when(p, a, b * 2), which is a different formula.
    constexpr auto overFifty = var<Strength> > formula::constant<formula::unit::Megapascal>(rat(50));
    constexpr auto chosen = formula::when(overFifty, var<Strength> * rat(2), var<Strength> * rat(4));

    CHECK(formula::render(chosen * rat(2)) == "(if f > 50 MPa then f * 2 else f * 4) * 2");
}

// ------------------------------------------------- phase 8: numeric_value_of

TEST_CASE("render: a numeric-value escape hatch renders as numeric(... in unit)", "[render][escape]")
{
    constexpr auto numeric =
        formula::numeric_value_of<formula::unit::Megapascal, "empirical fit is only valid stated in MPa">(
            var<Strength>);

    CHECK(formula::render<Dialect::Plain>(numeric) == "numeric(f in MPa)");
    CHECK(formula::render<Dialect::Markdown>(numeric) == "numeric(`f` in MPa)");
    CHECK(formula::render<Dialect::LaTeX>(numeric) == "\\{f/\\mathrm{MPa}\\}");
    // The justification string does not appear above -- see the comment on
    // NumericValueNode's render_node for why: it is an audit trail for
    // document(), not part of the formula's stated arithmetic.
}

TEST_CASE("render: a numeric-value escape hatch inside a power and inside a product keeps no extra bracket",
          "[render][escape]")
{
    constexpr auto numeric =
        formula::numeric_value_of<formula::unit::Megapascal, "empirical fit is only valid stated in MPa">(
            var<Strength>);

    CHECK(formula::render(formula::pow<2>(numeric)) == "numeric(f in MPa)^2");
    CHECK(formula::render(numeric * rat(2)) == "numeric(f in MPa) * 2");
}
