# Lookup tables

A method's own algebra sometimes needs a number no formula computes. The
procedure does not derive a size-correction factor from anything; it publishes
a table and tells you to read the right row out of it. formula-cpp models that
as an ordinary node, so a table sits inside an expression wherever a number of
its dimension would sit, and renders, documents, evaluates and traces like
everything else.

There are **three** kinds, and they are not interchangeable:

| Kind | What selects a row | What you get back |
|---|---|---|
| **Banded** (`banded_lookup`) | a measured value falls in an interval | the correction that interval stores |
| **Exact** (`exact_lookup`) | a category key names a row directly | that row's correction |
| **Interpolating** (`interpolating_lookup`) | a measured value sits between two rows | the value those two rows imply there — a number in no row at all |

The worked example is `examples/lookup_tables.cpp`; every block on this page
formatted as program output is copied verbatim from that program's actual
output, the same way [Constraints and verdicts](constraints.md) does for
`examples/constraints.cpp`.

## What this is for — and what it is not for

**A test method is code. A product specification is data.** A method's
algebra is fixed: it is published once, it is the same for every customer, and
it belongs in a library. The limit tables a product is *judged against* are
master data — registered per customer, per region, per contract, changed
without anyone recompiling anything. This library expresses methods, and never
specification tables.

That line runs straight through the middle of a lookup node, which is why it is
worth stating before anything else. A table's **structure** — which rows exist,
the unit its keys are stated in, the unit its values are stated in — is part of
the method, so it lives in the node's *type*, as non-type template parameters.
A table's **contents** — the number each row actually gives — are a registered
table's data, so they are an ordinary runtime member, handed to the factory:

```cpp
[[nodiscard]] constexpr auto sizeFactor()
{
    return formula::banded_lookup<unit::Millimetre, SizeBands, unit::Percent>(var<Diameter>,
                                                                              { rat(95), rat(100), rat(105) });
}
```

Everything in the **template argument list** — the unit the keys are stated in,
the bands themselves, the unit the values are stated in — is the method: fixed,
published once, and part of what this formula *is*. The **braced list** is the
registered table's data, and a caller may build the node from numbers that
arrived over the wire a moment ago.

Getting a band boundary wrong is a compile error naming the offending bands.
Getting a *correction* wrong is a data problem, and this library cannot and
should not try to catch it.

A reader who misses this distinction will reach for a lookup node to hold a
customer's acceptance limits. Do not: those change per contract, and a
compile-time template argument is the one place they cannot live.

## Banded: a measured value falls in an interval

A band table is declared as `formula::BandTable<N>`, each row a low bound
(inclusive) and a high bound (exclusive), both as exact numerator/denominator
pairs:

```cpp
inline constexpr formula::BandTable<3> SizeBands {
    formula::band(0, 1, 100, 1),   // 0 to under 100 mm
    formula::band(100, 1, 150, 1), // 100 to under 150 mm
    formula::band(150, 1, 200, 1), // 150 to under 200 mm -- 200 mm itself is NOT in it
};
```

`int64` pairs rather than `Rational`, for the reason
[Dimensions and units](dimensions.md) gives for `Unit`'s own magnitude and
offset fields: `Rational` keeps its members private, so it is not a
*structural* type and cannot be a non-type template parameter. An aggregate of
plain integers is, and so is a `std::array` of them — which is what makes it
possible to validate a whole table with `static_assert` rather than only when
it happens to be loaded at run time.

The node renders as one field per row, in the table's own declared order:

```
banded:        lookup(d, 0 to under 100 mm gives 95 %, 100 to under 150 mm gives 100 %, 150 to under 200 mm gives 105 %)
```

and looking a diameter up in it gives back that row's correction:

```
d = 120 mm:    1
```

`1`, exactly: 120 mm falls in the middle band, whose correction the table
states as `100 %`, converted into the dimensionless unit the result quantity
declares. Both sides of a table are converted — the key into the unit the bands
are stated in, and the value out of the unit the rows are stated in — so a
table may be written in whatever units the published document uses.

### Bands are half-open, and reconciling that against your source document is your job

A band runs from its low bound **up to but not including** its high bound. A
value sitting exactly on a boundary belongs to the band whose *low* bound it
is, never the band whose high bound it is:

```
d = 100 mm:    1 (the band above the boundary, never the one below)
```

A published table that writes one row as "30 to 40" and the next as "40 to 50"
leaves the value 40 ambiguous *on the page*. This library resolves it one way,
uniformly, rather than guessing which the author of a given table meant — and
it says so here rather than leaving you to find out from a mis-bucketed
specimen.

**So a published row that genuinely means "up to and including the maximum"
must be written with its high bound at the next tick past that maximum.** Not
approximately past it: a real, exact number. A physical measurement is always
read to some declared decimal precision — `Unit::decimals` — so "the next tick"
always exists. `unit::Millimetre` declares one decimal, so a row meaning "150 mm
to 200 mm inclusive" is written with its high bound at 200.1 mm:

```cpp
inline constexpr formula::BandTable<1> TopRowInclusive {
    formula::band(150, 1, 2001, 10), // 150 to under 200.1 mm -- 200 mm IS in it
};
```

Both spellings, evaluated at exactly 200 mm, side by side:

```
d = 200 mm:    argument outside the domain of the operation
inclusive top: lookup(d, 150 to under 2001/10 mm gives 105 %)
d = 200 mm:    21/20
```

The first table's last row stops under 200 mm, so 200 mm is in no band and
there is no answer. The second reaches it.

There is deliberately **no closed-upper-bound flag** on `Band` to spare you
this. Adding one would only move the ambiguity from "which bound is closed" to
"is the flag set correctly on the right row", and it would give the one
validation rule below two different adjacency rules to reconcile instead of
one.

## A table with a gap does not compile, and the message says where

A published table can contain a typo, and the two that matter are a **gap** (a
region of the domain no row covers) and an **overlap** (a region two rows both
claim). Both are refused at compile time.

This is not a claim about the library; it is a file in it.
`test/negative/lookup_band_gap.cpp` declares a four-row table with a gap in its
*middle* pair — a defect at either end is the easy case — and CI asserts both
that it fails to build and that it fails for the stated reason:

```cpp
    inline constexpr formula::BandTable<4> GappedTable {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
        formula::band(25, 1, 35, 1), // gap: band[1]'s high (20) != band[2]'s low (25)
        formula::band(35, 1, 45, 1),
    };

    inline constexpr auto broken =
        formula::banded_lookup<formula::unit::Millimetre, GappedTable, formula::unit::One>(
            formula::var<Diameter>,
            { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } });
```

Compiling it says, verbatim, on clang-cl 22.1.3 (the `clangcl-debug` preset's
compiler), with the rest of the instantiation backtrace below these lines:

```
In file included from test\negative\lookup_band_gap.cpp:9:
In file included from include\formula-cpp/lookup.hpp:469:
include\formula-cpp/band.hpp(219,19): error: static assertion failed due to requirement 'bands_are_adjacent(formula::Band{10, 1, 20, 1}, formula::Band{25, 1, 35, 1})': formula: this band table has a gap or overlap between two adjacent bands; the earlier band's declared high bound and the later band's declared low bound do not match exactly, and the two offending Band values appear in this diagnostic as the template arguments First and Second of RequireBandsAdjacent
  219 |     static_assert(bands_are_adjacent(First, Second),
      |                   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
include\formula-cpp/band.hpp(265,17): note: in instantiation of template class 'formula::RequireBandsAdjacent<Band{10, 1, 20, 1}, Band{25, 1, 35, 1}>' requested here
```

The message names **both offending rows**, as the values you typed: the one
ending at 20 and the one starting at 25. You are not told "a table is invalid"
and left to find which row; you are told which two.

**Consider what a library that accepted this table would produce.** It would
compile, link, run, and answer for every diameter — including the ones from
20 mm up to 25 mm, where the method defines nothing at all. Whatever it
returned there would be invented: the band below, the band above, zero, the
first row. A number for an input the method never defined is worse than no
number, because nothing downstream can tell it apart from one the method did
define. A test report carrying it is wrong in a way no reader can see. Refusing
the table at compile time is the only outcome that cannot mislead anybody.

The same check runs over every band and every adjacent pair, not just the last
of either, and there is a matching `band_table_is_well_formed` for a table that
only arrives at runtime — built on the same two predicates, so the compile-time
and the runtime answer cannot drift apart.

Three more mistakes are refused the same way: a row whose low bound is not below
its high bound, a zero-width row, and a corrections list of the wrong length.
That last one is worth its own sentence. `Corrections<N>` exists precisely so
that a short braced list is a compile error naming both counts — handed to a
bare `std::array<Rational, N>`, a short list silently value-initialises the rest
to zero, and a row whose correction you forgot to type would then answer `0`,
confidently, indistinguishable from a deliberate zero.

It is the **node's own member type**, not only the factory's parameter type,
and that distinction was bought the hard way. A lookup node is a public
aggregate with public members, so it can be declared without calling a factory
at all — and while the member was a raw array, that route bypassed the check
entirely and the untyped rows evaluated to `0` on all three kinds. The factory's
parameter cannot see a call that never happens. A consequence worth knowing:
a lookup node has no default constructor, because `{}` for a table of three
rows is a count of zero, which is exactly the mistake being refused.

## A miss is not a value

A diameter in no band. A key no row names. An age off the end of a curve. In
all three cases the lookup **found nothing**, and this library says so — three
separate lines of the example's output, one per kind, gathered here:

```
d = 200 mm:    argument outside the domain of the operation
DrilledCore:   argument outside the domain of the operation (a key no row of the table names)
d = 220 mm:    argument outside the domain of the operation (no extrapolation past the last row)
```

That is `ArithmeticError::DomainError`, arriving through the same channel every
other arithmetic failure in this library arrives through: `checked_evaluate`
returns a `std::expected` whose error it is. `DomainError` is not a euphemism
borrowed to fit — a banded table's domain **is** the union of its bands, an
exact table's domain **is** its set of keys, and an interpolating curve's
domain **is** the span from its first row to its last, exactly and literally.

**There is no default-value parameter, and there will not be one.** Not zero,
not the nearest band, not the first row, not the last one, and — for an
interpolating table — no extrapolation past the outermost breakpoints. Every one
of those is a number the published document never states, invented by this
library and then handed to somebody who cannot tell it apart from a number the
document did state. It is the same argument
[Constraints and verdicts](constraints.md) makes about `bool satisfied()`: an
API that must answer *something* for the unresolved case, where every available
answer is a lie.

**A method that genuinely says "hold the last value beyond the final row" is a
clamp its author writes explicitly, in the formula, where a reader can see it.**
That is a real thing some methods say, and writing it down is a one-line
`when()` ([Rounding and conditionals](rounding-and-conditionals.md)). What must
never happen is this library applying it silently to a method that did not say
it.

The trace is where a miss stops being opaque — see
[below](#how-a-lookup-traces).

## Exact: a category key names a row

Not every published table buckets a measurement. A method just as often
publishes one row per *variant* — a specimen shape, an apparatus type, a curing
regime — and the row is chosen by which variant is in front of you, not by how
large anything is. That key is a discriminator, not a quantity: no dimension,
no unit, no order. A band table cannot express it.

A key is an enumerator of a **scoped** enumeration you declare, and the table is
a `KeyTable<Key, N>`:

```cpp
enum class LookupExampleShape : std::uint8_t
{
    Cube = 3,
    Cylinder = 7,
    Prism = 11,
    DrilledCore = 13, // deliberately absent from ShapeKeys below -- the miss
};

inline constexpr formula::KeyTable<LookupExampleShape, 3> ShapeKeys {
    LookupExampleShape::Cube,
    LookupExampleShape::Cylinder,
    LookupExampleShape::Prism,
};
```

A scoped enumeration rather than a string, and the deciding question is what
happens when a key is **absent**. Write `"cylindr"` where the table says
`"cylinder"` and you get a clean compile and a formula that reports a domain
error for every specimen, with nothing anywhere saying which key was wrong.
Write `LookupExampleShape::Cylindr` and the language itself stops you at the
offending token, quoting the misspelling and listing the enumerators that do
exist. No library diagnostic beats that, and none is needed to get it. A key's
*type* also names the categorisation, so two tables keyed on `Shape` and on
`Apparatus` cannot be crossed — two tables keyed on strings both accept `"a"`.

An unscoped enumeration and a plain `int` are both refused, because both convert
to and from arithmetic silently, which gives back exactly the property the
scoped enumeration was chosen for.

A repeated key is refused at compile time too, naming both offending keys. It is
not harmless: the second row becomes unreachable, so a correction somebody
entered is silently never selected.

### The key renders as its underlying value, not the enumerator's name

```
exact:         lookup(key 7, key 3 gives 100 %, key 7 gives 97 %, key 11 gives 92 %)
```

`key 7` is `Cylinder`, and the rendering cannot say so. **A C++ enumerator has
no name at run time** — there is no portable way to get `Cylinder` back out of a
`LookupExampleShape` — so what a reader is given is the underlying value, which
is the only thing that survives. It is the *value*, not the row's index: an
author who numbers theirs `{ Cube = 3, Cylinder = 7 }` -- as the example
above does, for exactly this reason -- sees 3 and 7, numbers that appear in
their own source and nowhere in a row count.

**So a reader reconciling a rendered exact lookup against a published table has
to carry your `enum class` declaration across.** `key 7` says which row, not
which variant. Per-row labels are deliberately not modelled on the node — a
table's identity is `documented()`'s job, not a field smuggled into the
arithmetic — so if you publish a rendering of an exact lookup, publish the
enumeration next to it.

### Give each translation unit's key enumeration a name of its own

A key enumeration declared in an anonymous namespace — the ordinary way to write
one in a `.cpp` — has internal linkage, so `KeyTable<Shape, N>` in two such
files names two *different* types. Clang spells an anonymous namespace
`_GLOBAL__N_1` with no per-translation-unit discriminator, so if two files'
enumerations share a **name** *and* their tables share their **element values**,
both template parameter objects mangle to one name, land in one COMDAT group,
and the linker keeps one and points the other file's reference into a discarded
section.

This is worth a paragraph because of how it fails: **a link error with a
dangling relocation, naming a mangled symbol and not your key type, from a
program that compiled without a single diagnostic.** Measured on clang 20.1.8;
cl 19.51, clang-cl 22 and g++ 14.2 all link it without complaint, so it is the
kind of defect that appears on one leg of a build matrix and nowhere else.

Two things end it, and both cost nothing:

- give each translation unit's key enumeration a **distinct name** — this page's
  example calls its own `LookupExampleShape` rather than `Shape` for exactly
  this reason; or
- declare the enumeration in a **header**, where external linkage makes it one
  type and one specialisation, and the question does not arise.

### Where the key comes from

An `ExactLookupNode` has **no operand**. A category is not a quantity, and an
`Environment` carries measurements and nothing else, so there is no channel
through which a discriminator could arrive at evaluation time. The key is
runtime state on the node, handed to the factory — which means a formula whose
key varies per specimen is a *function of the key*:

```cpp
[[nodiscard]] constexpr auto shapeFactor(LookupExampleShape shape)
{
    return formula::exact_lookup<ShapeKeys, unit::Percent>(shape, { rat(100), rat(97), rat(92) });
}
```

A node is a cheap aggregate, so that is one build per specimen, not one
evaluation per specimen.

## Interpolating: a value between two rows

A published curve states a value at a handful of key points, and a specimen
measured between two of them is not "in a row": the method means the value those
two rows imply at that point. A band table cannot express that, because a band
stores one correction for its whole interval. This kind computes.

The table is **points, not intervals** — one key per row, and the interval
between two rows is implied by the rows rather than declared:

```cpp
inline constexpr formula::BreakpointTable<3> SizeCurve {
    formula::breakpoint(100),
    formula::breakpoint(150),
    formula::breakpoint(200),
};
```

so it renders as the points it is, with `at` rather than any interval wording:

```
interpolating: interpolate(d, at 100 mm gives 95 %, at 150 mm gives 100 %, at 200 mm gives 105 %)
```

and between two rows it produces a number that appears in neither:

```
d = 120 mm:    97/100 (between two rows -- in neither of them)
```

Every step of that is `Rational`'s own checked arithmetic — `y0 + (x - x0)(y1 -
y0)/(x1 - x0)` with no rounding anywhere — so a result no finite decimal can
hold comes back as the exact fraction it is. The only way the answer is not
exact is that some intermediate leaves `Rational`'s representable range, and
that is reported as `ArithmeticError::Overflow` rather than approximated away.

A table's well-formedness here is that its breakpoints **strictly ascend**. Two
rows at one key state two values at one key and leave a zero-width segment to
divide by; two rows out of order make "between these two rows" meaningless. One
rule, one diagnostic, refused at compile time and naming both offending rows.

### The two domains deliberately disagree at the top, and nobody should harmonise them

**A band table's top bound is excluded. An interpolating table's last
breakpoint is included.** Evaluated at the very same 200 mm:

```
d = 200 mm:    argument outside the domain of the operation
```
```
d = 200 mm:    21/20 (the last row, reached -- where the band table missed)
```

This is not an inconsistency and it is not an oversight. A band's high bound is
**shared with the next band's low bound**, and a value sitting on it has to
belong to exactly one of the two. A breakpoint is not a boundary between rows —
**it is a row**: the table states a value there. So excluding the last
breakpoint would make the table's own final row unreachable, and the curve would
answer for every input except the one it states most directly.

A test evaluates both kinds at that same number and asserts they differ, so
"tidying up" the two into agreement fails there rather than quietly in somebody's
method.

A value exactly on a row returns that row and is not interpolated. That is
observable in exactly one place — the last row, which begins no segment at all —
because interpolating across a segment a row begins would return that row's own
value anyway, at weight zero.

### There is no extrapolation

```
d = 220 mm:    argument outside the domain of the operation (no extrapolation past the last row)
```

Interpolation between two rows yields a value the table's author *implied*.
Running the final segment's slope onwards yields one nobody implied — invented
from the slope of wherever the table happened to stop. Clamping to the final row
invents a different one. Both are the lie the whole "a miss is not a value"
section refuses.

An **empty** table is valid and always misses; a **one-row** table is valid and
answers at exactly its own key and nowhere else. Neither is malformed; both are
merely narrow, and a table that leaves part of the domain undefined is something
this library can say honestly.

## A lookup inside another lookup

A lookup's operand is an ordinary expression, so it can be another lookup. This
is a real published-method shape rather than a curiosity: a curve produces a
continuous factor, and a second table buckets that factor into the class the
method actually applies.

```cpp
[[nodiscard]] constexpr auto classFactor()
{
    return formula::banded_lookup<unit::Percent, ClassBands, unit::Percent>(sizeCurveFactor(),
                                                                            { rat(95), rat(100), rat(105) });
}
```

It composes on every surface at once. It renders, nesting the inner call where
the operand goes:

```
nested:        lookup(interpolate(d, at 100 mm gives 95 %, at 150 mm gives 100 %, at 200 mm gives 105 %), 90 to under 100 % gives 95 %, 100 to under 110 % gives 100 %, 110 to under 120 % gives 105 %)
```

it evaluates, the inner answer becoming the outer key:

```
d = 120 mm:    19/20 (curve gives 97 %, which falls in the 90-to-under-100 % band)
```

it documents, the symbol table reaching through both tables to the one quantity
underneath:

```
nested symbols: 1
```

and it traces, each kind naming the row it answered from:

```
1. d = 120 mm
2. interpolate(#1) = 97 % [between 100 and 150 mm]
3. lookup(#2) = 95 % [90 to under 100 %]
```

## How a lookup renders

A band is written `<low> to under <high>`, and that is **the** spelling of a
half-open interval everywhere in this library — in the plain rendering, in
Markdown, in LaTeX, and in the trace.

**It is spelled that way because the obvious mathematical notation is Markdown
link syntax.** Phase 8 of this project rendered a rounding step as
`round[to 1 dp of mm](d)`; in CommonMark that is `[text](url)`, and renderers
silently dropped the operand and published a broken line. A test now asserts
that no Markdown rendering contains `](` or a bare `[`, and a bracketed interval
is exactly the character sequence that would defeat it. The wording chosen
carries no punctuation at all, so it survives every Markdown flavour untouched:

```
banded (md):   lookup(`d`, 0 to under 100 mm gives 95 %, 100 to under 150 mm gives 100 %, 150 to under 200 mm gives 105 %)
```

(The backticks around `d` are Markdown's, marking the symbol as code; the rows
are the same bytes the plain rendering produced.) `examples/lookup_tables.cpp`
checks that this rendering contains no `[` at all, rather than trusting it.

An interpolating table renders its rows with `at`, never with interval wording,
for the reason the section above gives: a rendering that spelled a breakpoint as
an interval would claim the table said something it does not, and at the last row
it would exclude the one key the table states most directly.

The three kinds share one call shape — `<name>(<what is looked up>, <row>,
<row>, …)` — the shape `round(d, to 1 dp of mm)` already has. Every field is a
self-describing clause rather than a positional argument whose meaning a reader
has to know, and the top-level separator is a comma, which means nothing to any
Markdown flavour, where `[`, `]`, `{` and `}` all do. An empty table renders as
`no rows` rather than as a complete-looking call with the table silently absent.

All three also render in LaTeX. The [Gallery](gallery.md) shows each kind
typeset, alongside a worked derivation that uses all three in one expression.

## How a lookup documents

A table is the part of a method that carries a source, so a citation on a lookup
is the normal case, not the exotic one. `documented()` wraps a lookup exactly as
it wraps anything else — there is nothing special to do:

```cpp
[[nodiscard]] constexpr auto correctedStrength(LookupExampleShape shape)
{
    return formula::documented(var<MeasuredStrength> * sizeFactor() * shapeFactor(shape),
                               { .title = "Corrected compressive strength",
                                 .reference = "Example Standard 8:2020",
                                 .section = "7.3",
                                 .equation = "(5)",
                                 .text = "The measured strength is corrected for specimen size and for specimen "
                                         "shape, each factor taken from the table the method publishes for it." });
}
```

`document()` walks through all three kinds — the nested example above documented
an interpolating one — collecting the citation and the symbol table. Here it is
over a banded and an exact lookup inside one formula:

```
method:        f_m * lookup(d, 0 to under 100 mm gives 95 %, 100 to under 150 mm gives 100 %, 150 to under 200 mm gives 105 %) * lookup(key 7, key 3 gives 100 %, key 7 gives 97 %, key 11 gives 92 %)
cited:         Corrected compressive strength, Example Standard 8:2020, 7.3 (5)
symbol:        f_m = measured compressive strength [MPa]
symbol:        d = specimen diameter [mm]
```

Two symbols — `f_m` and `d` — for a formula holding two tables. The banded
lookup contributes its operand's symbol; the exact lookup contributes none at
all, because it reads no quantity: its row is chosen by a key, which is data on
the node rather than a sub-expression.

## How a lookup traces

A lookup gets a step of its own, and the step ends in a bracketed clause saying
where the answer came from — the band a value fell in, or the two rows an
interpolation drew on:

```
1. f_m = 40 MPa
2. d = 120 mm
3. lookup(#2) = 100 % [100 to under 150 mm]
4. #1 * #3 = 40000000
5. lookup(key 7) = 97 %
6. #4 * #5 = 38800000
7. #6 = 38800000 [Corrected compressive strength, Example Standard 8:2020, 7.3, (5)]
```

The exact lookup on line 5 adds no such clause, and that is right: its key is
already the subject of the line, and the key *is* the row.

An interpolating lookup has **two** such clauses rather than one, and they say
genuinely different things. Between two rows:

```
1. d = 120 mm
2. interpolate(#1) = 97 % [between 100 and 150 mm]
```

and on a row:

```
1. d = 200 mm
2. interpolate(#1) = 105 % [on the row at 200 mm]
```

The first tells a reader there is an interpolation to check and that the answer
appears in neither named row; the second tells them the table stated that
number directly and there is nothing to check. Neither spelling is an interval:
`between 100 and 150 mm` names two rows and claims nothing about either end
being included or excluded, so it needs neither a band's `to under` nor a
curve's closed `to`.

Both lookup steps report in the unit their own table is stated in — `100 %`,
`97 %` — while lines 4 and 6 report the products in coherent SI, because an
intermediate that no quantity declares a unit for has none to be shown in. That
is ordinary trace behaviour rather than anything to do with tables; see
[Tracing and audit trails](tracing.md).

**On a miss, that clause is what keeps the line from lying:**

```
1. d = 200 mm
2. lookup(#1) = argument outside the domain of the operation [in no band; the bands cover 0 to under 200 mm]
```

All three kinds report every failure through one error channel, so a lookup step
carrying `DomainError` is ambiguous on its face between "the value fell in no
band" and "something below me failed and I am relaying it". Without the clause,
the line would read as a domain error for a case where nothing was outside any
domain and the real failure happened two levels down. So the clause says which:
a miss names what the table actually covers, a relayed failure says
`carried up from` and names the operand, and an interpolation that overflowed on
its own arithmetic says so rather than blaming anything beneath it.

### Tracing a miss

`formula::explain()` goes through the **throwing** `evaluate()`. A miss is an
ordinary outcome for a lookup rather than a defect, so explaining a formula that
misses throws `formula::ArithmeticException` — carrying
`argument outside the domain of the operation` — instead of handing back the
derivation that says why it missed. (Measured, with the other direction as a
control: the same `explain()` call over a value the table *does* cover returns
normally.) Build the sink yourself:

```cpp
template <typename Result, typename N, typename Env>
[[nodiscard]] std::string tracedEvaluation(N const& node, Env const& environment)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    [[maybe_unused]] auto const outcome = formula::checked_evaluate<Result>(node, environment, sink);
    return formula::render_trace(trace, { .maxSteps = 10 });
}
```

`checked_evaluate` reports the miss in its return value and leaves you the
trace, which is the whole point of having one.

## One representation: `Rational`

All three kinds refuse to compile for any representation but `Rational`, and the
reason differs per kind rather than being copied across.

Deciding which band a value falls in is exactly the operation binary floating
point is unreliable at: a value a few ULPs off an intended boundary picks the
wrong band, silently, with no error to report. An interpolating lookup has that
same ground and a stronger one of its own — it *computes*, so a representation
that rounds hands back a number that is not the one the rows imply.

Deciding whether two enumerators are the same is not arithmetic at all and would
be exact in any representation; what closes `Rep` for the exact lookup is the
lookup family speaking with one voice, so that a formula containing a table
evaluates in `Rational` full stop, rather than in whichever representation
happens to be legal for the kind that got used.

`checked_evaluate<Result>` always computes in `Rational` internally, so this
restriction is never reached from the entry point you actually call.

## Every citation here is invented

Every table, threshold and citation on this page and in
`examples/lookup_tables.cpp` is made up — generic physics with fictional
`Example Standard` references, never a real one, for the reason
[Citations and rendering](citations.md) gives in full: a real test method's
clause numbers, thresholds and table values are copyrighted material, and this
is a public repository.
