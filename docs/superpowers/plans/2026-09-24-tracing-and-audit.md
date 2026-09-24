# Tracing and Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every evaluation able to explain itself — the same expression, the same evaluator, either silent or recording a step-by-step derivation — without the silent path paying for the recording one.

**Architecture:** The evaluator gains a third parameter, a **sink**, passed **by value** and defaulted to a stateless `NullSink`. Every recursive call goes through a dispatcher that prefers a sink-aware overload and falls back to the two-parameter one phase 5 published, so a consumer's own node keeps working. A `RecordingSink` is a *handle* — a pointer to a `Trace` the caller owns — because a by-value sink that owned its arena would copy the arena at every node. The trace itself is a flat `std::vector<Step>` in which a step names its operands by **index**, which makes teardown a single vector destructor with no recursion at any depth.

**Tech Stack:** C++23, header-only. Catch2 via CPM for tests. MSVC `cl`, `clang-cl`, `clang++`, GCC.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — §11 is this phase. Read §9 (expressions), §10 (provenance) and the evaluation half of §5 for the surface being extended.

**Prep artefacts (read these before Task 1, they are the reasoning behind the design):**
- `.superpowers/sdd/phase-7-prep/sink-overhead-measurement.md` — the codegen measurements, and why §11's existing zero-overhead claim does not transfer to the shipped library.
- `.superpowers/sdd/phase-7-prep/design-decisions.md` — the eight decisions those measurements force, each with its cost-if-wrong.

## Global Constraints

- **C++23, header-only.** No `.cpp` in `include/`.
- **Must compile warnings-clean on MSVC `cl`, `clang-cl`, `clang++` and GCC.** Warnings are errors in CI.
- **Open source, no company IP, as generic as possible.** This repository is public.
- **No norm content.** Nothing from a real standard appears here: not its text, tables, equations, threshold or constant values, and not its identifiers, clause or table numbers — not even as a bare citation. Every citation you write is invented, of the form `Example Standard N:2020`. Examples use **generic physics only** (density, flow rate, mass, volume).
- **No macros for traceability.** (§11's opening requirement.)
- **`<string>`, `<vector>`, `<format>` and `<iostream>` must not reach `include/formula-cpp/formula.hpp`,** directly or through any header it includes. `render.hpp`, `document.hpp` and (new in this phase) `trace.hpp` and `trace_render.hpp` are the declared opt-in exceptions, enforced by `cmake/CheckPublicHeaderIncludes.cmake`. Each declares only the banned headers it actually uses, and the check fails if it uses another.
- **A failed assertion must never open a Windows dialog.**
- **The evaluator must never pre-render anything for the sink.** It hands over raw nodes and raw values; the recording sink does the formatting. (§11, and the measurements depend on it — see Task 3.)
- **Never recurse on teardown.** (§11 scar: a chain of tens of thousands of nodes segfaulted on destruction in earlier work.)
- **Always bound rendered steps.** (§11 scar: an unbounded render collapsed a hundred thousand steps into one unusable line.)

---

## File Structure

| File | Responsibility | In the umbrella? |
|---|---|---|
| `include/formula-cpp/sink.hpp` (new) | The `entered`/`produced` seam, `NullSink`, and `detail::dispatch`. Pulls no container and no string. | **Yes** |
| `include/formula-cpp/evaluate.hpp` (modify) | Every `checked_evaluate_si` overload gains `Sink sink = {}`; recursion goes through `dispatch`. | Yes (already) |
| `include/formula-cpp/function.hpp` (modify) | Same, for `PowerNode`, `RootNode`, `PiNode`. | Yes (already) |
| `include/formula-cpp/citation.hpp` (modify) | Same, for `DocumentedNode`. | Yes (already) |
| `include/formula-cpp/trace.hpp` (new) | `StepKind`, `Step`, `Trace`, `RecordingSink`, `explain`. Pulls `<vector>` (not `<string>` — nothing here formats). | **No — opt-in** |
| `include/formula-cpp/trace_render.hpp` (new) | Bounded rendering of a `Trace` to text. Pulls `<string>`. | **No — opt-in** |
| `test/sink_tests.cpp` (new) | The seam: dispatch selects, fallback works, `NullSink` changes nothing. | — |
| `test/trace_tests.cpp` (new) | Arena shape, operand indices, short-circuit, deep-tree teardown. | — |
| `test/trace_render_tests.cpp` (new) | Bounding, elision, exact output. | — |
| `docs/tracing.md` (new) | The guide. Every snippet taken from compiled code. | — |
| `tools/gallery/main.cpp` (modify) | One worked derivation, so the trace is dogfooded. | — |

**Why two opt-in headers rather than one.** `trace.hpp` is what you need to *record* a derivation; `trace_render.hpp` is what you need to *print* one. An audit pipeline that serialises a trace to its own format needs the first and not the second, and should not compile string formatting it never calls. This mirrors the split phase 6 already established between `document.hpp` and `render.hpp`.

---

## Task 1: The tracing seam — `sink.hpp`

**Files:**
- Create: `include/formula-cpp/sink.hpp`
- Create: `test/sink_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `sink_tests.cpp` to the source list, alphabetically — it goes between `rounding_tests.cpp` and `unit_tests.cpp`)

**Interfaces:**
- Consumes: `Node` concept from `expression.hpp`.
- Produces: `formula::NullSink`; `formula::detail::dispatch<Rep>(node, environment, sink)`. Task 2 routes every recursive evaluator call through `dispatch`. Tasks 3–5 write sinks that satisfy the same shape as `NullSink`.

**Context you need.** `checked_evaluate_si` is an overload set found by ADL at the point of instantiation — `evaluate.hpp` declares overloads for `VarNode`, `ConstantNode`, `UnaryNode` and `BinaryNode`, `function.hpp` adds three more, `citation.hpp` one more. `dispatch` is declared in `sink.hpp`, which is included *before* any of them, so at the point `dispatch` is **defined** no `checked_evaluate_si` is visible. This is fine and was measured: the body is only instantiated later, when ADL can see the whole set. All four compilers accept it. Do not try to "fix" it by moving `dispatch` later — `evaluate.hpp` needs it.

- [ ] **Step 1: Write the failing test**

Create `test/sink_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/sink.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Volume: formula::Quantity<Volume, "V", "specimen volume", unit::CubicMetre>
{
};

/// Counts the nodes it is told about. Not `NullSink`: if the dispatcher ever
/// silently preferred the two-parameter overload, this would count zero while
/// still producing the right number -- which is exactly the failure a codegen
/// comparison cannot see.
struct CountingSink
{
    int* enteredCount {};
    int* producedCount {};

    template <formula::Node N>
    constexpr void entered(N const&) noexcept
    {
        ++*enteredCount;
    }

    template <formula::Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
        ++*producedCount;
    }
};

[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}
} // namespace

TEST_CASE("NullSink is empty, stateless, and satisfies the seam for every node kind", "[sink]")
{
    // Empty matters: the measurements that justify passing a sink by value
    // depend on there being nothing to pass.
    STATIC_REQUIRE(std::is_empty_v<formula::NullSink>);

    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    STATIC_REQUIRE(formula::SinkFor<formula::NullSink, decltype(density), formula::Rational>);
    STATIC_REQUIRE(formula::SinkFor<formula::NullSink, decltype(var<Mass>), formula::Rational>);

    // Callable, and callable in a constant expression.
    formula::NullSink sink {};
    sink.entered(density);
    sink.produced(density, formula::Rational { 12 });
}

TEST_CASE("CountingSink satisfies the seam too, so Task 2 can use it", "[sink]")
{
    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    sink.entered(density);
    sink.produced(density, formula::Rational { 12 });

    CHECK(entered == 1);
    CHECK(produced == 1);
}
```

`<type_traits>` is needed for `std::is_empty_v`.

**The tests that matter -- a sink being told about all four nodes of a tree,
and `NullSink` not changing the answer -- belong to Task 2, because they need
the evaluator to accept a third argument. Task 1 delivers the seam; Task 2
connects it. Do not try to write them here.**

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build out/build/cl-debug
```

Expected: **compile error**, `'sink.hpp': No such file or directory`.

- [ ] **Step 3: Write `sink.hpp`**

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The tracing seam: what the evaluator tells an observer, and the observer
/// that costs nothing.
///
/// A sink is passed to the evaluator **by value**, not by reference. That is
/// not a style choice: passing a stateless sink by reference forces the
/// compiler to materialise the address of an empty object that nothing reads,
/// which clang emits as a real instruction at every call site. By value it
/// disappears. Measured on cl 19.51, clang-cl 22.1.3, clang++ 22.1.3 and
/// g++ 13.3 -- see the phase-7 prep notes.
///
/// The consequence for anyone writing a stateful sink: keep it small and
/// cheap to copy. A sink that owns its storage would copy that storage at
/// every node. `RecordingSink` in `trace.hpp` is a pointer to a `Trace` the
/// caller owns, for exactly this reason.

#include <formula-cpp/expression.hpp>

namespace formula
{

/// What the evaluator tells an observer as it walks a tree.
///
/// `entered` comes before the node's operands are evaluated, `produced` after
/// the node has its answer -- so a sink sees the tree on the way down and the
/// values on the way up.
///
/// **`produced` is not guaranteed for every node that was entered.** When a
/// node fails, its parent returns without evaluating its remaining operands,
/// so those operands are neither entered nor produced. A sink that pairs the
/// two must tolerate an `entered` with no matching `produced` for the failing
/// node's siblings. `RecordingSink` handles this by recording what actually
/// arrived rather than what the node's arity predicts.
template <typename S, typename N, typename V>
concept SinkFor = requires(S sink, N const& node, V const& value) {
    sink.entered(node);
    sink.produced(node, value);
};

/// An observer that observes nothing: the default, and the one the untraced
/// path uses. Empty and stateless, so a by-value copy is free.
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

namespace detail
{
    /// Evaluates @p node with @p sink, through whichever overload exists.
    ///
    /// Phase 5 published `checked_evaluate_si(node, environment)` as an
    /// extension point: a consumer with their own node kind writes an overload
    /// and the evaluator finds it by ADL. This phase adds a third parameter,
    /// which would leave every such overload unreachable. The `requires` below
    /// prefers a sink-aware overload where one exists and falls back to the
    /// two-parameter one where it does not, so a consumer's existing node keeps
    /// evaluating correctly. It simply contributes no trace steps -- the honest
    /// outcome, since the library was never told how to trace it. A consumer
    /// who wants their node traced adds the third parameter.
    ///
    /// `checked_evaluate_si` is not declared at this point in the include
    /// order, and does not need to be: the body is instantiated later, where
    /// ADL sees the whole overload set. Verified on all four compilers.
    template <typename Rep, typename N, typename Env, typename Sink>
    [[nodiscard]] constexpr auto dispatch(N const& node, Env const& environment, Sink sink) noexcept
    {
        if constexpr (requires { checked_evaluate_si<Rep>(node, environment, sink); })
            return checked_evaluate_si<Rep>(node, environment, sink);
        else
            return checked_evaluate_si<Rep>(node, environment);
    }
} // namespace detail

} // namespace formula
```

- [ ] **Step 4: Register the test**

Add `sink_tests.cpp` to the source list in `test/CMakeLists.txt`.

- [ ] **Step 5: Run and confirm green**

```bash
cmake --build out/build/cl-debug && ctest --test-dir out/build/cl-debug --output-on-failure
```

Expected: PASS, 238 tests (236 + the 2 new ones).

- [ ] **Step 6: All four presets**

```bash
for p in cl-debug cl-release clangcl-debug clangcl-release; do cmake --build out/build/$p && ctest --test-dir out/build/$p; done
```

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/sink.hpp test/sink_tests.cpp test/CMakeLists.txt
git commit -m "feat(sink): the seam a tracing observer plugs into"
```

---

## Task 2: Thread the sink through every evaluator overload

**Files:**
- Modify: `include/formula-cpp/evaluate.hpp` — the four `checked_evaluate_si` overloads, plus `checked_evaluate` and `evaluate`
- Modify: `include/formula-cpp/function.hpp` — `PowerNode`, `RootNode`, `PiNode`
- Modify: `include/formula-cpp/citation.hpp` — `DocumentedNode`
- Modify: `test/sink_tests.cpp` — add the two tests that need the third parameter
- Modify: `docs/citations.md` if it documents the extension point's signature (grep for `checked_evaluate_si` first)

**Interfaces:**
- Consumes: `NullSink`, `detail::dispatch` from Task 1.
- Produces: every `checked_evaluate_si` overload now has the shape
  `checked_evaluate_si<Rep>(Node const&, Env const&, Sink sink = {})`, and
  `explain` in Task 5 relies on being able to pass a `RecordingSink` to the outermost call.

**The acceptance criterion for this task is that nothing else changes.** All 238 tests from Task 1 pass unmodified. If a test needs editing, you have changed behaviour and should stop and say so rather than editing the test.

- [ ] **Step 1: Write the failing tests**

Add to `test/sink_tests.cpp`. First the environment helper — Task 1's file does
**not** define it (an unused helper there fails `-Wunused-function -Werror` on
clang-cl), so add it inside the existing anonymous namespace:

```cpp
[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}
```

Then the tests:

```cpp
TEST_CASE("a sink is told about every node of the tree", "[sink]")
{
    // pow<2>(m) / V is four nodes: the division, the power, and two variables.
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;

    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);

    CHECK(entered == 4);
    CHECK(produced == 4);
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    CHECK(**result == formula::Rational { 12 });
}

TEST_CASE("NullSink changes neither the answer nor whether one is produced", "[sink]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    auto const environment = environmentOf(6, 3);

    auto const untraced = formula::checked_evaluate_si<formula::Rational>(density, environment);
    formula::NullSink sink {};
    auto const traced = formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    REQUIRE(untraced.has_value());
    REQUIRE(traced.has_value());
    CHECK(*untraced == *traced);
}
```

Build.

Expected: FAIL -- `no matching function for call to 'checked_evaluate_si'` with three arguments.

- [ ] **Step 2: Add the parameter to the four overloads in `evaluate.hpp`**

Each of the four gains `typename Sink = NullSink` at the end of its template parameter list and `Sink sink = {}` at the end of its function parameter list, calls `sink.entered(node)` first and `sink.produced(node, <result>)` on **every** return path, and routes recursion through `detail::dispatch`.

`VarNode` — note its node parameter is currently unnamed and must be named:

```cpp
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(VarNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Measured<Q> const measured = environment.template get<Q>();
    if (measured.is_absent())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }
    Evaluated<Rep> const result = detail::in_si<Rep>(*measured.stored(), Describe<Q>::unit);
    sink.produced(node, result);
    return result;
}
```

`ConstantNode`:

```cpp
template <typename Rep = Rational, Unit U, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ConstantNode<U> const& node,
                                                           Env const&,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const result = detail::in_si<Rep>(node.number, U);
    sink.produced(node, result);
    return result;
}
```

`UnaryNode`:

```cpp
template <typename Rep = Rational, UnaryOperator Op, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UnaryNode<Op, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const operand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!operand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { operand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!operand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    std::expected<Rep, ArithmeticError> const negated = RepTraits<Rep>::negate(**operand);
    Evaluated<Rep> const result =
        negated.has_value() ? detail::present<Rep>(*negated) : Evaluated<Rep> { std::unexpected { negated.error() } };
    sink.produced(node, result);
    return result;
}
```

`BinaryNode` — keep the existing left-then-right, error-before-absence order exactly as it is; only add the sink calls and the `dispatch` routing:

```cpp
template <typename Rep = Rational, BinaryOperator Op, Node Left, Node Right, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(BinaryNode<Op, Left, Right> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const lhs = detail::dispatch<Rep>(node.lhs, environment, sink);
    if (!lhs.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { lhs.error() };
        sink.produced(node, failed);
        return failed;
    }
    Evaluated<Rep> const rhs = detail::dispatch<Rep>(node.rhs, environment, sink);
    if (!rhs.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { rhs.error() };
        sink.produced(node, failed);
        return failed;
    }

    if (!lhs->has_value() || !rhs->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const combined = [&] {
        if constexpr (Op == BinaryOperator::Add)
            return RepTraits<Rep>::add(**lhs, **rhs);
        else if constexpr (Op == BinaryOperator::Subtract)
            return RepTraits<Rep>::subtract(**lhs, **rhs);
        else if constexpr (Op == BinaryOperator::Multiply)
            return RepTraits<Rep>::multiply(**lhs, **rhs);
        else
            return RepTraits<Rep>::divide(**lhs, **rhs);
    }();
    Evaluated<Rep> const result =
        combined.has_value() ? detail::present<Rep>(*combined) : Evaluated<Rep> { std::unexpected { combined.error() } };
    sink.produced(node, result);
    return result;
}
```

Add `#include <formula-cpp/sink.hpp>` to `evaluate.hpp`'s include list.

- [ ] **Step 3: Same for `function.hpp` and `citation.hpp`**

`PowerNode` and `RootNode` follow `UnaryNode`'s shape exactly: `sink.entered(node)` first, `detail::dispatch<Rep>(node.operand, environment, sink)` for the operand, `sink.produced(node, result)` on every return path. `PiNode` follows `ConstantNode`'s shape and must have its node parameter **named** (it is currently unnamed). `DocumentedNode` in `citation.hpp` follows `UnaryNode`'s shape, recursing into `node.inner`.

Add `#include <formula-cpp/sink.hpp>` to both.

- [ ] **Step 4: Give `checked_evaluate` and `evaluate` a sink too**

Both currently call `checked_evaluate_si<Rational>(expression, environment)`. Give each `typename Sink = NullSink` and a trailing `Sink sink = {}`, and pass it down. Their existing two-argument call sites keep compiling because the parameter is defaulted — verify this by **not** editing any existing test.

- [ ] **Step 5: Run the tests**

```bash
cmake --build out/build/cl-debug && ctest --test-dir out/build/cl-debug --output-on-failure
```

Expected: PASS, 240 tests (238 + the 2 added in Step 1). **No test from Task 1 or earlier edited.**

- [ ] **Step 6: Prove the fallback branch is real**

Add to `test/sink_tests.cpp` a node type with only a two-parameter overload, and confirm it evaluates correctly while contributing nothing to the count:

```cpp
namespace
{
/// A consumer's own node, written against the extension point as phase 5
/// published it: two parameters, no knowledge of sinks.
struct LegacyNode: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};
} // namespace

namespace formula
{
template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(LegacyNode const&, Env const&) noexcept
{
    return detail::present<Rep>(Rep { 7 });
}
} // namespace formula

TEST_CASE("a node written against the two-parameter extension point still evaluates", "[sink]")
{
    constexpr auto expression = LegacyNode {} + var<Mass>;

    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    auto const result = formula::checked_evaluate_si<formula::Rational>(expression, environmentOf(5, 1), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    // 7 from the legacy node plus 5 kg: the legacy overload was called and its
    // answer used.
    CHECK(**result == formula::Rational { 12 });
    // Two nodes reported, not three: the legacy node is evaluated but not
    // traced, because nothing told the library how to trace it.
    CHECK(entered == 2);
    CHECK(produced == 2);
}
```

- [ ] **Step 7: Confirm the dispatcher is not silently always-falling-back**

This is the check that makes Step 6 meaningful. Temporarily change `dispatch`'s `if constexpr (requires { ... })` to `if constexpr (false)`. Rebuild.

Expected: **both** `CountingSink` tests fail, each reporting `entered == 1`
rather than their expected 4 and 2, while every computed value stays correct.

One, not zero, and both tests rather than one: the tests call
`checked_evaluate_si` directly at the top level, naming the sink themselves, so
the root node is entered and produced whatever `dispatch` does. Only *recursive*
calls go through `dispatch`, so under the mutation every node below the root
loses its tracing — including `VarNode<Mass>` in the legacy test, which has a
perfectly good three-parameter overload. The legacy node itself was never traced
either way; what the mutation destroys is the tracing of everything around it.

Restore the condition, rebuild, confirm both pass at 4 and 2. **Record both
directions in your report.** A dispatcher that always took the fallback would
compile, produce every correct number, and trace almost nothing — no codegen
comparison and no green test suite can see that.

- [ ] **Step 8: All four presets, then commit**

```bash
git add include/formula-cpp/evaluate.hpp include/formula-cpp/function.hpp include/formula-cpp/citation.hpp test/sink_tests.cpp
git commit -m "feat(evaluate): carry a sink through the walk, defaulting to silence"
```

---

## Task 3: Record what §11's zero-overhead claim actually means

**Files:**
- Modify: `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` §11

**Interfaces:**
- Consumes: nothing.
- Produces: nothing in code. This task exists because a claim nobody can reproduce is worse than no claim.

**Why this is a task and not a footnote.** §11 currently says the untraced path emits *byte-identical machine code to a hand-written expression*, and shows `divsd xmm0, xmm1 / ret`. That was measured on the godbolt prototype, which evaluated bare `double`s. The shipped library cannot reproduce it and never could: `Measured<Q>` stores an exact `Rational` and evaluation returns `std::expected<std::optional<Rep>, ArithmeticError>`. Leaving the claim as written means the next person to check it will find it false and have no way to know whether the design regressed or the claim was always wrong.

- [ ] **Step 1: Replace the claim**

Replace §11's "**Verified zero-overhead**" paragraph and its code block with:

```markdown
**Zero-overhead, with its boundary stated.** The claim worth making is not that
the untraced path matches a hand-written expression — it cannot, because
`Measured<Q>` stores an exact `Rational` and evaluation returns
`std::expected<std::optional<Rep>, ArithmeticError>`. The claim is that
**adding a sink does not change what the evaluator emits**, measured against
the sinkless evaluator as the baseline.

Measured at `-O2`/`/O2` on the real library, counting instructions in the
compiled probe body:

| Compiler | Sink passed | Base | With `NullSink` | Difference |
|---|---|---|---|---|
| clang++ 22.1.3 | by reference | 176 | 177 | one `leaq` materialising an unread address |
| clang++ 22.1.3 | by value | 176 | 176 | callee's mangled name only |
| clang-cl 22.1.3 | by value | 189 | 189 | callee's mangled name only |
| cl 19.51 | by value | 80 | 81 | one `xor r9d, r9d`, never read |
| g++ 13.3 | by value | 126 | 126 | nothing |

The `cl` row is the boundary: when the evaluator does **not** inline, MSVC
materialises a zero for the empty parameter. When it does inline — the normal
case — every compiler measured emits identical code, MSVC included, with not
even a symbol-name difference. The same probe with a formula small enough to
inline is 162 instructions on `cl` with and without the sink.

This is why the sink is a **by-value** parameter and why `RecordingSink` is a
handle rather than an owner: the by-reference row above is the cost of getting
that wrong.
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/specs/2026-09-23-formula-cpp-design.md
git commit -m "docs(spec): state the zero-overhead claim the library can actually meet"
```

---

## Task 4: The trace arena — `trace.hpp`

**Files:**
- Create: `include/formula-cpp/trace.hpp`
- Create: `test/trace_tests.cpp`
- Modify: `test/CMakeLists.txt`
- Modify: `cmake/CheckPublicHeaderIncludes.cmake` — add `trace.hpp` to `exemptHeaders` and `"trace.hpp=vector"` to `exemptAllowances` (not `string,vector`: the header never includes `<string>`)

**Interfaces:**
- Consumes: `NullSink`'s shape from Task 1; the sink calls added in Task 2.
- Produces: `formula::StepKind`, `formula::Step<Rep>`, `formula::Trace<Rep>`, `formula::RecordingSink<Rep>`. Task 5 renders a `Trace`; Task 6's `explain` returns one.

**The structural decision, and why.** A step names its operands by **index into a flat vector**, never by pointer or by owning a child. Two consequences, both required by §11: destroying a trace is one vector destructor regardless of depth (the "never recurse on teardown" scar), and a trace serialises without recursion.

**How operands are discovered.** The obvious approach — pop `arity(node)` indices when a node is produced — is wrong here, and the reason is in the evaluator's control flow: when `BinaryNode`'s left operand fails, it returns **before evaluating the right one**, so a binary node can produce with only one operand recorded. Instead, `entered` pushes a mark (the arena size at entry) and `produced` claims every *unclaimed* step recorded at or after that mark. A node therefore records the operands it actually got, not the operands its type predicts.

- [ ] **Step 1: Write the failing test**

Create `test/trace_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Volume: formula::Quantity<Volume, "V", "specimen volume", unit::CubicMetre>
{
};

[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}
} // namespace

TEST_CASE("a trace records one step per node, children before parents", "[trace]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 4);

    // Post-order: m, m^2, V, then the division.
    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK(trace.steps[0].symbol == "m");
    CHECK(trace.steps[0].value == formula::Rational { 6 });

    CHECK(trace.steps[1].kind == formula::StepKind::Power);
    CHECK(trace.steps[1].exponent == 2);
    CHECK(trace.steps[1].value == formula::Rational { 36 });
    REQUIRE(trace.steps[1].operands.size() == 1);
    CHECK(trace.steps[1].operands[0] == 0);

    CHECK(trace.steps[2].kind == formula::StepKind::Variable);
    CHECK(trace.steps[2].symbol == "V");

    CHECK(trace.steps[3].kind == formula::StepKind::Divide);
    CHECK(trace.steps[3].value == formula::Rational { 12 });
    REQUIRE(trace.steps[3].operands.size() == 2);
    CHECK(trace.steps[3].operands[0] == 1);
    CHECK(trace.steps[3].operands[1] == 2);

    // The root is the last step: nothing claimed it.
    CHECK(trace.root() == 3);
}

TEST_CASE("a short-circuited operand leaves the parent with one operand, not two", "[trace]")
{
    // The failing subtree must be a **left** operand, and must sit below a
    // node that does not itself short-circuit. `BinaryNode` returns early only
    // when its OWN left operand errors; a right-operand error is discovered
    // after both operands have already been dispatched, so nothing is skipped
    // and a wrong arity-based implementation would agree with the right answer
    // by coincidence.
    //
    // Here the middle `Divide`'s own left operand fails, so its right
    // `var<Mass>` is genuinely never dispatched, while the outer `Multiply`
    // dispatches both of its children normally. Multiply rather than a sum
    // because a sum would require both sides to share a dimension, which is
    // not what this test is about.
    constexpr auto bad = var<Mass> * ((var<Volume> / formula::number(formula::Rational { 0 })) / var<Mass>);

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(bad, environmentOf(6, 3), sink);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == formula::ArithmeticError::DivisionByZero);

    // The outer division recorded the operands it actually got: its left side
    // and the failing right side. Nothing was invented for what never ran.
    auto const& outer = trace.steps[trace.root()];
    CHECK(outer.kind == formula::StepKind::Divide);
    CHECK(outer.error == formula::ArithmeticError::DivisionByZero);
    CHECK(outer.operands.size() == 2);
}

TEST_CASE("an absent variable is recorded as absent, not as an error", "[trace]")
{
    constexpr auto density = var<Mass> / var<Volume>;
    auto const environment = formula::environment(formula::Measured<Mass>::absent(),
                                                  formula::Measured<Volume> { formula::Rational { 3 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    REQUIRE(result.has_value());
    CHECK_FALSE(result->has_value());

    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK_FALSE(trace.steps[0].value.has_value());
    CHECK_FALSE(trace.steps[0].error.has_value());
}

TEST_CASE("a citation contributes its own step", "[trace]")
{
    constexpr auto cited = formula::documented(var<Mass> / var<Volume>,
                                               { .title = "Bulk density",
                                                 .reference = "Example Standard 7:2020",
                                                 .section = "4.1" });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(cited, environmentOf(6, 3), sink);

    REQUIRE(result.has_value());
    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Documented);
    CHECK(root.citation.title == "Bulk density");
    CHECK(root.citation.reference == "Example Standard 7:2020");
    // It forwards its inner value unchanged -- invisible to arithmetic, but
    // not invisible to the trace: "why this formula" is what a trace is for.
    CHECK(root.value == formula::Rational { 2 });
}

TEST_CASE("a deep tree is destroyed without recursing", "[trace]")
{
    // Build a long left-leaning chain at runtime by recording many steps
    // directly: the concern is the arena's destructor, not the expression
    // template, which is bounded by what a compiler will instantiate.
    formula::Trace<> trace {};
    for (std::size_t i = 0; i < 200'000; ++i)
    {
        formula::Step<> step {};
        step.kind = formula::StepKind::Add;
        if (i > 0)
            step.operands.push_back(i - 1);
        trace.steps.push_back(std::move(step));
    }
    CHECK(trace.steps.size() == 200'000);
    // Destruction happens at the end of this scope. A tree of owning nodes
    // would recurse 200'000 deep here and overflow the stack; a flat vector
    // of index-referencing steps cannot.
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: `'formula-cpp/trace.hpp': No such file or directory`.

- [ ] **Step 3: Write `trace.hpp`**

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A derivation, recorded: what the evaluator did, step by step.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<vector>`, and a consumer who only evaluates numbers must not compile an
/// arena into every translation unit. It does **not** pull `<string>`: nothing
/// here formats anything, which is what keeps `trace_render.hpp` a separate,
/// separately-optional header. Include this one to record a derivation, and
/// that one as well to print it.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/sink.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace formula
{

/// What kind of node a step came from.
enum class StepKind : std::uint8_t
{
    Variable,
    Constant,
    Pi,
    Negate,
    Add,
    Subtract,
    Multiply,
    Divide,
    Power,
    Root,
    Documented,
};

/// One node's contribution to a derivation.
///
/// Operands are **indices into the owning `Trace`'s `steps`**, never pointers
/// and never owned children. That is what makes a trace destructible at any
/// depth without recursion, and serialisable without it either.
template <typename Rep = Rational>
struct Step
{
    /// Which kind of node produced this step.
    StepKind kind {};

    /// For `Variable`: how the quantity is written. Points into the static
    /// storage of the quantity's `Describe` specialisation, so it outlives any
    /// trace -- the same guarantee `document.hpp`'s `SymbolEntry` relies on.
    std::string_view symbol {};

    /// For `Documented`: what the wrapped formula cites.
    Citation citation {};

    /// For `Power`: the exponent. For `Root`: the degree. Zero otherwise.
    int exponent {};

    /// The dimension of what this step produced.
    Dimension dimension {};

    /// What the step produced, in the coherent SI unit of `dimension`. Empty
    /// when the value was **absent** -- which is not an error and must not be
    /// rendered as one.
    std::optional<Rep> value {};

    /// Set when this step failed. A step has a `value` or an `error` or
    /// neither (absent); never both.
    std::optional<ArithmeticError> error {};

    /// Indices of the steps this one consumed, in evaluation order.
    ///
    /// **Not necessarily as many as the node kind suggests.** When an operand
    /// fails, the evaluator returns without evaluating the remaining ones, so
    /// a `Divide` may hold one operand rather than two. What is recorded is
    /// what actually ran.
    std::vector<std::size_t> operands {};
};

/// A recorded derivation: a flat arena of steps.
template <typename Rep = Rational>
struct Trace
{
    /// Every step, in the order they completed -- children before parents.
    std::vector<Step<Rep>> steps {};

    /// Bookkeeping written by `RecordingSink` during a walk. Meaningless once
    /// the walk is over; kept here rather than in the sink because a sink is
    /// copied by value at every node and must stay cheap.
    /// @{
    std::vector<std::size_t> marks {};
    std::vector<std::size_t> unclaimed {};
    /// @}

    /// The index of the outermost step -- the one nothing else consumed.
    ///
    /// @pre `steps` is not empty.
    [[nodiscard]] std::size_t root() const noexcept { return steps.size() - 1; }

    /// Whether anything was recorded.
    [[nodiscard]] bool empty() const noexcept { return steps.empty(); }
};

namespace detail
{
    /// The `StepKind` a node maps to, as a compile-time property of its type.
    template <typename N>
    struct StepKindOf;

    template <Described Q>
    struct StepKindOf<VarNode<Q>>
    {
        static constexpr StepKind value = StepKind::Variable;
    };

    template <Unit U>
    struct StepKindOf<ConstantNode<U>>
    {
        static constexpr StepKind value = StepKind::Constant;
    };

    template <>
    struct StepKindOf<PiNode>
    {
        static constexpr StepKind value = StepKind::Pi;
    };

    template <UnaryOperator Op, Node Operand>
    struct StepKindOf<UnaryNode<Op, Operand>>
    {
        static constexpr StepKind value = StepKind::Negate;
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct StepKindOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr StepKind value = Op == BinaryOperator::Add        ? StepKind::Add
                                          : Op == BinaryOperator::Subtract ? StepKind::Subtract
                                          : Op == BinaryOperator::Multiply ? StepKind::Multiply
                                                                           : StepKind::Divide;
    };

    template <int Exponent, Node Operand>
    struct StepKindOf<PowerNode<Exponent, Operand>>
    {
        static constexpr StepKind value = StepKind::Power;
    };

    template <int Degree, Node Operand>
    struct StepKindOf<RootNode<Degree, Operand>>
    {
        static constexpr StepKind value = StepKind::Root;
    };

    template <Node Inner>
    struct StepKindOf<DocumentedNode<Inner>>
    {
        static constexpr StepKind value = StepKind::Documented;
    };
} // namespace detail

/// Records a derivation into a `Trace` the caller owns.
///
/// A **handle**, not an owner: the evaluator copies its sink by value at every
/// node, so a sink that owned a `std::vector` would copy the whole arena each
/// time. One pointer copies for free. See `sink.hpp` for the measurement that
/// forces this.
template <typename Rep = Rational>
class RecordingSink
{
  public:
    /// @p trace must outlive the evaluation.
    explicit constexpr RecordingSink(Trace<Rep>& trace) noexcept: _trace { &trace } {}

    /// Remembers how much of the arena predates this node, so `produced` can
    /// tell which steps are its operands.
    template <Node N>
    void entered(N const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records the step, claiming as its operands every step recorded at or
    /// after the matching `entered` that nothing else has claimed.
    template <Node N>
    void produced(N const& node, Evaluated<Rep> const& result)
    {
        std::size_t const mark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> step {};
        step.kind = detail::StepKindOf<N>::value;
        step.dimension = N::dimension;

        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.symbol = Describe<typename N::quantity>::symbol;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Documented)
            step.citation = node.citation;
        if constexpr (requires { N::exponent; })
            step.exponent = N::exponent;
        else if constexpr (requires { N::degree; })
            step.exponent = N::degree;

        if (!result.has_value())
            step.error = result.error();
        else if (result->has_value())
            step.value = **result;

        // Everything unclaimed from `mark` onwards belongs to this node.
        auto first = _trace->unclaimed.begin();
        while (first != _trace->unclaimed.end() && *first < mark)
            ++first;
        step.operands.assign(first, _trace->unclaimed.end());
        _trace->unclaimed.erase(first, _trace->unclaimed.end());

        _trace->steps.push_back(std::move(step));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

  private:
    Trace<Rep>* _trace;
};

} // namespace formula
```

**Names verified against the headers, not guessed:** `VarNode<Q>` exposes `using quantity = Q;` (lower case — `N::Quantity` does not exist and will not compile). `PowerNode` defines `static constexpr int exponent` and `RootNode` defines `static constexpr int degree`, so the two `requires` clauses above resolve as written. `Describe<Q>::symbol` has static storage duration, which is what makes storing a `std::string_view` to it safe.

- [ ] **Step 4: Register the header and the test**

Add `trace_tests.cpp` to `test/CMakeLists.txt`. Add to `cmake/CheckPublicHeaderIncludes.cmake`:

```cmake
set(exemptHeaders
    "${SOURCE_DIR}/include/formula-cpp/render.hpp"
    "${SOURCE_DIR}/include/formula-cpp/document.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace.hpp"
)
```

and

```cmake
set(exemptAllowances
    "render.hpp=string"
    "document.hpp=string,vector"
    "trace.hpp=vector"
)
```

`trace.hpp` as written uses `<vector>` but not `<string>` — declare only what it uses, and let the check fail if that changes. That is the point of the per-include allowance.

- [ ] **Step 5: Build, run, confirm green**

```bash
cmake --build out/build/cl-debug && ctest --test-dir out/build/cl-debug --output-on-failure
```

Expected: PASS -- every test that passed before, plus the 5 added here. If any *earlier* test now fails, stop: recording a trace must not change what the evaluator computes.

- [ ] **Step 6: Prove the short-circuit test is not vacuous**

Change `RecordingSink::produced` to claim a fixed operand count based on node
arity instead of the mark — 0 for `Variable`/`Constant`/`Pi`, 1 for
`Negate`/`Power`/`Root`/`Documented`, 2 for the binary kinds — popping from the
tail of `unclaimed` and clamping to its size. Rebuild.

Expected: the short-circuit test **fails**, with the outer node's
`operands.size()` reported as **1** rather than 2. Under the mutation the
middle `Divide` claims two operands because its arity says so, reaches past
itself, and steals the outer node's own left child.

Restore the mark-based version and confirm it passes. Record both directions.

**If the mutation does not break anything, the test is wrong, not the design.**
Say so rather than reporting a pass: an earlier draft of this test put the
failing subtree on the *right*, where nothing is ever skipped, and it passed
under the mutation it was written to catch.

- [ ] **Step 7: All four presets, then commit**

```bash
git add include/formula-cpp/trace.hpp test/trace_tests.cpp test/CMakeLists.txt cmake/CheckPublicHeaderIncludes.cmake
git commit -m "feat(trace): a flat arena a derivation records into"
```

---

## Task 5: Bounded rendering — `trace_render.hpp`

**This task now also owns a change to `Step`, decided after task 4 shipped.**
Running the recorder on `documented(var<WaterVolume> / var<CementVolume>)` with
180 l and 300 l produced this:

```
  #1  variable    V_w  = 9/50
  #2  variable    V_c  = 3/10
  #3  divide           = 3/5   from #1 #2
  #4  documented       = 3/5   from #3   [Water/cement ratio, Example Standard 1:2020]
```

`9/50` is correct — 180 l *is* 9/50 m3, because a step records its value in the
coherent SI unit of its dimension. It is also close to useless in an audit
trail. A person checking a report entered 180 litres and needs to see 180
litres; a derivation that silently restates every input in a unit nobody typed
is a worse record than the calculation it documents.

The renderer cannot recover the declared unit on its own: by the time a `Step`
exists, the quantity type is erased. So the recorder must capture it.

**`Step<Rep>` gains one field:**

```cpp
    /// The unit this step's value was **declared** in -- `Describe<Q>::unit`
    /// for a variable, the constant's own unit for a constant, and the
    /// coherent SI unit of `dimension` for anything computed, which has no
    /// declared unit of its own.
    ///
    /// `value` is always in the coherent SI unit, so that steps are
    /// comparable; this is what a renderer converts back to before showing a
    /// number to a person.
    Unit unit {};
```

populated in `RecordingSink::produced` alongside the existing fields:

```cpp
        step.unit = coherent(N::dimension);
        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.unit = Describe<typename N::quantity>::unit;
        else if constexpr (requires { N::unitOf; })
            step.unit = N::unitOf;
```

**Read `ConstantNode` in `expression.hpp` first and use whatever it actually
names its unit** — it is a `Unit` non-type template parameter, so the member
may be spelled differently from `unitOf` or may not exist as a member at all,
in which case match on the `ConstantNode<U>` specialisation instead of a
`requires`. Do not invent a name; the last three times this plan guessed one,
one of the guesses was wrong.

Add a test pinning the declared unit for all three cases — a variable declared
in litres, a constant, and a computed node — and a test that rendering shows
`180 l` rather than `9/50`.

**Files:**
- Create: `include/formula-cpp/trace_render.hpp`
- Create: `test/trace_render_tests.cpp`
- Modify: `test/CMakeLists.txt`, `cmake/CheckPublicHeaderIncludes.cmake`

**Interfaces:**
- Consumes: `Trace<Rep>`, `Step<Rep>`, `StepKind` from Task 4.
- Produces: `formula::render_trace(Trace<Rep> const&, TraceRenderOptions)` returning `std::string`.

**The bound is a required argument, not a default.** §11's second scar is an unbounded render that collapsed a hundred thousand steps into one unusable line. A default limit is a limit someone forgets; a required one is a limit someone chooses.

- [ ] **Step 1: Write the failing test**

Create `test/trace_render_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Volume: formula::Quantity<Volume, "V", "specimen volume", unit::CubicMetre>
{
};
} // namespace

TEST_CASE("a derivation renders one line per step, in order", "[trace-render]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    auto const environment = formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                                                  formula::Measured<Volume> { formula::Rational { 3 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text == "1. m = 6\n"
                  "2. #1^2 = 36\n"
                  "3. V = 3\n"
                  "4. #2 / #3 = 12\n");
}

TEST_CASE("a derivation longer than the limit is cut, and says so", "[trace-render]")
{
    formula::Trace<> trace {};
    for (std::size_t i = 0; i < 100; ++i)
    {
        formula::Step<> step {};
        step.kind = formula::StepKind::Constant;
        step.value = formula::Rational { static_cast<long long>(i) };
        trace.steps.push_back(std::move(step));
    }

    std::string const text = formula::render_trace(trace, { .maxSteps = 3 });

    // Exactly three steps, then an honest count of what was left out -- not a
    // silent truncation and not a hundred lines.
    CHECK(text == "1. 0\n"
                  "2. 1\n"
                  "3. 2\n"
                  "... 97 further steps not shown\n");
}

TEST_CASE("a failing step renders its error, and an absent one renders absence", "[trace-render]")
{
    formula::Trace<> trace {};

    formula::Step<> failed {};
    failed.kind = formula::StepKind::Divide;
    failed.error = formula::ArithmeticError::DivisionByZero;
    trace.steps.push_back(std::move(failed));

    formula::Step<> absent {};
    absent.kind = formula::StepKind::Variable;
    absent.symbol = "m";
    trace.steps.push_back(std::move(absent));

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text == "1. / = division by zero\n"
                  "2. m = (not measured)\n");
}

TEST_CASE("an empty trace renders nothing rather than a stray header", "[trace-render]")
{
    formula::Trace<> const trace {};
    CHECK(formula::render_trace(trace, { .maxSteps = 10 }).empty());
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: `'formula-cpp/trace_render.hpp': No such file or directory`.

- [ ] **Step 3: Write `trace_render.hpp`**

Implement `TraceRenderOptions` with a single `std::size_t maxSteps` member having **no default**, and `render_trace`. Render each step as `<n>. <expression> = <value>` where the expression names operands as `#<index+1>`, a variable renders its symbol, a constant renders its number, and a power renders `#k^e`. Render an error with `formula::describe(ArithmeticError)` from `error.hpp`,
which already exists and returns a `std::string_view` — do not write a second
one. It returns `"division by zero"` for `DivisionByZero`, which is what the
third test above pins. Render an absent value as `(not measured)`.

When `trace.steps.size() > maxSteps`, render the first `maxSteps` and then exactly one line: `... N further steps not shown\n` where N is the remainder.

Add `"trace_render.hpp=string"` to `exemptAllowances` and the path to `exemptHeaders`.

- [ ] **Step 4: Run and confirm green**

Expected: PASS -- every test that passed before, plus the 4 added here.

- [ ] **Step 5: Confirm the bound is load-bearing**

Temporarily render every step regardless of `maxSteps`. Expected: the second test fails with 100 lines instead of 4. Restore and confirm. Record both.

- [ ] **Step 6: All four presets, then commit**

```bash
git commit -m "feat(trace): render a derivation, bounded by a limit the caller must choose"
```

---

## Task 6: `explain` — the public entry point

**Files:**
- Modify: `include/formula-cpp/trace.hpp` (add `Explained` and `explain`)
- Modify: `include/formula-cpp/evaluate.hpp` (route the two entry points through `detail::dispatch`)
- Modify: `test/trace_tests.cpp`, `test/sink_tests.cpp`

**Interfaces:**
- Consumes: `Trace`, `RecordingSink` from Task 4; `evaluate<Result>` from phase 5; `detail::dispatch` from Task 1.
- Produces: `formula::Explained<Result, Rep>` and `formula::explain<Result>(expression, environment)`.

**Close the hole at the root, first.** Task 2's review found that
`detail::dispatch` protects a consumer's phase-5-era two-parameter overload on
every *recursive* call, but `checked_evaluate` and `evaluate` call
`checked_evaluate_si` **directly** — so a custom node handed straight to
`evaluate<Result>` as the root of an expression fails to compile, while the
same node nested one level deep works fine. A compatibility guarantee with a
hole at the root is not one anyone can rely on.

Change both entry points to call `detail::dispatch<Rational>(expression,
environment, sink)` instead of `checked_evaluate_si<Rational>(...)`, and add
this test to `test/sink_tests.cpp`, beside the existing legacy-node test:

```cpp
TEST_CASE("a two-parameter custom node works as the root of an expression too", "[sink]")
{
    // Nested, this already worked -- every recursive call routes through
    // dispatch. As the root it used to fail to compile, because the entry
    // points called the evaluator directly.
    int entered = 0;
    int produced = 0;
    CountingSink sink { &entered, &produced };

    auto const result =
        formula::checked_evaluate_si<formula::Rational>(LegacyNode {}, environmentOf(5, 1), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    CHECK(**result == formula::Rational { 7 });
    // Untraced, because nothing told the library how to trace it -- but it
    // evaluated, which is the point.
    CHECK(entered == 0);
    CHECK(produced == 0);
}
```

`dispatch` is `if constexpr`, so this costs nothing at runtime. Confirm the
existing 241 tests still pass unchanged.

**On the name.** §11 writes the untraced entry point as `value_of`. The library shipped `evaluate<Result>` in phase 5, and every test, example and guide calls it. Renaming the whole surface for a synonym buys nothing, so `evaluate<Result>` stays and §11's prototype spelling is what gets corrected. `explain` is new and matches §11.

- [ ] **Step 1: Write the failing test**

Add to `test/trace_tests.cpp`:

```cpp
TEST_CASE("explain returns the same outcome evaluate would, plus the derivation", "[trace]")
{
    // Mass / Volume. NOT pow<2>(var<Mass>) / var<Volume>, which is
    // Mass^2/Volume and is not a density -- the library's own
    // RequireResultDimension static_assert rejects it, correctly.
    constexpr auto density = var<Mass> / var<Volume>;
    auto const environment = environmentOf(6, 3);

    auto const plain = formula::evaluate<Density>(density, environment);
    auto const explained = formula::explain<Density>(density, environment);

    CHECK(explained.outcome == plain);
    CHECK(explained.trace.steps.size() == 3);
    CHECK(explained.trace.steps[explained.trace.root()].value == formula::Rational { 2 });
}
```

Declare a `Density` quantity alongside `Mass` and `Volume` in the anonymous namespace:

```cpp
struct Density: formula::Quantity<Density, "rho", "bulk density", formula::coherent(formula::dim::Density)>
{
};
```

There is no named `unit::KilogramPerCubicMetre`. `dim::Density` exists
(`dimension.hpp`, defined as `Mass / Volume`) and `coherent(Dimension)` in
`evaluate.hpp` is `constexpr`, so `coherent(dim::Density)` is a valid non-type
template argument and names the coherent SI unit of that dimension.

- [ ] **Step 2: Implement**

```cpp
/// An outcome together with the derivation that produced it.
template <Described Result, typename Rep = Rational>
struct Explained
{
    /// Exactly what `evaluate<Result>` would have returned.
    Outcome<Result> outcome {};
    /// How it was reached.
    Trace<Rep> trace {};
};

/// Evaluates @p expression for @p Result and records how.
///
/// The outcome is identical to `evaluate<Result>(expression, environment)` --
/// tracing observes, it does not participate.
template <Described Result, typename Rep = Rational, Node Expression, typename Env>
[[nodiscard]] Explained<Result, Rep> explain(Expression const& expression, Env const& environment)
{
    Explained<Result, Rep> explained {};
    RecordingSink<Rep> sink { explained.trace };
    explained.outcome = evaluate<Result>(expression, environment, sink);
    return explained;
}
```

- [ ] **Step 3: Run, all four presets, commit**

```bash
git commit -m "feat(trace): explain() -- the answer and how it was reached"
```

---

## Task 7: The guide and the gallery

**Files:**
- Create: `docs/tracing.md`
- Modify: `mkdocs.yml` (add to `nav`)
- Modify: `tools/gallery/main.cpp` and `docs/gallery.md`
- Create: `examples/tracing.cpp`, modify `examples/CMakeLists.txt`

**Interfaces:**
- Consumes: everything above.
- Produces: nothing code depends on.

**Snippet rule.** Take every snippet from compiled code — a test, the example, or a header's own declarations quoted verbatim. A snippet you retype into the page is compiled nowhere. Every numeric claim must come from **running** the code and pasting what it printed; if you catch yourself working out a number, stop and run something.

- [ ] **Step 1: Write `examples/tracing.cpp`**

A worked derivation with generic physics and an invented citation, printing the trace. Register it in `examples/CMakeLists.txt` the way `citations.cpp` is registered, including its ctest entry.

- [ ] **Step 2: Write `docs/tracing.md`**

Cover: what a sink is and why it is by value; the two entry points and when to use each; reading a derivation; the bound on rendering and why it is required; and the extension point — a consumer's own node evaluates but is not traced until it grows a sink-aware overload, with the diagnostic-free silence that implies called out plainly.

- [ ] **Step 3: Extend the gallery with one worked derivation**

Add a derivation section to `tools/gallery/main.cpp`, regenerate `docs/gallery.md`, confirm `gallery.is-current` passes.

- [ ] **Step 4: All four presets and the GCC build, then commit**

```bash
git commit -m "docs: a guide to tracing, with a worked derivation in the gallery"
```

---

## Self-Review

**1. Spec coverage (§11).**

| §11 requirement | Task |
|---|---|
| No preprocessor gating; traceability through composition | 1, 2 — a sink parameter, no macro anywhere |
| Evaluator parameterised on a sink; `NullSink` stateless with empty methods | 1 |
| `RecordingSink` builds a derivation | 4 |
| Same tree, same evaluator, different composition | 2, 6 |
| Zero-overhead untraced path, verified | 3 — with the claim corrected to one the library can meet |
| Evaluator never pre-renders for the sink | 1 (stated in the header), 3 (why it matters), 5 (rendering lives in the sink's consumer) |
| Arena of steps with parent/operand indices, not a nested tree | 4 |
| A memoised sub-result appears once with a back-reference | **Not implemented.** There is no memoisation in the evaluator yet — nothing computes a sub-result twice, so there is nothing to back-reference. The index-based arena is what makes it expressible when memoisation arrives. Called out rather than silently skipped. |
| Serialises without recursion | 4 — flat vector, index operands |
| Never recurse on teardown | 4, with a 200,000-step destruction test |
| Always bound rendered steps | 5, with the bound as a required argument |
| Audit reports | 5, 6, 7 — a rendered derivation with its citations is the audit report; no separate format is introduced, and if one is wanted it belongs in a later phase against a real requirement |

**2. Placeholder scan.** No "TBD", no "handle edge cases", no "similar to Task N". Three names I had initially deferred are now resolved against the headers and written out: `VarNode`'s member is `quantity` (lower case, and `N::Quantity` would not have compiled), there is no `unit::KilogramPerCubicMetre` so the density quantity uses `coherent(dim::Density)`, and `describe(ArithmeticError)` already exists in `error.hpp`. Deferring them would have handed the implementer one name that is simply wrong.

**3. Type consistency.** `Step<Rep>`, `Trace<Rep>`, `RecordingSink<Rep>`, `Explained<Result, Rep>` are consistent across Tasks 4–6. `StepKind` enumerators are used in Tasks 4, 5 and 7 with the same spellings. `render_trace(trace, {.maxSteps = N})` has the same signature in Tasks 5 and 7. `detail::dispatch<Rep>(node, environment, sink)` is defined in Task 1 and used in Task 2 with the same argument order.

**4. One thing an implementer will hit that the tasks above should make obvious.** `Step::symbol` is a `std::string_view` into the static storage of a `Describe` specialisation. This is safe and `document.hpp` already relies on it. It is *not* safe to store a view into anything a caller builds at runtime, which is why `Citation` is copied into the step by value rather than referenced.
