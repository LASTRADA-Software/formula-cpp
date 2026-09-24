# Expressions and Evaluation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the expression layer — operator-built trees whose dimension is
checked where the formula is written, a type-keyed environment, and an evaluator
that returns a sum type rather than a number.

**Architecture:** Nodes are empty structs (except constants, which carry a
`Rational`) composed by `operator+ - * /` into a nested type; every node exposes
`static constexpr Dimension dimension` computed at class scope, so a dimensional
error is a compile error at the formula's own source line. Evaluation walks the
tree in the **coherent SI unit** of each dimension, through a representation
trait so `Rational` (exact, the default) and `double` (opt-in) share one
evaluator, and converts once at the boundary into the declared unit of the
result quantity. The result is `Outcome<Q>` — value, empty, verdict or invalid —
not a scalar.

**Tech Stack:** C++23 header-only; `std::expected`, `std::optional`, `std::tuple`;
Catch2 v3 via CPM; CMake ≥ 3.23 + Ninja; cl, clang-cl, clang++ and g++.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — this plan
implements **§9 (Expressions)** minus the documented-wrapper (§10, phase 6),
conditionals and rounding nodes (phase 8), constraints (phase 9) and the series
nodes (§12, phase 12). It is **spec phase 5 of §17**.

## Global Constraints

- **C++23, header-only, no macros.** Every header compiles standalone under
  `cl`, `clang-cl`, `clang++` and `g++`.
- **Four presets before any commit.** `cl-debug`, `cl-release`,
  `clangcl-debug`, `clangcl-release` must all configure, build and test green.
  A change tested on one preset only is not tested.
- **No norm content in this repository, and no citations either.** Never name a
  standard, and never transcribe its text, equations, tables, clause numbers or
  threshold values. Published standards are copyrighted and sold, and this
  repository is public. Public examples use **generic physics only**, with
  fictional `Example Standard` citations where a citation's *shape* has to be
  demonstrated.
- **Two-layer error model.** `checked_<name>` returns
  `std::expected<T, ArithmeticError>` and is `noexcept`; the bare `<name>`
  throws `ArithmeticException`. Never a wrong number.
- **Absence propagates.** A `Measured<Q>` with no value is *absent*, never zero.
  An absent operand makes the whole evaluation empty.
- **Diagnostics are part of the product.** Every `static_assert` message starts
  with `formula: `, states the problem in a whole sentence, and says where in
  the diagnostic the offending types appear. Every message is pinned by a
  negative-compile test that greps for a distinctive phrase.
- **No test may pass for the wrong reason.** Every new test is mutation-tested:
  break the code it covers, confirm *that* test fails with *that* reason, then
  restore. Record the mutation and the observed failure in the task report.
- **Examples link `support/fail_without_dialogs.cpp`.** Use the
  `formula_add_example(name source expectedOutput)` function in
  `examples/CMakeLists.txt`; never add an executable by hand. A failed assertion
  must print to stderr and exit, never raise a modal Windows dialog.
- **British spelling** in prose and doc comments; `snake_case` for free
  functions, `PascalCase` for types, `camelCase` for locals and members with a
  leading underscore for private data members. Match `.clang-format` exactly;
  run it before committing.

## What the spike already settled

A throwaway spike of this design was compiled on **cl 19.51**, **clang-cl 22**
and **g++ 13.3** at C++23 with the project's warning flags before this plan was
written. These are measurements, not expectations:

| Question | Measured answer |
|---|---|
| Do empty node structs composed by operators compile on all three? | Yes, `/W4 /WX` and `-Wall -Wextra -Werror -pedantic` clean |
| Does a dimensional error point at the user's line? | Yes on all three; cl and clang both name the operand node types and the source line of the formula |
| Is `Op` readable in the diagnostic? | **No** when the check mentions `Op`: clang prints `(formula::BinaryOperator)'\x00'` with an `unsigned char` underlying type and `(formula::BinaryOperator)0` without one. Moving the check into `detail::RequireAddendsAgree<Left, Right>` — whose template arguments are the *node types* — prints `formula::VarNode<WaterVolume>::dimension == formula::VarNode<BeamLength>::dimension`. **The plan uses that shape.** |
| Does a missing environment entry name the quantity? | Yes: cl and clang both print `get<BeamLength>` and `Environment<WaterVolume>` |
| Does `std::expected<std::optional<Rep>, ArithmeticError>` survive constexpr evaluation on all three? | Yes; whole evaluations are usable inside `static_assert` |
| Do `Rational` and `double` share one evaluator? | Yes, through a `RepTraits<Rep>` seam; `180 l / 300 l` is exactly `3/5` in `Rational` |

---

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/outcome.hpp` | **Create.** `ValueSource`, `Value<Q>`, `Verdict`, `InvalidReason`, `Outcome<Q>` — what an evaluation returns. |
| `include/formula-cpp/expression.hpp` | **Create.** `NodeBase`, the `Node` concept, `VarNode`, `ConstantNode`, `UnaryNode`, `BinaryNode`, the dimension checks, and the operators. |
| `include/formula-cpp/function.hpp` | **Create.** `PowerNode`, `RootNode`, `pi`, and the `pow<N>` / `sqrt` / `cbrt` spellings. |
| `include/formula-cpp/environment.hpp` | **Create.** `Entered<Q>`, `Environment<...>`, `environment(...)`. |
| `include/formula-cpp/evaluate.hpp` | **Create.** `RepTraits`, `coherent()`, `checked_evaluate` / `evaluate`. |
| `include/formula-cpp/rational.hpp` | **Modify.** Add `checked_sqrt`, `checked_nth_root` and `Pi` (task 5). |
| `include/formula-cpp/formula.hpp` | **Modify.** Include the new headers; drop `evaluation.hpp`. |
| `include/formula-cpp/evaluation.hpp` | **Delete** (task 6) — the PoC evaluator the spec retires in this phase. |
| `include/formula-cpp/detail/type_list.hpp` | **Delete** (task 6) if nothing else uses it after `evaluation.hpp` goes. |
| `test/outcome_tests.cpp` | **Create.** |
| `test/expression_tests.cpp` | **Create.** |
| `test/function_tests.cpp` | **Create.** |
| `test/environment_tests.cpp` | **Create.** |
| `test/evaluate_tests.cpp` | **Create.** |
| `test/expression_cross_tu.hpp`, `test/expression_cross_tu_b.cpp` | **Create.** Two translation units must agree on a formula's type. |
| `test/negative/*.cpp` | **Create** eight; **delete** `duplicate_producer.cpp` (task 6). |
| `examples/expressions.cpp` | **Create.** |
| `examples/simple.cpp` | **Rewrite** against the new API. |
| `docs/expressions.md` | **Create**, and add to `mkdocs.yml`. |

---

## Task 1: The evaluation outcome

**Files:**
- Create: `include/formula-cpp/outcome.hpp`
- Test: `test/outcome_tests.cpp`
- Modify: `test/CMakeLists.txt` (add the new test source)

**Interfaces:**
- Consumes: `formula::Measured<Q>` and the `Described` concept from
  `include/formula-cpp/measured.hpp` and `include/formula-cpp/quantity.hpp`.
- Produces:
  ```cpp
  enum class ValueSource { Derived, Measured, ManuallyEntered };
  enum class OutcomeKind { Value, Empty, Verdict, Invalid };
  struct Verdict { std::string_view label; };
  struct InvalidReason { std::string_view label; };
  template <Described Q> struct Value { Measured<Q> measurement; ValueSource source; };
  template <Described Q> class Outcome;   // see below for the full surface
  ```

**Why a sum type at all.** The spec's §16.2 is blunt about it: a method may
yield a verdict instead of a number, or discard a result outright, and a
`double`-returning evaluator has nowhere to put those. Modelling them as
exceptions or sentinels makes them invisible to the trace, which is exactly
where they need to be visible.

**Ruling: `overridden` is not a fifth kind.** The spec sketches the sum type as
`value | verdict | invalid(reason) | overridden(value, source)`. This plan
folds `overridden` into the `Value` kind, distinguished by
`Value<Q>::source == ValueSource::ManuallyEntered`, and exposes it as
`is_overridden()`. Rationale: provenance then has exactly one home, so a kind
and a source field can never disagree. Cost if wrong: renaming a query.

**Ruling: `Empty` is a kind, and it is an invariant, not a second opinion.**
`Outcome::value()` inspects the `Measured` it is handed and yields `Empty`
when it is absent. `kind() == Value` therefore implies the measurement has a
value, and the class asserts it. Cost if wrong: one redundant branch at call
sites that already check `has_value()`.

- [ ] **Step 1: Write the failing test**

Create `test/outcome_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/outcome.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Mass: formula::Quantity<Mass, "m", "specimen mass", formula::unit::Kilogram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

} // namespace

TEST_CASE("outcome: a present measurement is a value", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Value);
    STATIC_REQUIRE(outcome.is_value());
    STATIC_REQUIRE_FALSE(outcome.is_empty());
    STATIC_REQUIRE_FALSE(outcome.is_overridden());
    STATIC_REQUIRE(outcome.measurement().value() == rat(3));
    STATIC_REQUIRE(outcome.source() == formula::ValueSource::Derived);
}

TEST_CASE("outcome: an absent measurement is empty, not a value", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass>::absent(), formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Empty);
    STATIC_REQUIRE(outcome.is_empty());
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.measurement().is_absent());
}

TEST_CASE("outcome: a manually entered value is a value and is overridden", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(7, 2) },
                                      formula::ValueSource::ManuallyEntered);

    STATIC_REQUIRE(outcome.is_value());
    STATIC_REQUIRE(outcome.is_overridden());
    STATIC_REQUIRE(outcome.source() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(outcome.measurement().value() == rat(7, 2));
}

TEST_CASE("outcome: a verdict carries its label and no measurement", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::verdict(formula::Verdict { "repeat the test" });

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Verdict);
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.verdict_label() == std::string_view { "repeat the test" });
    STATIC_REQUIRE(outcome.measurement().is_absent());
}

TEST_CASE("outcome: an invalid result carries its reason", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::invalid(formula::InvalidReason { "outlier rejection removed every specimen" });

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Invalid);
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.reason_label()
                   == std::string_view { "outlier rejection removed every specimen" });
}

TEST_CASE("outcome: two outcomes of the same kind and payload compare equal", "[outcome]")
{
    constexpr formula::Outcome<Mass> left = formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) },
                                                                          formula::ValueSource::Measured);
    constexpr formula::Outcome<Mass> right = formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) },
                                                                           formula::ValueSource::Measured);
    constexpr formula::Outcome<Mass> other = formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) },
                                                                           formula::ValueSource::Derived);

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("outcome: the label of a non-verdict outcome is empty rather than stale", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(1) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.verdict_label().empty());
    STATIC_REQUIRE(outcome.reason_label().empty());
}
```

Add the source to `test/CMakeLists.txt` in the existing test-source list,
keeping the list alphabetical.

- [ ] **Step 2: Run the test to verify it fails**

```
cmake --preset cl-debug && cmake --build --preset cl-debug
```
Expected: a compile error, `Cannot open include file: 'formula-cpp/outcome.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/outcome.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// What an evaluation produces.
///
/// A calculation defined by a test method does not always produce a number. It
/// may reject the specimen, discard the whole result, or call for the test to
/// be repeated -- and for a substantial minority of methods that verdict *is*
/// the primary output rather than an exception path. A `double`-returning
/// evaluator has nowhere to put any of it, and a library that models them as
/// exceptions or sentinel values makes them invisible to the audit trail, which
/// is precisely where they have to be visible.
///
/// So evaluation returns `Outcome<Q>`: a value, an empty, a verdict or an
/// invalidation. `Verdict` and `InvalidReason` carry a bare label here; the
/// vocabulary that produces them arrives with constraints, and nothing in this
/// header needs to know it.

#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>

#include <cstdint>
#include <string_view>

namespace formula
{

/// How a value came to be. A result that was typed in by an operator must never
/// be presented as though the library had derived it, so provenance travels
/// with the number rather than beside it.
enum class ValueSource : std::uint8_t
{
    /// Computed by this library from other values.
    Derived,
    /// Read from an instrument or entered as an observation of the specimen.
    Measured,
    /// Typed in by a person, replacing whatever would have been computed.
    ManuallyEntered,
};

/// Which alternative an `Outcome` holds.
///
/// There is deliberately no `Overridden` alternative: an override is the `Value`
/// alternative whose source is `ManuallyEntered`, so provenance has exactly one
/// home and a kind cannot contradict a source. `Outcome::is_overridden()` asks
/// the question directly.
enum class OutcomeKind : std::uint8_t
{
    Value,
    Empty,
    Verdict,
    Invalid,
};

/// A decision rather than a number: "reject the specimen", "repeat the test".
struct Verdict
{
    std::string_view label {};

    [[nodiscard]] constexpr bool operator==(Verdict const&) const noexcept = default;
};

/// Why a result was discarded entirely.
struct InvalidReason
{
    std::string_view label {};

    [[nodiscard]] constexpr bool operator==(InvalidReason const&) const noexcept = default;
};

/// A number together with where it came from.
template <Described Q>
struct Value
{
    Measured<Q> measurement {};
    ValueSource source = ValueSource::Derived;

    [[nodiscard]] constexpr bool operator==(Value const&) const noexcept = default;
};

/// The result of evaluating an expression for quantity @p Q.
template <Described Q>
class Outcome
{
  public:
    /// A computed or measured number. An **absent** measurement yields `Empty`,
    /// not a `Value` holding nothing: the two would otherwise be two ways of
    /// saying the same thing, and callers would have to check both.
    [[nodiscard]] static constexpr Outcome value(Measured<Q> measurement, ValueSource source) noexcept
    {
        Outcome result {};
        result._kind = measurement.has_value() ? OutcomeKind::Value : OutcomeKind::Empty;
        result._value = Value<Q> { measurement, source };
        return result;
    }

    /// No value, because an input was never measured.
    [[nodiscard]] static constexpr Outcome empty() noexcept
    {
        return value(Measured<Q>::absent(), ValueSource::Derived);
    }

    [[nodiscard]] static constexpr Outcome verdict(Verdict decision) noexcept
    {
        Outcome result {};
        result._kind = OutcomeKind::Verdict;
        result._verdict = decision;
        return result;
    }

    [[nodiscard]] static constexpr Outcome invalid(InvalidReason reason) noexcept
    {
        Outcome result {};
        result._kind = OutcomeKind::Invalid;
        result._reason = reason;
        return result;
    }

    [[nodiscard]] constexpr OutcomeKind kind() const noexcept { return _kind; }

    [[nodiscard]] constexpr bool is_value() const noexcept { return _kind == OutcomeKind::Value; }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return _kind == OutcomeKind::Empty; }
    [[nodiscard]] constexpr bool is_verdict() const noexcept { return _kind == OutcomeKind::Verdict; }
    [[nodiscard]] constexpr bool is_invalid() const noexcept { return _kind == OutcomeKind::Invalid; }

    /// True when a person typed this result in place of a computed one.
    [[nodiscard]] constexpr bool is_overridden() const noexcept
    {
        return is_value() && _value.source == ValueSource::ManuallyEntered;
    }

    /// The measurement, absent for every kind but `Value`.
    [[nodiscard]] constexpr Measured<Q> measurement() const noexcept { return _value.measurement; }

    /// Where the number came from. Meaningful only when `is_value()`.
    [[nodiscard]] constexpr ValueSource source() const noexcept { return _value.source; }

    /// Empty for every kind but `Verdict` -- never a stale label from a
    /// different alternative.
    [[nodiscard]] constexpr std::string_view verdict_label() const noexcept { return _verdict.label; }

    /// Empty for every kind but `Invalid`.
    [[nodiscard]] constexpr std::string_view reason_label() const noexcept { return _reason.label; }

    [[nodiscard]] constexpr bool operator==(Outcome const&) const noexcept = default;

  private:
    OutcomeKind _kind = OutcomeKind::Empty;
    Value<Q> _value {};
    Verdict _verdict {};
    InvalidReason _reason {};
};

} // namespace formula
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug && ctest --preset cl-debug
```
Expected: every `[outcome]` test passes.

- [ ] **Step 5: Mutation-test the suite**

Make each of these three changes on its own, rebuild, record which test fails
and with what message, then restore:

1. In `Outcome::value`, replace the ternary with `result._kind = OutcomeKind::Value;`.
   Expected: *"an absent measurement is empty, not a value"* fails on
   `kind() == OutcomeKind::Empty`.
2. In `is_overridden`, drop the `is_value() &&` conjunct.
   Expected: no test fails — **so add one**: a `verdict` outcome whose
   `is_overridden()` must be false. Add it, confirm it fails under the
   mutation, restore, confirm it passes.
3. In `verdict_label`, return `_reason.label`.
   Expected: *"a verdict carries its label"* fails.

Record all three in the task report, including mutation 2's new test.

- [ ] **Step 6: Run the other three presets**

```
cmake --preset cl-release       && cmake --build --preset cl-release       && ctest --preset cl-release
cmake --preset clangcl-debug    && cmake --build --preset clangcl-debug    && ctest --preset clangcl-debug
cmake --preset clangcl-release  && cmake --build --preset clangcl-release  && ctest --preset clangcl-release
```
Expected: green on all four.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/outcome.hpp test/outcome_tests.cpp test/CMakeLists.txt
git commit -m "feat(outcome): evaluation yields a sum type, not a number"
```
---

## Task 2: Expression nodes and the operators

**Files:**
- Create: `include/formula-cpp/expression.hpp`
- Test: `test/expression_tests.cpp`
- Test: `test/expression_cross_tu.hpp`, `test/expression_cross_tu_b.cpp`
- Create: `test/negative/expression_add_dimension_mismatch.cpp`
- Create: `test/negative/expression_subtract_dimension_mismatch.cpp`
- Create: `test/negative/expression_inconsistent_describe.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Dimension`, `dim::*`, `power`, `nth_root`, `is_dimensionless` from
  `dimension.hpp`; `Unit`, `unit::One` from `unit.hpp`; `Describe<Q>`,
  `Described`, `DescribesConsistentDimension` from `quantity.hpp`; `Rational`
  from `rational.hpp`.
- Produces:
  ```cpp
  struct NodeBase {};
  template <typename T> concept Node = std::derived_from<std::remove_cvref_t<T>, NodeBase>;

  template <Described Q> struct VarNode;          // ::quantity, ::dimension
  template <Described Q> inline constexpr VarNode<Q> var {};

  template <Unit U> struct ConstantNode;          // .number, ::unit, ::dimension
  template <Unit U> constexpr ConstantNode<U> constant(Rational) noexcept;
  constexpr ConstantNode<unit::One> number(Rational) noexcept;

  enum class UnaryOperator { Negate };
  enum class BinaryOperator { Add, Subtract, Multiply, Divide };
  template <UnaryOperator Op, Node Operand> struct UnaryNode;   // .operand
  template <BinaryOperator Op, Node Left, Node Right> struct BinaryNode;  // .lhs, .rhs

  // operator+ - * / over two Nodes, and over a Node and a Rational either way;
  // unary operator- over one Node.
  ```

**The whole point of this task.** `static constexpr Dimension dimension` is
computed at *class* scope, so instantiating the node runs the check — and the
node is instantiated by the operator the user wrote. The error therefore lands
on the formula's own source line rather than at evaluation, which may be in a
different file compiled much later.

**Ruling: the additive check lives in `detail::RequireAddendsAgree<Left, Right>`,
not in `BinaryNode`'s own `static_assert`.** Measured on the spike: an assert
whose condition mentions `Op` makes clang print
`(formula::BinaryOperator)'\x00'` (with an `unsigned char` underlying type) or
`(formula::BinaryOperator)0` (without one) in its *due to requirement* clause.
Hoisting the comparison into a helper templated on the operand types only makes
clang print `formula::VarNode<WaterVolume>::dimension ==
formula::VarNode<BeamLength>::dimension`, and cl and g++ name the same two types
in their instantiation notes. The helper costs one indirection and buys a
diagnostic a reader can act on.

**Ruling: mixing a bare `Rational` into a formula wraps it as a dimensionless
constant.** `var<Diameter> * number(rat(1, 4))` is the spelling with no
shorthand; `var<Diameter> * rat(1, 4)` is the shorthand, and it means exactly
the same tree. A dimensioned constant must be spelled `constant<unit::Metre>(…)`
— there is no way to guess a unit for a bare number, and guessing wrong is the
failure mode the whole dimension layer exists to prevent. Cost if wrong: the
shorthand is additive and can be removed without touching the node types.

- [ ] **Step 1: Write the failing test**

Create `test/expression_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/expression.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct BeamLength: formula::Quantity<BeamLength, "L", "beam length", formula::unit::Millimetre>
{
};
struct AppliedForce: formula::Quantity<AppliedForce, "F", "applied force", formula::unit::Kilogram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

} // namespace

TEST_CASE("expression: a variable carries its quantity's dimension", "[expression]")
{
    STATIC_REQUIRE(formula::VarNode<WaterVolume>::dimension == formula::dim::Volume);
    STATIC_REQUIRE(formula::VarNode<BeamLength>::dimension == formula::dim::Length);
    STATIC_REQUIRE(std::is_same_v<formula::VarNode<WaterVolume>::quantity, WaterVolume>);
}

TEST_CASE("expression: every node satisfies the Node concept", "[expression]")
{
    STATIC_REQUIRE(formula::Node<formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE(formula::Node<decltype(var<WaterVolume> + var<CementVolume>)>);
    STATIC_REQUIRE(formula::Node<decltype(-var<WaterVolume>)>);
    STATIC_REQUIRE(formula::Node<decltype(formula::number(rat(2)))>);
    STATIC_REQUIRE_FALSE(formula::Node<formula::Rational>);
    STATIC_REQUIRE_FALSE(formula::Node<int>);
}

TEST_CASE("expression: multiplication and division combine dimensions", "[expression]")
{
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
    constexpr auto moment = var<AppliedForce> * var<BeamLength>;

    STATIC_REQUIRE(formula::is_dimensionless(decltype(ratio)::dimension));
    STATIC_REQUIRE(decltype(moment)::dimension == formula::dim::Mass * formula::dim::Length);
}

TEST_CASE("expression: addition and subtraction keep the shared dimension", "[expression]")
{
    constexpr auto total = var<WaterVolume> + var<CementVolume>;
    constexpr auto excess = var<WaterVolume> - var<CementVolume>;

    STATIC_REQUIRE(decltype(total)::dimension == formula::dim::Volume);
    STATIC_REQUIRE(decltype(excess)::dimension == formula::dim::Volume);
}

TEST_CASE("expression: negation keeps the operand's dimension", "[expression]")
{
    constexpr auto negated = -var<BeamLength>;

    STATIC_REQUIRE(decltype(negated)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(negated)::op == formula::UnaryOperator::Negate);
}

TEST_CASE("expression: a bare number is dimensionless and keeps its value", "[expression]")
{
    constexpr auto half = formula::number(rat(1, 2));

    STATIC_REQUIRE(formula::is_dimensionless(decltype(half)::dimension));
    STATIC_REQUIRE(decltype(half)::unit == formula::unit::One);
    STATIC_REQUIRE(half.number == rat(1, 2));
}

TEST_CASE("expression: a dimensioned constant carries its unit's dimension", "[expression]")
{
    constexpr auto span = formula::constant<formula::unit::Millimetre>(rat(150));

    STATIC_REQUIRE(decltype(span)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(span)::unit == formula::unit::Millimetre);
    STATIC_REQUIRE(span.number == rat(150));
}

TEST_CASE("expression: a Rational mixed into a formula becomes a dimensionless constant",
          "[expression]")
{
    constexpr auto scaledRight = var<BeamLength> * rat(1, 4);
    constexpr auto scaledLeft = rat(1, 4) * var<BeamLength>;

    STATIC_REQUIRE(decltype(scaledRight)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(scaledLeft)::dimension == formula::dim::Length);
    STATIC_REQUIRE(std::is_same_v<decltype(scaledRight),
                                  formula::BinaryNode<formula::BinaryOperator::Multiply,
                                                      formula::VarNode<BeamLength>,
                                                      formula::ConstantNode<formula::unit::One>> const>);
    // `scaledRight` is itself `constexpr`, so `decltype` of the *variable* does
    // carry the const -- unlike `decltype(ratio.lhs)` above, which names a member.
}

TEST_CASE("expression: the tree keeps its shape and its operands", "[expression]")
{
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;

    STATIC_REQUIRE(decltype(ratio)::op == formula::BinaryOperator::Divide);
    // `decltype` of a member access yields the member's declared type, with no
    // const from the object it was read through -- so no `const` here.
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.lhs), formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.rhs), formula::VarNode<CementVolume>>);
}

TEST_CASE("expression: a deep tree carries no state beyond its constants", "[expression]")
{
    constexpr auto deep =
        (var<WaterVolume> + var<CementVolume>) / (var<WaterVolume> - var<CementVolume>) * var<BeamLength>;

    STATIC_REQUIRE(decltype(deep)::dimension == formula::dim::Length);

    // A variable declares no members; a constant declares exactly its number.
    // Do NOT assert a size for a composed tree: a node holds its children by
    // value, and how much an empty child costs inside its parent is the
    // compiler's business. Measured, the five-leaf tree above was 5 bytes on
    // cl and clang-cl and 9 on g++ -- correct on all three, portable on none.
    STATIC_REQUIRE(std::is_empty_v<formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE_FALSE(std::is_empty_v<formula::ConstantNode<formula::unit::One>>);
}

TEST_CASE("expression: the same formula written twice is the same type", "[expression]")
{
    constexpr auto first = var<WaterVolume> / var<CementVolume>;
    constexpr auto second = var<WaterVolume> / var<CementVolume>;

    STATIC_REQUIRE(std::is_same_v<decltype(first), decltype(second)>);
}
```

Create `test/expression_cross_tu.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <formula-cpp/expression.hpp>

struct SampleVolume: formula::Quantity<SampleVolume, "V", "sample volume", formula::unit::Litre>
{
};
struct SampleMass: formula::Quantity<SampleMass, "m", "sample mass", formula::unit::Kilogram>
{
};

using Density = decltype(formula::var<SampleMass> / formula::var<SampleVolume>);

/// Defined in `expression_cross_tu_b.cpp`: the address of the `var` object that
/// translation unit sees, so the test can prove both units share one object
/// rather than each getting a private copy.
[[nodiscard]] void const* sample_mass_variable_address_in_other_tu() noexcept;

/// Defined in `expression_cross_tu_b.cpp`: a density formula built there.
[[nodiscard]] Density density_built_in_other_tu() noexcept;
```

Create `test/expression_cross_tu_b.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include "expression_cross_tu.hpp"

void const* sample_mass_variable_address_in_other_tu() noexcept
{
    return static_cast<void const*>(&formula::var<SampleMass>);
}

Density density_built_in_other_tu() noexcept
{
    return formula::var<SampleMass> / formula::var<SampleVolume>;
}
```

Append to `test/expression_tests.cpp`:

```cpp
#include "expression_cross_tu.hpp"

TEST_CASE("expression: two translation units agree on a formula's type", "[expression]")
{
    // The declaration in the header already forces this: `density_built_in_other_tu`
    // returns `Density`, and the other TU builds the tree with its own operators.
    // If the types differed the program would not link.
    auto const fromOtherTranslationUnit = density_built_in_other_tu();
    auto const fromThisTranslationUnit = formula::var<SampleMass> / formula::var<SampleVolume>;

    STATIC_REQUIRE(std::is_same_v<decltype(fromOtherTranslationUnit), decltype(fromThisTranslationUnit)>);
    CHECK(decltype(fromThisTranslationUnit)::dimension == formula::dim::Density);
}

TEST_CASE("expression: a variable template is one object across translation units", "[expression]")
{
    // `inline constexpr` gives the variable template external linkage with one
    // definition; a missing `inline` would give each TU its own copy, and this
    // is the only test that would notice.
    CHECK(sample_mass_variable_address_in_other_tu() == static_cast<void const*>(&formula::var<SampleMass>));
}
```

Create the three negative-compile tests. `test/negative/expression_add_dimension_mismatch.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
#include <formula-cpp/expression.hpp>

struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// A volume plus a length has no meaning, and must not compile.
inline constexpr auto broken = formula::var<Volume> + formula::var<Length>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
```

`test/negative/expression_subtract_dimension_mismatch.cpp` is the same file with
`+` replaced by `-`; write it out in full rather than referring to its
neighbour.

`test/negative/expression_inconsistent_describe.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: describes a dimension its own unit does not measure
#include <formula-cpp/expression.hpp>

struct Muddle
{
};

// A hand-written Describe whose declared dimension contradicts its unit: the
// unit measures a length, the dimension claims a mass.
template <>
struct formula::Describe<Muddle>
{
    static constexpr std::string_view symbol = "x";
    static constexpr std::string_view description = "a muddled quantity";
    static constexpr formula::Unit unit = formula::unit::Metre;
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};

inline constexpr auto broken = formula::var<Muddle>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.mass.numerator);
}
```

Register all three in `test/CMakeLists.txt` beside the existing negative tests,
each with the `EXPECT` phrase from its header comment.

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --preset cl-debug && cmake --build --preset cl-debug
```
Expected: `Cannot open include file: 'formula-cpp/expression.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/expression.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The expression layer: formulas written with ordinary operators.
///
/// A formula is a *type*. `var<WaterVolume> / var<CementVolume>` builds a
/// `BinaryNode<Divide, VarNode<WaterVolume>, VarNode<CementVolume>>`, and every
/// node publishes `static constexpr Dimension dimension` computed at class
/// scope. Because the operator the user wrote is what instantiates the node,
/// a dimensional mistake is a compile error on the line the formula is written
/// on -- not at evaluation, which may happen in another file compiled an hour
/// later.
///
/// A structure node declares no members of its own -- its whole shape is in the
/// type -- so a tree is a compile-time entity and traversing it inlines away.
/// It is not literally an empty class: a node holds its children by value, and
/// an empty child still occupies a byte, so a tree costs roughly one byte per
/// leaf and nothing per level. `ConstantNode` is the one node with real state,
/// because a coefficient may arrive from a table at runtime.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/unit.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace formula
{

/// The base every expression node derives from, so one concept recognises them
/// all. Empty, and inherited rather than held, so an empty node stays empty.
struct NodeBase
{
};

/// Anything that can appear in a formula.
template <typename T>
concept Node = std::derived_from<std::remove_cvref_t<T>, NodeBase>;

/// A named input: the quantity is the key the environment is asked with, and
/// the source of the node's dimension.
template <Described Q>
struct VarNode: NodeBase
{
    static_assert(DescribesConsistentDimension<Q>,
                  "formula: this quantity describes a dimension its own unit does not measure, so "
                  "no formula containing it can be trusted; the quantity appears in this "
                  "diagnostic as the template argument of VarNode");

    using quantity = Q;

    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The spelling of a variable in a formula: `var<WaterVolume>`.
///
/// `inline` matters and is not decoration. Without it each translation unit
/// would get its own object, two of which are not the same variable; with it
/// the whole program shares one, which `expression_cross_tu` pins.
template <Described Q>
inline constexpr VarNode<Q> var {};

/// A literal coefficient, stated in a unit so it can be converted like any
/// other value. The unit is part of the type; the number is not, because a
/// coefficient may arrive from a table at runtime.
template <Unit U>
struct ConstantNode: NodeBase
{
    Rational number {};

    static constexpr Unit unit = U;
    static constexpr Dimension dimension = U.dimension;
};

/// A coefficient with a unit: `constant<unit::Millimetre>(rat(150))`.
template <Unit U>
[[nodiscard]] constexpr ConstantNode<U> constant(Rational value) noexcept
{
    return ConstantNode<U> { {}, value };
}

/// A dimensionless coefficient: `number(rat(1, 4))`.
///
/// There is deliberately no overload that guesses a unit for a bare number.
/// Guessing wrong is exactly the failure the dimension layer exists to prevent.
[[nodiscard]] constexpr ConstantNode<unit::One> number(Rational value) noexcept
{
    return constant<unit::One>(value);
}

enum class UnaryOperator : std::uint8_t
{
    Negate,
};

enum class BinaryOperator : std::uint8_t
{
    Add,
    Subtract,
    Multiply,
    Divide,
};

namespace detail
{
    /// Fails to compile when the two sides of an addition or subtraction measure
    /// different dimensions.
    ///
    /// A named template, and templated on the *operands* rather than on the
    /// operator, entirely for the diagnostic's sake. Measured on cl 19.51,
    /// clang-cl 22 and g++ 13.3: an assertion whose condition mentions the
    /// operator makes clang print `(formula::BinaryOperator)0` in its "due to
    /// requirement" clause, because an enumerator used as a value in a
    /// dependent expression is rendered as a cast. Written this way, clang
    /// prints `formula::VarNode<WaterVolume>::dimension ==
    /// formula::VarNode<BeamLength>::dimension` instead, and all three
    /// compilers name the two operand types and the formula's own source line.
    template <Node Left, Node Right>
    struct RequireAddendsAgree
    {
        static_assert(Left::dimension == Right::dimension,
                      "formula: the two sides of this addition or subtraction measure different "
                      "dimensions; the offending operands appear in this diagnostic as the "
                      "template arguments of RequireAddendsAgree");

        static constexpr bool value = true;
    };

    /// True for multiplication and division, which impose nothing; the two
    /// specialisations below route addition and subtraction through the check.
    template <BinaryOperator Op, Node Left, Node Right>
    struct AdditiveDimensionsAgree: std::true_type
    {
    };

    template <Node Left, Node Right>
    struct AdditiveDimensionsAgree<BinaryOperator::Add, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    template <Node Left, Node Right>
    struct AdditiveDimensionsAgree<BinaryOperator::Subtract, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    template <BinaryOperator Op, Dimension Left, Dimension Right>
    [[nodiscard]] constexpr Dimension combined_dimension() noexcept
    {
        if constexpr (Op == BinaryOperator::Multiply)
            return Left * Right;
        else if constexpr (Op == BinaryOperator::Divide)
            return Left / Right;
        else
            return Left;
    }
} // namespace detail

template <UnaryOperator Op, Node Operand>
struct UnaryNode: NodeBase
{
    Operand operand {};

    static constexpr UnaryOperator op = Op;
    static constexpr Dimension dimension = Operand::dimension;
};

template <BinaryOperator Op, Node Left, Node Right>
struct BinaryNode: NodeBase
{
    static_assert(detail::AdditiveDimensionsAgree<Op, Left, Right>::value);

    Left lhs {};
    Right rhs {};

    static constexpr BinaryOperator op = Op;
    static constexpr Dimension dimension =
        detail::combined_dimension<Op, Left::dimension, Right::dimension>();
};

// ---------------------------------------------------------------- operators
//
// Taken and returned by value: nodes are empty or hold one `Rational`, so there
// is nothing to save by reference, and a reference into a temporary subtree is
// a dangling read waiting to happen.

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator+(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Add, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator-(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Subtract, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator*(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Multiply, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator/(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Divide, Left, Right> { {}, lhs, rhs };
}

template <Node Operand>
[[nodiscard]] constexpr auto operator-(Operand operand) noexcept
{
    return UnaryNode<UnaryOperator::Negate, Operand> { {}, operand };
}

// A bare `Rational` in a formula is a dimensionless coefficient. Spelled out
// per operator rather than through a converting constructor, so that a
// `Rational` never silently becomes a node anywhere else.

template <Node Left>
[[nodiscard]] constexpr auto operator+(Left lhs, Rational rhs) noexcept
{
    return lhs + number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator+(Rational lhs, Right rhs) noexcept
{
    return number(lhs) + rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator-(Left lhs, Rational rhs) noexcept
{
    return lhs - number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator-(Rational lhs, Right rhs) noexcept
{
    return number(lhs) - rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator*(Left lhs, Rational rhs) noexcept
{
    return lhs * number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator*(Rational lhs, Right rhs) noexcept
{
    return number(lhs) * rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator/(Left lhs, Rational rhs) noexcept
{
    return lhs / number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator/(Rational lhs, Right rhs) noexcept
{
    return number(lhs) / rhs;
}

} // namespace formula
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug && ctest --preset cl-debug
```
Expected: every `[expression]` test passes, the three negative tests compile-fail
with their expected phrases, and the two-TU test links.

- [ ] **Step 5: Mutation-test the suite**

Apply each on its own, rebuild, record the failing test and its message, restore:

1. In `combined_dimension`, swap `Left * Right` for `Left / Right`.
   Expected: *"multiplication and division combine dimensions"* fails on the
   moment's dimension.
2. In `combined_dimension`'s `else` branch, return `Dimension {}`.
   Expected: *"addition and subtraction keep the shared dimension"* fails.
3. Delete `inline` from the `var` variable template.
   Expected: *"a variable template is one object across translation units"*
   fails — or the link fails. Either counts; record which.
4. Change `AdditiveDimensionsAgree`'s `Subtract` specialisation to inherit
   `std::true_type`. Expected: the negative test
   `expression_subtract_dimension_mismatch` stops failing to compile, so the
   negative-test harness reports it as an unexpected success.
5. In `UnaryNode`, set `dimension` to `dim::Scalar`.
   Expected: *"negation keeps the operand's dimension"* fails.

If any mutation fails no test, the suite is incomplete: add the missing test
before moving on, and say so in the report.

- [ ] **Step 6: Run the other three presets**

As in task 1. Expected: green on all four.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/expression.hpp test/expression_tests.cpp \
        test/expression_cross_tu.hpp test/expression_cross_tu_b.cpp \
        test/negative/expression_add_dimension_mismatch.cpp \
        test/negative/expression_subtract_dimension_mismatch.cpp \
        test/negative/expression_inconsistent_describe.cpp test/CMakeLists.txt
git commit -m "feat(expression): formulas built from operators, checked where written"
```

---

## Task 3: The environment

**Files:**
- Create: `include/formula-cpp/environment.hpp`
- Test: `test/environment_tests.cpp`
- Create: `test/negative/environment_duplicate_quantity.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Measured<Q>`, `Described`; `ValueSource` from `outcome.hpp`.
- Produces:
  ```cpp
  template <Described Q> struct Entered { Measured<Q> measurement; };
  template <Described Q> constexpr Entered<Q> entered(Measured<Q>) noexcept;

  template <typename... Entries>
  class Environment
  {
      template <Described Q> static constexpr bool provides;
      template <Described Q> static constexpr bool is_entered;
      template <Described Q> constexpr Measured<Q> get() const noexcept;
      template <Described Q> constexpr ValueSource source_of() const noexcept;
  };

  template <typename... Entries> constexpr Environment<Entries...> environment(Entries...) noexcept;
  ```

**Why entries, not just measurements.** §16.1 records that a lab may type a
final result in by hand in place of the computed one, and that the trace must
never present a typed-in number as though the library derived it. An
environment therefore holds two kinds of entry: a `Measured<Q>` is an
observation that feeds a formula, and an `Entered<Q>` is a value a person
supplied. The evaluator (task 4) returns an `Entered` entry for the *result*
quantity instead of computing — which is what an override is — and leaves
`Measured` entries alone.

**Ruling: an override must be spelled `entered(...)`.** The alternative, letting
any `Measured<Result>` in the environment override the formula, makes an
accidental entry silently replace a computed result with no diagnostic. Cost if
wrong: one more word at the call site.

**Ruling: supplying the same quantity twice is a compile error.** Two entries
for one quantity have no defensible resolution — first wins and last wins are
equally arbitrary, and the caller certainly meant one of them. Cost if wrong:
a caller that wanted "replace this entry" writes a new environment instead.

- [ ] **Step 1: Write the failing test**

Create `test/environment_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/environment.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

constexpr auto twoInputs = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                formula::Measured<CementVolume> { rat(300) });

} // namespace

TEST_CASE("environment: it answers for the quantities it holds", "[environment]")
{
    STATIC_REQUIRE(decltype(twoInputs)::provides<WaterVolume>);
    STATIC_REQUIRE(decltype(twoInputs)::provides<CementVolume>);
    STATIC_REQUIRE_FALSE(decltype(twoInputs)::provides<Ratio>);
}

TEST_CASE("environment: a value comes back as it went in", "[environment]")
{
    STATIC_REQUIRE(twoInputs.get<WaterVolume>().value() == rat(180));
    STATIC_REQUIRE(twoInputs.get<CementVolume>().value() == rat(300));
}

TEST_CASE("environment: an absent measurement stays absent", "[environment]")
{
    constexpr auto partial =
        formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(300) });

    STATIC_REQUIRE(partial.get<WaterVolume>().is_absent());
    STATIC_REQUIRE(partial.get<CementVolume>().has_value());
}

TEST_CASE("environment: a measurement is sourced as measured", "[environment]")
{
    STATIC_REQUIRE(twoInputs.source_of<WaterVolume>() == formula::ValueSource::Measured);
    STATIC_REQUIRE_FALSE(decltype(twoInputs)::is_entered<WaterVolume>);
}

TEST_CASE("environment: an entered value is sourced as manually entered", "[environment]")
{
    constexpr auto withOverride = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                       formula::entered(formula::Measured<Ratio> { rat(45, 100) }));

    STATIC_REQUIRE(decltype(withOverride)::provides<Ratio>);
    STATIC_REQUIRE(decltype(withOverride)::is_entered<Ratio>);
    STATIC_REQUIRE(withOverride.source_of<Ratio>() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(withOverride.get<Ratio>().value() == rat(45, 100));
    STATIC_REQUIRE_FALSE(decltype(withOverride)::is_entered<WaterVolume>);
}

TEST_CASE("environment: an empty environment provides nothing", "[environment]")
{
    constexpr auto nothing = formula::environment();

    STATIC_REQUIRE_FALSE(decltype(nothing)::provides<WaterVolume>);
}

TEST_CASE("environment: entries keep their identity whatever order they are given in",
          "[environment]")
{
    constexpr auto reversed = formula::environment(formula::Measured<CementVolume> { rat(300) },
                                                   formula::Measured<WaterVolume> { rat(180) });

    STATIC_REQUIRE(reversed.get<WaterVolume>().value() == rat(180));
    STATIC_REQUIRE(reversed.get<CementVolume>().value() == rat(300));
}
```

Create `test/negative/environment_duplicate_quantity.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: supplies the same quantity more than once
#include <formula-cpp/environment.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};

// Two entries for one quantity: there is no defensible way to pick one.
inline constexpr auto broken = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                    formula::Measured<WaterVolume> { formula::Rational { 200 } });

int main()
{
    return broken.get<WaterVolume>().has_value() ? 0 : 1;
}
```

Also add a negative test `test/negative/environment_missing_quantity.cpp`
asking an environment for a quantity it does not hold, with
`// EXPECT: provides no value for this quantity`.

- [ ] **Step 2: Run to verify it fails**

Expected: `Cannot open include file: 'formula-cpp/environment.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/environment.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Where a formula's inputs come from.
///
/// An environment is keyed by quantity *type*, not by name: asking for a
/// quantity it does not hold is a compile error naming that quantity, so a
/// formula can never silently evaluate against a missing input.
///
/// Entries come in two kinds. A `Measured<Q>` is an observation that feeds the
/// formula. An `Entered<Q>` is a number a person typed in; when the evaluator
/// finds one for the quantity it was asked to produce, it returns that instead
/// of computing, and says so in the result's source. A result that was typed in
/// must never be presented as though the library had derived it.

#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace formula
{

/// A value a person supplied, as opposed to one the apparatus reported.
template <Described Q>
struct Entered
{
    Measured<Q> measurement {};

    [[nodiscard]] constexpr bool operator==(Entered const&) const noexcept = default;
};

/// Marks a measurement as manually entered: `entered(Measured<Ratio> { … })`.
template <Described Q>
[[nodiscard]] constexpr Entered<Q> entered(Measured<Q> measurement) noexcept
{
    return Entered<Q> { measurement };
}

namespace detail
{
    /// Reads the quantity and the provenance out of an environment entry. The
    /// primary template is deliberately empty, so a type that is neither a
    /// `Measured` nor an `Entered` fails at the point it is used rather than
    /// silently satisfying something.
    template <typename Entry>
    struct EntryTraits
    {
    };

    template <Described Q>
    struct EntryTraits<Measured<Q>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::Measured;
        static constexpr bool isEntered = false;

        [[nodiscard]] static constexpr Measured<Q> measurement(Measured<Q> entry) noexcept { return entry; }
    };

    template <Described Q>
    struct EntryTraits<Entered<Q>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::ManuallyEntered;
        static constexpr bool isEntered = true;

        [[nodiscard]] static constexpr Measured<Q> measurement(Entered<Q> entry) noexcept
        {
            return entry.measurement;
        }
    };

    /// An entry this environment understands.
    template <typename Entry>
    concept EnvironmentEntry = requires { typename EntryTraits<Entry>::quantity; };

    /// Fails to compile when one quantity is supplied twice. First-wins and
    /// last-wins are equally arbitrary, and the caller meant exactly one of
    /// them, so neither is guessed.
    template <typename... Entries>
    struct RequireDistinctQuantities
    {
        static constexpr std::size_t count = sizeof...(Entries);

        template <typename Entry>
        static constexpr std::size_t occurrences =
            (std::size_t { 0 } + ...
             + std::size_t { std::is_same_v<typename EntryTraits<Entry>::quantity,
                                            typename EntryTraits<Entries>::quantity> });

        static_assert((... && (occurrences<Entries> == 1)),
                      "formula: this environment supplies the same quantity more than once; the "
                      "entries appear in this diagnostic as the template arguments of "
                      "RequireDistinctQuantities");

        static constexpr bool value = true;
    };

    /// Fails to compile when an environment is asked for a quantity it does not
    /// hold. Named so the quantity and the environment both print.
    template <typename Q, typename Environment>
    struct RequireProvided
    {
        static_assert(Environment::template provides<Q>,
                      "formula: this environment provides no value for this quantity; the quantity "
                      "and the environment appear in this diagnostic as the template arguments of "
                      "RequireProvided");

        static constexpr bool value = true;
    };
} // namespace detail

/// A set of inputs, keyed by quantity type.
template <detail::EnvironmentEntry... Entries>
class Environment
{
  public:
    static_assert(detail::RequireDistinctQuantities<Entries...>::value);

    /// The only constructor. With no entries it is also the default one, which
    /// is why there is no separate `= default` declaration: for `Environment<>`
    /// the two would be the same signature declared twice.
    constexpr explicit Environment(Entries... entries) noexcept: _entries { entries... } {}

    /// Does this environment hold a value for @p Q?
    template <Described Q>
    static constexpr bool provides =
        (std::is_same_v<Q, typename detail::EntryTraits<Entries>::quantity> || ...);

    /// Was the value for @p Q typed in by a person rather than measured?
    ///
    /// Written as one fold rather than as a call to a private helper: the
    /// initialiser of a static data member template is **not** a complete-class
    /// context, so a helper declared further down the class would not be
    /// visible here. Member function bodies are, which is why `index_of` may
    /// stay private and below.
    template <Described Q>
    static constexpr bool is_entered =
        (... || (std::is_same_v<Q, typename detail::EntryTraits<Entries>::quantity>
                 && detail::EntryTraits<Entries>::isEntered));

    /// The value held for @p Q. Asking for a quantity this environment does not
    /// hold is a compile error naming it -- never a zero, and never a silent
    /// default.
    template <Described Q>
    [[nodiscard]] constexpr Measured<Q> get() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        if constexpr (provides<Q>)
        {
            constexpr std::size_t index = index_of<Q>();
            using Entry = std::tuple_element_t<index, std::tuple<Entries...>>;
            return detail::EntryTraits<Entry>::measurement(std::get<index>(_entries));
        }
        else
            return Measured<Q>::absent();
    }

    /// Where the value for @p Q came from.
    template <Described Q>
    [[nodiscard]] constexpr ValueSource source_of() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        if constexpr (provides<Q>)
        {
            constexpr std::size_t index = index_of<Q>();
            return detail::EntryTraits<std::tuple_element_t<index, std::tuple<Entries...>>>::source;
        }
        else
            return ValueSource::Derived;
    }

  private:
    template <Described Q>
    [[nodiscard]] static constexpr std::size_t index_of() noexcept
    {
        std::size_t found = sizeof...(Entries);
        std::size_t position = 0;
        (((std::is_same_v<Q, typename detail::EntryTraits<Entries>::quantity> ? (found = position) : found),
          ++position),
         ...);
        return found;
    }

    std::tuple<Entries...> _entries {};
};

/// Builds an environment: `environment(Measured<A> { … }, entered(Measured<B> { … }))`.
template <detail::EnvironmentEntry... Entries>
[[nodiscard]] constexpr Environment<Entries...> environment(Entries... entries) noexcept
{
    return Environment<Entries...> { entries... };
}

} // namespace formula
```

- [ ] **Step 4: Run the tests to verify they pass**

Expected: every `[environment]` test passes and both negative tests fail to
compile with their expected phrases.

- [ ] **Step 5: Mutation-test the suite**

1. In `EntryTraits<Entered<Q>>`, set `source` to `ValueSource::Measured`.
   Expected: *"an entered value is sourced as manually entered"* fails.
2. In `index_of`, return `0` unconditionally.
   Expected: *"entries keep their identity whatever order they are given in"*
   fails.
3. Change `RequireDistinctQuantities`'s condition to `true`.
   Expected: `environment_duplicate_quantity` stops failing to compile.
4. In `EntryTraits<Entered<Q>>::measurement`, return `Measured<Q>::absent()`.
   Expected: *"an entered value is sourced as manually entered"* fails on the
   value check.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/environment.hpp test/environment_tests.cpp \
        test/negative/environment_duplicate_quantity.cpp \
        test/negative/environment_missing_quantity.cpp test/CMakeLists.txt
git commit -m "feat(environment): inputs keyed by quantity, with entered values distinguished"
```
---

## Task 4: Evaluation

**Files:**
- Create: `include/formula-cpp/evaluate.hpp`
- Test: `test/evaluate_tests.cpp`
- Create: `test/negative/evaluate_result_dimension_mismatch.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: every node type from `expression.hpp`; `Environment` and
  `Entered` from `environment.hpp`; `Outcome`, `Value`, `ValueSource` from
  `outcome.hpp`; `checked_convert` and `Unit` from `unit.hpp`; the `checked_*`
  arithmetic from `rational.hpp`.
- Produces:
  ```cpp
  constexpr Unit coherent(Dimension) noexcept;

  template <typename Rep> struct RepTraits;            // specialised for Rational and double
  template <typename Rep> using Evaluated = std::expected<std::optional<Rep>, ArithmeticError>;

  template <typename Rep = Rational, Node Expression, typename Env>
  constexpr Evaluated<Rep> checked_evaluate_si(Expression, Env const&) noexcept;

  template <Described Result, Node Expression, typename Env>
  constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(Expression, Env const&) noexcept;

  template <Described Result, Node Expression, typename Env>
  constexpr Outcome<Result> evaluate(Expression, Env const&);
  ```

**How a value travels.** An input is stored in its quantity's own declared unit
— 180 in litres. Arithmetic cannot happen in mixed units, so every leaf is
converted to the **coherent SI unit of its dimension** on the way in (180 l →
9/50 m³), the whole tree is evaluated there, and the result is converted once,
at the end, into the declared unit of the quantity it was asked to produce. The
conversion is the exact multiply-then-divide from phase 3, so nothing is lost
in either direction.

**Ruling: `Result` is an explicit template argument, never deduced.** The same
rule `combine<Result>` already follows. An expression's dimension does not name
a quantity — mass over volume is a density, but *which* density is the author's
decision, and a library that picks one silently will sooner or later label a
number with the wrong symbol and description. Cost if wrong: a call site says
what it wants, which it should anyway.

**Ruling: `Rep` is offered on the representation-agnostic entry point only.**
`checked_evaluate_si<Rep>` is where a caller chooses exact rationals or
`double`. `checked_evaluate<Result>` is exact, always, because it is the entry
point that produces an auditable `Outcome`, and an audit trail carrying a
binary-floating-point result would have to explain itself. Cost if wrong: an
`Outcome` parameterised on its representation is an additive change to
`Measured` and `Outcome` later, not a rewrite of this evaluator.

**Ruling: an `Entered` result short-circuits the whole formula.** When the
environment holds an `entered(...)` value for the result quantity, evaluation
returns it with `ValueSource::ManuallyEntered` and does not evaluate the
expression at all — not even to compare. That is what a lab override is, and
§16.1 requires that such a value never be presented as derived. Cost if wrong:
phase 7's tracing may want the computed value alongside the override, which is
a tracing concern and can be added there.

- [ ] **Step 1: Write the failing test**

Create `test/evaluate_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/evaluate.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct TotalVolume: formula::Quantity<TotalVolume, "V", "total volume", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "specimen mass", formula::unit::Gram>
{
};
struct MassInKilogram: formula::Quantity<MassInKilogram, "m", "mass", formula::unit::Kilogram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
constexpr auto total = var<WaterVolume> + var<CementVolume>;

constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(300) });

} // namespace

TEST_CASE("evaluate: a ratio of like quantities is exact and dimensionless", "[evaluate]")
{
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
}

TEST_CASE("evaluate: a sum comes back in the result quantity's own unit", "[evaluate]")
{
    // 180 l + 300 l is 480 l. Evaluated in cubic metres and converted back, it
    // must be exactly 480 -- not 0,48 and not 479,999999.
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(total, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(480));
}

TEST_CASE("evaluate: the representation-agnostic core agrees with the exact one", "[evaluate]")
{
    constexpr formula::Evaluated<formula::Rational> exact =
        formula::checked_evaluate_si<formula::Rational>(ratio, inputs);
    constexpr formula::Evaluated<double> approximate = formula::checked_evaluate_si<double>(ratio, inputs);

    STATIC_REQUIRE(exact.has_value() && exact->has_value());
    STATIC_REQUIRE(**exact == rat(3, 5));
    STATIC_REQUIRE(approximate.has_value() && approximate->has_value());
    CHECK(**approximate == 0.6);
}

TEST_CASE("evaluate: an absent input makes the whole result empty, not zero", "[evaluate]")
{
    constexpr auto partial = formula::environment(formula::Measured<WaterVolume>::absent(),
                                                  formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
    STATIC_REQUIRE_FALSE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().is_absent());
}

TEST_CASE("evaluate: division by zero is an error, never a number", "[evaluate]")
{
    constexpr auto zeroed = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                 formula::Measured<CementVolume> { rat(0) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, zeroed);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: the throwing spelling throws what the checked one reports", "[evaluate]")
{
    auto const zeroed = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                             formula::Measured<CementVolume> { rat(0) });

    CHECK_THROWS_AS(formula::evaluate<Ratio>(ratio, zeroed), formula::ArithmeticException);
    CHECK(formula::evaluate<Ratio>(ratio, inputs).measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: an entered result replaces the formula and says so", "[evaluate]")
{
    constexpr auto overridden = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                     formula::Measured<CementVolume> { rat(300) },
                                                     formula::entered(formula::Measured<Ratio> { rat(45, 100) }));
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, overridden);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->is_overridden());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::ManuallyEntered);
    // The formula would have produced 3/5; the entered value wins outright.
    STATIC_REQUIRE(computed->measurement().value() == rat(45, 100));
}

TEST_CASE("evaluate: an entered result short-circuits an otherwise failing formula", "[evaluate]")
{
    // The formula divides by zero. The override must be returned anyway -- proof
    // that the expression is not evaluated at all, rather than evaluated and
    // discarded.
    constexpr auto overridden = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                     formula::Measured<CementVolume> { rat(0) },
                                                     formula::entered(formula::Measured<Ratio> { rat(45, 100) }));
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, overridden);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(45, 100));
}

TEST_CASE("evaluate: an entered input is still just an input", "[evaluate]")
{
    // `entered` on an *input* changes where the number came from, not how the
    // formula is evaluated: the result is still derived.
    constexpr auto mixed = formula::environment(formula::entered(formula::Measured<WaterVolume> { rat(180) }),
                                                formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, mixed);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: a constant enters the arithmetic in its own unit", "[evaluate]")
{
    // half a cubic metre, added to 180 l + 300 l, is 980 l.
    constexpr auto withConstant = total + formula::constant<formula::unit::CubicMetre>(rat(1, 2));
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(withConstant, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(980));
}

TEST_CASE("evaluate: negation negates", "[evaluate]")
{
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(-total, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(-480));
}

TEST_CASE("evaluate: a value in a different unit converts exactly", "[evaluate]")
{
    // The input is in grams; the formula is dimensionally a mass; and the
    // result quantity declares kilograms.
    constexpr auto grams = formula::environment(formula::Measured<SpecimenMass> { rat(2500) });
    constexpr auto computed = formula::checked_evaluate<MassInKilogram>(var<SpecimenMass>, grams);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(5, 2));
}

TEST_CASE("evaluate: an overflowing computation is reported, not wrapped", "[evaluate]")
{
    // Anything above the square root of the representable range cannot be
    // squared, and sqrt(int64 max) is about 3,04e9. 4e9 squared is 1,6e19,
    // comfortably over, so the test does not sit on the boundary.
    constexpr std::int64_t huge = 4'000'000'000LL;
    auto const big = formula::environment(formula::Measured<WaterVolume> { rat(huge) },
                                          formula::Measured<CementVolume> { rat(1, huge) });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, big);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("evaluate: a result at the edge of the range is computed, not refused", "[evaluate]")
{
    // The companion to the test above: together they say where the edge is.
    // 3e9 litres over 1/3e9 litres is exactly 9e18, which fits in a 64-bit
    // integer with room to spare -- and it fits only because `checked_mul`
    // cross-reduces before multiplying. A naive implementation would overflow
    // on the way to a perfectly representable answer.
    constexpr std::int64_t large = 3'000'000'000LL;
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { rat(large) },
                                             formula::Measured<CementVolume> { rat(1, large) });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, inputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->is_value());
    CHECK(computed->measurement().value() == rat(9'000'000'000'000'000'000LL));
}
```

Create `test/negative/evaluate_result_dimension_mismatch.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: does not measure the dimension this expression computes
#include <formula-cpp/evaluate.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

int main()
{
    // The expression is dimensionless; `Length` is not.
    constexpr auto inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                 formula::Measured<CementVolume> { formula::Rational { 300 } });
    auto const broken =
        formula::checked_evaluate<Length>(formula::var<WaterVolume> / formula::var<CementVolume>, inputs);
    return broken.has_value() ? 0 : 1;
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: `Cannot open include file: 'formula-cpp/evaluate.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/evaluate.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula and a set of inputs into an outcome.
///
/// Every leaf is converted to the **coherent SI unit** of its dimension on the
/// way in, the tree is evaluated there, and the result is converted once at the
/// end into the declared unit of the quantity it was asked to produce. Both
/// conversions are the exact multiply-then-divide of the unit layer, so 180 l
/// plus 300 l is exactly 480 l and not 479,999999.
///
/// Two entry points, deliberately different:
///
///  - `checked_evaluate_si<Rep>` is the representation-agnostic core. It answers
///    in the coherent SI unit and in whatever `Rep` the caller asked for --
///    exact `Rational` by default, `double` when a formula needs values exact
///    rationals cannot hold.
///  - `checked_evaluate<Result>` is the auditable one. It is always exact,
///    because a result that goes into an audit trail as binary floating point
///    would have to explain itself, and it returns an `Outcome<Result>` in
///    `Result`'s own unit.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/unit.hpp>

#include <expected>
#include <optional>

namespace formula
{

/// The coherent SI unit of a dimension: magnitude one, offset zero, no symbol.
///
/// Every `Unit` already states its own exact conversion to this one, so it is
/// the single scale on which values from different units can meet.
[[nodiscard]] constexpr Unit coherent(Dimension value) noexcept
{
    return Unit { .dimension = value };
}

/// How arithmetic is done for one representation.
///
/// The primary template is deliberately undefined: a representation that has
/// not been taught to the library fails at the point of use, naming itself,
/// rather than silently selecting something plausible.
template <typename Rep>
struct RepTraits;

/// Exact rational arithmetic -- the default, and the only one an `Outcome` is
/// built from.
template <>
struct RepTraits<Rational>
{
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from(Rational value) noexcept
    {
        return value;
    }
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> add(Rational lhs,
                                                                                Rational rhs) noexcept
    {
        return checked_add(lhs, rhs);
    }
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> subtract(Rational lhs,
                                                                                     Rational rhs) noexcept
    {
        return checked_sub(lhs, rhs);
    }
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> multiply(Rational lhs,
                                                                                     Rational rhs) noexcept
    {
        return checked_mul(lhs, rhs);
    }
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> divide(Rational lhs,
                                                                                   Rational rhs) noexcept
    {
        return checked_div(lhs, rhs);
    }
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> negate(Rational value) noexcept
    {
        return checked_negate(value);
    }
};

/// Binary floating point, for formulas whose values exact rationals cannot
/// hold. Overflow is not reported here: a `double` says `inf` and the caller
/// asked for `double`.
template <>
struct RepTraits<double>
{
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> from(Rational value) noexcept
    {
        return value.to_double();
    }
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> add(double lhs, double rhs) noexcept
    {
        return lhs + rhs;
    }
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> subtract(double lhs,
                                                                                   double rhs) noexcept
    {
        return lhs - rhs;
    }
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> multiply(double lhs,
                                                                                   double rhs) noexcept
    {
        return lhs * rhs;
    }
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> divide(double lhs,
                                                                                 double rhs) noexcept
    {
        if (rhs == 0.0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        return lhs / rhs;
    }
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> negate(double value) noexcept
    {
        return -value;
    }
};

/// The result of evaluating a subtree: a number, nothing (an input was never
/// measured), or an arithmetic error. The two layers are different questions --
/// "there is no value" is a fact about the specimen, "the arithmetic failed" is
/// a fact about the library -- and collapsing them would lose one of them.
template <typename Rep>
using Evaluated = std::expected<std::optional<Rep>, ArithmeticError>;

namespace detail
{
    /// Fails to compile when the result quantity does not measure what the
    /// expression computes.
    template <typename Result, typename Expression>
    struct RequireResultDimension
    {
        static_assert(Describe<Result>::dimension == Expression::dimension,
                      "formula: this result quantity does not measure the dimension this expression "
                      "computes; the quantity and the expression appear in this diagnostic as the "
                      "template arguments of RequireResultDimension");

        static constexpr bool value = true;
    };

    /// Wraps a bare `Rep` as a present value.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> present(Rep value) noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> { value } };
    }

    /// The absent result, which every operator propagates.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> nothing() noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> {} };
    }

    /// A `Rational` stated in @p from, converted to the coherent SI unit and
    /// then into @p Rep.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> in_si(Rational value, Unit from) noexcept
    {
        std::expected<Rational, ArithmeticError> const converted =
            checked_convert(value, from, coherent(from.dimension));
        if (!converted.has_value())
            return std::unexpected { converted.error() };

        std::expected<Rep, ArithmeticError> const represented = RepTraits<Rep>::from(*converted);
        if (!represented.has_value())
            return std::unexpected { represented.error() };
        return present<Rep>(*represented);
    }
} // namespace detail

template <typename Rep = Rational, Described Q, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(VarNode<Q> const&, Env const& environment) noexcept
{
    Measured<Q> const measured = environment.template get<Q>();
    if (measured.is_absent())
        return detail::nothing<Rep>();
    return detail::in_si<Rep>(*measured.stored(), Describe<Q>::unit);
}

template <typename Rep = Rational, Unit U, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ConstantNode<U> const& node, Env const&) noexcept
{
    return detail::in_si<Rep>(node.number, U);
}

template <typename Rep = Rational, UnaryOperator Op, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UnaryNode<Op, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    std::expected<Rep, ArithmeticError> const negated = RepTraits<Rep>::negate(**operand);
    if (!negated.has_value())
        return std::unexpected { negated.error() };
    return detail::present<Rep>(*negated);
}

template <typename Rep = Rational, BinaryOperator Op, Node Left, Node Right, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(BinaryNode<Op, Left, Right> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const lhs = checked_evaluate_si<Rep>(node.lhs, environment);
    if (!lhs.has_value())
        return std::unexpected { lhs.error() };
    Evaluated<Rep> const rhs = checked_evaluate_si<Rep>(node.rhs, environment);
    if (!rhs.has_value())
        return std::unexpected { rhs.error() };

    // Absence wins over arithmetic, but only after both sides have been asked:
    // an arithmetic error in the side that *is* present is still an error, and
    // hiding it behind the other side's absence would lose it.
    if (!lhs->has_value() || !rhs->has_value())
        return detail::nothing<Rep>();

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
    if (!combined.has_value())
        return std::unexpected { combined.error() };
    return detail::present<Rep>(*combined);
}

/// Evaluates @p expression for quantity @p Result.
///
/// `Result` is never deduced. An expression's dimension does not name a
/// quantity -- a mass over a volume is *a* density, but which one is the
/// author's decision -- and a library that picked one would eventually label a
/// number with someone else's symbol and description.
///
/// When the environment holds an `entered` value for `Result`, that value is
/// returned with `ValueSource::ManuallyEntered` and the expression is not
/// evaluated at all. That is what an override is; a number a person typed in
/// must never be reported as though the library derived it.
template <Described Result, Node Expression, typename Env>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(
    Expression const& expression, Env const& environment) noexcept
{
    static_assert(detail::RequireResultDimension<Result, Expression>::value);

    if constexpr (Env::template is_entered<Result>)
    {
        return Outcome<Result>::value(environment.template get<Result>(), ValueSource::ManuallyEntered);
    }
    else
    {
        Evaluated<Rational> const computed = checked_evaluate_si<Rational>(expression, environment);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        if (!computed->has_value())
            return Outcome<Result>::empty();

        std::expected<Rational, ArithmeticError> const inDeclaredUnit =
            checked_convert(**computed, coherent(Expression::dimension), Describe<Result>::unit);
        if (!inDeclaredUnit.has_value())
            return std::unexpected { inDeclaredUnit.error() };

        return Outcome<Result>::value(Measured<Result> { *inDeclaredUnit }, ValueSource::Derived);
    }
}

/// Throwing spelling of `checked_evaluate`, for callers who would only rethrow.
template <Described Result, Node Expression, typename Env>
[[nodiscard]] constexpr Outcome<Result> evaluate(Expression const& expression, Env const& environment)
{
    return detail::or_throw(checked_evaluate<Result>(expression, environment));
}

} // namespace formula
```

- [ ] **Step 4: Run the tests to verify they pass**

Expected: every `[evaluate]` test passes and the negative test fails to compile
with its expected phrase.

- [ ] **Step 5: Mutation-test the suite**

1. In `checked_evaluate`, convert from `coherent(...)` to `coherent(...)`
   instead of to `Describe<Result>::unit`.
   Expected: *"a sum comes back in the result quantity's own unit"* fails,
   reporting 12/25 instead of 480.
2. Delete the `is_entered<Result>` branch so the formula is always evaluated.
   Expected: *"an entered result replaces the formula and says so"* and
   *"an entered result short-circuits an otherwise failing formula"* both fail.
3. Change `is_entered<Result>` to `provides<Result>`.
   Expected: no existing test fails on a *measured* result entry — **add one**:
   an environment holding a plain `Measured<Ratio>` must NOT override, and the
   formula's own answer must come back. Add it, confirm the mutation breaks it,
   restore.
4. In the binary evaluator, move the absence check above the `rhs` evaluation.
   Expected: *"division by zero is an error, never a number"* still passes, so
   **add** a test where the left side is absent and the right divides by zero:
   the error must still win over the absence. Record what the original code does
   and make the test assert that.
5. Set `ValueSource::Derived` where the entered branch writes `ManuallyEntered`.
   Expected: *"an entered result replaces the formula and says so"* fails.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/evaluate.hpp test/evaluate_tests.cpp \
        test/negative/evaluate_result_dimension_mismatch.cpp test/CMakeLists.txt
git commit -m "feat(evaluate): formulas evaluate in SI and answer with an outcome"
```

---

## What was measured before tasks 4 and 5 were written

The headers in tasks 1 to 5 were extracted from this plan and compiled, exactly
as written, on **cl 19.51** (`/std:c++latest /W4 /WX /EHsc /permissive- /utf-8`),
**clang-cl 22** (`/std:c++latest /W4 /WX /EHsc`) and **g++ 13.3**
(`-std=c++23 -Wall -Wextra -Werror -pedantic`). Every assertion below ran as a
`static_assert`, not as a runtime check:

| Claim | Result on all three |
|---|---|
| `180 l / 300 l` for a dimensionless quantity | exactly `3/5` |
| `180 l + 300 l` reported in litres | exactly `480`, not `12/25` |
| `2500 g` reported in kilograms | exactly `5/2` |
| `480 l + 0,5 m³` | exactly `980 l` |
| `sqrt(2 m²)` in the exact representation | refused, not approximated |
| `sqrt(2 m²)` through `checked_evaluate_si<double>` | 1,414214 |
| `pi * d² / 4` at d = 100 mm | 0,007854 m² |
| an absent input | empty outcome, not zero |
| a zero denominator | `DivisionByZero`, no value |
| an `entered` result over a formula that divides by zero | the entered value, no error |
| the four new `static_assert`s | each names the user's own source line |

**One claim in task 4's tests was never run and was wrong.** The overflow test
originally used 3e9, whose result is exactly 9e18 -- about 2,5% *under* the
64-bit limit, so the computation succeeds and the test asserted the opposite of
what happens. It was a runtime `auto const` case rather than a `static_assert`,
so it is absent from the table above, which is exactly why it escaped. Measured
against the landed headers: 3e9 succeeds at 9000000000000000000/1, 3,1e9
overflows, 4e9 overflows. The test now uses 4e9 and has gained a companion that
pins the succeeding side of the boundary.

The three programs printed identical output. Two things the check found and this
plan already reflects: `decltype(node.lhs)` is **not** const-qualified even when
read through a `constexpr` object, and a composed tree is **not** an empty class
— see task 2.

---

## Task 5: Powers, roots and pi

**Files:**
- Modify: `include/formula-cpp/error.hpp` (add `ArithmeticError::Inexact`)
- Modify: `include/formula-cpp/rational.hpp` (add `Pi` and `checked_exact_nth_root`)
- Create: `include/formula-cpp/function.hpp`
- Test: `test/function_tests.cpp`
- Modify: `test/rational_tests.cpp`, `test/error_tests.cpp`
- Create: `test/negative/function_zero_root_degree.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `power`, `nth_root` from `dimension.hpp`; `checked_pow` from
  `rational.hpp`; `Node`, `NodeBase` from `expression.hpp`; `RepTraits`,
  `Evaluated` from `evaluate.hpp`.
- Produces:
  ```cpp
  // error.hpp
  enum class ArithmeticError { …, Inexact };

  // rational.hpp
  inline constexpr Rational Pi { 245'850'922, 78'256'779 };
  constexpr std::expected<Rational, ArithmeticError> checked_exact_nth_root(Rational, int degree) noexcept;

  // function.hpp
  template <int Exponent, Node Operand> struct PowerNode;    // .operand, ::exponent
  template <int Degree, Node Operand> struct RootNode;       // .operand, ::degree
  struct PiNode;
  template <int Exponent, Node Operand> constexpr auto pow(Operand) noexcept;
  template <Node Operand> constexpr auto sqrt(Operand) noexcept;   // RootNode<2>
  template <Node Operand> constexpr auto cbrt(Operand) noexcept;   // RootNode<3>
  inline constexpr PiNode pi {};
  ```

**Ruling: an inexact root is an error, not a silent approximation.** Exact
rational arithmetic on 64-bit integers cannot represent the square root of two,
and a layer whose entire promise is "never a wrong number" must not quietly
return one. `checked_exact_nth_root` answers exactly when an exact root exists
(4 → 2, 9/4 → 3/2) and returns `ArithmeticError::Inexact` otherwise. A formula
that genuinely needs an irrational root evaluates through
`checked_evaluate_si<double>` today, and a *rounded* root — a root computed to a
declared number of decimals — belongs with phase 8, where rounding is already a
first-class tree node with a trace entry of its own. Cost if wrong: phase 8 adds
an overload; nothing written here changes.

**Ruling: π is an exact rational that is not π.** `Rational::Pi` is
245 850 922 / 78 256 779, a convergent of π's continued fraction that differs
from π by less than 6·10⁻¹⁷ and fits comfortably in 64 bits. That is a
deliberate, documented approximation — the header says so in as many words —
and it is the only one in the exact layer. Cost if wrong: a caller needing more
digits than a double can hold must use a representation that has them, which no
`Rational` over `std::int64_t` does.

- [ ] **Step 1: Write the failing tests**

Add to `test/error_tests.cpp` a case asserting `describe(ArithmeticError::Inexact)`
returns a sentence naming inexactness, and extend whatever existing test
enumerates the error codes.

Add to `test/rational_tests.cpp`:

```cpp
TEST_CASE("rational: an exact root comes back exactly", "[rational]")
{
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 4 }, 2).value()
                   == formula::Rational { 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 9, 4 }, 2).value()
                   == formula::Rational { 3, 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 27, 8 }, 3).value()
                   == formula::Rational { 3, 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { -27 }, 3).value()
                   == formula::Rational { -3 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 1 }, 5).value()
                   == formula::Rational { 1 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational {}, 2).value() == formula::Rational {});
}

TEST_CASE("rational: an inexact root is refused rather than approximated", "[rational]")
{
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 2 }, 2).error()
                   == formula::ArithmeticError::Inexact);
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 10 }, 3).error()
                   == formula::ArithmeticError::Inexact);
}

TEST_CASE("rational: a root outside the domain is refused", "[rational]")
{
    // An even root of a negative number is not a real number.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { -4 }, 2).error()
                   == formula::ArithmeticError::DomainError);
    // Degree zero describes no root at all.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 4 }, 0).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("rational: Pi is a stated approximation, close enough to be useful", "[rational]")
{
    // Deliberately asserted as a bound rather than an equality: the point is
    // that the documented error bound holds, not that the fraction is memorised
    // in two places.
    constexpr formula::Rational squared = formula::Pi * formula::Pi;
    CHECK(squared.to_double() > 9.8696044010893);
    CHECK(squared.to_double() < 9.8696044010897);
    STATIC_REQUIRE(formula::Pi.denominator() > 1);
}
```

Create `test/function_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/function.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", formula::unit::SquareMetre>
{
};
struct Volume: formula::Quantity<Volume, "V", "volume", formula::unit::CubicMetre>
{
};
struct Edge: formula::Quantity<Edge, "a", "cube edge", formula::unit::Metre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

} // namespace

TEST_CASE("function: a power multiplies the dimension's exponents", "[function]")
{
    constexpr auto squared = formula::pow<2>(var<Diameter>);
    constexpr auto cubed = formula::pow<3>(var<Diameter>);
    constexpr auto reciprocal = formula::pow<-1>(var<Diameter>);

    STATIC_REQUIRE(decltype(squared)::dimension == formula::dim::Area);
    STATIC_REQUIRE(decltype(cubed)::dimension == formula::dim::Volume);
    STATIC_REQUIRE(decltype(reciprocal)::dimension == formula::dim::Scalar / formula::dim::Length);
    STATIC_REQUIRE(decltype(squared)::exponent == 2);
}

TEST_CASE("function: a root divides the dimension's exponents", "[function]")
{
    constexpr auto side = formula::sqrt(var<Area>);
    constexpr auto edge = formula::cbrt(var<Volume>);

    STATIC_REQUIRE(decltype(side)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(edge)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(side)::degree == 2);
    STATIC_REQUIRE(decltype(edge)::degree == 3);
}

TEST_CASE("function: a root of an odd dimension gives a fractional exponent", "[function]")
{
    // The square root of a length is dimensionally half a length. Norms do ask
    // for this, so it must be expressible rather than rejected.
    constexpr auto odd = formula::sqrt(var<Edge>);

    STATIC_REQUIRE(odd.dimension.length == formula::exponent(1, 2));
}

TEST_CASE("function: pi is dimensionless", "[function]")
{
    STATIC_REQUIRE(formula::is_dimensionless(formula::PiNode::dimension));
    STATIC_REQUIRE(formula::Node<formula::PiNode>);
}

TEST_CASE("function: a power evaluates exactly", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Diameter> { rat(150) });
    constexpr auto computed = formula::checked_evaluate<Area>(formula::pow<2>(var<Diameter>), inputs);

    // 150 mm is 3/20 m, squared is 9/400 m2.
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(9, 400));
}

TEST_CASE("function: an exact root evaluates exactly", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Area> { rat(9, 400) });
    constexpr auto computed = formula::checked_evaluate<Edge>(formula::sqrt(var<Area>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 20));
}

TEST_CASE("function: an inexact root is refused by the exact representation", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Area> { rat(2) });
    constexpr auto computed = formula::checked_evaluate<Edge>(formula::sqrt(var<Area>), inputs);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Inexact);
}

TEST_CASE("function: the double representation answers where the exact one cannot", "[function]")
{
    auto const inputs = formula::environment(formula::Measured<Area> { rat(2) });
    auto const computed = formula::checked_evaluate_si<double>(formula::sqrt(var<Area>), inputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->has_value());
    CHECK(**computed > 1.41421356);
    CHECK(**computed < 1.41421357);
}

TEST_CASE("function: pi evaluates to the documented approximation", "[function]")
{
    constexpr auto inputs = formula::environment();
    constexpr auto computed = formula::checked_evaluate_si<formula::Rational>(formula::pi, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(**computed == formula::Pi);
}

TEST_CASE("function: a circular area reads like the formula it is", "[function]")
{
    // A = pi * d^2 / 4, with an exact-rational pi.
    constexpr auto area = formula::pi * formula::pow<2>(var<Diameter>) / rat(4);
    constexpr auto inputs = formula::environment(formula::Measured<Diameter> { rat(100) });
    constexpr auto computed = formula::checked_evaluate<Area>(area, inputs);

    STATIC_REQUIRE(computed.has_value());
    CHECK(computed->measurement().value().to_double() > 0.00785398);
    CHECK(computed->measurement().value().to_double() < 0.00785399);
}

TEST_CASE("function: an absent input still propagates through a power", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto computed = formula::checked_evaluate<Area>(formula::pow<2>(var<Diameter>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}
```

Create `test/negative/function_zero_root_degree.cpp` with
`// EXPECT: a root of degree zero describes no operation`.

- [ ] **Step 2: Run to verify the tests fail**

- [ ] **Step 3: Write the code**

In `include/formula-cpp/error.hpp`, add to `ArithmeticError`:

```cpp
    /// The exact answer exists mathematically but is not a rational number, so
    /// this layer refuses rather than returning a nearby one. The square root of
    /// two is the canonical case.
    Inexact,
```
and to `describe()`:
```cpp
        case ArithmeticError::Inexact:
            return "no exact rational result exists";
```

In `include/formula-cpp/rational.hpp`, after the free arithmetic functions:

```cpp
/// A rational that is **not** pi.
///
/// 245 850 922 / 78 256 779 is a convergent of pi's continued fraction; it
/// differs from pi by less than 6e-17, which is finer than a `double` can
/// distinguish, and both halves fit comfortably in 64 bits. It is the one
/// deliberate approximation in the exact layer, and it is written here rather
/// than computed so that every caller gets the same number and the trace can
/// state which number it was.
inline constexpr Rational Pi { 245'850'922, 78'256'779 };

namespace detail
{
    /// The integer @p degree-th root of @p value, or nothing when it is not
    /// exact. Binary search rather than Newton: the search space is bounded by
    /// the value itself, every step stays inside `Int`, and there is no
    /// convergence question to get wrong.
    [[nodiscard]] constexpr std::optional<Rational::Int> exact_integer_root(Rational::Int value,
                                                                            int degree) noexcept
    {
        if (value < 0)
            return std::nullopt;
        if (value < 2)
            return value;

        Rational::Int low = 1;
        Rational::Int high = value;
        while (low <= high)
        {
            Rational::Int const middle = low + (high - low) / 2;

            // middle^degree, abandoning the moment it exceeds `value` so the
            // multiplication can never overflow.
            Rational::Int power = 1;
            bool tooBig = false;
            for (int step = 0; step < degree; ++step)
            {
                std::optional<Rational::Int> const next = mul_checked_or_none(power, middle);
                if (!next || *next > value)
                {
                    tooBig = true;
                    break;
                }
                power = *next;
            }

            if (tooBig)
                high = middle - 1;
            else if (power == value)
                return middle;
            else
                low = middle + 1;
        }
        return std::nullopt;
    }
} // namespace detail

/// The exact @p degree-th root of @p value.
///
/// Answers only when the answer is a rational number: the root of 4 is 2 and the
/// root of 9/4 is 3/2, but the root of 2 is `ArithmeticError::Inexact` rather
/// than a nearby fraction. A layer whose promise is "never a wrong number" has
/// no business rounding silently; a formula that needs an irrational root is
/// evaluated in a representation that has room for one.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_exact_nth_root(Rational value,
                                                                                        int degree) noexcept
{
    if (degree < 1)
        return std::unexpected { ArithmeticError::DomainError };
    if (value.sign() < 0 && degree % 2 == 0)
        return std::unexpected { ArithmeticError::DomainError };

    bool const negative = value.sign() < 0;
    Rational::Int const numerator = negative ? -value.numerator() : value.numerator();

    std::optional<Rational::Int> const rootedNumerator = detail::exact_integer_root(numerator, degree);
    std::optional<Rational::Int> const rootedDenominator =
        detail::exact_integer_root(value.denominator(), degree);
    if (!rootedNumerator || !rootedDenominator)
        return std::unexpected { ArithmeticError::Inexact };

    return Rational::make(negative ? -*rootedNumerator : *rootedNumerator, *rootedDenominator);
}
```

Create `include/formula-cpp/function.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Function nodes: integer powers, roots, and pi.
///
/// Powers and roots act on the dimension as well as the number -- the square of
/// a length is an area, the cube root of a volume is a length -- which is why
/// they are nodes rather than free functions a caller applies to an evaluated
/// number. A root may also produce a fractional exponent, which is exactly why
/// `Dimension` carries rational exponents.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>

#include <cmath>

namespace formula
{

/// @p Operand raised to an integer power. Negative exponents are allowed and
/// invert the dimension with the value.
template <int Exponent, Node Operand>
struct PowerNode: NodeBase
{
    Operand operand {};

    static constexpr int exponent = Exponent;
    static constexpr Dimension dimension = power(Operand::dimension, Exponent);
};

namespace detail
{
    /// Fails to compile for a root of degree zero, which names no operation.
    template <int Degree>
    struct RequirePositiveRootDegree
    {
        static_assert(Degree > 0,
                      "formula: a root of degree zero describes no operation; the degree appears in "
                      "this diagnostic as the template argument of RequirePositiveRootDegree");

        static constexpr bool value = true;
    };
} // namespace detail

/// The @p Degree-th root of @p Operand.
template <int Degree, Node Operand>
struct RootNode: NodeBase
{
    static_assert(detail::RequirePositiveRootDegree<Degree>::value);

    Operand operand {};

    static constexpr int degree = Degree;
    static constexpr Dimension dimension = nth_root(Operand::dimension, Degree);
};

/// Pi, as a node, so that a formula containing it stays a formula.
struct PiNode: NodeBase
{
    static constexpr Dimension dimension = dim::Scalar;
};

template <int Exponent, Node Operand>
[[nodiscard]] constexpr auto pow(Operand operand) noexcept
{
    return PowerNode<Exponent, Operand> { {}, operand };
}

template <Node Operand>
[[nodiscard]] constexpr auto sqrt(Operand operand) noexcept
{
    return RootNode<2, Operand> { {}, operand };
}

template <Node Operand>
[[nodiscard]] constexpr auto cbrt(Operand operand) noexcept
{
    return RootNode<3, Operand> { {}, operand };
}

template <int Degree, Node Operand>
[[nodiscard]] constexpr auto root(Operand operand) noexcept
{
    return RootNode<Degree, Operand> { {}, operand };
}

/// The spelling of pi in a formula.
inline constexpr PiNode pi {};

// ---------------------------------------------------------------- evaluation

/// Powers and roots per representation. Kept here rather than in `RepTraits`
/// itself so that `evaluate.hpp` stays free of `<cmath>`: a consumer who never
/// writes a root never compiles it.
template <typename Rep>
struct RepFunctions;

template <>
struct RepFunctions<Rational>
{
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> raise(Rational base,
                                                                                   int exponent) noexcept
    {
        return checked_pow(base, exponent);
    }

    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> root(Rational value,
                                                                                 int degree) noexcept
    {
        return checked_exact_nth_root(value, degree);
    }

    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> pi_value() noexcept
    {
        return Pi;
    }
};

template <>
struct RepFunctions<double>
{
    [[nodiscard]] static std::expected<double, ArithmeticError> raise(double base, int exponent) noexcept
    {
        return std::pow(base, static_cast<double>(exponent));
    }

    [[nodiscard]] static std::expected<double, ArithmeticError> root(double value, int degree) noexcept
    {
        if (value < 0.0 && degree % 2 == 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (value < 0.0)
            return -std::pow(-value, 1.0 / static_cast<double>(degree));
        return std::pow(value, 1.0 / static_cast<double>(degree));
    }

    [[nodiscard]] static std::expected<double, ArithmeticError> pi_value() noexcept
    {
        return 3.141592653589793238462643383279502884;
    }
};

template <typename Rep = Rational, int Exponent, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PowerNode<Exponent, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    std::expected<Rep, ArithmeticError> const raised = RepFunctions<Rep>::raise(**operand, Exponent);
    if (!raised.has_value())
        return std::unexpected { raised.error() };
    return detail::present<Rep>(*raised);
}

template <typename Rep = Rational, int Degree, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RootNode<Degree, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    std::expected<Rep, ArithmeticError> const rooted = RepFunctions<Rep>::root(**operand, Degree);
    if (!rooted.has_value())
        return std::unexpected { rooted.error() };
    return detail::present<Rep>(*rooted);
}

template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PiNode const&, Env const&) noexcept
{
    std::expected<Rep, ArithmeticError> const value = RepFunctions<Rep>::pi_value();
    if (!value.has_value())
        return std::unexpected { value.error() };
    return detail::present<Rep>(*value);
}

} // namespace formula
```

**Note for the implementer — overload lookup, already measured.** The
`checked_evaluate_si` overloads for `PowerNode`, `RootNode` and `PiNode` are
declared in `function.hpp`, *after* the binary and unary overloads in
`evaluate.hpp` that call them recursively. Name lookup inside a template is
two-phase, so this works only if ADL at the point of *instantiation* finds the
later overloads. It does: `pi * pow<2>(var<Diameter>) / rat(4)` — a binary node
over two function nodes — was compiled and evaluated as a constant expression on
cl 19.51, clang-cl 22 and g++ 13.3 before this plan was written, and all three
produced 0,007854 m² for a 100 mm diameter. Do not restructure the headers to
avoid a problem that is not there; if a compiler you try does disagree, fold
`function.hpp`'s evaluation overloads into `evaluate.hpp` and say so.

- [ ] **Step 4: Run the tests to verify they pass**

- [ ] **Step 5: Mutation-test the suite**

1. In `PowerNode`, set `dimension` to `Operand::dimension`.
   Expected: *"a power multiplies the dimension's exponents"* fails.
2. In `RootNode`, set `dimension` to `Operand::dimension`.
   Expected: *"a root divides the dimension's exponents"* fails.
3. In `exact_integer_root`, return `low` instead of `std::nullopt` at the end.
   Expected: *"an inexact root is refused rather than approximated"* fails.
4. In `checked_exact_nth_root`, drop the even-degree negative check.
   Expected: *"a root outside the domain is refused"* fails.
5. Change `Pi`'s numerator to `245'850'923`.
   Expected: *"Pi is a stated approximation"* fails its upper bound.
6. In `exact_integer_root`, remove the `*next > value` guard.
   Expected: on a large input the multiplication overflows. **Add a test**
   rooting a value near `IntMax` and confirm it returns `Inexact` rather than
   wrapping; confirm the mutation breaks it.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/error.hpp include/formula-cpp/rational.hpp \
        include/formula-cpp/function.hpp test/function_tests.cpp \
        test/rational_tests.cpp test/error_tests.cpp \
        test/negative/function_zero_root_degree.cpp test/CMakeLists.txt
git commit -m "feat(function): powers, roots and pi as nodes that carry their dimension"
```

---

## Task 6: Retire the prototype evaluator and document the layer

**Files:**
- Delete: `include/formula-cpp/evaluation.hpp`
- Delete: `include/formula-cpp/detail/type_list.hpp` (only if nothing else includes it)
- Delete: `test/negative/duplicate_producer.cpp`
- Modify: `include/formula-cpp/formula.hpp`
- Modify: `CMakeLists.txt` (the `FILE_SET HEADERS` list)
- Modify: `test/CMakeLists.txt`, `test/compile_time_tests.cpp`, `test/runtime_tests.cpp`
- Rewrite: `examples/simple.cpp`
- Create: `examples/expressions.cpp`
- Modify: `examples/CMakeLists.txt`
- Create: `docs/expressions.md`
- Modify: `mkdocs.yml`, `README.md`

**Why now and not earlier.** §17 says this phase retires `Evaluation`,
`EvaluationFunctors`, `Overloader` and the `decltype([]{})` idiom. They are the
godbolt prototype the project started from: a type-keyed lazy dependency graph
with a runtime `assert` for a missing input, no dimensions, no units, no
provenance and no outcome. Everything it did is now done by the expression layer
with a compile error where it had an assertion. Leaving it in place would ship
two evaluators with different semantics under one namespace.

- [ ] **Step 1: Find every reference**

```bash
grep -rn "Evaluation\|EvaluationFunctors\|EvaluationArguments\|Overloader\|type_list\|index_in_tuple" \
     include test examples docs README.md CMakeLists.txt
```
Record the list in the task report before deleting anything — the point is to
know what breaks, not to discover it during the build.

- [ ] **Step 2: Delete and re-point**

Remove `include/formula-cpp/evaluation.hpp` and its entry in the `FILE_SET
HEADERS` list in `CMakeLists.txt`. Remove `test/negative/duplicate_producer.cpp`
and its registration — it pins a diagnostic of the deleted evaluator. Delete
`detail/type_list.hpp` and its header-list entry **only if** step 1 found no
other user; if something else uses it, keep it and say so.

Update `include/formula-cpp/formula.hpp` so the umbrella reads:

```cpp
#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/version.hpp>
```
and add the five new headers to the `FILE_SET HEADERS` list in `CMakeLists.txt`,
keeping it alphabetical. The install-and-consume CI leg checks that list, so a
missing entry is caught there rather than by a consumer.

- [ ] **Step 3: Rewrite `examples/simple.cpp`**

```cpp
// SPDX-License-Identifier: Apache-2.0
/// The shortest complete formula this library can express: two measured inputs,
/// one formula, one traceable result.

#include <formula-cpp/formula.hpp>

#include <cstdio>

/// A quantity is a type. It carries its own symbol, its own description and the
/// unit its values are stated in, and it is distinct from every other quantity
/// even when the unit is the same.
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct WaterCementRatio:
    formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", formula::unit::One>
{
};

/// The formula is written once, with ordinary operators, and is a compile-time
/// entity: this line builds a type, not a computation.
inline constexpr auto waterCementRatio = formula::var<WaterVolume> / formula::var<CementVolume>;

int main()
{
    auto const batch = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                            formula::Measured<CementVolume> { formula::Rational { 300 } });

    formula::Outcome<WaterCementRatio> const result =
        formula::evaluate<WaterCementRatio>(waterCementRatio, batch);

    std::printf("%s = %f (%s)\n",
                formula::Describe<WaterCementRatio>::symbol.data(),
                result.measurement().value().to_double(),
                result.is_value() ? "computed" : "no value");
    return 0;
}
```

Expected output, which `formula_add_example` pins exactly:

```
w/c = 0.600000 (computed)
```

- [ ] **Step 4: Write `examples/expressions.cpp`**

A single program showing, in this order, each with a printed line: a formula
with a constant and a power; a formula whose result quantity is in a different
unit from its inputs; an absent input propagating to an empty result; a
dimensional error **in a comment** explaining what it would say if uncommented;
and a manually entered result replacing a computed one, printing both the value
and the fact that it was entered. Register it with:

```cmake
formula_add_example(expressions expressions.cpp "<the exact expected output>")
```

Do not add an `add_executable` by hand: `formula_add_example` is what links
`support/fail_without_dialogs.cpp`, and an example without it can raise a modal
dialog on Windows instead of failing.

- [ ] **Step 5: Write `docs/expressions.md`**

Cover, with runnable snippets taken from the tests so they cannot drift:

- writing a formula with operators, and what the type of a formula is
- where a dimensional error appears, with the **actual** diagnostic text from
  one compiler, copied from a real build rather than reconstructed
- the environment: inputs keyed by quantity, and what a missing one says
- absence propagating, and why an absent input is not zero
- the outcome: value, empty, verdict, invalid, and what `is_overridden` means
- choosing a representation, and why the auditable entry point is exact only
- powers, roots and pi, including the `Inexact` refusal and why it exists

Add the page to `mkdocs.yml`'s nav after the quantities page, and add one line
to the feature list in `README.md`.

- [ ] **Step 6: Build, test and run every example on all four presets**

```
cmake --preset cl-debug      && cmake --build --preset cl-debug      && ctest --preset cl-debug
cmake --preset cl-release    && cmake --build --preset cl-release    && ctest --preset cl-release
cmake --preset clangcl-debug && cmake --build --preset clangcl-debug && ctest --preset clangcl-debug
cmake --preset clangcl-release && cmake --build --preset clangcl-release && ctest --preset clangcl-release
```
Expected: green on all four, including the example-output tests and every
negative-compile test.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: retire the prototype evaluator in favour of the expression layer"
```

---

## Self-review

**Spec coverage.** §9's node list is covered except the documented wrapper
(§10, phase 6), conditionals (phase 8) and the series nodes (§12, phase 12) —
each named in the plan header as out of scope. §9's "dimension computed at class
scope", "type-keyed environment", "missing input is a `static_assert` naming the
variable", "empty propagation" and "the result is not a number" each have a task
and at least one test. §16.1's override requirement is task 3 plus task 4.
Tier A #1's "integer powers, square and cube roots, and π" is task 5, with the
inexact-root boundary ruled on explicitly. `Constraint` (§9, §16.2) is phase 9
and is deliberately absent.

**Placeholder scan.** No TBDs. The one place the plan says "the exact expected
output" rather than giving it — task 6, step 4 — is a string that depends on
code the implementer is writing in that step; the surrounding instruction says
precisely what the program must print.

**Type consistency.** `checked_evaluate_si` takes `(node, environment)` and
returns `Evaluated<Rep>` in all seven overloads. `Outcome<Q>::value` takes
`(Measured<Q>, ValueSource)` in task 1 and is called that way in task 4.
`Environment::is_entered<Q>` and `provides<Q>` are static data member templates
in task 3 and are used as `Env::template is_entered<Result>` in task 4.
`detail::nothing<Rep>()` and `detail::present<Rep>()` are defined in task 4 and
used in task 5.

**Known risk, flagged rather than hidden.** Task 5's evaluation overloads are
found by ADL at the point of instantiation from templates defined earlier in
task 4. The plan says to verify this on all four presets rather than assume it,
and gives the fallback if a compiler disagrees.
