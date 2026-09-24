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
explicit constexpr RecordingSink(Trace<Rep>& trace) noexcept: _trace { &trace }
{
    _trace->marks.clear();
    _trace->unclaimed.clear();
}
```

and that pointer, `Trace<Rep>* _trace`, is the whole of `RecordingSink`'s
storage (`trace.hpp`). `RecordingSink` is a **handle**, not an owner: the
caller owns the `Trace` and it must outlive the walk. One pointer copies for
free at every node; a `Trace` copied at every node would not.

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
completely as if the formula had been written without it. A consumer who wants
their own node traced writes the third-parameter overload -- calling
`sink.entered(node)` before evaluating its operands and `sink.produced(node,
result)` after, the same shape every evaluator overload in this library
already follows.

## Every citation here is invented

Every citation used to demonstrate tracing on this page -- and in
`examples/tracing.cpp` and the gallery's derivation -- names a fictional
`Example Standard`, never a real one, for the reason `docs/citations.md` gives
in full: a real standard's clause numbers and equations are copyrighted
material, and this is a public repository.
