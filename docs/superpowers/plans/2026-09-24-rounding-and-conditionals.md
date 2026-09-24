# Rounding Nodes and Conditionals Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a formula say *where* rounding happens, *which* of two formulas applies, and — when a standard genuinely demands it — that a rule is stated over a bare number rather than over a quantity.

**Architecture:** Three new node kinds, each changing what evaluation means at one point in the tree rather than at the edges. `RoundNode` rounds in a **named unit** at its position, so intermediate and final rounding are separately expressible. `WhenNode` evaluates a predicate and then **only the branch it selects**, recording which one it took. `NumericValueNode` strips a dimension deliberately, carries a mandatory justification, and is loud about it in the trace. All three are ordinary nodes: they compose, they trace, and they render.

**Tech Stack:** C++23, header-only. Catch2 via CPM. MSVC `cl`, `clang-cl`, `clang++`, GCC.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — §16.3 Tier A items 2 and 4, §16.4 Tier B item 9, and §17's phase 8 row. Read §9 (expressions) and §11 (traceability) for the surface being extended.

## Global Constraints

- **C++23, header-only.** No `.cpp` under `include/`.
- **Warnings are errors on MSVC `cl`, `clang-cl`, `clang++` and GCC.** GCC is the only configuration running `-Wshadow -Wconversion -Wpedantic`, and in phase 7 it was the only one to catch an enumerator shadowing a global and a constructor parameter shadowing its member. **Every task builds under GCC before it reports DONE.**
- **Open source, no company IP, as generic as possible.** The repository is public.
- **No norm content.** Nothing from a real standard appears here: not its text, tables, equations, threshold or constant values, and not its identifiers, clause or table numbers — not even as a bare citation. Every citation is invented, of the form `Example Standard N:2020`. Examples use **generic physics only**.
- **No macros.**
- **`<string>`, `<vector>`, `<format>`, `<iostream>` must not reach `formula.hpp`** or any header it includes. `render.hpp`, `document.hpp`, `trace.hpp` and `trace_render.hpp` are the declared opt-in exceptions.
- **`evaluate` and `checked_evaluate` must remain usable in constant expressions.** `test/sink_tests.cpp` pins this with `static_assert`s.
- **A failed assertion must never open a Windows dialog.**

---

## What phase 8 deliberately does *not* implement

§16.3 item 4 names **five** kinds of conditional, and they are not interchangeable. This phase implements the two whose prerequisites exist:

| Kind | Phase |
|---|---|
| A numeric threshold on an intermediate selects between two formulas | **8** — `WhenNode` |
| Banded piecewise with **non-numeric outcomes** | **10** — needs a category result type, which arrives with lookup tables |
| **Variant selection by apparatus, method or specimen geometry** | **11** — methods and jurisdiction overlays |
| **Conditional aggregation** — which observations enter the mean depends on the observations | **12** — needs series |
| **Interaction and suppression** — one penalty suppresses another | **9** — needs constraints as peers of formulas |

This is a narrowing of §16.3 item 4 and is stated here rather than discovered later. Each deferred kind is deferred to the phase that supplies what it needs, not to "later".

---

## File Structure

| File | Responsibility | In the umbrella? |
|---|---|---|
| `include/formula-cpp/rounding_node.hpp` (new) | `RoundNode`, `RoundSignificantNode`, `rounded()`, `rounded_to_digits()` | **Yes** |
| `include/formula-cpp/predicate.hpp` (new) | `Comparison`, `PredicateNode`, the comparison operators, `Predicate` concept | **Yes** |
| `include/formula-cpp/conditional.hpp` (new) | `WhenNode`, `when()` | **Yes** |
| `include/formula-cpp/escape.hpp` (new) | `NumericValueNode`, `numeric_value_of()` | **Yes** |
| `include/formula-cpp/formula.hpp` (modify) | Include the four above | — |
| `include/formula-cpp/trace.hpp` (modify) | Four new `StepKind`s and their `collect`/`produced` handling | No (opt-in) |
| `include/formula-cpp/trace_render.hpp` (modify) | Render the four new step kinds | No (opt-in) |
| `include/formula-cpp/render.hpp` (modify) | Render the four new node kinds in three dialects | No (opt-in) |
| `include/formula-cpp/document.hpp` (modify) | Walk the four new node kinds | No (opt-in) |
| `test/rounding_node_tests.cpp`, `test/predicate_tests.cpp`, `test/conditional_tests.cpp`, `test/escape_tests.cpp` (new) | — | — |
| `docs/rounding-and-conditionals.md` (new), `examples/rounding_and_conditionals.cpp` (new) | — | — |

**Why four small headers rather than one.** Each is a distinct concept with its own tests and its own reviewer gate: a reader can adopt rounding nodes without meeting predicates, and `escape.hpp` is a deliberate hole in the type system that deserves to be findable by name. This follows the split the library already uses.

---

## Task 1: Rounding as a node, in a named unit

**Files:**
- Create: `include/formula-cpp/rounding_node.hpp`
- Create: `test/rounding_node_tests.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `checked_round(Rational, DecimalPlaces, RoundingMode)` and `checked_round(Rational, SignificantDigits, RoundingMode)` from `rounding.hpp`; `checked_convert(Rational, Unit, Unit)` from `unit.hpp`; `Node`, `NodeBase` from `expression.hpp`.
- Produces: `RoundNode<U, Places, Mode, Operand>`, `RoundSignificantNode<U, Digits, Mode, Operand>`, `rounded<U, Places, Mode>(operand)`, `rounded_to_digits<U, Digits, Mode>(operand)`. Tasks 5–8 render, trace, walk and document these.

**The decision this task turns on, and why it is not negotiable.** Rounding a *quantity* is meaningless without a unit. "Round to two decimal places" applied to a length gives a different number in metres than in millimetres, and a method that says "record the diameter to 0.1 mm" means 0.1 **mm**, not 0.1 of whatever the evaluator happens to work in. The evaluator works in coherent SI. So the node states its unit, converts into it, rounds there, and converts back:

```cpp
rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter>)
```

Verified before this plan was written: `DecimalPlaces`, `SignificantDigits` and `Unit` all work as non-type template parameters on all four compilers, and the same arguments give the same type.

A rounding node **does not change the dimension** of what it wraps.

- [ ] **Step 1: Write the failing test**

Create `test/rounding_node_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;
using formula::DecimalPlaces;
using formula::RoundingMode;

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Length: formula::Quantity<Length, "L", "specimen length", unit::Millimetre>
{
};

[[nodiscard]] constexpr auto millimetres(long long value)
{
    return formula::environment(formula::Measured<Diameter> { formula::Rational { value, 100 } });
}
} // namespace

TEST_CASE("a rounding node rounds in the unit it names, not in coherent SI", "[rounding-node]")
{
    // 12.345 mm to one decimal place is 12.3 mm. In coherent SI the same
    // value is 0.012345 m, and rounding *that* to one decimal place gives 0.0
    // -- which is why the unit is part of the node and not an afterthought.
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(
        var<Diameter>);
    constexpr auto environment = millimetres(1234);   // 12.34 mm

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    // 12.34 mm rounds to 12.3 mm. An Outcome reports in the RESULT
    // QUANTITY's declared unit -- Diameter is declared in millimetres -- not
    // in the coherent SI unit the evaluator works in. Verified directly: a
    // Volume declared in litres and measured at 180 l reports 180, not 9/50.
    CHECK(result->measurement().value() == formula::Rational { 123, 10 });
}

TEST_CASE("rounding does not change the dimension of what it wraps", "[rounding-node]")
{
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::Ceiling>(var<Diameter>);
    STATIC_REQUIRE(decltype(node)::dimension == formula::Describe<Diameter>::dimension);
}

TEST_CASE("an absent operand stays absent rather than rounding to zero", "[rounding-node]")
{
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::Floor>(var<Diameter>);
    constexpr auto environment = formula::environment(formula::Measured<Diameter>::absent());

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}

TEST_CASE("intermediate and final rounding are separately expressible", "[rounding-node]")
{
    // The point of rounding being a node: a method may round an input to a
    // coarse granularity BEFORE it enters a formula and round the result
    // differently afterwards. A library that rounds only at output cannot say
    // this, and produces a different number.
    constexpr auto coarseInput =
        formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
    constexpr auto doubled = coarseInput + coarseInput;
    constexpr auto finalResult =
        formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(doubled);

    constexpr auto environment = millimetres(1250);   // 12.50 mm

    // Input rounds to 13 mm (half away from zero), doubled is 26 mm, and the
    // final rounding leaves it. Rounding only at the end would have given
    // 25.0 mm -- a different number, from the same formula and the same input.
    constexpr auto result = formula::checked_evaluate<Diameter>(finalResult, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    CHECK(result->measurement().value() == formula::Rational { 26, 1 });
}

TEST_CASE("significant digits are available as a node too", "[rounding-node]")
{
    constexpr auto node =
        formula::rounded_to_digits<unit::Millimetre, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    constexpr auto environment = millimetres(1234);   // 12.34 mm -> 12 mm

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    CHECK(result->measurement().value() == formula::Rational { 12, 1 });
}
```

**Compute each expected value by hand before you trust it, and if the code disagrees, work out which is wrong rather than editing the expectation.** Three tests in phase 7 had to be corrected because the plan's expected value was wrong; one asked for a `Density` from `Mass²/Volume`.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build out/build/cl-debug
```
Expected: `'rounded': is not a member of 'formula'`.

- [ ] **Step 3: Write `rounding_node.hpp`**

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Rounding as a position in a formula, not as output formatting.
///
/// A method may round an input to a coarse granularity *before* it enters a
/// formula and round the result differently afterwards. Both are specified,
/// and they give different numbers, so a library that rounds only at output
/// produces wrong ones. That is why rounding is a node: it happens where the
/// formula says it happens.
///
/// Every rounding node names the **unit it rounds in**, and this is not
/// decoration. "To two decimal places" means nothing about a quantity until
/// you say two decimal places *of what*: a length rounded to two places in
/// metres and the same length rounded to two places in millimetres are
/// different numbers. The evaluator works in the coherent SI unit of each
/// dimension, so a node converts into its stated unit, rounds there, and
/// converts back.

#include <formula-cpp/expression.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>

namespace formula
{

namespace detail
{
    /// Fails to compile when a rounding node's unit does not measure the
    /// dimension of what it wraps -- rounding a mass "to 0.1 mm" is not a
    /// rounding error, it is a category error.
    template <Unit U, typename Operand>
    struct RequireRoundingUnitMatches
    {
        static_assert(U.dimension == Operand::dimension,
                      "formula: this rounding node names a unit that does not measure the dimension of the "
                      "expression it rounds; the unit's dimension and the operand appear in this diagnostic as "
                      "the template arguments of RequireRoundingUnitMatches");

        static constexpr bool value = true;
    };
} // namespace detail

/// @p Operand rounded to @p Places decimal places of @p U, under @p Mode.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
struct RoundNode: NodeBase
{
    static_assert(detail::RequireRoundingUnitMatches<U, Operand>::value);

    /// The expression being rounded.
    Operand operand {};

    /// The unit the rounding happens in.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;
    /// Rounding changes a number, never its dimension.
    static constexpr Dimension dimension = Operand::dimension;
};

/// @p Operand rounded to @p Digits significant digits of @p U, under @p Mode.
template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
struct RoundSignificantNode: NodeBase
{
    static_assert(detail::RequireRoundingUnitMatches<U, Operand>::value);

    /// The expression being rounded.
    Operand operand {};

    /// The unit the rounding happens in.
    static constexpr Unit unit = U;
    /// How many significant digits to keep.
    static constexpr SignificantDigits digits = Digits;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;
    /// Rounding changes a number, never its dimension.
    static constexpr Dimension dimension = Operand::dimension;
};

/// `operand` rounded to `Places` decimal places of `U`:
/// `rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter>)`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded(Operand operand) noexcept
{
    return RoundNode<U, Places, Mode, Operand> { {}, operand };
}

/// `operand` rounded to `Digits` significant digits of `U`.
template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_to_digits(Operand operand) noexcept
{
    return RoundSignificantNode<U, Digits, Mode, Operand> { {}, operand };
}

} // namespace formula
```

- [ ] **Step 4: Write the evaluator overloads**

These go in `rounding_node.hpp` after the node definitions, and follow the shape every other node's evaluator uses — `sink.entered(node)` first, `detail::dispatch` for the operand, `sink.produced(node, result)` on **every** return path:

```cpp
/// Evaluates the operand, converts into `U`, rounds, and converts back.
template <typename Rep = Rational, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand, typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundNode<U, Places, Mode, Operand> const& node,
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

    Evaluated<Rep> const result = detail::round_in_unit<Rep>(**operand, U, Places, Mode);
    sink.produced(node, result);
    return result;
}
```

**`RepRounding<Rep>` — a new public extension point, and why it has to be one.**
Read `evaluate.hpp`'s `RepTraits` and `function.hpp`'s `RepFunctions` first.
`RepTraits<Rep>` has `from`, `add`, `subtract`, `multiply`, `divide` and
`negate` — **no `convert`**, because the evaluator converts units in `Rational`
*before* reaching `Rep` (see `detail::in_si`, `evaluate.hpp:184`). A rounding
node already holds a `Rep`, so it has no such path.

Rounding a `double` in a unit is genuinely a different operation from rounding
a `Rational` in one — one is exact, the other is not — and this library keeps
per-representation differences in an explicit trait rather than an
`if constexpr`. So add a third extension point beside `RepTraits` and
`RepFunctions`, in `rounding_node.hpp`, following `RepFunctions`' shape and
carrying its own "this is a public extension point" comment:

```cpp
/// Rounding per representation, including the unit conversion it needs.
///
/// A **public extension point**, for the same reason as `RepTraits` and
/// `RepFunctions`: a consumer teaching the evaluator a new representation
/// specialises all three, and specialising two of them reaches only as far as
/// the first rounding node in a formula.
///
/// Rounding lives here rather than in `RepTraits` because it needs the unit
/// conversion too, and because the conversion is exact for `Rational` and is
/// not for `double` -- a difference that belongs in a named specialisation
/// rather than inside an `if constexpr` in the evaluator.
template <typename Rep>
struct RepRounding;

template <>
struct RepRounding<Rational>
{
    /// Converts into @p unit, rounds there, and converts back -- all exactly.
    /// Fails with `DomainError` if the unit does not measure this dimension,
    /// and with `Overflow` if any step leaves `Rational`'s range.
    template <typename Parameter>
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError>
    round_in(Rational value, Unit unit, Parameter parameter, RoundingMode mode) noexcept
    {
        std::expected<Rational, ArithmeticError> const inUnit =
            checked_convert(value, coherent(unit.dimension), unit);
        if (!inUnit.has_value())
            return inUnit;

        std::expected<Rational, ArithmeticError> const roundedValue = checked_round(*inUnit, parameter, mode);
        if (!roundedValue.has_value())
            return roundedValue;

        return checked_convert(*roundedValue, unit, coherent(unit.dimension));
    }
};
```

and a `RepRounding<double>` specialisation alongside it. **Decide how `double`
converts and rounds, and say in your report what you chose and why** — the unit
magnitudes are exact `Rational`s, so converting a `double` means turning a
magnitude into a `double` and multiplying, which is inexact, and rounding a
`double` to a decimal place is inexact again. If you conclude `double` rounding
cannot be made honest enough to ship, a clean `static_assert` restricting
rounding nodes to `Rational` is an acceptable answer — `render_trace` already
sets that precedent — but it must be a decision you state, not an omission.

The evaluator overloads then call `RepRounding<Rep>::round_in(**operand, U, Places, Mode)`
and wrap the result with `detail::present<Rep>` / `std::unexpected`.

- [ ] **Step 5: Run the tests**

```bash
cmake --build out/build/cl-debug && ctest --test-dir out/build/cl-debug --output-on-failure
```
Expected: PASS, with the five new cases.

- [ ] **Step 6: Prove the unit is load-bearing**

Temporarily change the node to round in `coherent(Operand::dimension)` instead of `U`. Expected: "a rounding node rounds in the unit it names" **fails**, because 12.34 mm is 0.01234 m and one decimal place of that is 0.0. Restore and confirm. Record both directions. If the mutation changes nothing, the test is wrong — say so.

- [ ] **Step 7: All four presets and GCC, then commit**

```bash
wsl.exe bash -lc "cd /mnt/d/formula-cpp && cmake --build ~/fcpp-gcc && cd ~/fcpp-gcc && ctest"
git commit -m "feat(rounding): rounding as a position in a formula, in a named unit"
```

---

## Task 2: Predicates — comparing an expression to something

**Files:**
- Create: `include/formula-cpp/predicate.hpp`, `test/predicate_tests.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Node`, `NodeBase` from `expression.hpp`; the evaluator from `evaluate.hpp`.
- Produces: `enum class Comparison`, `PredicateNode<Op, Left, Right>`, comparison operators over nodes, the `Predicate` concept, and `checked_evaluate_predicate<Rep>(predicate, environment, sink)` returning `std::expected<std::optional<bool>, ArithmeticError>`. Task 3's `WhenNode` consumes all of it.

**Why a predicate is not a node.** A `Node` has a `dimension` and evaluates to a number. A predicate evaluates to a *truth value* and has no dimension, so making it a `Node` would mean either lying about its dimension or weakening what `Node` guarantees. It gets its own concept and its own evaluation entry point.

**Absence propagates through a predicate.** If either side is absent, the predicate is absent — not false. "The diameter nobody measured is not greater than 50" is a claim nobody is entitled to make, and answering `false` would silently choose a branch.

**Both sides must agree in dimension**, checked at compile time, with the same diagnostic shape the rest of the library uses.

- [ ] **Step 1: Write the failing test**

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto strengthOf(long long value)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { value } });
}
} // namespace

TEST_CASE("a predicate compares two expressions of the same dimension", "[predicate]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });

    constexpr auto high = formula::checked_evaluate_predicate(overFifty, strengthOf(60));
    REQUIRE(high.has_value());
    REQUIRE(high->has_value());
    CHECK(**high == true);

    constexpr auto low = formula::checked_evaluate_predicate(overFifty, strengthOf(40));
    REQUIRE(low.has_value());
    REQUIRE(low->has_value());
    CHECK(**low == false);
}

TEST_CASE("a boundary value is not over the threshold", "[predicate]")
{
    // Exactly 50 is not greater than 50. Worth its own case: an off-by-one
    // here silently selects the wrong formula for every specimen that lands
    // exactly on a threshold, which in a test method is not a rare input --
    // thresholds are chosen to fall on round numbers people aim at.
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto atFifty = formula::checked_evaluate_predicate(overFifty, strengthOf(50));
    REQUIRE(atFifty.has_value());
    REQUIRE(atFifty->has_value());
    CHECK(**atFifty == false);

    constexpr auto atMost = var<Strength> <= formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto atFiftyInclusive = formula::checked_evaluate_predicate(atMost, strengthOf(50));
    REQUIRE(atFiftyInclusive.has_value());
    REQUIRE(atFiftyInclusive->has_value());
    CHECK(**atFiftyInclusive == true);
}

TEST_CASE("an absent operand makes the predicate absent, not false", "[predicate]")
{
    // Answering `false` would silently pick a branch on the strength of a
    // measurement nobody took.
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto unknown = formula::checked_evaluate_predicate(overFifty, environment);
    REQUIRE(unknown.has_value());
    CHECK_FALSE(unknown->has_value());
}

TEST_CASE("an arithmetic failure in a predicate is an error, not a verdict", "[predicate]")
{
    constexpr auto divideByZero =
        (var<Strength> / formula::number(formula::Rational { 0 })) > formula::constant<unit::Megapascal>(formula::Rational { 1 });
    constexpr auto result = formula::checked_evaluate_predicate(divideByZero, strengthOf(60));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == formula::ArithmeticError::DivisionByZero);
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: no `operator>` over nodes, no `checked_evaluate_predicate`.

- [ ] **Step 3: Write `predicate.hpp`**

Define:

```cpp
/// Which way two expressions are compared.
enum class Comparison : std::uint8_t
{
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Equal,
    NotEqual,
};

/// A comparison of two expressions. Not a `Node`: it has no dimension and
/// evaluates to a truth value, and pretending otherwise would either lie
/// about its dimension or weaken what `Node` promises.
template <Comparison Op, Node Left, Node Right>
struct PredicateNode
{
    Left lhs {};
    Right rhs {};

    static constexpr Comparison comparison = Op;
};

/// Anything that compares two expressions.
template <typename P>
concept Predicate = /* detect a PredicateNode specialisation */;
```

plus the six operators over `Node`s, a `detail::RequireComparandsAgree` static_assert mirroring `RequireAddendsAgree` in `expression.hpp` (**read that one and follow its diagnostic wording**), and:

```cpp
/// The truth value of @p predicate, or absence when either side is absent.
template <typename Rep = Rational, Comparison Op, Node Left, Node Right, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<std::optional<bool>, ArithmeticError>
checked_evaluate_predicate(PredicateNode<Op, Left, Right> const& predicate, Env const& environment,
                           Sink sink = {}) noexcept;
```

Comparison of two `Rational`s is exact, so no rounding policy is needed and none should be invented.

- [ ] **Step 4: Run, then prove the boundary test is load-bearing**

Change `Greater` to use `>=` in the evaluator. Expected: "a boundary value is not over the threshold" **fails**. Restore, confirm. Record both directions.

- [ ] **Step 5: All four presets and GCC, then commit**

```bash
git commit -m "feat(predicate): compare two expressions, and stay absent when they are"
```

---

## Task 3: `when()` — a threshold selects between two formulas

**Files:**
- Create: `include/formula-cpp/conditional.hpp`, `test/conditional_tests.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: everything from task 2.
- Produces: `WhenNode<P, Then, Else>` and `when(predicate, thenBranch, elseBranch)`. Tasks 5–8 render, trace, walk and document it.

**Only the selected branch is evaluated.** Not both. Two reasons, and the second is the one that matters: evaluating the unselected branch could raise an arithmetic error that has nothing to do with the answer — a formula guarded by `when(v != 0, x / v, zero)` exists precisely because the other branch is invalid — and a trace recording work that did not determine the answer is a trace that misleads a reader.

**Both branches must have the same dimension.** A conditional whose answer is sometimes a length and sometimes an area has no dimension to report.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("a threshold selects between two formulas", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty,
                                          var<Strength> * formula::number(formula::Rational { 2 }),
                                          var<Strength> * formula::number(formula::Rational { 3 }));

    constexpr auto high = formula::checked_evaluate<Strength>(chosen, strengthOf(60));
    REQUIRE(high.has_value());
    CHECK(high->measurement().value() == formula::Rational { 120 });

    constexpr auto low = formula::checked_evaluate<Strength>(chosen, strengthOf(40));
    REQUIRE(low.has_value());
    CHECK(low->measurement().value() == formula::Rational { 120 });   // deliberately wrong; see step 2
}

TEST_CASE("the branch not taken is never evaluated", "[conditional]")
{
    // The else branch divides by zero. If both branches were evaluated, this
    // would fail; because only the selected one runs, it does not. This is
    // not a micro-optimisation -- a guarded formula exists precisely because
    // the other branch is invalid for these inputs.
    constexpr auto nonZero = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 0 });
    constexpr auto guarded = formula::when(nonZero,
                                           var<Strength>,
                                           var<Strength> / formula::number(formula::Rational { 0 }));

    constexpr auto result = formula::checked_evaluate<Strength>(guarded, strengthOf(60));
    REQUIRE(result.has_value());
    CHECK(result->measurement().value() == formula::Rational { 60 });
}

TEST_CASE("an absent predicate makes the result absent, not the else branch", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty, var<Strength>, formula::constant<unit::Megapascal>(formula::Rational { 0 }));
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto result = formula::checked_evaluate<Strength>(chosen, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}
```

**The second assertion in the first test is deliberately wrong** — `40 * 3` is 120 by coincidence of the numbers chosen, which hides a branch-selection bug. Fix it as part of step 2 by choosing multipliers whose products differ, and say in your report what you changed it to. This is here because a test whose two branches produce the same number cannot detect choosing the wrong one, and that is exactly the defect this test exists to catch.

- [ ] **Step 2: Fix the planted defect, then run to verify failure**

- [ ] **Step 3: Write `conditional.hpp`**

- [ ] **Step 4: Prove branch selection is load-bearing**

Invert the selection in the evaluator. Expected: the first test fails on both assertions (once the planted defect is fixed). Restore, confirm, record.

- [ ] **Step 5: All four presets and GCC, then commit**

---

## Task 4: `numeric_value_of` — the traced escape hatch

**Files:**
- Create: `include/formula-cpp/escape.hpp`, `test/escape_tests.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `test/CMakeLists.txt`

**Interfaces:**
- Produces: `NumericValueNode<U, Operand>` and `numeric_value_of<U>(operand, justification)`.

**What this is for, and why it is uncomfortable on purpose.** Some published rules are stated over the *numeric value* of a quantity in a named unit rather than over the quantity — an empirical fit whose coefficients only work when the input is expressed in particular units. Such a rule is dimensionally inconsistent by construction. The library cannot make it consistent, and pretending otherwise by silently dropping units would defeat the whole dimensional layer. So the hole is explicit, narrow, and loud:

- it names the unit the number must be read in,
- it requires a **justification string** — not defaulted, not optional,
- it produces a dimensionless value,
- and it appears in the trace as its own step, with the justification, so a reader of an audit trail sees exactly where the type system was deliberately stepped around.

```cpp
constexpr auto n = formula::numeric_value_of<unit::Megapascal>(
    var<Strength>, "Example Standard 9:2020 states this coefficient over the numeric value in MPa");
```

**The justification is mandatory at compile time**, not merely documented. Verify there is no way to construct the node without one, and add a negative-compile test in `test/negative/` proving it — read an existing case there for the harness shape.

- [ ] **Step 1: Write the failing test** — covering: the value is the operand's number in the stated unit; the result is dimensionless; the justification is carried; an absent operand stays absent; and a `static_assert` that the node is not constructible without a justification.

- [ ] **Step 2–5:** as the other tasks. Include a negative-compile test for the missing justification, and prove the unit is load-bearing by mutating it to coherent SI and watching the value change.

---

## Task 5: Render the four new node kinds

**Files:** modify `include/formula-cpp/render.hpp`, `test/render_tests.cpp`

Three dialects each. Suggested spellings, to be confirmed against what reads well in the guide:

| Node | Plain | LaTeX |
|---|---|---|
| `RoundNode` | `round(d, 1 mm)` | `\operatorname{round}_{1}(d)` |
| `WhenNode` | `if f > 50 then A else B` | a `\begin{cases}` block |
| `NumericValueNode` | `numeric(f / MPa)` | `\{f/\mathrm{MPa}\}` |

**Precedence matters here and is the part most likely to go wrong.** Phase 6 shipped two bugs of exactly this kind — a negative constant and a unit-bearing constant each rendering text that meant a different number than the tree. A `WhenNode` rendered inline inside a product without brackets would be read wrongly by anyone. Add `precedence_of` overloads and test each new node **inside a power and inside a product**, not only standalone.

---

## Task 6: Trace the four new node kinds

**Files:** modify `include/formula-cpp/trace.hpp`, `include/formula-cpp/trace_render.hpp`, `test/trace_tests.cpp`, `test/trace_render_tests.cpp`

New `StepKind`s: `Round`, `RoundSignificant`, `Conditional`, `NumericValue`.

**A `Conditional` step must record which branch it took** — that is the single most valuable thing a trace can say about a conditional, and "why that formula" is what §11 exists for. Record the predicate's truth value on the step.

**A `NumericValue` step must render its justification**, not merely carry it. The whole point of the escape hatch is that it is visible in the audit trail.

Beware the enumerator-shadowing trap: phase 7's `StepKind::Pi` shadowed the global `formula::Pi` and broke GCC while passing all four Windows presets. Check any new enumerator against the names in `formula::`.

---

## Task 7: Walk the four new node kinds in `document()`

**Files:** modify `include/formula-cpp/document.hpp`, `test/document_tests.cpp`

A `collect` overload per node kind. `WhenNode` must walk **both** branches and the predicate — unlike evaluation, documentation describes the whole formula, including the path not taken on this occasion. Say this in a comment, because it is the opposite of what task 3 does and a reader will wonder.

Phase 6's review found the walk had no test for one overload; enumerate all four here and name the test covering each.

---

## Task 8: Guide, example and gallery

**Files:** create `docs/rounding-and-conditionals.md`, `examples/rounding_and_conditionals.cpp`; modify `mkdocs.yml`, `docs/index.md`, `README.md`, `tools/gallery/main.cpp`, `docs/gallery.md`

Every snippet from compiled code; every output block pasted from a real run.

The guide must make the intermediate-versus-final rounding point with **two numbers that differ**, because that is the argument for the whole feature and prose alone does not make it.

Add a worked derivation to the gallery showing a conditional's trace naming the branch it took.

---

## Self-Review

**1. Spec coverage.**

| Requirement | Task |
|---|---|
| §16.3 #2 rounding as a tree node, decimals and significant figures, intermediate ≠ final | 1 |
| §16.3 #2 round-up distinct from round-half-up | Already shipped in phase 2's `RoundingMode`; task 1 exposes it as a node |
| §16.3 #4 numeric threshold selects between two formulas | 2, 3 |
| §16.3 #4 the other four kinds | **Deferred**, each to the phase supplying its prerequisite — see the table above |
| §16.4 #9 traced `numeric_value_of` with mandatory justification | 4 |
| §11 the new nodes trace | 6 |
| §18/§19 they render and document | 5, 7, 8 |

**2. Placeholder scan.** Tasks 4–8 give shape and constraints rather than complete code, because each depends on interfaces tasks 1–3 produce and writing them now would specify names that may not survive. **Each of those tasks must be re-read against the tree before dispatch, and its brief filled in with the real names then** — this is a controller instruction, not a licence for the implementer to invent.

**3. Type consistency.** `RoundNode<U, Places, Mode, Operand>`, `RoundSignificantNode<U, Digits, Mode, Operand>`, `PredicateNode<Op, Left, Right>`, `WhenNode<P, Then, Else>`, `NumericValueNode<U, Operand>` are used with these parameter orders throughout.

**4. The thing most likely to go wrong.** Rounding a quantity without naming a unit. Every part of this plan that touches rounding states the unit explicitly, and task 1 step 6 mutates it away to prove the tests would notice.
