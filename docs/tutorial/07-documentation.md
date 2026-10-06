# 7. Citations, rendering and documentation

This chapter records where the strength formula comes from, and writes the
formula out for a report: as text, as LaTeX, and as a table of the symbols
it reads. All three come from the same declaration that calculates the
strength.

The program is chapter 5's, with its first rounding rule only, the strength
formula wrapped in a citation, and one specimen; the formula is rendered and
documented before it is evaluated.

## Cite where the formula comes from

`formula::documented(expression, citation)` attaches a `formula::Citation`
to an expression. A citation has five fields: `title`, `reference`,
`section`, `equation` and `text`. Each is optional, and a field left out
reads back empty. The strength formula cites the method it is taken from:

```cpp
--8<-- "examples/tutorial/07_documentation.cpp:formulas"
```

The citation changes nothing that is calculated: the wrapped expression has
the same dimension and gives the same value as the expression alone. A
specimen 150 mm by 150 mm that carried 675 kN gives 30 MPa, as in chapter 4:

```cpp
--8<-- "examples/tutorial/07_documentation.cpp:evaluate"
```

## Render it

`formula::render(formula)` writes a formula as plain text, and
`formula::render<formula::Dialect::LaTeX>(formula)` writes the same formula
as LaTeX, from the same declaration:

```cpp
--8<-- "examples/tutorial/07_documentation.cpp:render"
```

The rounding is written with its rule: `to 1 dp of MPa` in text, one
decimal place of a megapascal, and the subscript `1\,\mathrm{MPa}` in LaTeX,
the places followed by the unit they count in.

`render` writes the formula a bound formula holds, and names no result:
`f_c` appears in neither line
([Naming the result once](../expressions.md#naming-the-result-once)). The
citation does not appear in either rendering: it describes the formula
rather than being part of it.

## Generate its documentation

`formula::document(formula)` returns a `formula::Documentation`: the
rendering, the symbol table and the citations, which is everything a
report's methods section needs:

```cpp
--8<-- "examples/tutorial/07_documentation.cpp:document"
```

`page.symbols` holds one `formula::SymbolEntry` per quantity the formula
reads, in the order the formula reads them, each with the symbol,
description and unit its declaration gives. Like the rendering, the symbol
table describes what the formula reads and names no result, so `f_c` has no
row. The symbol table of a calculation lists the values it calculates
first ([The graph, known while the program
compiles](../calculations.md#the-graph-known-while-the-program-compiles)).
`page.citations` holds every citation in the formula, with all five fields;
the program prints the title and the reference.

## Output

```text
--8<-- "examples/tutorial/07_documentation.expected.txt"
```

## Summary

- `formula::documented(expression, citation)` -- attaches a citation to an
  expression, without changing what it calculates.
- `formula::Citation` -- where a formula comes from: `title`, `reference`,
  `section`, `equation` and `text`, each optional.
- `formula::render(formula)` -- the formula as plain text.
- `formula::Dialect::LaTeX` -- `render<formula::Dialect::LaTeX>` writes the
  formula as LaTeX.
- `formula::document(formula)` -- the formula's rendering, symbol table and
  citations.
- `formula::Documentation` -- what `document` returns; its `formula`,
  `symbols` and `citations` hold the rendering, the symbol table and the
  citations.
- `formula::SymbolEntry` -- one row of the symbol table: `symbol`, `unit`
  and `description`.

## Further reading

- [Citations, rendering and generated documentation](../citations.md#attaching-a-citation)
- [The symbol table and its ordering rule](../citations.md#the-symbol-table-and-its-ordering-rule)
- [Gallery](../gallery.md)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
