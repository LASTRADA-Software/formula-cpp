# Citations, rendering and generated documentation

`formula-cpp` provides a way to attach where a formula comes from --
`formula::documented()`, from `citation.hpp` -- and two ways to get that
formula back out as something a reader can use: `formula::render()`, from
`render.hpp`, turns a formula into text in one of three dialects, and
`formula::document()`, from `document.hpp`, walks a formula for its rendered
text, its citations and its symbol table together. This page explains how to
attach a citation, why the wrapper it creates is invisible to arithmetic, what
order citations come back in when formulas are nested, how the same formula
looks in each dialect, how the symbol table is built and deduplicated, and how
a whole documentation page is generated from formulas like these. The worked
example is `examples/citations.cpp`; every block on this page formatted as
program output is copied verbatim from that program's actual output, exactly
as `docs/expressions.md` does for evaluation. For a finished page built this
way from several formulas at once, see [the gallery](gallery.md).

Neither `render.hpp` nor `document.hpp` is included by the umbrella header,
`formula.hpp`. Both pull in `<string>` -- `document.hpp` also pulls in
`<vector>` -- to turn a formula into text, and a consumer who only evaluates
numbers must not compile a string formatter or a symbol table into every
translation unit for a feature it never asked for. Include whichever you
need, by name:

```cpp
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
```

A three-part CMake check (`cmake/CheckPublicHeaderIncludes.cmake`) enforces
this: no other public header may include `<string>`, `<vector>`, `<format>`
or `<iostream>` directly, no other public header may reach `render.hpp` or
`document.hpp` either (which would smuggle those same headers in one hop
removed), and each of those two opt-in headers is checked against exactly the
standard headers it declares needing, so one of them quietly growing a third
dependency fails the same build.

## Attaching a citation

`formula::documented(expr, citation)` wraps an expression in a node that
carries a `formula::Citation` alongside it. A citation is five
`std::string_view` fields, every one of them optional in the sense that it
defaults to empty rather than being required:

```cpp
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });
```

(`test/citation_tests.cpp`, and the same declaration in
`examples/citations.cpp`.) A citation naming only the fields that apply is
just as valid:

```cpp
constexpr auto sparse = formula::documented(var<WaterVolume>, { .title = "A volume" });
```

and every field left unnamed reads back empty, not absent --
`sparse.citation.reference.empty()` holds, there is no separate "was this
field given" flag (`test/citation_tests.cpp`,
`"citation: a field not named is empty, not absent"`).

The braced designated initialiser on the second argument works because that
argument is a plain `formula::Citation`, not a deduced template parameter --
verified on cl 19.51, clang-cl 22 and g++ 13.3 before `documented()` was
written this way. And because every field is `std::string_view` rather than
an owning string, a `Citation` built entirely from string literals -- as
every example in this repository is -- is usable inside a `constexpr` tree
for free. A citation built from a runtime `std::string` is legal too, but
that string must outlive every node holding the view onto it.

## Why the wrapper is invisible to arithmetic

`DocumentedNode<Inner>` does not declare a dimension of its own; it forwards
`Inner`'s:

```cpp
static constexpr Dimension dimension = Inner::dimension;
```

and evaluating one evaluates what it wraps and nothing else:

```cpp
template <typename Rep = Rational, Node Inner, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(DocumentedNode<Inner> const& node,
                                                           Env const& environment) noexcept
{
    return checked_evaluate_si<Rep>(node.inner, environment);
}
```

(both from `citation.hpp`). Wrapping a formula therefore changes neither its
dimension nor the number it produces:

```cpp
STATIC_REQUIRE(decltype(ratio)::dimension == formula::dim::Scalar);
STATIC_REQUIRE(decltype(formula::documented(var<WaterVolume>, {}))::dimension == formula::dim::Volume);
```

```cpp
constexpr auto wrapped = formula::checked_evaluate<Ratio>(ratio, inputs);
constexpr auto bare = formula::checked_evaluate<Ratio>(var<WaterVolume> / var<CementVolume>, inputs);

STATIC_REQUIRE(wrapped.has_value());
STATIC_REQUIRE(wrapped->measurement().value() == rat(3, 5));
STATIC_REQUIRE(wrapped->measurement() == bare->measurement());
```

(`test/citation_tests.cpp`, `"citation: wrapping does not change the
dimension"` and `"citation: a wrapped formula evaluates to what it
wrapped"`.) `examples/citations.cpp` prints the same fact as a number rather
than a boolean -- the wrapped formula evaluates to exactly the ratio the bare
division would have produced:

```
w/c = 0.600000 (computed)
```

This is not merely an absence of a check; there is a check, and it still
runs. `test/negative/documented_dimension_mismatch.cpp` wraps a volume in a
citation and adds it to a length:

```cpp
struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// Wrapping must not smuggle a dimensional error past the check: the wrapper
// forwards the dimension, so the addition is still refused.
inline constexpr auto broken = formula::documented(formula::var<Volume>, { .title = "A volume" }) + formula::var<Length>;
```

which gives, verbatim, on MSVC's `cl.exe` (19.51, `cl-debug` preset):

```
D:\formula-cpp\include\formula-cpp/expression.hpp(135): error C2338: static assertion failed: 'formula: the two sides of this addition or subtraction measure different dimensions; the offending operands appear in this diagnostic as the template arguments of RequireAddendsAgree'
D:\formula-cpp\include\formula-cpp/expression.hpp(135): note: the template instantiation context (the oldest one first) is
D:\formula-cpp\test\negative\documented_dimension_mismatch.cpp(14): note: see reference to function template instantiation 'auto formula::operator +<formula::DocumentedNode<formula::VarNode<Volume>>,formula::VarNode<Length>>(Left,Right) noexcept' being compiled
        with
        [
            Left=formula::DocumentedNode<formula::VarNode<Volume>>,
            Right=formula::VarNode<Length>
        ]
D:\formula-cpp\include\formula-cpp/expression.hpp(216): note: see reference to class template instantiation 'formula::BinaryNode<formula::BinaryOperator::Add,formula::DocumentedNode<formula::VarNode<Volume>>,formula::VarNode<Length>>' being compiled
D:\formula-cpp\include\formula-cpp/expression.hpp(191): note: see reference to class template instantiation 'formula::detail::AdditiveDimensionsAgree<formula::BinaryOperator::Add,Left,Right>' being compiled
        with
        [
            Left=formula::DocumentedNode<formula::VarNode<Volume>>,
            Right=formula::VarNode<Length>
        ]
D:\formula-cpp\include\formula-cpp/expression.hpp(152): note: see reference to class template instantiation 'formula::detail::RequireAddendsAgree<Left,Right>' being compiled
        with
        [
            Left=formula::DocumentedNode<formula::VarNode<Volume>>,
            Right=formula::VarNode<Length>
        ]
```

The left operand the diagnostic names is `formula::DocumentedNode<formula::VarNode<Volume>>`,
not `formula::VarNode<Volume>` -- the wrapper is right there in the type the
check refused, which is exactly the proof that it forwarded the dimension
rather than hiding it.

A documented expression also composes like any other node -- `ratio * 100`
type-checks and carries the right dimension, because `ratio` is, dimension
and all, still a `Node` (`test/citation_tests.cpp`, `"citation: a documented
expression is still an expression"`) -- and a citation survives being
wrapped a second time, each copy keeping its own fields:

```cpp
constexpr auto outer = formula::documented(ratio, { .title = "Water/cement ratio, per cent" });

STATIC_REQUIRE(outer.citation.title == std::string_view { "Water/cement ratio, per cent" });
STATIC_REQUIRE(outer.inner.citation.title == std::string_view { "Water/cement ratio" });
STATIC_REQUIRE(decltype(outer)::dimension == formula::dim::Scalar);
```

(`test/citation_tests.cpp`, `"citation: a citation survives being wrapped
again"`.)

## Nesting: what order citations come back in

A formula built from an already-documented formula carries two citations,
one nested inside the other. `test/document_tests.cpp` builds a water/cement
ratio, then multiplies it by 100 and documents *that*:

```cpp
constexpr auto perCent = formula::documented(
    ratio * rat(100),
    { .title = "Water/cement ratio, per cent", .reference = "Example Standard 1:2020", .section = "5.4.3" });
```

`formula::document()` walks the tree and returns both citations, **outermost
first**:

```cpp
formula::Documentation const documentation = formula::document(perCent);

REQUIRE(documentation.citations.size() == 2);
CHECK(documentation.citations[0].title == std::string_view { "Water/cement ratio, per cent" });
CHECK(documentation.citations[0].section == std::string_view { "5.4.3" });
CHECK(documentation.citations[1].title == std::string_view { "Water/cement ratio" });
CHECK(documentation.citations[1].section == std::string_view { "5.4.2" });
```

(`test/document_tests.cpp`, `"document: citations come back outermost
first"`.) That order falls out of *when* the walk records a citation, not
out of any sorting afterwards: it pushes the citation it is looking at before
it recurses into what that citation wraps, so the node nearest the root of
the tree -- the outermost wrapper -- is visited, and therefore pushed, first.
A formula with no citation at all simply returns an empty list rather than a
placeholder entry (`test/document_tests.cpp`, `"document: an undocumented
formula yields an empty citation list"`).

## The three dialects

`formula::Dialect` (`render.hpp`) has three values -- `Plain`, `Markdown` and
`LaTeX` -- and `formula::render<D>(node)` spells the same formula three
different ways depending on which is asked for. The plain and LaTeX
renderings below are `examples/citations.cpp`'s own output for `ratio`, the
water/cement formula from the section above:

```
plain: V_w / V_c
latex: \frac{V_w}{V_c}
```

Markdown wraps every symbol in backticks, so a symbol containing an
underscore -- `V_w`, in this formula -- is not read as emphasis by a Markdown
renderer:

```cpp
CHECK(formula::render<Dialect::Markdown>(var<WaterVolume> / var<CementVolume>) == "`V_w` / `V_c`");
```

(`test/render_tests.cpp`, `"render: the Markdown dialect emphasises the
symbols"`.) A citation attached with `documented()` never appears in any of
the three renderings -- `render()` answers only what the formula is, not
where it comes from; `document()` is what surfaces the citation alongside
the rendered text (`test/render_tests.cpp`, `"render: a citation does not
appear in the rendered formula"`).

Two further rules, true of every dialect, keep the rendering from bracketing
a formula in a way a reader would find surprising. First, wrapping a formula
in a citation changes none of its brackets -- `documented()` binds exactly
as tightly as what it wraps, at both the type level and, for a wrapped node
whose bracketing depends on data the type does not carry (a constant's sign
or unit symbol), at the runtime level too -- so a documented sum inside a
quotient is still bracketed exactly as the bare sum would be:

```cpp
constexpr auto documented = formula::documented(var<WaterVolume> + var<CementVolume>, { .title = "Total volume" });

CHECK(formula::render(documented / var<Diameter>) == "(V_w + V_c) / d");
```

(`test/render_tests.cpp`, `"render: a documented sum inside a quotient keeps
its brackets"`.) Second, a negative constant is bracketed wherever its
leading `-` would otherwise be misread as a unary minus -- which is not
quite everywhere that could happen: `-(-5)` renders as `--5`, not `-(-5)`,
because a `UnaryNode` is not itself a context a `Unary`-precedence child
needs bracketing against. As the base of a power, though, where `-5^2`
would mean `-(5^2)` to a reader while the tree means `(-5)^2`, it is
bracketed --

```cpp
CHECK(formula::render(formula::pow<2>(formula::number(rat(-5)))) == "(-5)^2");
```

-- but as a factor, where there is no such ambiguity, it is not:

```cpp
CHECK(formula::render(formula::number(rat(-5)) * var<Diameter>) == "-5 * d");
```

(`test/render_tests.cpp`, `"render: a negative constant as the base of a
power keeps its bracket"` and `"render: a negative constant as a factor
stays unbracketed"`.)

## The symbol table and its ordering rule

`formula::document()` also walks a formula for every variable it reads,
returned as `documentation.symbols`: each entry's symbol, description and
unit, exactly as `formula::Describe<Q>` gives them for that quantity.
`examples/citations.cpp` prints one row per variable in `ratio`:

```
symbol: V_w = effective water content [l]
symbol: V_c = cement content [l]
```

Two rules govern that list. First, it is ordered by **first appearance**,
reading the formula left to right -- not alphabetically, which would put `A`
before `d` in a formula that reads `pi * d^2 / 4` and would not match how
anyone reads it:

```cpp
formula::Documentation const documentation =
    formula::document(formula::pi * formula::pow<2>(var<Diameter>) / var<WaterVolume>);

REQUIRE(documentation.symbols.size() == 2);
CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
CHECK(documentation.symbols[1].symbol == std::string_view { "V_w" });
```

(`test/document_tests.cpp`, `"document: symbols come back in first-appearance
order"`.) Second, it is deduplicated **by quantity type**, not by the symbol
that quantity happens to render as. A quantity used twice contributes one row
(`test/document_tests.cpp`, `"document: a quantity used twice appears
once"`), but two distinct quantities that happen to share a rendered symbol
are not the same quantity, and each still gets its own row:

```cpp
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
```

(`test/document_tests.cpp`, `"document: two quantities that share a symbol
both get a row"`.) A literal coefficient and `formula::pi` name no variable
and contribute nothing to the table (`test/document_tests.cpp`, `"document: a
constant contributes no symbol"`).

## Generating a page

Everything above -- the rendered formula, its citations, its symbol table --
is exactly what `formula::Documentation` (`document.hpp`) holds, and exactly
what a documentation page needs. [`docs/gallery.md`](gallery.md) is one,
built by running `tools/gallery/main.cpp` over a handful of formulas and
writing each one's `document()` result out as a Markdown section: the
rendered formula in a code block, the LaTeX rendering as a `$$ ... $$` block
for MathJax, the symbol table as a Markdown table, and the citation fields
as a short list. That page is generated and checked in, so it reads on
GitHub with nothing built at all, and `gallery.is-current`
(`cmake/CheckGalleryIsCurrent.cmake`) fails CI the moment the checked-in page
and the generator disagree. Building a documentation page for your own
formulas is the same walk over your own tree; the gallery is the worked
example for it, so this guide points there rather than repeating it.

## Every citation here is invented

Every citation in this repository -- in the tests, in `examples/citations.cpp`
and in the gallery -- names a fictional `Example Standard`, never a real one.
Publishing this repository with a real standard's clause numbers and
equations transcribed into it would put copyrighted material in a public
repository, so the library's own documentation of its citation feature is,
deliberately, the one place that feature is never used for its intended
purpose.
