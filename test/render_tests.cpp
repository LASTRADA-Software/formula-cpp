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
