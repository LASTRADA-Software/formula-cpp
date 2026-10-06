# Tracing and audit trails

`formula-cpp` provides a way to record how a formula reached its answer, not
only the answer itself: a **sink**, from `sink.hpp`, is a seam the evaluator
calls at every node; `RecordingSink`, from `trace.hpp`, is the sink that turns
those calls into a `Trace` -- a flat record of every step; and `render_trace()`,
from `trace_render.hpp`, turns a `Trace` into text a person reads. This page
explains what a sink is and why it costs nothing when nobody is listening, the
two ways to evaluate a formula and when to reach for each, how to read a
rendered derivation, why the renderer forces you to choose a bound, and what
happens when a consumer's own node kind meets a sink it was never told about.
The worked example is `examples/tracing.cpp`; every block on this page
formatted as program output is copied verbatim from that program's actual
output, exactly as `docs/citations.md` does for `examples/citations.cpp`. For
a documentation page built the same way from several formulas, including a
worked derivation, see [the gallery](gallery.md).

Neither `trace.hpp` nor `trace_render.hpp` is included by the umbrella header,
`formula.hpp`. `trace.hpp` pulls in `<vector>` for the arena a derivation is
recorded into; `trace_render.hpp` pulls in `<string>` to format one. A consumer
who only evaluates numbers must not compile either into a translation unit
that never asks for a trace, so include whichever you need, by name:

```cpp
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>
```

## What a sink is, and why it is passed by value

Every evaluator overload -- for a variable, a constant, a unary node, a binary
node -- takes a third parameter, a sink, and calls two methods on it: `entered`
before a node's operands are evaluated, `produced` after the node has its
answer. `sink.hpp` states the seam as a concept:

```cpp
template <typename S, typename N, typename V>
concept SinkFor = requires(S sink, N const& node, V const& value) {
    sink.entered(node);
    sink.produced(node, value);
};
```

and the default sink, the one every untraced evaluation uses, is `NullSink`:
observes nothing, is empty, and is stateless:

```cpp
struct NullSink
{
    template <Node N>
    constexpr void entered(N const&) noexcept
    {
    }

    template <Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
    }
};
```

That sink is passed **by value**, not by reference, and that is a deliberate
choice rather than a style preference. Passing an empty, stateless sink by
reference still forces a compiler to materialise the address of an object
nothing ever reads through, and at least one of the four compilers this
library targets emits a real instruction for exactly that -- a `leaq` clang++
does not emit when the same sink is passed by value. `sink.hpp`'s own file
comment states the measurement (by value, across cl, clang-cl, clang++ and
g++, at `-O2`/`/O2`); the full table, with the one compiler whose behaviour has
a narrow boundary condition, is in the design spec's traceability section
(`docs/superpowers/specs/2026-09-23-formula-cpp-design.md`, §11). The point
worth taking away without opening that table: adding a sink parameter, by
value, changes nothing the untraced path emits, on every compiler measured,
in the ordinary case where the call inlines.

That constraint has a consequence for anyone writing their own sink: keep it
small and cheap to copy, because the evaluator copies it at every node it
visits. A sink that owned a growable buffer would copy that buffer's contents
at every node in the tree -- which is exactly why `RecordingSink` does not own
the `Trace` it writes into. Its constructor takes one by reference and keeps
only a pointer:

```cpp
explicit constexpr RecordingSink(Trace<Rep>& trace, V vocabulary = V {}) noexcept:
    _trace { &trace },
    _vocabulary { vocabulary }
```

and that pointer, `Trace<Rep>* _trace`, is the whole of `RecordingSink`'s
storage when no vocabulary is given (`trace.hpp`): the default vocabulary is
empty and takes no space. `RecordingSink` is a **handle** to the `Trace`, not
its owner: the caller owns the `Trace` and it must outlive the walk. One
pointer copies for free at every node; a `Trace` copied at every node would
not. A jurisdiction's vocabulary, when one is given, is copied with the sink
-- a few views of string literals, see [Whose symbols](#whose-symbols).

There is a second consequence, and it is not optional the way "keep it
small" is a matter of degree: **a sink must not throw.** Every
`checked_evaluate_si` overload that calls a sink is `noexcept`, so an
exception thrown out of `entered` or `produced` does not become an exception
the caller can catch -- it calls `std::terminate`. This is a real risk, not
a theoretical one: `RecordingSink::produced` itself allocates on every call
(`trace.hpp`), because growing a `Trace`'s `steps` is exactly what recording
a derivation is. An allocating sink is fine; a sink that lets an allocation
failure, or anything else, escape as an exception is not. Catch inside
`entered` and `produced`, or otherwise guarantee they cannot throw, before
handing a sink to the evaluator.

## Two ways to evaluate, and when to reach for each

Both entry points walk the same tree with the same evaluator; only the sink
composed into the walk differs. `test/trace_tests.cpp` evaluates one formula
both ways and checks they agree:

```cpp
auto const plain = formula::evaluate<Density>(density, environment);
auto const explained = formula::explain<Density>(density, environment);

// Memberwise equality across every Outcome alternative (kind, value,
// source, verdict and invalid-reason labels) -- not merely that both
// happen to hold a value. Tracing observes; it must not participate.
CHECK(explained.outcome == plain);
CHECK(explained.trace.steps.size() == 3);
// Check empty() before indexing with root() -- see below for when a Trace
// can be empty even though outcome holds a value.
REQUIRE_FALSE(explained.trace.empty());
CHECK(explained.trace.steps[explained.trace.root()].value == formula::Rational { 2 });
```

(`test/trace_tests.cpp`, `"explain returns the same outcome evaluate would,
plus the derivation"`.) `formula::evaluate<Result>` (and
`formula::checked_evaluate<Result>`, its `std::expected`-returning form -- see
[Writing formulas](expressions.md) for the two of those) take a sink
parameter that defaults to `NullSink`, so calling either without a sink
argument is the untraced path: no `Trace` is built, and nothing is allocated
for one. `formula::explain<Result>` builds a `RecordingSink`
for you, evaluates through it, and returns the outcome beside the trace it
recorded (`trace.hpp` has the four-line body). `explained.outcome` is exactly
what `evaluate<Result>(expression, environment)` would have returned --
tracing observes, it does not participate -- and `explained.trace` is the
derivation. Reach for `evaluate` or
`checked_evaluate` on a path that runs often and never shows its work to
anyone; reach for `explain` at the point a derivation needs to be shown to a
person -- a report, a review, a place where "here is the number" is not
enough and "here is how" is what is actually being asked for.

`explained.trace` is not always populated, though. `evaluate<Result>` returns
a manual override outright, without dispatching `expression` at all, when
`environment` carries one for `Result` -- see [Writing formulas](expressions.md),
"The outcome". Nothing runs, so nothing is recorded:
`explained.outcome.is_overridden()` is true and
`explained.trace.empty()` is true at the same time. That is correct, not a
bug -- an overridden number was not derived, so there is nothing to trace --
but it means `explained.trace.steps[explained.trace.root()]`, the pattern the
snippet above uses, reads past the end of an empty vector whenever the result
happens to be an override. Check `empty()` before reading `root()`, the way
the snippet above now does.

One difference is not about cost but about **where** the call can happen.
`evaluate` and `checked_evaluate` are `constexpr` and remain usable in a
constant expression -- `test/sink_tests.cpp`'s constant-evaluation section
pins this with `static_assert`, including the sink-carrying overload with an
explicit `NullSink`. `explain` is simply not declared `constexpr`, and could
not usefully be. A `std::vector` *can* be built and grown during constant
evaluation -- that has been allowed since C++20 -- but what it builds there
cannot survive past that evaluation into a runtime object: the standard
requires every allocation a constant expression makes to be released again
before the expression finishes. `explain`'s whole purpose is to hand back a
`Trace` that keeps its steps, which is exactly the kind of surviving
allocation a constant expression is not allowed to produce. Writing the
test's call above as a constant,

```cpp
constexpr auto explained = formula::explain<Density>(density, environment);
```

fails to compile, verified with cl 19.51:

```
error C2131: expression did not evaluate to a constant
note: failure was caused by call of undefined function or one not declared 'constexpr'
note: see usage of 'formula::explain'
```

Evaluate at compile time when you can; `explain` is a run-time-only way to see
the working.

## Tracing any evaluation

`explain` traces a formula. The other verbs that take a sink -- a method, a
curve, a rejection, a constraint, a conformity check -- have a twin of their
own that returns the verb's result together with the trace it recorded. Over
`compressiveStrength`, the method of three variants that
`examples/methods_and_overlays.cpp` declares, and that example's `specimen`:

```cpp
auto const derived = formula::explain_method<Cube>(compressiveStrength, specimen);
auto const verdicts = formula::explain_check_all(compressiveStrength.constraintSet, specimen);
```

`derived.outcome` is exactly what `evaluate_method<Cube>` returns, and
`derived.trace` is the `Trace` a `RecordingSink` recorded while it did.
`explain_method`, `explain_check_method`, `explain_curve`, `explain_rejection`,
`explain_check`, `explain_check_all` and `explain_conformity` are the twins of
`evaluate_method`, `check_method`, `checked_evaluate_curve`,
`checked_evaluate_rejection`, `check`, `check_all` and `check_conformity`, and
each takes the vocabulary to write the symbols in as an optional last
argument, as `explain` does.

A verb without a twin -- or one of your own that takes a sink -- goes through
`traced`, which gives the evaluation a `RecordingSink` and returns what the
evaluation returned beside what the sink recorded. Over the `gradient` and the
`inputs` of `examples/tracing.cpp` ([Reading a derivation](#reading-a-derivation)):

```cpp
auto const run = formula::traced([&](auto recordingSink)
                                 { return formula::checked_evaluate<Gradient>(gradient, inputs, recordingSink); });
```

`run.outcome` is the `std::expected` that `checked_evaluate` returned, and
`run.trace` the four steps below.

`explain_series` and `explain_retry` share the shape: `outcome`, then `trace`.
A failure is in `outcome`, and `trace` holds the steps up to it; a value that
was typed in rather than derived leaves `trace` empty, as it does for
`explain`, which records in `Rational`: an evaluation that computes in
`double` is traced by calling its `checked_evaluate_si<double>` with your own
`RecordingSink<double>`.

A series handed to `explain` or `checked_explain` does not compile: both
trace a single value, and they say so in the library's words, pointing at
`explain_series`. Reduce the series to one value first (`sum`,
`interpolate_at`) to trace that value instead.

## Just the trace

Code that only shows how a number was reached has no use for the outcome, and
`traced` spells the lambda out each time. `trace_of` gives the `Trace` alone,
whether the evaluation succeeded or failed. With `density` and `environment` as
in the `explain` example above:

```cpp
auto const steps = formula::trace_of<Density>(density, environment);
auto const text = formula::render_trace(steps, { .maxSteps = 100 });
```

A failure while the formula is evaluated is the trace's last step (at every
node that reports to its sink, as each of this library's does; see
[The extension point](#the-extension-point-your-node-evaluates-but-is-it-traced)).
One converting the result into `Density`'s unit comes after it and is not in
the trace, so read the outcome where that matters. When `environment` holds a
value typed in for `Density`, that value is returned without evaluating and the
trace is empty, as it is for `explain`.

A bound formula names its quantity already, so
`trace_of(boundFormula, environment)` needs none.
`trace_of_si(density, environment)` traces the evaluation in SI units with no
result quantity named: it records the same steps for a derived result, and
since it consults no value typed in for a result -- an input typed in is read
as any other -- it traces the derivation even where `trace_of<Density>` is
empty. All three take the vocabulary to write the symbols in as an optional
last argument.

The outcome is deliberately not returned. A caller who needs it reads it with
`checked_evaluate`, and one who needs it together with its trace uses
`checked_explain`, which holds the trace on success and in its failure's
`trace` on error. `trace_of` is for display; a number that matters is read
where the failure can be handled.

## Reading a derivation

`examples/tracing.cpp` builds the same road gradient `examples/citations.cpp`
evaluates, `s = h / L`, with the same invented citation attached by
`documented()`. The rise is 90 m, and the run is declared in kilometres and
entered as 3 km:

```cpp
auto const inputs = formula::environment(formula::Measured<Rise> { 90 }, formula::Measured<Run> { 3 });
```

It prints the trace:

```cpp
auto const explained = formula::explain<Gradient>(gradient, inputs);

// render_trace has no default for maxSteps: TraceRenderOptions::maxSteps
// is a StepLimit, which has no default constructor, so a caller who
// writes render_trace(explained.trace, {}) does not compile, rather than
// risking an unbounded dump of a derivation many times this size.
std::string const rendered = formula::render_trace(explained.trace, { .maxSteps = 10 });
std::print("{}", rendered);
```

which prints, verbatim:

```
1. h = 90 m
2. L = 3 km
3. #1 / #2 = 3/100
4. #3 = 3/100 [Road gradient, Example Standard 1:2020, 5.4.2, (3)]
```

Every line is one node the evaluator visited, numbered from one in the order
each finished -- children before parents, so an operand's line always appears
above the line that names it. A step that consumed earlier steps names them by
number, `#1` and `#2`; the citation on the last line is the one `documented()`
attached, and it appears only on the step for the `DocumentedNode` itself, not
on the division it wraps.

The run reads `3 km`, as it was entered, not the `3000` metres the arithmetic
actually runs on. Every `Step` stores its value in the **coherent unit** of
its dimension (the SI unit, times one of each [named base
dimension](dimensions.md#base-dimensions-the-si-does-not-have) it carries) --
the one scale every step's value can be compared on -- but also remembers the
unit it is shown in, and `render_trace` converts back before printing and
writes that unit after the number. `Step`'s own comment states which unit
that is, and why the recorder, not the renderer, has to be the one holding
it:

```cpp
/// The unit this step's value is shown in:
///
///  - a variable, constant or rounding shows its declared unit --
///    `Describe<Q>::unit` for a variable or an overridden constant, the
///    constant's own unit for a constant, the node's own unit for a
///    `Round`, `RoundSignificant`, `RoundedRoot`, `RoundedOpaqueOutput` or
///    `RoundingRuleApplied` step;
///  - a `Documented`, `ReplacedVariant`, `VariantSelected` or `RecordScope`
///    step passes its operand's value through unchanged, so it shows the
///    unit that operand's line does, whenever that line is the wrapped
///    node's own and not the operands of a consumer's node;
///  - a value scaled by a pure number shows its operand's unit, and so
///    does a sum or difference on one scale under one name, a series' sum
///    and its range;
///  - a negation and an absolute value show their operand's unit;
///  - a mean and a rejection pass's mean are points on their sample's
///    scale, and show its unit, offset or not;
///  - a conditional and a precision limit show the unit of the step they
///    restate;
///  - an opaque operation's output shows an input's unit of its
///    dimension, or a quotient of two (`OpaqueOutputValue::unit`);
///  - an offset unit is never borrowed for a sum, difference, scaling,
///    negation or absolute value: such a value is no point on its scale;
///  - everything else is the coherent unit of `dimension`, which the
///    renderer writes after the number, spelt from its bases (`kg/m^3`).
///
/// A unit is borrowed only from operand steps that are provably the
/// operands' own, and only when it has a symbol: a value in a unit with
/// no symbol could not say what scale it is on, and reads in the coherent
/// unit instead.
///
/// `value` is always in the coherent unit, so that steps are
/// comparable; this is what a renderer converts back to before showing a
/// number to a person. Without it a derivation restates every input in a
/// unit nobody typed: someone who entered 3 km reads `3000 m`, which is
/// the same length and a worse record. The renderer cannot recover this
/// on its own -- by the time a `Step` exists the quantity type is erased,
/// so the recorder captures it here.
```

(`trace.hpp`.) A quantity's C++ type exists only while the evaluator is
walking that quantity's own node; by the time `RecordingSink::produced` builds
a `Step` for it, the type is gone and only the runtime `Unit` value survives.
Capturing anything less at that point -- the coherent unit alone, say --
would make `render_trace` unable to ever show `3 km` again; it would show
`3000 m`, arithmetically identical and a strictly worse record of what
someone actually typed.

A step that is a plain computation, `#1 / #2` above, has no declared unit of
its own. Where the steps it read say which unit it is in, it borrows theirs, by
the rules below; otherwise it is in the coherent unit of its dimension, and
`render_trace` writes that unit after the number, spelt from its base units.
`test/trace_render_tests.cpp` pins the second case directly for a squared mass
over a volume:

```
1. m = 6 kg
2. #1^2 = 36 kg^2
3. V = 3 m3
4. #2 / #3 = 12 kg^2/m^3
```

(`test/trace_render_tests.cpp`, `"a derivation renders one line per step, in
order"`.) `kg^2` and `kg^2/m^3` are no symbols anyone declared. `coherent()`
(`evaluate.hpp`) hands a computed step a `Unit` with no symbol at all, and the
renderer spells such a unit from the SI base units -- `m`, `kg`, `s`, `A`, `K`,
`mol`, `cd` -- with the name of each named base dimension ahead of them:
`kg/(m s^2)` for a pressure, `s^-1` for a frequency, `EUR` for a price per
kilowatt-hour times an energy. A unit with nothing above the slash is written
with negative exponents. So `20000/413 kg^-1` cannot read as a fraction
divided again, as `20000/413 1/kg` would. A computed mass reads `kg`, a
computed length `m`. Only a dimensionless value is a bare number: `#1 / #2`
above, a length over a length, reads `3/100`. A value declared in a unit of the
author's own that has no symbol reads in the coherent unit too, converted,
since its number alone could not say what scale it is on. A dimensionless
unit with a scale must have a symbol, so a bare number is always a value at
scale 1. A rounding in a unit with no symbol names that unit by its size in
the coherent unit: `round(#1, to 2 dp of 1/1000 kg) = 157/50000 kg`.

A computed step borrows its unit off the steps it read in these cases:

- A step that passes a value on unchanged -- a documented step, a
  jurisdiction's replacement, a variant's selection, a read from another
  record -- states it in the unit of the step it wraps, below.
- A value scaled by a pure number reads in its operand's unit -- 3/50 of a
  mean of 413/10 g is `#2 * #3 = 1239/500 g` -- and so does a sum or a
  difference of two values shown on one scale under one name, at the finer of
  their two precisions.
- A negation and an absolute value read in their operand's unit, and a
  conditional in its chosen branch's: `if #1 > #2 then #3 = 60 MPa`. A
  precision limit reads in its second pass's, and its first pass in the unit
  of the level it restates: a level constant in grams reads in grams on both
  lines.
- A value that is a point on its operand's scale -- a mean, a pass's mean, a
  rejected determination -- reads in that operand's unit when it has a
  symbol, offset or not: a mean of Celsius readings is a Celsius reading.
- A curve reads its points and values in the units of the steps it pairs, and
  a value read off it in its values' unit.
- A sum, a range, a running total and a series scaled by a pure number read in
  their series' unit, and a rejection's deviation from the mean in its
  sample's, when that unit has a symbol and no offset
  ([Series and grading curves](series.md)).
- An opaque output reads in an input's unit, or a quotient of two, under the
  same rule ([Opaque operations and bounded retry](opaque-and-retry.md)).

An offset unit is never borrowed for a sum, a difference, a scaling, a
negation or an absolute value: such a value is no point on its scale. The
difference of two Celsius readings is an interval, and reads `#1 - #2 = 5 K`,
not `5 °C`. Nor is a unit with no symbol borrowed, and nor is one read off a
step that is not provably the operand's own: over a consumer's node that hands
the sink on to its operands (below), the step reads in the coherent unit.

A binary step whose left operand failed never evaluated its right one, and
says so where the right operand would stand:

```
5. #1 / #4 = division by zero
6. #5 / (not evaluated) = division by zero
```

(`test/trace_render_tests.cpp`, `"a binary step names the side that failed,
the side never evaluated and a side that recorded no step"`.) A side computed
by a consumer's node that records no step of its own reads `(untraced)`.

A citation computes nothing, so a documented step states its value exactly as
the line it names does -- the same number, in the same unit and spelling. Over
a sample mass declared in grams:

```
1. m_s = 163/10 g
2. round(#1, to 0 dp of g) = 16 g [nearest, ties away from zero]
3. #2 = 16 g [Sample mass, Example Standard 1:2020, 4.1]
```

(`test/trace_render_tests.cpp`, `"a documented step shows its value as the step
it documents does"`.) A jurisdiction's replaced variant is the same: its line
reads as the replacement's own. Over a consumer's node that hands the sink on
to its operands (see below), there is no line of the node's own to read as --
only its operands', none of which holds its value -- so the documented step
states its value in the coherent unit, as a computed step with no unit to
borrow does.

A step that failed shows why instead of a value, and a step with no value at
all -- an absent measurement, which is not an error -- says so rather than
looking like one:

```
1. / = division by zero
2. m = (not measured)
```

(`test/trace_render_tests.cpp`, `"a failing step renders its error, and an
absent one renders absence"`.) The failing `Divide` above is a hand-built
`Step`, not the recording of a real division by zero -- a real one runs both
operands before the arithmetic fails, so it always records two. Zero operands
is reachable from a real tree only when **both** children are untraced
extension-point nodes (see below) that produce no step of their own for the
outer node to claim. `Step::operands` holds **exactly** what the evaluator
actually dispatched, not what the node's arity would predict -- when an
operand fails, its parent returns without evaluating the remaining ones, so a
`Divide` may hold one recorded operand, or, in the rare case above, none.

## Which variant a method chose

A method reports one quantity by more than one formula -- a cube, a cylinder
and a prism each have their own -- and which one applies is a property of the
specimen, stated by the caller as a tag: `evaluate_method<Cylinder>(...)`.
Spec section 9.1 asks that an inspector reading the result can ask *"why the
cylinder formula?"* and get an answer, so the selection is recorded as a step
of its own, `StepKind::VariantSelected`, and a derivation says which variant
fired and on what:

```cpp
auto const derived = formula::explain_method<specimen::Cube>(compressiveStrength, inputs);
std::print("{}", formula::render_trace(derived.trace, { .maxSteps = 20 }));
```

```
1. F = 562 kN
2. 19321 mm2
3. #1 / #2 = 562000000000/19321 kg/(m s^2)
4. round(#3, in MPa) = 291/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
5. #4 = 291/10 MPa [variant Cube (1st of 3), selected by tag]
```

(`test/trace_render_tests.cpp`, `"a variant step reads as its operand, with the
variant and its position in brackets"`, whose method has an invented cube,
cylinder and prism.) The selection is the last line and the walk's one root:
its operand is the variant that ran, rounded by the method's rule, and its
value is exactly what `evaluate_method` returned. The bracket carries both
halves of the answer. The tag's name is the discriminator the caller selected
with; the position -- one-based here, zero-based in `Step::variantIndex` --
is what a reader counts back to in the method's `variants(...)`, and it
survives even where the name cannot be read. It is the position in the
method **as published**: a jurisdiction's overlay that prunes the cube leaves
the cylinder the 2nd of 3, not the 1st of 2, because the published
`variants(...)` is the only one in the source to count in. `selected by tag` says how the
choice was made rather than only that it was. Nothing in the line names a
variant that was not taken: the neighbouring test selects the cylinder and
checks that the word `Cube` appears nowhere in its derivation.

When a jurisdiction's overlay narrowed the variants first, the bracket says so
in a second clause, with what the overlay cited: `pin_variant<Cylinder>(annex)`
gives `[variant Cylinder (2nd of 3), selected by tag; pinned by jurisdiction
overlay: ...]`, and a prune gives `; 1 of 3 pruned by jurisdiction overlay:
...` -- or, after prunes by more than one overlay, `; 2 of 3 pruned, the last
by jurisdiction overlay: ...`, naming what the last one cited. One overlay
cannot both pin and prune, but one jurisdiction may prune what a later one
pins, and then both clauses appear, the prune first. Both citations are
required, and escaped as every other piece of author text is.
(`test/overlay_tests.cpp`, `"a pin says which jurisdiction made the variant
mandatory"` and the three cases after it.)

A method tells a sink about its choice through two optional members,
`variant_entered` and `variant_produced`, with a `VariantSelection`
(`sink.hpp`) -- a method is not a node, so it cannot come through `entered`
and `produced`. A sink defines both or neither; `NullSink` defines neither and
pays nothing.

**The tag's name is recovered from the compiler**, the way an enumerator's is,
and it is the name as written, unqualified: `Cube`, whichever namespace or
class declares it, and never with an anonymous namespace in front of it --
which the four compilers this library is measured on would otherwise spell
in three different ways, and cl alone in two. A class template specialization keeps its arguments,
`Sized<163>`, with their qualification stripped the same way. One difference
cannot be evened out: cl prints a `bool`, `char` or enumeration argument as a
number, `Flag<1>` where the others print `Flag<true>`, and it prints a
defaulted argument the others leave out, `Opt<Cube, void>` for `Opt<Cube>`.
And some tags have no reflected name that could be shown at all: a lambda or
an unnamed class, or a specialization with one as an argument, or over a
`const` type, a function type, a pointer or a cast. The compilers print those
as file paths, as placeholders, as fragments, or -- for `TagBox<const ns::A>`
-- as a name that, once its qualifiers are gone, is `TagBox<A>`: the name of
a different type. A `char`, floating-point or class-type value as an argument
is refused too, because the compilers print it differently: `Ch<'x'>` from
clang and GCC is `Ch<120>` from cl, which cl then accepts, so such a tag
compiles on cl and nowhere else until it is named. Rather than record a wrong
name, the library refuses to compile such a tag and says to name it. An author who wants a
tag to read the same everywhere, or to read the way a published method words
the variant, specializes `formula::TagName` (`tag.hpp`), which has the shape
and the refusals of `EnumeratorName`:

```cpp
template <>
struct formula::TagName<Cylinder>
{
    static constexpr std::string_view of() noexcept { return "cylinder 135 x 271 mm"; }
};
```

## Whose rounding rule, and whose constant

The rounding line above is a step of its own kind,
`StepKind::RoundingRuleApplied`, rather than an ordinary rounding step. Spec
section 9.1 asks the trace to record which rounding rule applied **and where
it came from**, and `rounded to 1 dp` alone is true whether the method's
author chose the rule or a jurisdiction did. So the bracket says whose it was:
`(method default)` for the rule the method was declared with, and
`(jurisdiction overlay: ...)` for one an overlay's `with_rounding` put in its
place, followed by what the overlay cited:

```
8. round(#7, in MPa) = 601/100 MPa [rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, NA.4.1); nearest, ties away from zero]
```

(`test/overlay_tests.cpp`, `"the trace says where the rounding rule came
from"`.) A jurisdiction that restates the method's own granularity still gets
`(jurisdiction overlay: ...)`: the rule is then its rule, and the trace does
not decide whose it was by comparing numbers. The provenance is in
`Step::roundingProvenance`, and the citation in `Step::citation`.

Every overlay operation takes a citation argument, but an empty one compiles:
`with_rounding<...>({})`, `pin_variant<Cube>({})`, or an operation's aggregate
built directly, such as `VariantPin<Cube> {}`. Every clause an overlay adds then
says so, `(jurisdiction overlay (no citation given))` here and `[fixed by
jurisdiction overlay (no citation given)]` below, rather than a bare
`jurisdiction overlay` that a reader could take for a cited one.
(`test/overlay_tests.cpp`, `"an operation given an empty citation says so in
every clause"`.)

A constant an overlay fixed with `with_constant` is traced the same way, as
`StepKind::OverriddenConstant` rather than as a variable. It reads as its
quantity, but it says the value was not the specimen's:

```
1. k_s = 863/1000 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.3]
```

(`test/overlay_tests.cpp`, `"an overridden constant is traced as fixed by the
overlay, holding its value"`.) `document()` marks it too: the quantity's row
in the symbol table carries `fixedValue` and `fixedBy`, so a documentation
page does not ask a reader to supply a value the formula never reads. A formula
assembled by hand that both fixes a quantity and reads it from the specimen
gets a row saying both: `alsoReadAsInput` is set beside the fixed value.

A quantity a jurisdiction defines by an expression, with `add_derived`, is
traced as `StepKind::DerivedQuantity`: the quantity, equal to the step its
definition produced, marked as the overlay's:

```
3. #1 / #2 = 2/3
4. k_s = #3 = 2/3 [derived by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.3]
```

(`test/overlay_tests.cpp`, `"a derived quantity is traced as derived by the
overlay"`.) Its row in `document()`'s symbol table carries the definition in
the page's dialect as `derivedAs`, and the citation as `derivedBy`. A variant
whose formula a jurisdiction replaced wholesale, with `replace_variant`, is
traced as `StepKind::ReplacedVariant`, a step of its own under the variant
selection whose line ends `[replaced by jurisdiction overlay: ...]`. It is a
step of its own because what it marks is the formula that ran, not the choice
of which variant ran.

## Whose constraints

A method's constraints are checked with `check_method`, which answers one
outcome per constraint the method holds -- `check_all` over the set it
holds, handed over whole: `check_all(m.constraintSet, inputs)` for a method's
own constraints, `check_all(m.constraintSet.constraintSet(), inputs)` for a
jurisdiction's -- and tells a sink whose constraints they are. The verdicts are gathered under a step of their own,
`StepKind::AcceptanceChecked`, whose operands are the verdicts in the order
`check_method` returns them, and each verdict's bracket says whose check it
was:

```
1. F = 90000 N
2. 47300 N
3. require #1 >= #2 [satisfied; the method's own constraint]
4. acceptance(#3) [the method's own constraints]
```

(`test/overlay_tests.cpp`, `"each verdict says whether the method or a
jurisdiction's overlay supplied it"`.) An overlay's `with_constraints`
replaces the constraints wholesale, with as many as the jurisdiction states,
and every verdict of the overlaid method then ends `; jurisdiction overlay:`
and what the overlay cited. A jurisdiction that removes every constraint
still gets a line, so the removal is never silent:

```
1. acceptance(none) [jurisdiction overlay: Acceptance, Example Standard 12:2021 NA, NA.6]
```

(`test/overlay_tests.cpp`, `"an overlay removes every constraint, and the
trace says by whose authority"`.) The provenance is in
`Step::constraintProvenance`, set on the gathering step and on each verdict
it holds, with the overlay's citation in `Step::citation`. A constraint
checked on its own, with `check` or `check_all`, belongs to no method, and
its line reads as it always did. A method tells a sink about its constraints
through two optional members, `acceptance_entered` and `acceptance_produced`,
given a `ConstraintOrigin` read off the method's constraints; a sink defines
both or neither.

## Only the library states a provenance

The provenance a trace records in a `Step`'s fields is only ever the library's
to state. The nodes
an overlay leaves behind -- a fixed constant, a derived quantity, a replaced
formula -- can be built only by the overlay, and building one by hand is
refused in the library's words. A
`RoundingRule` claims a jurisdiction's overlay only when `with_rounding`
produced it, and the rounding node a method applies is built only by
`evaluate_method`, from the method's own rule, which it holds rather than a
provenance of its own. A method's constraints are a
jurisdiction's only when they are the `OverlaidConstraints` that
`with_constraints` produced -- which carries the overlay's citation with the
constraints themselves -- and building one by hand is refused. A variant's
published position and count are stated only by `variants(...)` and carried
by `apply` through a pin or a prune: a layout written by hand,
`{ { 5, 7 }, 9 }`, and one selected by hand from another pack's,
`published.select<5, 7>()`, are both refused.

**The structured fields are what is authoritative.** A `Step` records its
provenance in fields of its own -- `kind`, `roundingProvenance`,
`constraintProvenance`, `variantPinned`, `variantPrunedCount` and the citations
beside them -- and those are set only by the library. Code that has to decide
whose a value was reads them, not the rendered line.

**The rendered line is escaped so that author text cannot break its
structure.** A trace line is a numbered line whose provenance is a bracketed
clause at its end, and some of the words in it are the author's: a quantity's
symbol, a citation, a verdict's label, a justification, a unit's symbol, a
variant's tag and a lookup key's name. `render_trace` escapes every one of
them before it writes the line -- `\` as `\\`, `[` as `\[`, `]` as `\]`, `;` as
`\;`, a newline as `\n`, and any other control character as `\x` and two hex
digits -- and writes its own clauses as they are. So a declared symbol `k] [fixed
by jurisdiction overlay: X` reads

```
1. k\] \[fixed by jurisdiction overlay: X = 1
```

and cannot pass for the clause the library writes when an overlay did fix
`k`, and a verdict labelled `reject; jurisdiction overlay: X` cannot name a
second owner for a constraint. A `TagName` or `EnumeratorName` spelling, and a
vocabulary's symbol, go further: holding `[`, `]` or a control character, it is
refused at compile time.

**Author text may still contain any words.** The escape stops a clause from
being opened or closed, and a line from being ended; it does not stop a clause's
words. A `documented()` citation titled `replaced by jurisdiction overlay:
Example Standard 9:2022 NA` renders its `Documented` line exactly as a genuine
`replace_variant` citing that standard renders its own, and nothing in the text
tells them apart; `Step::kind` does. The method's author is trusted to cite
what the method cites.

Both rules are byte-level and ASCII. Unicode look-alikes of the library's
brackets, such as the fullwidth `［` and `］` (U+FF3B, U+FF3D), and the line and
paragraph separators U+2028 and U+2029 are neither escaped nor refused. They
cannot break the structure the library writes, which is ASCII throughout, though
a viewer may draw them as a bracket or break the line at a separator.

What the guard governs is how a rule, a set of constraints or a layout is
created, not where a copy travels, and a copy stays true of itself: a method
holding a copy of an overlay's rule is traced as that overlay's rule, and a
method built from an overlaid method's `constraintSet` checks the
jurisdiction's constraints and says so, because they are the jurisdiction's.
These routes remain, and no type can close them:

- `method(o.variantSet, o.rounding, o.constraintSet.constraintSet())` hands
  the jurisdiction's constraints over as a plain set in one call, which makes
  them the new method's own -- reading them has to be possible.
- Assigning a method's public `rounding` member, `m.rounding =
  rounding_rule<...>()`, replaces a jurisdiction's rule with a rule of the
  method's own, and the trace then says "(method default)". The member is
  public so that a method stays an aggregate.
- Copying a pack's layout, `pack.published = other.published`, or resetting it
  to declaration order with `pack.published = {}`, gives it a layout the
  library made for another pack -- positions, count and any pin or prune
  with what it cited. A pruned pack reset this way reports its variants as the
  1st and 2nd of 2 rather than where they were published, and says nothing of
  the prune.
- Reinterpreting an object's bytes makes it anything.
- Explicitly specialising a library template, or a member of one, forges
  anything, and no C++ library can stop it. An explicit specialisation of a
  member -- a constructor, an accessor such as `RoundingRule<...>::provenance()`,
  a defaulted default constructor -- is a member definition, with a member's
  access to the private fields; friend injection names a `detail::` type
  without spelling `detail::`. Both were measured making a method no overlay
  touched trace a jurisdiction's rounding rule. The only supported
  customisation points are `TagName`, `EnumeratorName`, `Describe`,
  `RepTraits`, `OpaqueOperation` (whose `compute` does its arithmetic through
  `RepTraits` and never throws; see [Opaque operations and bounded
  retry](opaque-and-retry.md)) and the vocabulary. Specialising any other
  formula-cpp template or member is outside the contract, and can make the
  trace say anything.

Nor does the guard reach a sink's own hooks, which are public: code that calls
them by hand, or fills in a `Step` by hand, writes whatever trace it likes.

## Whose symbols

A `Variable`, `OverriddenConstant` or `DerivedQuantity` step records its
quantity's symbol **when the formula is evaluated**, and `render_trace` only
reads it back. So a jurisdiction's vocabulary (see
[Citations and rendering](citations.md)) has to be given to the sink, not
only to `render()` -- a page rendered in one vocabulary and a trace recorded
in another would name one quantity with two different letters. An
`explain_*` twin hands the vocabulary it is given to the sink it builds. Over
`limit`, `crossedInputs` and `south`, the fixtures of
`test/vocabulary_tests.cpp`:

```cpp
auto const southern = formula::explain_check(limit, crossedInputs, south);
```

```
1. E = 30 MPa
2. R = 12 MPa
3. require #1 >= #2 [satisfied]
```

(`test/vocabulary_tests.cpp`, `"a constraint's trace names quantities in the
sink's vocabulary"`, which gives `south` to a `RecordingSink` of its own.)
`explain` takes the vocabulary as an optional third argument, and every
`explain_*` twin and `traced` as an optional last one. Those three step kinds
are the only ones that name a quantity. Every other step names none --
arithmetic, a lookup, a rounding rule, a constraint, a method's constraints,
a variant selection and a replaced variant refer to their operands by number --
and so reaches the vocabulary through the steps beneath it.

The sink keeps its own copy of the vocabulary -- plain data holding views of
string literals -- so, unlike the `Trace`, the vocabulary need not outlive
the evaluation, and a temporary one is fine.

## The bound is a required argument, not a default

```cpp
struct StepLimit
{
    StepLimit() = delete;
    constexpr StepLimit(std::size_t steps) noexcept: value { steps } {}

    std::size_t value {};
};

struct TraceRenderOptions
{
    StepLimit maxSteps;
    NumberStyle numbers = NumberStyle::fraction();
};
```

`maxSteps` is a `StepLimit`, not a plain `std::size_t`, on purpose: a caller
who writes `render_trace(trace, {})` does not compile. A plain `std::size_t`
member with no default initialiser would not achieve that --
`TraceRenderOptions` is an aggregate, so `{}` would still value-initialise it
to zero and render nothing at all, silently, which is a worse outcome than
either a diagnostic or an unbounded render. `StepLimit` has no default
constructor, so there is no zero for `{}` to produce; `{.maxSteps = 10}` and
`{25}` both still work, because `StepLimit`'s own constructor is not
`explicit`. Every other option this library exposes with a sensible default
gets one -- `numbers`, the notation every value is written in, defaults to
fractions, and [Displaying numbers](display.md) shows the decimal styles --
while this one does not, because a sensible default does not exist. An
unbounded render of a derivation with a hundred thousand steps once collapsed
into one wall of text long enough to be practically unusable -- the same
failure mode `trace.hpp`'s flat, index-addressed arena exists to make
representable without recursion, just at the rendering end instead of the
storage end. A default limit is a limit someone forgets to raise or lower for
their own formula; a required one is a limit someone actually chose. When a
trace is longer than the bound,
`render_trace` shows the first `maxSteps` lines and then exactly one line
saying how many were left out -- never a silent truncation and never all of
them:

```
1. 0
2. 1
3. 2
... 97 further steps not shown
```

(`test/trace_render_tests.cpp`, `"a derivation longer than the limit is cut,
and says so"`, a 100-step trace rendered with `maxSteps = 3`.)

## Walking a `Trace` more than once

Constructing a `RecordingSink` over a `Trace` **begins a walk**, and a `Trace`
may hold the steps from more than one walk at once -- `root()` always names
the most recent one. `test/trace_tests.cpp` evaluates the same formula twice
into one `Trace`, with a fresh `RecordingSink` each time:

```cpp
formula::Trace<> trace {};

{
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);
    REQUIRE(result.has_value());
}
REQUIRE(trace.steps.size() == 4);
REQUIRE(trace.unclaimed == std::vector<std::size_t> { trace.root() });

{
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(10, 5), sink);
    REQUIRE(result.has_value());
}

CHECK(trace.steps.size() == 8);
CHECK(trace.unclaimed == std::vector<std::size_t> { trace.root() });
CHECK(trace.root() == 7);
```

(`test/trace_tests.cpp`, `"a second walk into the same Trace does not leave the
first walk's root unclaimed forever"`.) What is not supported, and carries no
runtime guard, is two sinks walking the same `Trace` **at once**: constructing
a second `RecordingSink` clears bookkeeping the first walk is still using, and
that first walk's next `produced` call reads out of an empty vector --
undefined behaviour. Nothing in this library does that to itself; it is a
precondition on a consumer who shares one `Trace` across two evaluations that
are not sequenced. Walk a `Trace` in sequence, never concurrently.

## The extension point: your node evaluates, but is it traced?

Evaluation has a two-parameter extension point -- a consumer writes their
own node kind and a `checked_evaluate_si(node, environment)` overload for it,
found by ADL. Adding a sink parameter to every overload the library ships
could have broken every such overload by making it invisible to the
dispatcher; instead, `detail::dispatch` prefers a sink-aware, three-parameter
overload where one exists for a node and falls back to the older
two-parameter one where it does not:

```cpp
template <typename Rep, typename N, typename Env, typename Sink>
[[nodiscard]] constexpr auto dispatch(N const& node, Env const& environment, Sink sink) noexcept
{
    if constexpr (requires { checked_evaluate_si<Rep>(node, environment, sink); })
        return checked_evaluate_si<Rep>(node, environment, sink);
    else
        return checked_evaluate_si<Rep>(node, environment);
}
```

(`sink.hpp`.) A node written against the older, two-parameter extension point
therefore keeps evaluating correctly, with the right answer, composed with any
other node exactly as before. What it does **not** do is contribute anything
to a trace -- there is no overload to call the sink through, so `entered` and
`produced` are simply never called for that node. `test/sink_tests.cpp` proves
both halves of this at once, by counting: a `LegacyNode` added to a `Mass`
gets the right sum, `12`, but the sink only ever hears about the two nodes
that know it exists:

```cpp
auto const result = formula::checked_evaluate_si<formula::Rational>(expression, environmentOf(5, 1), sink);

REQUIRE(result.has_value());
REQUIRE(result->has_value());
CHECK(**result == formula::Rational { 12 });
// Two nodes reported, not three: the legacy node is evaluated but not
// traced, because nothing told the library how to trace it.
CHECK(entered == 2);
CHECK(produced == 2);
```

(`test/sink_tests.cpp`, `"a node written against the two-parameter extension
point still evaluates"`.) This is worth stating plainly rather than leaving it
to be discovered: adding your own node kind to this library gets you correct
arithmetic for free and a traced subtree for nothing -- no warning, no
diagnostic, just a derivation with a gap in it exactly where that node stood.
`render_trace` cannot even show the gap, because nothing was ever recorded to
show; the tree beneath an untraced node vanishes from the derivation as
completely as if the formula had been written without it.

**That graceful degradation belongs to the two-parameter overload alone.** It
would be natural to conclude that a consumer who wants their node traced
writes the three-parameter overload instead -- calling `sink.entered(node)`
before evaluating its operands and `sink.produced(node, result)` after, the
shape every evaluator overload in this library follows. Against `NullSink`, or
a sink of the consumer's own, that compiles and works. Against
`RecordingSink` it **does not compile**: `RecordingSink` looks up every node's
kind in `detail::StepKindOf` (`trace.hpp`), a closed registry whose primary
template is deliberately left undefined, and a consumer's node has no entry
there. g++ 13.3 reports "incomplete type
`formula::detail::StepKindOf<AwareNode>` used in nested name specifier", and
cl 19.51 reports C2027, "use of undefined type". So today a consumer's own
node cannot appear in a recorded trace at all. What does compile is a
three-parameter overload that only hands the sink on to its operands'
`detail::dispatch` and reports nothing of its own: its operands are traced,
and it is not -- measured on the same two compilers. Opening the registry to consumers is a separate change from
anything this guide describes. A computation of a consumer's own that the page need not
spell out can be traced today as an opaque operation instead: its inputs and outputs are
traced, and its line says its inside is not shown -- see
[Opaque operations and bounded retry](opaque-and-retry.md).

## Every citation here is invented

Every citation used to demonstrate tracing on this page -- and in
`examples/tracing.cpp` and the gallery's derivation -- names a fictional
`Example Standard`, never a real one, for the reason `docs/citations.md` gives
in full: a real standard's clause numbers and equations are copyrighted
material, and this is a public repository.
