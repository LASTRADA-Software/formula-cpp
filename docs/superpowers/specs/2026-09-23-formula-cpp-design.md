# formula-cpp — design specification

**Status:** draft, ready for review · **Date:** 2026-09-23 · **Owner:** Christian Parpart

> Every design claim in §13 was verified by compiling and linking on MSVC 19.51, clang-cl 22.1.3
> and clang++ 22.1.3. §16 states requirements established by reading real test standards in their
> calculation clauses; those documents are copyrighted and held privately, and none of their content
> is reproduced here. Nothing is asserted from memory; where something is unverified it says so.

---

## 1. Purpose

`formula-cpp` is a header-only C++23 library for expressing calculations **declaratively**: a
formula is written once with ordinary operators, carries its own documentation and provenance,
is checked for dimensional consistency at compile time, and can be evaluated, traced, rendered
and documented without being rewritten.

It exists to be the foundation for a reusable set of libraries implementing published test
standards for LASTRADA's lab QA software — asphalt/bitumen first, then concrete, cement, aggregates
and geotechnics. Those norm libraries are separate downstream projects. This one ships only the
machinery.

The shape the owner asked for:

```cpp
template <typename WaterVol, typename CementVol>
constexpr auto water_cement_ratio = formula::Value(WaterVol) / formula::Value(CementVol);
```

Declarative, parametrisable, composable — and from that single declaration you can get a number,
an audit trail, a LaTeX rendering and a documentation page.

## 2. Goals

1. **Declarative.** A formula is a value you can name, store and pass around *before* any data
   exists. Not a by-product of computing.
2. **Parametrised.** A formula is a variable template over the quantity types filling its slots.
3. **Composable.** A formula is usable as an input to another formula, and citations stay
   reachable from the composed root.
4. **Traceable.** Evaluation can produce an auditable derivation a human inspector can follow.
5. **Self-documenting.** Every formula carries title, description and provenance (norm, edition,
   section, page, equation) as compile-time data.
6. **Dimensionally checked.** Wrong physics is a compile error, with a readable message.
7. **Exact where it matters.** Norm rounding rules are arithmetic, not formatting.

## 3. Non-goals and hard constraints

- **Open source, no company IP.** Apache-2.0, matching morph and Lightweight.
- **No norm content in this repository.** Standards are copyrighted and sold. Nothing from one
  appears here: not their text, tables, equations, threshold or constant values, and not their
  identifiers, clause or table numbers — not even as a bare citation. Public examples use
  **generic physics only** (density, flow rate) with fictional `Example Standard` citations. Real
  norm content lives in downstream, possibly closed, libraries.
- **No third-party dependencies in the core.** Specifically **no glaze**. Serialization belongs
  to the consumer: morph adds its own `glz::meta` specialisations for our types.
- **No macros for traceability.** (§11.)
- **Compilers:** MSVC `cl`, `clang-cl`, `clang++`. GCC welcome if free.
- **Must compile as C++23 everywhere.**

## 4. Dependency direction

```
Lastrada  ──►  morph  ──►  formula-cpp  ──►  (nothing)
```

`formula-cpp` depends on nothing. `morph` will eventually **retire** its own rational, quantity and
equation types and use ours instead. `Lastrada` consumes formula-cpp via vcpkg (directly today,
transitively through morph later).

This inverts the usual instinct and it constrains v1: the downstream codebase cannot drop its types
unless ours **replace** them, so formula-cpp must *ship* an exact rational and a quantity, not
merely accept one. See §15 for the migration.

## 5. Architecture

Six layers, each usable without the ones above it.

| Layer | Responsibility | Depends on |
|---|---|---|
| **Numbers** | exact rational arithmetic, decimal places, rounding modes | — |
| **Dimensions** | exponent vector over SI base dimensions; the algebra consumers otherwise supply themselves | — |
| **Units** | unit descriptors, exact conversion, display precision, bounds | numbers, dimensions |
| **Quantities** | a typed, documented, optionally-empty value | units |
| **Expressions** | the symbolic formula tree, operators, composition, citations, constraints | quantities |
| **Interpretation** | evaluation, tracing, rendering, documentation generation | expressions |

Above these, **and inside this library**, sits a **method** layer — the unit a lab actually runs:
a set of formulas and constraints over a sample, with variant selection (apparatus, geometry,
procedure), rounding rules and jurisdiction overrides. §16 shows why this cannot be modelled as one
formula per result, and §9.1 states why it belongs here rather than in each norm library.

The load-bearing idea: **the expression tree is a type, known at compile time, independent of
data.** Everything in the top layer is a traversal of that type. That is what makes
parametrisation, documentation-without-data and compile-time checking possible, and it is the
single deepest difference from the runtime-AST approach of the prior scaffold (§14).

## 6. Numbers

The default representation is an **exact rational** with an explicit decimal-place tag and
rounding mode, carrying no serialization, logging or payload-shape-tagging dependency of its own.

Rationale: norm rounding rules are *specified behaviour*, not presentation. "Round the result to
0.1 %" is part of the method. `double` cannot express that faithfully, and it makes exact
round-tripping impossible — 450 l stored as 0.45 m³ and rendered back is not reliably 450.

`double` remains available as an opt-in representation. The expression layer is
**representation-agnostic**: `Rep` is chosen at evaluation, not baked into the tree (§9).

**Rule:** unit conversion applies exact integer factors by multiply-then-divide, never a
precomputed floating-point factor. `30 MPa → 30 × 1'000'000 / 1 = 3e7 Pa` exactly, and back.

## 7. Dimensions

A structural exponent vector over the seven SI base dimensions, usable as a non-type template
parameter. **Verified working on all three compilers, including cross-TU mangling** (§13).

```cpp
struct Dimension
{
    Exponent length, mass, time, current, temperature, amount, luminosity;
    constexpr bool operator==(Dimension const&) const noexcept = default;
};
```

Exponents are **rational**, not integer. Driver: norm formulas take roots and fractional powers
(`sqrt(A) → length`, and pressure to the 2/3 appears in structural concrete work). Integer
exponents cannot express the intermediate results at all.

Users never spell exponents; they compose named constants (`dim::volume = length³`,
`dim::density = mass / volume`). That keeps the representation swappable.

**Dimension mismatch is a `static_assert` inside a named helper template**, not a constrained
operator and not a deleted overload. Measured rationale in §13: the helper makes both compilers
print the offending exponent vectors, and it keeps the diagnostic's wording ours.

## 8. Units, quantities and metadata

A **unit** carries its dimension, an exact conversion magnitude, an optional affine offset (°C),
a display symbol, a **default decimal precision**, and optional **validity bounds** — the last
two because standards specify both.

A **quantity type** is the identity of a variable in a formula. The declared spelling, chosen
after compiling all three candidates (§13):

```cpp
struct WaterVolume:
    formula::Quantity<WaterVolume,                         // tag: the type's own name
                      "V_w",                               // symbol
                      "volume of the effective mixing water",
                      formula::unit::Litre>                // dimension comes from the unit
{
};
```

**Amended 2026-09-24, before phase 4 was planned.** This section originally passed the dimension
as a fifth parameter, alongside the unit. A spike compiled all three candidate spellings on cl,
clang-cl and clang++ and measured what each does when the two disagree:

| Spelling | A quantity declared `dim::Mass` with `unit::Litre` |
|---|---|
| dimension and unit, unchecked | **compiles silently on all three compilers** |
| dimension and unit, `static_assert` they agree | fails loudly, as intended |
| unit only | cannot be written |

A `Unit` already carries its dimension, so the fifth parameter states it a second time and lets
the two contradict each other. The unchecked spelling reproduces exactly the failure this library
forbids everywhere else — a wrong value with no signal — and even the checked spelling only
detects a mistake that the fourth spelling makes unwritable. The dimension is not lost to a
reader: `Describe<T>` exposes it, derived from the unit, so generated documentation still states
it. Preferring the design where the invalid state cannot be expressed over the one where it is
merely diagnosed.
The CRTP tag **earns its place**, though not for the reason first given here. Distinctness across
translation units is separately verified by linking.

**Corrected 2026-09-24, during phase 4, after a reviewer checked the claim.** This paragraph
originally said that without the tag, two variables whose documentation coincides "collapse into
one type — verified". That is false for the declaration spelling above, and the word *verified* was
doing work nothing had done. C++ types are nominal, so two separately declared `struct`s are
distinct however identical their base. Measured with the tag removed from a stand-in template:

| | |
|---|---|
| two derived structs are the same type | no |
| two **aliases** are the same type | **yes** |
| their bases are the same type | yes |
| a base-taking function accepts both | yes |

So the tag buys two narrower things, both real: it makes the alias spelling impossible to get wrong
— `using A = Quantity<…>; using B = Quantity<…>;` with identical arguments silently gives one type,
and a tag cannot be omitted without noticing — and it keeps the **bases** distinct, so nothing that
takes or dispatches on the base can confuse two quantities. The conclusion stands; the argument for
it did not.

**Amended 2026-09-24, at the final review of phase 4.** This section also claimed a one-line macro
spelling (`FORMULA_QUANTITY(...)`) was "also provided" and "verified to link across translation
units". Phase 4 provides no such macro, deliberately: the standing requirement is traceability
without macros, and a convenience spelling for quantity declaration would be this library's only
one. Nothing was ever built or verified under that name; the sentence was aspirational when written
and should have been removed when the plan dropped the macro, rather than left asserting a fact
about the code that is false. If a macro spelling is ever wanted it is future work, decided on its
own merits against the no-macros default — not something already delivered.

All metadata is read through a **single access point**, `formula::Describe<T>`, whose primary
template detects the CRTP base and can be explicitly specialised for foreign types you do not own
(`double`, a Lastrada type, a vendor SDK type). Nothing above this layer knows how metadata was
declared.

## 9. Expressions

Node types: variable, binary op, unary op, function call, constant, documented-wrapper,
conditional, and the series nodes of §12.

Structure nodes are **empty** — all information is in the type — so the whole tree is a compile-time
entity and traversals cost nothing at runtime. `Constant` and the documentation payload carry
`constexpr` state, because a coefficient may come from a runtime table lookup and a citation is
data.

Each node exposes `static constexpr Dimension dimension`, computed at class scope, so a
dimensional error fires **where the formula is written**, not where it is evaluated.

Inputs are supplied through a type-keyed environment. A missing input is a `static_assert` naming
the variable, replacing the PoC's runtime `assert(_setFlags[index] && "input value was never set")`.

**Empty propagation.** A quantity may be "not entered / not measured". Emptiness propagates
through a formula rather than producing a wrong number — a genuine lab requirement, and one the
prior scaffold already modelled with an optional payload.

**The result is not a number.** Evaluation yields a sum type:

```cpp
value(Quantity)            // the computed result
| verdict(Outcome)         // "reject the specimen", "rebuild the layer", "repeat the test"
| invalid(Reason)          // the whole test result is discarded
| empty                    // an input was never measured
```

This is forced by the standards themselves (§16.2), not a generalisation for its own sake: a test
method may reject a specimen outright on a dimensional check rather than compute a value; another
may discard an entire result after repeated outlier rejection; a third may return a remedial
instruction rather than a number. A `double`-returning evaluator has nowhere to put these, and a
library that models them as exceptions or sentinel values makes them invisible to the trace, which
is precisely where an auditor needs to see them.

**Constraints are peers of formulas.** A standard routinely states a relationship purely to
*validate* rather than to compute, written in the same symbols as the computing formulas but
existing only to check. `Constraint` is therefore a first-class declaration alongside `Formula`,
evaluating to a verdict rather than a value, and appearing in the trace as its own step.

### 9.1 Methods

A norm result is rarely one formula. Four patterns, all observed in real standards:

| Pattern | What it means |
|---|---|
| The formula depends on the specimen | one reported quantity, computed by different algebra for different specimen geometries |
| The correction depends on the apparatus | a correction term applies for one instrument variant and is omitted entirely for another |
| Constants and rounding depend on jurisdiction | a constant is deliberately left for a national body to fix, and rounding granularity is tightened nationally |
| An input comes from another test on another specimen | a stress level derived from a prior test's result, or a reference value measured on separate material |

A `Method` therefore bundles: **variants** with declarative selection predicates, a **rounding
rule** (jurisdiction-overridable), **constraints**, and a **sample/test context** able to reference
other samples and tests.

```cpp
inline constexpr auto compressive_strength = formula::method(
    formula::variants(formula::when<Cube>(     var<Force> / (var<Xm> * var<Ym>)),
                      formula::when<Cylinder>( var<Force> / (pi * pow<2>(var<Dm>) / 4))),
    formula::rounding(decimals(1)),
    formula::constraints(dimensional_tolerance));
```

**Why this lives in the library rather than in each norm library.** Written as ordinary C++ —
an `if` on specimen shape, a `roundTo()` after the call — variant selection and rounding are
control flow the library never sees, so they cannot reach the trace. An inspector asking *"why the
cylinder formula, and why rounded to one decimal?"* would get no answer. Since the audit trail is the
product, the decisions that shape a result must be **declared**, not merely executed.

The trace consequently records, as its own steps: which variant fired and on what discriminator;
which rounding rule applied and where it came from (the method's own default or a jurisdiction
overlay); and each constraint's verdict.

## 10. Provenance

Owner-chosen design: a wrapper that is itself an expression, so nesting keeps every citation
reachable from a composed root.

```cpp
template <typename WaterVol, typename CementVol>
constexpr auto wc_ratio = formula::documented(
    formula::var<WaterVol> / formula::var<CementVol>,
    { .title     = "Water/cement ratio",
      .reference = "Example Standard 1:2020",
      .section   = "5.4.2",
      .equation  = "(3)",
      .text      = "Ratio of the effective water content to the cement content." });
```

`DocumentedNode` forwards dimension and precedence, so wrapping changes neither the arithmetic nor
the rendering. Only the documentation walk and the trace sink notice it. Designated-initialiser
binding into this non-deduced parameter was verified on all three compilers.

## 11. Traceability — composable, no macros

**Requirement (owner):** no preprocessor gating; traceability through high-level composition.

The evaluator is parameterised on a **sink**. `NullSink` is stateless and its methods are empty;
`RecordingSink` builds a derivation. Same tree, same evaluator, different composition:

```cpp
auto v         = formula::value_of(expr, env);   // number only
auto explained = formula::explain(expr, env);    // number + derivation
```

**Verified zero-overhead** (clang 22.1.3, `-O2`): the untraced path emits *byte-identical* machine
code to a hand-written expression.

```
hand-written  ref_div : ['divsd xmm0, xmm1', 'ret']
formula (NullSink)    : ['divsd xmm0, xmm1', 'ret']
IDENTICAL CODEGEN: True
```

This removes the preprocessor-gated provenance macro and its call sites that the prior approach
needed, and the two-ABI problem that comes with them.

**Invariant:** the evaluator must never pre-render anything for the sink. It hands over raw nodes
and raw values; the recording sink does the formatting. Violating this makes the untraced path pay.

**Trace structure:** an arena of steps with parent/operand indices, not a nested tree — so a
memoised sub-result appears once with a back-reference, and the structure serialises without
recursion. Two constraints are scars carried over from earlier work, each from a real failure:

- **Never recurse on teardown.** A chain of tens of thousands of nodes segfaulted on destruction.
- **Always bound rendered steps.** An unbounded render collapsed a hundred thousand steps into one
  unusable line.

## 12. Series / vector-valued data

Designed in from the start, per owner decision. Sieve analysis and grading curves are inherently
sequences (sieve size → passing %), not scalars, and retrofitting them after tracing and rendering
exist would rework both.

The value type is a parameter, so a quantity may hold a series; reductions (sum, cumulative,
interpolate) are expression nodes with their own trace steps and renderings.

## 13. Portability — measured, not assumed

A throwaway spike compiled and linked every construct below on **MSVC 19.51.36257 (VS 2026)**,
**clang-cl 22.1.3** and **clang++ 22.1.3**.

| Construct | Result |
|---|---|
| `Dimension` as class-type NTTP | **Works.** Computed `volume/volume` and literal `scalar` are the same type, and a template instantiated on one **links** against the other across TUs on all three |
| `if consteval` | Works |
| `std::to_chars(double)` | Works (Windows; libc++ on Linux still unverified) |
| `static constexpr` in a constexpr function (P2647) | Works |
| Designated init into a non-deduced template parameter | Works |
| Out-of-namespace qualified explicit specialisation under `/permissive-` | Works |
| Whole pipeline in a constant expression | Works — `static_assert(result == 1.5)` passes |
| **`decltype([]{})` as a default template argument** | **BROKEN across TUs on all three.** The closure gets *internal linkage* |
| **NTTP-generated alias without a tag** | **Collides.** Identical metadata ⇒ identical type, on all three |

The two failures are why §8 looks the way it does.

`decltype([]{})` is in `examples/simple.cpp` today. It survives only because everything is in one
translation unit; in a header-only library it fails at link time with a message that never
mentions the cause:

```
cl:       warning C5046: 'consume': Symbol involving type with internal linkage not defined
          error LNK2019: unresolved external symbol ... Tagged<int,class <lambda_1_> >
clang++:  warning: function 'consume' has internal linkage but is not defined
          lld-link: error: undefined symbol: ... Tagged<int, class <lambda_1>>
```

**It must be removed.**

On diagnostics: asserting the compound condition inline prints only the operand *type names*;
moving it into a named helper prints the actual **values** —

```
cl:       RequireSameDimension<Dimension{int:3,int:0,...}, Dimension{int:0,int:1,...}>
clang++:  'Dimension{3, 0, 0, 0, 0, 0, 0} == Dimension{0, 1, 0, 0, 0, 0, 0}'
```

L³ versus M: the reader sees *volume vs mass*. The library's `static_assert` text also appears
verbatim in all three compilers' output, which is what makes the must-not-compile test harness
(§18) viable.

## 14. Relationship to the prior scaffold

A colleague's earlier scaffold attacked the same problem with a **runtime AST**. What that approach
does well and we adopt: exact rational arithmetic with explicit decimals and rounding; an optional
payload for "not measured"; per-unit default precision and bounds; the two rendering scars above.

Where we diverge, and why:

| | runtime-AST scaffold | formula-cpp |
|---|---|---|
| Formula representation | runtime AST, one heap-allocated node per operation | compile-time type |
| When the formula exists | only after computing it | before any data |
| Dimensional algebra | **delegated to the application** | **shipped** |
| Provenance | preprocessor-gated, macro-built | composable sink, no preprocessor |
| Untraced cost | allocation per operation | zero, verified |
| Documentation generation | requires running a calculation; yields numbers | pure traversal; yields symbols |

The decisive point is the third row: an `operator*` that *requires* a dimensional algebra the
library does not provide leaves every application to write its own. Filling that hole is precisely
formula-cpp's job, and an adopting application can then delete what it wrote.

## 15. morph migration path

Not a bootstrap step. The exact-rational and quantity types formula-cpp replaces are long
established downstream and widely depended on there, and they are entangled with that codebase's
serialization and logging choices — which is exactly why this library takes none of those
dependencies (§3).

Sequence: formula-cpp ships and stabilises → morph adds it as a dependency and supplies its own
serialization and tagging adapters for our types → morph's own copies are deleted and their
dependents migrate. Each step is independently reviewable.

## 16. Norm-realism scope

Everything in this section was established by reading real test standards in their calculation
clauses — across several material domains, several issuing bodies and several jurisdictions. Those
documents are copyrighted, are held privately, and **nothing from them is reproduced here**: no
identifier, no clause or table number, no equation, no threshold or constant. What survives is only
what the reading established about the *shape* of the problem. That shape is this project's own
finding, and it is what the architecture above is answering to.

Coverage was deliberately broad rather than exhaustive, and part of the material could not be read
at all. So this section states a **floor** on what the library must express, not a ceiling. Where a
requirement is suspected rather than established, it says so.

### 16.1 Two findings that bound the problem

- **Test methods are code; product specifications are data.** A method's algebra is fixed and
  belongs in a library. The limit tables a product is judged against are master data, registered per
  customer, per region, per contract. formula-cpp expresses methods and never specification tables.
- **Any computed result may be overridden by direct entry.** Labs routinely record a final result
  by hand in place of a computed one. The evaluation model must therefore distinguish *measured*,
  *derived* and *manually entered*, and the trace must say which. A value that was typed in must
  never be presented as though it were derived.

### 16.2 The model is bigger than "formula"

The single most consequential finding: **a calculation defined by a standard does not always produce
a number.** A method may instead yield a verdict — reject this specimen, repeat the test, discard
the whole result, overhaul the apparatus, investigate the plant, or carry out remedial work — and
for a substantial minority of methods the verdict *is* the primary output rather than an exception
path. Some contractual documents go further and make the outcome a monetary deduction.

So evaluation returns a **sum type** — `value | verdict | invalid(reason) | overridden(value,
source)` — not a scalar.

Standards also state relationships **purely to validate**: written in the same symbols as the
computing formulas, but existing only to check a result rather than to produce one. `Constraint` is
therefore a first-class concept alongside `Formula`.

### 16.3 Tier A — required for v1

1. **Scalar arithmetic** including integer powers, square and cube roots, and π.
2. **Rounding as a tree node, not formatting.** Round-to-decimals, round-to-integer, **round-up**
   specifically (distinct from round-half-up, and separately from round-away-from-zero), and
   significant figures. Critically: **intermediate and final rounding differ and both are
   specified** — a method may round an input to a coarse granularity *before* it enters a formula
   and round the result differently afterwards, so a library that rounds only at output produces
   wrong numbers. Rounding may also differ per element within one vector.
3. **Table lookup**, in three distinct kinds, all needed: **exact** (a category key selects a row),
   **banded** (a measured value falls in an interval that selects a correction), and
   **interpolating** (a value between two rows is interpolated). Band tables must be validated for
   gaps and overlaps, because real published tables contain typos and undefined regions:
   **the loader must say so rather than silently mis-bucket.**
4. **Conditionals / piecewise / variant dispatch**, in five kinds that are not interchangeable:
   - a numeric threshold on an intermediate selects between two formulas
   - banded piecewise with **non-numeric outcomes**
   - **variant selection by apparatus, method or specimen geometry**
   - **conditional aggregation** — which observations enter the mean depends on the observations
     themselves
   - **interaction and suppression** — one penalty suppresses another rather than adding to it
5. **Series as a first-class type** (already an owner decision; the reading supplies the evidence).
   Required operations: per-element map; **cumulative sum running from one end of the series**;
   Σ over the series; **per-element and whole-series aggregates derived from the same data**;
   per-element conformity against a band vector; **a series domain that is itself computed and then
   snapped to the nearest permitted value** of a standardised set; splicing two series on different
   bases into one monotone curve; binning raw observations into classes.

### 16.4 Tier B — required beyond a single specimen

6. **Statistics with sample-mutating outlier rejection.** The hard part is not `mean` / `stddev` but
   that rejection **re-runs the aggregate** — possibly more than once — and can terminate in *abort*
   rather than in a value. A bounded fixed point with a terminal *invalid* state; the result needs
   `{ value, rejected_indices, verdict }`.
7. **Repeatability / reproducibility limits.** Two forms, both needed: a constant per method and
   level, read from a table; and **a function of the result level**, which makes the precision limit
   depend on the mean of the very results being checked ⇒ **two-pass evaluation**.
8. **Interpolation** — linear, both within lookup tables and along a measured curve.
9. **Unit handling with explicit escape hatches**, in three situations: ordinary conversion;
   **alternative unit systems inside one formula**, where the *pairing* of two inputs' units is what
   must be enforced rather than either unit alone; and **deliberate dimensional inconsistency that
   the standards name themselves** — a rule stated over the *numeric value* of a quantity in a named
   unit rather than over the quantity. The third vindicates the traced `numeric_value_of(x, unit)`
   escape hatch, with a mandatory justification string.

### 16.5 Tier C — deferred, scoped honestly

10. **Least-squares regression.** A fit whose coefficients then feed a reported quantity is **not
    expressible as an expression tree at all**; it must be an opaque named operation over a series.
11. **Iteration / bounded retry.** Present, but shallow — repeat a step until a condition holds.
    **No genuine root-find or implicit equation was found.** Build the bounded-retry / fixpoint
    shape, *not* a solver.
12. **Jurisdiction parameterisation.** Constants **and rounding rules** must be overridable per
    country or contract, not compiled in (§16.7).

### 16.6 Consequences for the architecture

- Evaluation result is a sum type (§16.2), which propagates through tracing and rendering.
- `Constraint` joins `Formula` as a top-level concept.
- `Rounding` is a node with a mode enum, visible in the trace.
- Lookup tables are a data structure with validation, not a `switch`.
- Two-pass evaluation is a supported mode (precision checks).
- Variant selection, rounding rules and jurisdiction overrides are **declared, not executed**, so
  they appear in the trace (§9.1).
- A jurisdiction is an **overlay over a method**, not an argument to it (§16.7).
- Every result carries its origin — measured, derived, or manually entered — and the trace says
  which (§16.1).
- Symbols and names are jurisdiction-scoped, because the same word denotes different quantities in
  different countries (§16.7).
- The evaluation context must be able to reference **other samples and other tests**, so it cannot
  be scoped to one specimen's inputs.

### 16.7 Jurisdiction is an overlay, not a parameter

Tested directly: one physical measurement, followed through five jurisdictions. The core algebra is
stable everywhere. Around it, each jurisdiction varies independently —

- **which constants apply**, together with the preconditions that travel with them (a jurisdiction
  may replace a computed correction by a fixed value only because it also fixes the condition that
  correction accounted for; constant and precondition move together)
- **which method branches exist at all** — one may delete a branch outright, another keep every
  branch but reclassify which counts as the reference
- **which branch is mandatory** for which material or product family
- **the rounding granularity and the declared unit** — one physical quantity reported in three
  different units across three documents
- **the acceptance thresholds, and their *arity*** — one jurisdiction comparing a pair of
  determinations where another simultaneously requires a rolling mean over a longer window, and a
  third replacing numeric limits with category codes
- **extra derived quantities** with no counterpart in the base standard
- **the formula itself, for the same reported quantity** — computed from entirely different inputs
  by different algebra, yet reported under the same name
- **the terminology** — the same word naming different physical quantities in two countries, with
  the two meanings crossed over

**Therefore:** a `jurisdiction` enum threaded into one function does not survive contact with the
last two. The shape is a **shared kernel of methods plus a per-jurisdiction overlay** that can
override constants, prune and pin variants, change rounding and declared units, add derived
quantities with no counterpart, replace a formula wholesale, and supply its own acceptance logic of
a different arity. The terminology collision also means **symbols and names are
jurisdiction-scoped**, not global — a renderer must know whose vocabulary it is printing.

### 16.8 Beyond expression trees

Some things real methods require cannot be expressed as a tree of scalars under any encoding:

- **Rolling-window state machines with hysteresis.** A conformity level derived from the last *n*
  results, which then sets the testing frequency; determined periodically from the previous period's
  worst level, pinned to the worst level until enough history exists, and demoted and frozen after a
  shutdown or an equipment overhaul. It depends on calendar time and plant lifecycle events, not on
  the sample in front of you.
- **Best-of-two evaluation strategies.** Two whole procedures are run over the same data and the
  more severe outcome is taken; where two findings coincide, only the larger applies rather than
  both.
- **Set partitioning.** Specimens must be split into subgroups balanced on a measured property — an
  assignment problem whose solution changes the reported result.
- **Graphical constructions.** A result defined by a construction on a plotted curve, or by
  classifying a curve's shape qualitatively into diagnoses. There is no formula.
- **Lineage predicates.** A computation permitted only if its two inputs came from the same method
  and the same material batch — a constraint over two test *records*, not over two numbers.
- **Same numbers, different legal force.** Figures that are informative in one document and a
  mandatory rejection rule in another.

These belong to the downstream norm libraries and to LASTRADA, **not** to formula-cpp. Recording
them here fixes the boundary: this library expresses methods, constraints and their traces; it does
not model plant lifecycles, solve assignment problems, or read graphs.

## 17. Phasing

Each phase builds, tests and is left green.

| Phase | Content | Rationale |
|---|---|---|
| **1** | Repo hygiene, Apache-2.0, trimmed `.clang-format`/`.clang-tidy`, CMake INTERFACE target with install/export, presets for cl/clang-cl/clang++, Catch2 via CPM, **must-not-compile harness**, CI incl. install-and-consume | The consumable surface and the negative-test mechanism are the stable base everything else is added around |
| **2** | Exact rational numbers; rounding modes incl. round-up and significant figures | Tier A #2; everything numeric depends on it |
| **3** | Dimensions (rational exponents), units, exact conversion, decimals + bounds | Tier A #1; the algebra downstream consumers lack |
| **4** | Quantities, `Describe<T>`, the CRTP declaration, empty/not-measured propagation | §8 |
| **5** | Expression layer, operators, `Value<T>`, environment, evaluation returning the **sum type** incl. `overridden` | Tier A #1; delivers the owner's headline syntax |
| **6** | Citations, `documented()`, documentation generation, MkDocs+Doxygen site, generated gallery | §10, §19 |
| **7** | Composable tracing sinks, trace arena, bounded rendering, audit reports | §11 |
| **8** | Rounding nodes, conditionals/piecewise, `numeric_value_of` escape | Tier A #2/#4, Tier B #9 |
| **9** | Constraints as peers of formulas; verdicts as trace steps | §16.2 |
| **10** | Lookup tables (exact / banded / interpolating) with gap-and-overlap validation | Tier A #3 |
| **11** | **Methods and jurisdiction overlays**: declarative variants with selection predicates, overlays that override constants, prune and pin variants, change rounding and declared units, add derived quantities and replace formulas — each recorded as its own trace step | §9.1, §16.7; the trace must explain *why that formula* |
| **12** | Series: map, cumulative, reductions, per-element rounding and conformity, snapping a computed domain value to the nearest permitted one, splicing, binning | Tier A #5 |
| **13** | Statistics with sample-mutating outlier rejection; r/R incl. level-dependent two-pass | Tier B #6/#7 |
| **14** | Cross-sample / cross-test evaluation context and lineage predicates | §16.6, §16.8 |
| **15** | Opaque named operations over series (regression); bounded-retry shape | Tier C #10/#11 |

Phases 1–7 constitute a genuinely useful library on their own. Phases 8–12 are what make real
norm methods expressible; 13–15 are what make a whole test series expressible. Retire `Evaluation`/`EvaluationFunctors`/`Overloader` and the `decltype([]{})` idiom
in phase 5, rewriting `examples/simple.cpp` against the new API.

## 18. Project, packaging and CI

- **Target:** `INTERFACE` library, `formula-cpp::formula-cpp`, `target_compile_features(INTERFACE cxx_std_23)`,
  `FILE_SET HEADERS`, `PROJECT_IS_TOP_LEVEL`-gated so subproject consumers get only the library.
- **Install/export:** `install(EXPORT)` + `configure_package_config_file` +
  `write_basic_package_version_file`, producing a config that mentions neither Catch2, CPM, nor any
  absolute path. CI asserts that mechanically.
- **vcpkg** is the primary consumption path; the port must stay trivial.
- **Version:** a committed literal, **not** `git describe` — `vcpkg_from_github` extracts a tarball
  with no `.git`, which would silently install a `0.0.0` config.
- **Dependencies:** Catch2 via a pinned, hash-checked CPM bootstrap (fastcached's).
- **Presets:** `cl`, `clang-cl`, `clang++` (+ gcc), Ninja, OS-gated conditions.
- **Warnings:** `/W4 /permissive- /utf-8 /Zc:__cplusplus` for the MSVC family; `-Wall -Wextra` for
  clang++. `-Werror` must be applied by **frontend variant**, not compiler id — `CMAKE_CXX_COMPILER_ID`
  reports `Clang` for clang-cl.
- **Tests:** Catch2 + `STATIC_REQUIRE` for compile-time behaviour, plus a **must-not-compile**
  harness that asserts both that the build failed *and* that the failure text contains the library's
  own `static_assert` message. Those message strings are tested API.
- **CI:** build matrix over the three compilers × platforms; an **install-and-consume** job that
  installs to a staging prefix and configures a consumer with `find_package(formula-cpp CONFIG REQUIRED)`
  — the regression guard for the Lastrada path; lint (clang-format, clang-tidy); docs.
- **Inherited configs must be trimmed.** `.clang-tidy`'s `HeaderFilterRegex` currently matches
  **nothing** in this repo, so clang-tidy would report green having analysed zero code. Its 28-line
  header comment describes another program entirely (ODBC, Win32 sockets, `Base64.cpp`, 376 TUs).
  `.clang-format` declares `Standard: Cpp11`, which mis-parses the concepts and nested `>>` this
  library is made of.

## 19. Documentation

The library's pitch is human-readable formulas, so documentation is a deliverable, not a chore.

- **MkDocs Material** for prose guides, **Doxygen** published under `/api`, both to GitHub Pages.
- **Guides:** quantities & units · writing formulas · composition & parametrisation · citations ·
  tracing & audit reports · rendering dialects · exact numbers & rounding · series/grading curves ·
  design rationale · portability notes.
- **Generated formula gallery** — a page produced by *running* the library over the example
  formulas, each rendering itself with its symbol table and citation. Dogfoods the headline feature
  on every commit.
- **CI enforces:** every fenced C++ block compiles (and runs, diffed against its documented output);
  `WARN_AS_ERROR` on undocumented public entities; the gallery regenerates; links and spelling.
- **Examples use generic physics only** (§3).

## 20. Open questions

1. **Which test methods the first downstream library should cover.** Nothing here blocks this
   library, but the first norm library built on it needs a starting set chosen deliberately rather
   than by whatever was read first.
2. **Part of the source material could not be read** and may add requirements beyond §16.
   Curve-maximum extraction and log-linear interpolation are both suspected but **unverified**.
3. Minimum supported MSVC and clang versions. Everything in §13 was verified on cl 19.51 / clang
   22.1.3; the floor we *support* is a separate decision.
4. `std::to_chars` for floating point on libc++ (Linux clang++ leg) — unverified.
5. Whether the downstream enum-keyed unit style must survive the migration, or whether its
   dependents move to dimension-vector units.
