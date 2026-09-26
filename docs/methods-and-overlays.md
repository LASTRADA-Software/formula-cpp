# Methods and jurisdiction overlays

A test method often reports one quantity by more than one formula: the
compressive strength of a cube, a cylinder and a prism are three different
expressions for the same thing. Which formula applies is a property of the
specimen, not of a number in the environment, and it has to be *declared*
rather than hidden in an `if` the library never sees. An `if` on specimen shape
is control flow no trace can report, so an inspector asking *"why the cylinder
formula?"* would get no answer.

The same method then varies by jurisdiction. The core algebra stays the same
everywhere, but one national annex fixes a constant the base standard leaves
open, another computes it, a third replaces a formula outright or drops a
variant it does not use. formula-cpp models each jurisdiction as an **overlay**:
a declared list of changes, applied to a method, that yields a method.

The worked example is `examples/methods_and_overlays.cpp`. The page holds two
kinds of quoted block. **Program output** is copied verbatim from that
program's actual output, and `docs.methods-and-overlays-output` fails unless
each of these blocks is a run of consecutive lines the program prints, exactly
as quoted (`cmake/CheckGuideOutput.cmake`). **Compiler diagnostics** -- the blocks
opening `static assertion failed` -- are the library's refusals as g++ 13.3
printed them, captured from this repository's negative tests.

**Code** is copied from the example's source, and
`docs.methods-and-overlays-snippets` fails unless each code block appears there
as a run of consecutive lines, compared without their indentation
(`cmake/CheckGuideSnippets.cmake`). A code block that is deliberately *not*
from the example -- a misuse shown in order to say what happens -- would carry
a `<!-- snippet: not from the example -->` comment directly above it, which
the check skips. No code block on this page carries one.

## A method: variants, tags, and one rounding rule

A method is built from three parts, always in this order: the variants, the
rounding rule, and the constraints.

```cpp
inline constexpr auto compressiveStrength = formula::method(
    formula::variants(formula::variant<Prism>(var<Force> / (var<EdgeA> * var<EdgeA>)),
                      formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeA> * var<EdgeB>)),
                      formula::variant<Cylinder>(formula::constant<unit::One>(rat(4)) * var<Force>
                                                 / (formula::pi * formula::pow<2>(var<Diameter>)))),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
```

- **A tag** such as `Cube` is an empty struct, never instantiated, and it need
  not even be complete. What a variant applies to is a *type*, so selecting
  one is a compile-time fact rather than a string nobody checks.
- **The variants** are expressions of different types. They are held in a
  `std::tuple` rather than an array for that reason. What they must share is
  the dimension they report.
- **The rounding rule** is part of the method, not something applied
  afterwards. It rounds whichever variant is selected.

`evaluate_method<Tag>` selects a variant by tag. The tag is always stated,
never deduced: which variant applies is a property of the specimen, and the
caller says what the specimen is.

```cpp
auto const cube = formula::evaluate_method<Cube>(compressiveStrength, specimen);
```

```text
cube:   6000000 Pa
```

**A method answers in coherent SI**, here pascals, like every `Evaluated`
value in this library. The rule rounded the value in megapascals, to one
decimal (6.00666... MPa became 6.0 MPa), and the answer is that rounded value
expressed in pascals. A method has no typed result. It knows only its variants'
dimension, so its answer is a number in the coherent SI unit of that dimension.

Traced, the selection is a step of its own, the root of the derivation, with
the rounded variant beneath it. The rounding step says whose rule it was:

```text
1. k_s = 1
2. F = 90100 N
3. #1 * #2 = 90100
4. a = 150 mm
5. b = 100 mm
6. #4 * #5 = 3/200
7. #3 / #6 = 18020000/3
8. round(#7, in MPa) = 6 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
9. #8 = 6 MPa [variant Cube (2nd of 3), selected by tag]
```

### Naming a variant as the published method does

A tag is shown under its own name, `Cube`. Where the published method words a
variant differently, specialise `formula::TagName` once for that tag. The shape
is `EnumeratorName`'s, without the argument:

```cpp
template <>
struct formula::TagName<Cylinder>
{
    static constexpr std::string_view of() noexcept
    {
        return "cylinder 150 x 300 mm";
    }
};
```

```text
10. #9 = 51/10 MPa [variant cylinder 150 x 300 mm (3rd of 3), selected by tag]
```

`(3rd of 3)` is the variant's position in the method **as published**. It stays
that way after an overlay prunes or pins, so the number refers to the list a
reader can find in the standard -- unless an author reassigns the method's
published layout by hand, which the section on provenance below describes.

## Why `variant<Tag>`, not `when<Tag>`

The design specification sketches the selector as `formula::when<Cube>(expr)`.
This library spells it `variant<Cube>(expr)` because of measured evidence, not
preference. `conditional.hpp` already ships `when(predicate, then, else)`: a
runtime-predicate ternary, a different question with a different arity. When
both were declared in namespace `formula`, the phase-11 spike measured the
following on cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and g++-14
14.2.0 (`method.hpp`'s file comment records the measurement):

- `when<P>(pred, then, else)` binds `P` to the ternary's first template
  parameter and **compiles silently** on all five. An author who believes
  they are naming a tag there gets the ternary instead.
- Every misspelling produces one uninformative diagnostic on all five: clang
  and g++ print "no matching function for call to 'when'", and cl prints
  "C2672: 'formula::when': no matching overloaded function found". It names
  neither concept, because both are in the overload set and both failed.

The spec's own one-argument spelling, `when<Cube>(expr)`, would have resolved
correctly. The departure rests on the silent mis-binding next to it, and on no
diagnostic being able to tell the two apart.

## What is checked, and when

A mistake in a method is caught at compile time wherever the mistake is a fact
about types. Where it is a fact about values, it is not caught, and this page
says so rather than implying otherwise.

**When the method is declared**, a variants pack is refused unless:

- every argument is a variant;
- there is at least one;
- all variants report the same dimension;
- no two declare the same tag;
- every tag is a plain class type.

Two of those refusals, as g++ 13.3 prints them:

```
static assertion failed: formula: two variants of this method measure different dimensions; every variant must report the same quantity, because a method reports one -- the two offending variants appear in this diagnostic as the template arguments of RequireVariantsAgree
```

```
static assertion failed: formula: this method declares two variants for the same tag; a method with two variants for one tag has no answer to which of them applies -- the tag appears in this diagnostic as the template argument Tag of RequireTagDeclaredOnce, and First and Second are the ZERO-BASED positions of the two variants that declare it, so 0 is the first variant
```

A rounding rule whose unit does not measure the variants' dimension is refused
too:

```
static assertion failed: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report; the rule rounds whichever variant is selected, so its unit must measure what every variant measures -- the variants and the rounding rule appear in this diagnostic as the template arguments Vs and Rounding of RequireRoundingRuleMeasuresVariants
```

**When a variant is selected**, a tag no variant declares is refused. A method
that matches nothing has **no result**. There is no fallback variant and no
"first match wins":

```
static assertion failed: formula: this method declares no variant for that tag; a method that matches nothing has no result, so add a variant for it or select a tag it declares
```

**What is not checked:**

- **Completeness.** Whether the method covers every specimen shape is only
  decidable against a list of shapes that someone wrote down, and the library
  has no such list. It checks every *call*: each `evaluate_method<Tag>` names
  a tag, and a tag with no variant does not compile.
- **Overlap of runtime conditions.** Two tags are two types, so a repeated tag
  is always caught. If you choose between formulas with `when()` on a
  measured value instead, nothing checks whether two conditions overlap or
  leave a gap. The number a condition compares against is a runtime value,
  not part of the type. Selecting by tag is what buys the compile-time checks.

## An overlay yields a method

Six operations. **Four of them are said in the trace**, each by a step of its
own naming the jurisdiction's overlay and what it cited: a fixed constant, a
derived quantity, a replaced formula and a jurisdiction's rounding rule.
`document()` marks the first three on the page. **A pin and a prune are not**:
they take no citation, record no step and mark nothing on the page, and the
only sign of either in a trace is the selection's published count -- `(2nd of
3)` for a method now holding fewer variants than three.

| Operation | What it does |
|---|---|
| `with_constant<Q>(value, citation)` | fixes `Q` at `value` (in `Q`'s declared unit) wherever the method uses it |
| `add_derived<Q>(expression, citation)` | defines `Q` by an expression over other inputs wherever the method uses it |
| `replace_variant<Tag>(expression, citation)` | replaces one variant's formula wholesale |
| `pin_variant<Tag>()` | keeps only that variant, making it mandatory |
| `prune_variant<Tag>()` | deletes that variant |
| `with_rounding<U, Places, Mode>(citation)` | replaces the method's rounding rule |

`apply(overlay, method)` returns a new method, a new type built at compile
time. The base method is unchanged, so one base method can carry every
jurisdiction's overlay side by side. Operations apply **in the order the
overlay lists them**, each to the method the previous one produced.

### Fixing a constant, and the unit a jurisdiction reports in

```cpp
inline constexpr auto north =
    formula::overlay(formula::with_constant<ShapeFactor>(rat(97, 100), northConstant),
                     formula::with_rounding<unit::NewtonPerSquareMillimetre,
                                            formula::DecimalPlaces { 2 },
                                            formula::RoundingMode::HalfAwayFromZero>(northRounding));
```

```text
north cube: 5830000 Pa

1. k_s = 97/100 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.1]
2. F = 90100 N
3. #1 * #2 = 87397
4. a = 150 mm
5. b = 100 mm
6. #4 * #5 = 3/200
7. #3 / #6 = 17479400/3
8. round(#7, in N/mm2) = 583/100 N/mm2 [rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, NA.4); nearest, ties away from zero]
9. #8 = 583/100 N/mm2 [variant Cube (2nd of 3), selected by tag]
```

**What "changing the declared unit" means for a method.** The design
specification asks that a jurisdiction can change the unit a result is
declared in. A method's answer is always coherent SI, so what a jurisdiction
changes is the unit it **reports** in, which is the unit of its rounding rule.
The north rounds in N/mm² to two decimals where the base method rounds in MPa
to one, and the trace says whose rule that was.

The reporting unit can change only within the dimension. A rule whose unit
measures anything else is refused, and so is a replacement formula of another
dimension:

```
static assertion failed: formula: this overlay replaces a variant with a formula of a different dimension from the method's; a replacement changes one variant, never what the method reports -- the tag, the method's dimension and the replacement's appear in this diagnostic as the template arguments Tag, Reported and Replacement of RequireReplacementKeepsDimension
```

No unit field exists anywhere to change instead. A `Measured<Q>` is a value in
`Q`'s declared unit and carries no unit of its own ([Quantities and
measurements](quantities.md)), so a number can never disagree with its label.

### Deriving a quantity, replacing a formula, dropping a variant

```cpp
inline constexpr auto south = formula::overlay(
    formula::replace_variant<Cylinder>(
        var<Force> / (formula::constant<unit::One>(rat(3, 4)) * formula::pow<2>(var<Diameter>)), southReplacement),
    formula::add_derived<ShapeFactor>(var<EdgeB> / var<EdgeA>, southDefinition),
    formula::prune_variant<Prism>());
```

A derived quantity is traced as the quantity equal to its definition's step:

```text
south cube: 4000000 Pa

1. b = 100 mm
2. a = 150 mm
3. #1 / #2 = 2/3
4. k_s = #3 = 2/3 [derived by jurisdiction overlay: Example Standard 7:2019 A, A.3]
5. F = 90100 N
6. #4 * #5 = 180200/3
7. a = 150 mm
8. b = 100 mm
9. #7 * #8 = 3/200
10. #6 / #9 = 36040000/9
11. round(#10, in MPa) = 4 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
12. #11 = 4 MPa [variant Cube (2nd of 3), selected by tag]
```

A replaced formula is marked as the jurisdiction's. The selection still counts
the Cylinder as the 3rd of 3, although the Prism published before it is gone --
the position is the published one, not the Cylinder's place in what is left:

```text
1. F = 90100 N
2. 3/4
3. d = 150 mm
4. #3^2 = 9/400
5. #2 * #4 = 27/1600
6. #1 / #5 = 144160000/27
7. #6 = 144160000/27 [replaced by jurisdiction overlay: Example Standard 7:2019 A, A.5]
8. round(#7, in MPa) = 53/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
9. #8 = 53/10 MPa [variant cylinder 150 x 300 mm (3rd of 3), selected by tag]
```

`document()` marks the same things on the page. A fixed quantity's row carries
`fixedValue` and `fixedBy`, and a derived one carries `derivedAs` (the
definition, rendered in the page's dialect) and `derivedBy`. Every replacement,
cited or not, is listed in `Documentation::replacedBy`. The south's cube, as
the example prints its page:

```text
documentation of the south's cube:
  k_s * F / (a * b)
  k_s: shape factor -- derived as b / a
  b: second loaded edge
  a: first loaded edge
  F: maximum load at failure
```

A pin keeps one variant, and the east's cube is still `(2nd of 3)`:

```text
east: 1 variant(s) left after the pin
```

### The provenance is the library's to state

A trace that says "fixed by jurisdiction overlay", "derived", "replaced" or
"(jurisdiction overlay)" is the audit trail, so none of those can be claimed by
hand:

- only an overlay builds the nodes a trace reads "fixed", "derived" or
  "replaced" from;
- only `with_rounding` creates a rule claiming a jurisdiction;
- only `evaluate_method` builds the rounding node a method applies.

Building one of those nodes directly is refused:

```
static assertion failed: formula: only an overlay builds this node; it makes a trace say a jurisdiction fixed a value, defined a quantity or replaced a formula, so one built by hand would say so of something no overlay did -- use with_constant, add_derived or replace_variant in an overlay(...) given to apply; the node appears in this diagnostic as the template argument OverlayNode of RequireOverlayMadeNode
```

What the guard governs is how a rule or node is *created*, not where a copy
travels. A method holding a copy of an overlay's rule is traced as that
overlay's rule, which is true of it. Two public members are documented as
relabellings an author can make on purpose, and nothing refuses them:

- **assigning `Method::rounding`** a method's own rule, which makes a trace say
  `(method default)` of what was a jurisdiction's rule;
- **assigning `Variants::published`** another well-formed layout, which makes a
  trace count variants in that layout -- `(2nd of 3)` means the 2nd of the
  layout the method holds.

Both are explicit acts on public members (`method.hpp` says so where each is
declared). What no author can do is create a rule or node that states a
provenance the library did not give it.

### An override that would do nothing is refused

An overlay is written once per jurisdiction and read by nobody until an
inspector asks why a number came out as it did. An operation that names the
wrong quantity or the wrong variant would change nothing, produce no error,
and leave the base method under a jurisdiction's name. So these are build
errors, judged against the method the overlay **produces**, whatever order its
operations are listed in:

```
static assertion failed: formula: this overlay overrides a quantity that no variant or constraint of the method uses; an override nobody reads would silently do nothing, most likely because it names the wrong quantity -- the quantity appears in this diagnostic as the template argument Q of RequireConstantUsed
```

```
static assertion failed: formula: this overlay derives a quantity that no variant or constraint of the method uses; a definition nobody reads would silently do nothing, most likely because it names the wrong quantity -- the quantity appears in this diagnostic as the template argument Q of RequireDerivationUsed
```

```
static assertion failed: formula: this overlay both pins a variant and prunes one; a pin already keeps exactly one variant, so a prune beside it either removes a variant the pin drops anyway or removes the one it pins -- list the pin alone, or the prunes alone
```

```
static assertion failed: formula: this overlay prunes every variant of the method; a method left with nothing to choose between can never produce a result, so an overlay that removes its last variant is a mistake rather than a jurisdiction -- the last variant's tag appears in this diagnostic as the template argument Tag of RequirePruneLeavesAVariant
```

The two an author meets first are a name that matches nothing, and the same
change listed twice:

```
static assertion failed: formula: this overlay pins or prunes a variant the method does not declare; an overlay that names a variant by mistake would silently do nothing -- the tag appears in this diagnostic as the template argument Tag of RequireOverlayNamesDeclaredVariant
```

```
static assertion failed: formula: this overlay replaces a variant the method does not declare; a replacement that names a variant by mistake would silently do nothing -- the tag appears in this diagnostic as the template argument Tag of RequireReplacementNamesDeclaredVariant
```

```
static assertion failed: formula: this overlay lists the same operation twice; two constants or definitions of one quantity, two pins, prunes or replacements of one variant, or two rounding overrides, leave the first silently doing nothing -- the first of the two appears in this diagnostic as the template argument Operation of RequireOperationListedOnce, and First and Second are the ZERO-BASED positions of the two arguments that list it, so 0 is the first argument
```

A produced method that reads a quantity both where an overlay fixed or derived
it and, elsewhere, straight from the environment is refused as well. A later
operation, a later overlay, or two definitions that read each other can put
such a plain read back:

```
static assertion failed: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from the environment; one formula would evaluate one quantity at two values -- an operation listed after the substitution, a later overlay, or definitions that read each other put the plain use back; the quantity appears in this diagnostic as the template argument Q of RequireSubstitutionEverywhere
```

The full list of refusals, with the reason for each, is `overlay.hpp`'s file
comment. The messages above are g++ 13.3's, copied from the build of this
repository's negative tests. cl and clang print the same library text in their
own frame.

## The jurisdiction set is compiled in; which one applies is data

Every overlaid method is a different type. Every one answers the same
`Evaluated<Rational>`, though, so choosing among them for a sample is an
ordinary `switch` on a runtime value:

```cpp
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
```

```text
jurisdiction 0: 6000000 Pa
jurisdiction 1: 5830000 Pa
jurisdiction 2: 4000000 Pa
```

**Why not an overlay chosen at run time, from a registry?** A fixed constant,
a rounding rule, a pin and a prune are data, and could be chosen at run time.
A **new formula** could not: a replaced variant's formula and a derived
quantity's definition are both expressions, each of its own type. Choosing
either at run time means holding formulas of different types behind one
interface: type erasure, a virtual call per formula. The trace would not survive that for free.
`RecordingSink`'s `entered` and `produced` are member templates on the node
type, and a member template cannot be virtual. So an erased formula would have
to name one concrete sink type, one `Rep` and one vocabulary in its virtual
signature, permanently, for every consumer. The phase-11 spike built that shape
for replacement and recorded its cost; `add_derived`, added later, meets the
same obstacle for the same reason. The rule here follows: the set of
jurisdictions is closed and lives in the type, and which one applies to a
sample is a runtime index. That fits a registry "per customer, per region, per
contract" for everything except a formula nobody compiled.

## The same formula in two jurisdictions' words

The same symbol can name different quantities in different jurisdictions. In
the example the two jurisdictions write the specimen's two loaded edges with
each other's letters, so a page in the wrong words states the wrong formula. A
vocabulary ([Citations and rendering](citations.md#whose-symbols-a-jurisdictions-vocabulary))
resolves every symbol:

```cpp
inline constexpr auto northernWords = formula::vocabulary(formula::renames<EdgeA>("a"), formula::renames<EdgeB>("b"));
inline constexpr auto southernWords = formula::vocabulary(formula::renames<EdgeA>("b"), formula::renames<EdgeB>("a"));
```

```text
north: k_s * F / (a * b)
south: k_s * F / (b * a)

the southern page's symbol table:
  k_s: shape factor
  F: maximum load at failure
  b: first loaded edge
  a: second loaded edge
```

The symbol moves and the meaning stays: in the south, `b` is the first loaded
edge. A trace records symbols **when the method is evaluated**, so the sink
must be given the same vocabulary as the page:

```cpp
formula::Trace<> trace {};
(void) formula::evaluate_method<Cube>(compressiveStrength, specimen, formula::RecordingSink { trace, southernWords });
```

```text
4. b = 150 mm
5. a = 100 mm
```

A vocabulary renames quantities only. A variant's tag (`TagName`) and a lookup
key's enumerator (`EnumeratorName`) have their own customisation traits and are
not renamed by it. The fixed, derived and replaced parts of an overlaid method
follow the vocabulary too: a derived quantity's `derivedAs` is rendered in the
page's words.

## Every citation here is invented

Every citation on this page and in `examples/methods_and_overlays.cpp` names a
fictional `Example Standard`, never a real one. See [the home page](index.md).
