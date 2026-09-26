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
for one. `formula::explain<Result>` builds a `RecordingSink` for you and
evaluates through it:

```cpp
template <Described Result, typename Rep = Rational, Node Expression, typename Env>
[[nodiscard]] Explained<Result, Rep> explain(Expression const& expression, Env const& environment)
{
    static_assert(std::is_same_v<Rep, Rational>, /* ... */);

    Explained<Result, Rep> explained {};
    RecordingSink<Rep> sink { explained.trace };
    explained.outcome = evaluate<Result>(expression, environment, sink);
    return explained;
}
```

(`trace.hpp`.) `explained.outcome` is exactly what `evaluate<Result>(expression,
environment)` would have returned -- tracing observes, it does not
participate -- and `explained.trace` is the derivation. Reach for `evaluate` or
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
allocation a constant expression is not allowed to produce. Writing

```cpp
constexpr auto explained = formula::explain<Density>(densityFormula, env);
```

fails to compile, verified with cl 19.51:

```
error C2131: expression did not evaluate to a constant
note: failure was caused by call of undefined function or one not declared 'constexpr'
note: see usage of 'formula::explain'
```

Evaluate at compile time when you can; `explain` is a run-time-only way to see
the working.

## Reading a derivation

`examples/tracing.cpp` builds the same water/cement ratio
`examples/citations.cpp` evaluates -- 180 l of water, 300 l of cement, with the
same invented citation attached by `documented()` -- and prints its trace:

```cpp
formula::Explained<WaterCementRatio> const explained = formula::explain<WaterCementRatio>(ratio, inputs);
std::string const trace = formula::render_trace(explained.trace, { .maxSteps = 10 });
std::printf("%s", trace.c_str());
```

which prints, verbatim:

```
1. V_w = 180 l
2. V_c = 300 l
3. #1 / #2 = 3/5
4. #3 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)]
```

Every line is one node the evaluator visited, numbered from one in the order
each finished -- children before parents, so an operand's line always appears
above the line that names it. A step that consumed earlier steps names them by
number, `#1` and `#2`; the citation on the last line is the one `documented()`
attached, and it appears only on the step for the `DocumentedNode` itself, not
on the division it wraps.

The two leaves read `180 l` and `300 l`, not the `9/50` and `3/10` cubic
metres the arithmetic actually runs on. Every `Step` stores its value in the
**coherent SI unit** of its dimension -- the one scale every step's value can
be compared on -- but also remembers the unit it was *declared* in, and
`render_trace` converts back before printing. `Step`'s own comment explains why
the recorder, not the renderer, has to be the one holding that unit:

```cpp
/// The unit this step's value was **declared** in -- `Describe<Q>::unit`
/// for a variable, the constant's own unit for a constant, and the
/// coherent SI unit of `dimension` for anything computed, which has no
/// declared unit of its own.
///
/// `value` is always in the coherent SI unit, so that steps are
/// comparable; this is what a renderer converts back to before showing a
/// number to a person. Without it a derivation restates every input in a
/// unit nobody typed: someone who entered 180 l reads `9/50`, which is
/// the same volume and a worse record. The renderer cannot recover this
/// on its own -- by the time a `Step` exists the quantity type is erased,
/// so the recorder captures it here.
Unit unit {};
```

(`trace.hpp`.) A quantity's C++ type exists only while the evaluator is
walking that quantity's own node; by the time `RecordingSink::produced` builds
a `Step` for it, the type is gone and only the runtime `Unit` value survives.
Capturing anything less at that point -- the coherent SI unit alone, say --
would make `render_trace` unable to ever show `180 l` again; it would show
`9/50 m3`, arithmetically identical and a strictly worse record of what
someone actually typed.

A step that is a plain computation, `#1 / #2` above, carries no declared unit
of its own -- it's whatever the coherent SI unit of its dimension is, which
`test/trace_render_tests.cpp` pins directly for a squared mass over a volume:

```
1. m = 6 kg
2. #1^2 = 36
3. V = 3 m3
4. #2 / #3 = 12
```

(`test/trace_render_tests.cpp`, `"a derivation renders one line per step, in
order"`.) `#1^2` and `#2 / #3` carry no unit symbol at all -- and the reason is
not that `kg2` and `kg2/m3` are awkward to spell. `coherent()`
(`evaluate.hpp`) hands **every** computed step a `Unit` with no symbol at all,
whatever its dimension: a computed *mass* prints no `kg` either, nor a
computed length its `m`. A compound dimension is simply the case where the
absence is most obvious, since there is no everyday symbol to miss; the
behaviour itself applies uniformly to anything the evaluator computed rather
than declared.

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
formula::Trace<> trace {};
formula::RecordingSink<> sink { trace };
(void) formula::evaluate_method<specimen::Cube>(compressiveStrength, inputs, sink);
std::printf("%s", formula::render_trace(trace, { .maxSteps = 20 }).c_str());
```

```
1. F = 562 kN
2. 22500 mm2
3. #1 / #2 = 224800000/9
4. round(#3, in MPa) = 25 MPa [rounded to 1 dp (method default); nearest, ties away from zero]
5. #4 = 25 MPa [variant Cube (1st of 3), selected by tag]
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
`Sized<150>`, with their qualification stripped the same way. One difference
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
    static constexpr std::string_view of() noexcept { return "cylinder 150 x 300 mm"; }
};
```

## Whose rounding rule, and whose constant

The rounding line above is a step of its own kind,
`StepKind::RoundingRuleApplied`, rather than an ordinary rounding step. Spec
section 9.1 asks the trace to record which rounding rule applied **and where
it came from**, and `rounded to 1 dp` alone is true whether the method's
author chose the rule or a jurisdiction did. So the bracket says whose it was:
`(method default)` for the rule the method was declared with, and
`(jurisdiction overlay)` for one an overlay's `with_rounding` put in its place,
followed by what the overlay cited when it cited anything:

```
8. round(#7, in MPa) = 601/100 MPa [rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, NA.4.1); nearest, ties away from zero]
```

(`test/overlay_tests.cpp`, `"the trace says where the rounding rule came
from"`.) A jurisdiction that restates the method's own granularity still gets
`(jurisdiction overlay)`: the rule is then its rule, and the trace does not
decide whose it was by comparing numbers. The provenance is in
`Step::roundingProvenance`, and the citation in `Step::citation`.

A constant an overlay fixed with `with_constant` is traced the same way, as
`StepKind::OverriddenConstant` rather than as a variable. It reads as its
quantity, but it says the value was not the specimen's:

```
1. k_s = 97/100 [fixed by jurisdiction overlay: Shape factor, Example Standard 12:2021 NA, NA.2.3]
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
2. 50000 N
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

The provenance a trace reports is only ever the library's to state. The nodes
an overlay leaves behind -- a fixed constant, a derived quantity, a replaced
formula -- can be built only by the overlay, and building one by hand is
refused in the library's words. A
`RoundingRule` claims a jurisdiction's overlay only when `with_rounding`
produced it, and the rounding node a method applies is built only by
`evaluate_method`, from the method's own rule, which it holds rather than a
provenance of its own. So "(method default)" and "(jurisdiction overlay)"
are only ever said of a method's rule. A method's constraints are a
jurisdiction's only when they are the `OverlaidConstraints` that
`with_constraints` produced -- which carries the overlay's citation with the
constraints themselves -- and building one by hand is refused.

What the guard governs is how a rule or a set of constraints is created, not
where a copy travels, and a copy stays true of itself: a method holding a copy
of an overlay's rule is traced as that overlay's rule, and a method built from
an overlaid method's `constraintSet` checks the jurisdiction's constraints and
says so, because they are the jurisdiction's. Three routes remain that no
type can close. `method(o.variantSet, o.rounding, o.constraintSet.constraintSet())`
hands the jurisdiction's constraints over as a plain set in one call, which
makes them the new method's own -- reading them has to be possible.
Reinterpreting an object's bytes makes it anything. And explicitly
specialising `OverlaidConstraints` over a program's own types declares
whatever the specialisation likes: specialising a library template is outside
this library's contract, and no code can forbid it.
Nor does the guard reach a sink's own hooks, which are public: code that calls
them by hand writes whatever trace it likes.

## Whose symbols

A `Variable`, `OverriddenConstant` or `DerivedQuantity` step records its
quantity's symbol **when the formula is evaluated**, and `render_trace` only
reads it back. So a jurisdiction's vocabulary (see [Citations and rendering](citations.md)) has to
be given to the sink, not only to `render()` -- a page rendered in one
vocabulary and a trace recorded in another would name one quantity with two
different letters:

```cpp
formula::Trace<> southern {};
(void) formula::check(limit, crossedInputs, formula::RecordingSink { southern, south });
```

```
1. E = 30 MPa
2. R = 12 MPa
3. require #1 >= #2 [satisfied]
```

(`test/vocabulary_tests.cpp`, `"a constraint's trace names quantities in the
sink's vocabulary"`.) `explain` takes the vocabulary as an optional third
argument. Those three step kinds are the only ones that name a quantity.
Every other step names none -- arithmetic, a lookup, a rounding rule, a
constraint, a method's constraints, a variant selection and a replaced
variant refer to their operands by number -- and so reaches the vocabulary through the steps beneath
it.

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
gets one; this one does not, because a sensible default does not exist. An
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

Phase 5 published a two-parameter extension point -- a consumer writes their
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
anything this guide describes.

## Every citation here is invented

Every citation used to demonstrate tracing on this page -- and in
`examples/tracing.cpp` and the gallery's derivation -- names a fictional
`Example Standard`, never a real one, for the reason `docs/citations.md` gives
in full: a real standard's clause numbers and equations are copyrighted
material, and this is a public repository.
