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
rounding rule, and the constraints. The rounding rule is named once, as a
`DecimalRounding`: a unit, how many decimal places of it to keep, and which way
to break ties.

```cpp
inline constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                                     formula::DecimalPlaces { 1 },
                                                     formula::RoundingMode::HalfAwayFromZero };
```

```cpp
inline constexpr auto compressiveStrength = formula::method(
    formula::variants(formula::variant<Prism>(var<Force> / (var<EdgeA> * var<EdgeA>)),
                      formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeA> * var<EdgeB>)),
                      formula::variant<Cylinder>(formula::constant<unit::One>(4) * var<Force>
                                                 / (formula::pi * formula::pow<2>(var<Diameter>)))),
    formula::rounding_rule<tenthMpa>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(47.3_r),
                                             formula::Verdict { "the load at failure is below 47.3 kN" })));
```

- **A tag** such as `Cube` is an empty struct, never instantiated, and it need
  not even be complete. What a variant applies to is a *type*, so selecting
  one is a compile-time fact rather than a string nobody checks.
- **The variants** are expressions of different types. They are held in a
  `std::tuple` rather than an array for that reason. What they must share is
  the dimension they report.
- **The rounding rule** is part of the method, not something applied
  afterwards. It rounds whichever variant is selected.
- **The constraints** are the checks a result must pass to be accepted, as
  [Constraints and verdicts](constraints.md) describes them. `evaluate_method`
  does not check them; `check_method` does, and says whose they are -- see
  [Whose acceptance logic](#whose-acceptance-logic).

`evaluate_method<Tag>` selects a variant by tag. The tag is always stated,
never deduced: which variant applies is a property of the specimen, and the
caller says what the specimen is.

The example calls `explain_method<Tag>`, which is `evaluate_method<Tag>` with a
recording sink. It returns what `evaluate_method` returned in `outcome`, and
the derivation in `trace`, from one evaluation:

```cpp
auto const cube = formula::explain_method<Cube>(compressiveStrength, specimen);
```

```text
cube:   5500000 Pa
```

**A method answers in the coherent unit**, here pascals, like every `Evaluated`
value in this library. The rule rounded the value in megapascals, to one
decimal (5.5477... MPa became 5.5 MPa), and the answer is that rounded value
expressed in pascals. A method has no typed result. It knows only its variants'
dimension, so its answer is a number in the coherent unit of that dimension.

Traced, the selection is a step of its own, the root of the derivation, with
the rounded variant beneath it. The rounding step says whose rule it was:

```text
1. k_s = 1043/1000
2. F = 89300 N
3. #1 * #2 = 931399/10 N
4. a = 163 mm
5. b = 103 mm
6. #4 * #5 = 16789/1000000 m^2
7. #3 / #6 = 93139900000/16789 kg/(m s^2)
8. round(#7, in MPa) = 11/2 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
9. #8 = 11/2 MPa [variant Cube (2nd of 3), selected by tag]
```

Each computed step reads in a unit written after it. The force scaled by the
shape factor, a pure number, keeps the force's newtons. The area and the
stress borrow neither operand's unit, so each reads in the coherent unit of its
dimension, spelt from the base units: `m^2`, and `kg/(m s^2)` for the pascal.

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
        return "cylinder 135 x 271 mm";
    }
};
```

```text
10. #9 = 31/5 MPa [variant cylinder 135 x 271 mm (3rd of 3), selected by tag]
```

A spelling holding a square bracket or a control character is refused, since
a trace line ends in a bracketed clause saying where a value came from, and
such a spelling could imitate one -- see [the provenance
section](#the-provenance-is-the-librarys-to-state). So are two variants of one
method spelt the same, since the line naming the variant that ran could not
say which it was.

`(3rd of 3)` is the variant's position in the method **as published**. It stays
that way after an overlay prunes or pins, so the number refers to the list a
reader can find in the standard -- unless an author copies another pack's
layout into the method's, which the section on provenance below describes.

## Why `variant<Tag>`, not `when<Tag>`

The design specification sketches the selector as `formula::when<Cube>(expr)`.
This library spells it `variant<Cube>(expr)` because of measured evidence, not
preference. `conditional.hpp` already ships `when(predicate, then, else)`: a
runtime-predicate ternary, a different question with a different arity. When
both were declared in namespace `formula`, a measurement made while this
selector was being designed found the following on cl 19.51, clang-cl 22.1.3,
clang++ 20.1.8, g++ 13.3 and g++-14 14.2.0 (`method.hpp`'s file comment records
the measurement):

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
- no two are spelt the same in a trace (a `TagName` can make two tags read
  alike);
- every tag is a plain class type.

Two of those refusals, as g++ 13.3 prints them:

```
static assertion failed: formula: two variants of this method measure different dimensions; every variant must report the same quantity, because a method reports one -- the two offending variants appear in this diagnostic as the template arguments of RequireVariantsAgree
```

```
static assertion failed: formula: this method declares two variants for the same tag; a method with two variants for one tag has no answer to which of them applies -- the tag appears in this diagnostic as the template argument Tag of RequireTagDeclaredOnce, and First and Second are the ZERO-BASED positions of the two variants that declare it, so 0 is the first variant
```

Two tags spelt alike:

```
static assertion failed: formula: two variants of this method are spelt the same in a trace, so a line naming the variant that ran could not say which of them it was -- the tags appear in this diagnostic as the template arguments FirstTag and SecondTag of RequireTagNamesDistinct, and First and Second are the ZERO-BASED positions of their variants; spell them apart with formula::TagName
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

Seven operations, and **every one of them requires a citation and is said in
the trace** with what the jurisdiction cited: a fixed constant, a derived
quantity and a replaced formula each on a step of its own, a jurisdiction's
rounding rule on the rounding step, a pin or a prune on the step that names the
selected variant, and a jurisdiction's constraints beside each verdict (see
[Whose acceptance logic](#whose-acceptance-logic)).

**`document()` marks less than the trace says.** It documents a formula, and
marks on the page only what an overlay put inside one: a fixed, a derived and
a replaced part. A pin, a prune, a rounding rule and a set of constraints are
parts of a method, not of a formula, and no page shows them: there is no
`document()` for a whole method yet. A `document(method)` is future work.

| Operation | What it does |
|---|---|
| `with_constant<Q>(value, citation)` | fixes `Q` at `value` (in `Q`'s declared unit) wherever the method uses it |
| `add_derived<Q>(expression, citation)` | defines `Q` by an expression over other inputs wherever the method uses it |
| `replace_variant<Tag>(expression, citation)` | replaces one variant's formula wholesale |
| `pin_variant<Tag>(citation)` | keeps only that variant, making it mandatory |
| `prune_variant<Tag>(citation)` | deletes that variant |
| `with_rounding<U, Places, Mode>(citation)` | replaces the method's rounding rule |
| `with_constraints(constraints(...), citation)` | replaces the method's constraints wholesale |

`apply(overlay, method)` returns a new method, a new type built at compile
time. The base method is unchanged, so one base method can carry every
jurisdiction's overlay side by side. Operations apply **in the order the
overlay lists them**, each to the method the previous one produced.

An operation given no citation is refused, in words naming the operation:

```
static assertion failed: formula: pin_variant<Tag>() was given no citation; which variant is mandatory is a jurisdiction's decision, and a trace must say whose -- pass the Citation of the clause that makes it, pin_variant<Tag>(citation)
```

### Fixing a constant, and the unit a jurisdiction reports in

```cpp
inline constexpr auto north =
    formula::overlay(formula::with_constant<ShapeFactor>(0.863_r, northConstant),
                     formula::with_rounding<unit::NewtonPerSquareMillimetre,
                                            formula::DecimalPlaces { 2 },
                                            formula::RoundingMode::HalfAwayFromZero>(northRounding));
```

```text
north cube: 4590000 Pa

1. k_s = 863/1000 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.1]
2. F = 89300 N
3. #1 * #2 = 770659/10 N
4. a = 163 mm
5. b = 103 mm
6. #4 * #5 = 16789/1000000 m^2
7. #3 / #6 = 77065900000/16789 kg/(m s^2)
8. round(#7, in N/mm2) = 459/100 N/mm2 [rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, NA.4); nearest, ties away from zero]
9. #8 = 459/100 N/mm2 [variant Cube (2nd of 3), selected by tag]
```

**What "changing the declared unit" means for a method.** The design
specification asks that a jurisdiction can change the unit a result is
declared in. A method's answer is always in the coherent unit, so what a jurisdiction
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
        var<Force> / (formula::constant<unit::One>(1.127_r) * formula::pow<2>(var<Diameter>)), southReplacement),
    formula::add_derived<ShapeFactor>(var<EdgeB> / var<EdgeA>, southDefinition),
    formula::prune_variant<Prism>(southScope));
```

A derived quantity is traced as the quantity equal to its definition's step:

```text
south cube: 3400000 Pa

1. b = 103 mm
2. a = 163 mm
3. #1 / #2 = 103/163
4. k_s = #3 = 103/163 [derived by jurisdiction overlay: Example Standard 7:2019 A, A.3]
5. F = 89300 N
6. #4 * #5 = 9197900/163 N
7. a = 163 mm
8. b = 103 mm
9. #7 * #8 = 16789/1000000 m^2
10. #6 / #9 = 89300000000/26569 kg/(m s^2)
11. round(#10, in MPa) = 17/5 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
12. #11 = 17/5 MPa [variant Cube (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example Standard 7:2019 A, A.1]
```

The last line says the prune: one of the method's three published variants is
gone, and by whose clause.

A replaced formula is marked as the jurisdiction's. The selection still counts
the Cylinder as the 3rd of 3, although the Prism published before it is gone --
the position is the published one, not the Cylinder's place in what is left:

```text
1. F = 89300 N
2. 1127/1000
3. d = 135 mm
4. #3^2 = 729/40000 m^2
5. #2 * #4 = 821583/40000000 m^2
6. #1 / #5 = 3572000000000/821583 kg/(m s^2)
7. #6 = 3572000000000/821583 kg/(m s^2) [replaced by jurisdiction overlay: Example Standard 7:2019 A, A.5]
8. round(#7, in MPa) = 43/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
9. #8 = 43/10 MPa [variant cylinder 135 x 271 mm (3rd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: Example Standard 7:2019 A, A.1]
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

A pin keeps one variant and makes it mandatory:

```cpp
/// The east tests cubes only, so the cube variant is mandatory there.
inline constexpr auto east = formula::overlay(formula::pin_variant<Cube>(eastScope));
```

The east's cube is still `(2nd of 3)`, and the selection says the pin:

```text
9. #8 = 11/2 MPa [variant Cube (2nd of 3), selected by tag; pinned by jurisdiction overlay: Example Standard 3:2023 E, E.1]

east: 1 variant(s) left after the pin
```

One overlay cannot both pin and prune, but one jurisdiction may prune what a
later one pins; the trace then says both, the prune first.

### The provenance is the library's to state

A trace that says "fixed by jurisdiction overlay", "derived", "replaced" or
"(jurisdiction overlay)" is the audit trail, so none of those can be claimed by
hand:

- only an overlay builds the nodes a trace reads "fixed", "derived" or
  "replaced" from;
- only `with_rounding` creates a rule claiming a jurisdiction;
- only `evaluate_method` builds the rounding node a method applies;
- only `variants(...)` states a variant's published position and count, and
  only `apply`, through a pin or a prune, carries them on with what the
  overlay cited.

Building one of those nodes directly is refused:

```
static assertion failed: formula: only an overlay builds this node; it makes a trace say a jurisdiction fixed a value, defined a quantity or replaced a formula, so one built by hand would say so of something no overlay did -- use with_constant, add_derived or replace_variant in an overlay(...) given to apply; the node appears in this diagnostic as the template argument OverlayNode of RequireOverlayMadeNode
```

A published layout written by hand -- `{ { 5, 7 }, 9 }`, which would make a
trace call a variant the 5th of 9 -- is refused, whether as an initialiser or
assigned later:

```
static assertion failed: formula: a variant's published position is stated only by the library -- by variants(...), and carried by apply() through a pin or a prune -- since a layout written by hand could make a trace report a position and a count no method has; build the pack with variants(...)
```

**What is authoritative is the `Step`, not the rendered line.** A trace step
records its provenance in fields only the library sets -- `kind`,
`roundingProvenance`, `constraintProvenance`, `variantPinned`,
`variantPrunedCount` and the citations beside them -- and code that must decide
whose a value was reads those.

**The rendered line is escaped so that author text cannot break its
structure.** Some of the words in a trace line are the author's: a symbol, a
citation, a verdict, a unit's symbol, a variant's tag, a lookup key's name.
`render_trace` escapes every one of them -- `\` as `\\`, `[` as `\[`, `]` as
`\]`, `;` as `\;`, a newline as `\n` and any other control character as `\x`
and two hex digits -- and writes its own clauses as they are, so author text
cannot open or close a clause or end a line. It may still contain any
*words*: a citation titled like a library clause renders like one, and only
`Step::kind` tells them apart (`docs/tracing.md` gives the example). A `TagName`
or `EnumeratorName` spelling goes further: holding a square bracket or a
control character, it is refused at compile time:

```
static assertion failed: formula: this TagName spelling holds a square bracket or a control character (a newline, a tab, any byte below 0x20, or 0x7f); a trace line ends in a bracketed clause saying where a value came from, and a line ends at a newline, so such a spelling could make a trace claim an overlay replaced or fixed something no overlay touched, or add a line that is no step -- the tag appears in this diagnostic as template argument Tag of RequireTagNameSpelling -- spell the tag without them
```

What the guard governs is how a rule, a node or a layout is *created*, not
where a copy travels. A method holding a copy of an overlay's rule is traced as
that overlay's rule, which is true of it. Two relabellings on public members
are documented, and nothing refuses them:

- **assigning `Method::rounding`** a method's own rule, which makes a trace say
  `(method default)` of what was a jurisdiction's rule;
- **copying another pack's layout** into `Variants::published`, or resetting
  it to declaration order with `{}`, which gives the pack a layout the library
  made for another -- positions, count and any pin or prune with its citation.
  A pruned pack reset this way reports its variants as the 1st and 2nd of 2,
  and says nothing of the prune.

Both are explicit acts on public members (`docs/tracing.md` lists every such
route). What no author can do is create a rule, node or layout that states a
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
static assertion failed: formula: this overlay lists the same operation twice; two constants or definitions of one quantity, two pins, prunes or replacements of one variant, two rounding overrides, or two replacements of the constraints, leave the first silently doing nothing -- the first of the two appears in this diagnostic as the template argument Operation of RequireOperationListedOnce, and First and Second are the ZERO-BASED positions of the two arguments that list it, so 0 is the first argument
```

A produced method that reads a quantity both where an overlay fixed or derived
it and, elsewhere, straight from the environment is refused as well. A later
operation, a later overlay, or two definitions that read each other can put
such a plain read back:

```
static assertion failed: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from the environment; one formula would evaluate one quantity at two values -- an operation listed after the substitution, a later overlay, or definitions that read each other put the plain use back; the quantity appears in this diagnostic as the template argument Q of RequireSubstitutionEverywhere
```

A constant or definition that a later operation left with nothing to fix is
refused too, in words that say whether it was bypassed or listed too early --
see [An overlay's acceptance logic among its other operations](#an-overlays-acceptance-logic-among-its-other-operations).

The full list of refusals, with the reason for each, is `overlay.hpp`'s file
comment. The messages above are g++ 13.3's, copied from the build of this
repository's negative tests. cl and clang print the same library text in their
own frame.

## Whose acceptance logic

A method's third part is its constraints: the checks a result must pass before
it is accepted. The base method holds one of its own, `F >= 47.3 kN` (declared
[above](#a-method-variants-tags-and-one-rounding-rule)), and the west
replaces it with two checks of its own:

```cpp
inline constexpr auto west = formula::overlay(formula::with_constraints(
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(97.3_r),
                                             formula::Verdict { "the load at failure is below 97.3 kN" }),
                         formula::constraint(var<EdgeA> <= formula::number(1.73_r) * var<EdgeB>,
                                             formula::Verdict { "the loaded face is more than 1.73 times as long as wide" })),
    westAcceptance));
```

`check_method(m, environment)` checks every constraint the method holds and
answers one `ConstraintOutcome` per constraint, at the index the method holds
it. It never stops at the first failure, for the reason [Constraints and
verdicts](constraints.md) gives:

```cpp
auto const baseCheck = formula::explain_check_method(compressiveStrength, specimen);
auto const westCheck = formula::explain_check_method(western, specimen);
```

`explain_check_method` is `check_method` with a recording sink: it returns the
outcomes in `outcome` and the derivation in `trace`, from one run. Here are the
outcomes:

```text
base: 1 constraint(s)
  [0] satisfied
west: 2 constraint(s)
  [0] violated: the load at failure is below 97.3 kN
  [1] satisfied
```

**`with_constraints` replaces the constraints, it does not add to them.** The
west's method does not check the base method's 47.3 kN. A jurisdiction that
keeps a base check restates it in its own set, with its own citation. An
overlay can also leave fewer constraints than the method had, or none:
`with_constraints(formula::constraints(), citation)` is accepted, because a
jurisdiction that checks nothing is a position it can state and cite.

### Whose each verdict is

`constraint_origin(m)` answers whose constraints a method holds, and what the
overlay that supplied them cited:

```cpp
formula::ConstraintOrigin const origin = formula::constraint_origin(m);
if (origin.provenance() == formula::ConstraintProvenance::MethodOwn)
    return "the method's own";
```

```text
base constraints: the method's own
west constraints: jurisdiction overlay, Example Standard 9:2022 B, B.2
```

The trace says the same beside every verdict. `check_method` records the
verdicts under an `acceptance` step of their own, in the order it returns
them:

```text
1. F = 89300 N
2. 473/10 kN
3. require #1 >= #2 [satisfied; the method's own constraint]
4. acceptance(#3) [the method's own constraints]
```

```text
1. F = 89300 N
2. 973/10 kN
3. require #1 >= #2 [the load at failure is below 97.3 kN; jurisdiction overlay: Acceptance, Example Standard 9:2022 B, B.2]
4. a = 163 mm
5. 173/100
6. b = 103 mm
7. #5 * #6 = 17819/100 mm
8. require #4 <= #7 [satisfied; jurisdiction overlay: Acceptance, Example Standard 9:2022 B, B.2]
9. acceptance(#3, #8) [jurisdiction overlay: Acceptance, Example Standard 9:2022 B, B.2]
```

A method with no constraints still gets its `acceptance` line --
`acceptance(none)`, with whose it is -- so a jurisdiction that removed every
check is never silent about it. Step 7 is `1.73 x 103 mm`, a length scaled by a
pure number, so it reads in the length's millimetres, `17819/100 mm`, as the
force scaled by the shape factor in the cube's trace above reads in newtons.

The constraints carry whose they are with them: `with_constraints` puts an
`OverlaidConstraints` in the method -- the jurisdiction's set together with
its citation -- and `check_method` and `constraint_origin` read it from there.
So a method built from an overlaid method's parts,
`formula::method(o.variantSet, o.rounding, o.constraintSet)`, still checks the
jurisdiction's constraints and still says so. `check_all` takes a plain
`ConstraintSet`, which an overlaid method's `constraintSet` is not; call
`check_method` to check a method's constraints.

Building an `OverlaidConstraints` by hand is refused:

```
static assertion failed: formula: whose a method's constraints are is the library's to state, not an author's; method(..., constraints(...)) declares the method's own, and with_constraints(...) applied by an overlay a jurisdiction's
```

Two things are not refused, and are relabellings an author makes on purpose:

- **building a method from the plain set**:
  `formula::method(o.variantSet, o.rounding, o.constraintSet.constraintSet())`
  checks the jurisdiction's constraints as the new method's own;
- **explicitly specialising `OverlaidConstraints`** for a predicate over a
  quantity of one's own, which can then claim any citation.

Both compile without a diagnostic on cl 19.51 at `/W4`, and on g++ 13.3 and
clang++ 20.1.8 at `-Wall -Wextra` (measured with a probe written for this
page, not a test of this repository).

### An overlay's acceptance logic among its other operations

`with_constraints` is an operation like the others, applied in the order the
overlay lists it. An operation listed **after** it reaches inside the new
constraints: a `with_constant<Q>` listed after it fixes `Q` in the
jurisdiction's constraints as well as in the variants
(`test/overlay_tests.cpp`, "a constant listed after an overlay's constraints
reaches inside them"). A constant listed **before** it does nothing for the
new constraints, and an overlay whose new constraints then read `Q` straight
from the environment is refused. Which refusal depends on what the constant
met where it is listed. If it fixed `Q` in the method's own constraints,
which `with_constraints` then discarded, it was bypassed:

```
static assertion failed: formula: this overlay fixes a quantity, and an operation listed after the constant removed every use it fixed and put back one that reads the quantity from the environment; the method it produces reads the quantity only unsubstituted, so the constant does nothing -- list the constant after that operation; the quantity appears in this diagnostic as the template argument Q of RequireConstantNotBypassed
```

If nothing read `Q` where the constant is listed, it came too early:

```
static assertion failed: formula: this overlay fixes a quantity that nothing read where the constant is listed, and an operation listed after it reads the quantity from the environment; the method it produces reads the quantity only unfixed, so the constant does nothing -- list the constant after that operation; the quantity appears in this diagnostic as the template argument Q of RequireConstantPrecedesItsUse
```

If the new constraints do not read `Q` either, and nothing else in the method
does, the constant is refused as an override nobody reads. `add_derived` has
the same three refusals, in its own words. Either way the fix is to list the
constant after the constraints it is meant to reach. The messages are g++
13.3's, from `overlay_constant_before_constraints_read_plainly` and
`overlay_constant_never_read_then_constraints_read_it` in `test/negative/`.

### What constraints cannot yet say

Two limits, stated here so that nobody mistakes them for supported cases:

- **A constraint cannot yet judge the specimen's own category.** A category
  code is written into a constraint through `exact_lookup`, which takes its
  key as a value, not from the environment. So a jurisdiction that accepts,
  say, square specimens only can write a constraint that judges *one fixed*
  category, the same for every specimen, but not one that looks up the
  category of the specimen in front of it. The verdict does name the category
  by its enumerator (`test/overlay_tests.cpp`, "an overlay's constraint judges
  a category code, and its verdict names the category").
- **Stacked overlays: the later overlay's constraints hold.** Applying a
  second overlay to an overlaid method replaces the constraints again,
  wholesale. The earlier overlay's constraints are gone, not merged, and
  `constraint_origin` names only the later citation:

```cpp
/// A later revision of the west's annex, applied on top of the west's method:
/// its one constraint is all the stacked method checks.
inline constexpr auto westRevised = formula::overlay(formula::with_constraints(
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(83.1_r),
                                             formula::Verdict { "the load at failure is below 83.1 kN" })),
    westRevision));

inline constexpr auto western = formula::apply(west, compressiveStrength);
inline constexpr auto westernRevised = formula::apply(westRevised, western);
```

```text
west, revised on top: 1 constraint(s)
  [0] satisfied
revised constraints: jurisdiction overlay, Example Standard 9:2025 B, B.2
```

A later overlay's `with_constraints` may also discard every place an earlier
overlay's constant or definition had fixed a quantity. That is accepted as
"the later overlay holds", although the same two operations inside **one**
overlay are refused, as described above.

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
jurisdiction 0: 5500000 Pa
jurisdiction 1: 4590000 Pa
jurisdiction 2: 3400000 Pa
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
signature, permanently, for every consumer. An experiment made while overlays
were being designed built that shape for replacement and recorded its cost;
`add_derived`, added later, meets the same obstacle for the same reason. The rule here follows: the set of
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
edge. A trace records symbols **when the method is evaluated**, so the trace
must be recorded with the same vocabulary as the page:

```cpp
auto const southernRun = formula::explain_method<Cube>(compressiveStrength, specimen, southernWords);
```

```text
4. b = 163 mm
5. a = 103 mm
```

A vocabulary renames quantities only. A variant's tag (`TagName`) and a lookup
key's enumerator (`EnumeratorName`) have their own customisation traits and are
not renamed by it. The fixed, derived and replaced parts of an overlaid method
follow the vocabulary too: a derived quantity's `derivedAs` is rendered in the
page's words.

## Every citation here is invented

Every citation on this page and in `examples/methods_and_overlays.cpp` names a
fictional `Example Standard`, never a real one. See [the home page](index.md).
