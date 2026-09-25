# Methods and Jurisdiction Overlays Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make variant selection, rounding and jurisdiction overrides *declared* rather than executed, so that every decision shaping a result appears in the trace and an inspector can ask "why that formula, and why rounded that way?" and get an answer.

**Architecture:** A `Method` bundles a heterogeneous pack of variants (each an expression selected by a tag), a rounding rule, and a `ConstraintSet`. An `Overlay` is applied *to* a method and yields a method — never a parameter threaded through evaluation. The set of jurisdictions is closed and lives in the type; *which* one applies is a runtime index. A `ScopedVocabulary` resolves symbols through the overlay, because the same word names different quantities in different countries.

**Tech Stack:** C++23, header-only. Catch2 via CPM. `STATIC_REQUIRE` for compile-time behaviour, plus the `test/negative/` must-not-compile harness whose asserted message strings are tested API.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` §9.1, §16.6, §16.7.

**Design decisions and spike evidence:** `.superpowers/sdd/phase-11-prep/design-decisions.md` (rulings V1–V5 and C1–C4) and `spike-findings.md`. **Read the design decisions before starting any task** — four of this plan's rulings exist because something already written in this codebase contradicted the first design.

## Global Constraints

- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. Verify on all seven presets: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`, `clang-release`, `gcc-release`.
- **Also build the Doxygen target and run `mkdocs build --strict` on every task**, not only before a merge. Both were merge-time-only rules during phase 10 and the Doxygen leg was silently red for six tasks.
- **Never hardcode `/std:c++23`.** The standard comes from `set(CMAKE_CXX_STANDARD 23)` + `CMAKE_CXX_STANDARD_REQUIRED ON` + `target_compile_features(formula-cpp INTERFACE cxx_std_23)`. On this machine a literal `/std:c++23` silently drops `cl` to pre-C++17 and `clang-cl` to C++14.
- **No third-party standard text** — no real published standard's values, thresholds, clause numbers or citations, in code, tests, guide or gallery. Invented `Example Standard` citations only.
- **Never redirect a build to `/dev/null`** — a `STATIC_REQUIRE` failure is a *build* error, and hiding it lets the next `ctest` report a stale pass.
- Every negative test asserts **both** that the build fails **and** that the message contains the library's own `static_assert` text. Those strings are tested API.
- **`FORMULA_WERROR=ON`** on every configuration; zero warnings. GCC is the only leg running `-Wshadow -Wconversion -Wpedantic`, and it has caught defects no other compiler saw.
- Baseline at branch point: **512 tests** on seven configurations, `master` at `1108097`.

## Rules bought with real defects, which apply to every task here

- **A mutation survives whenever *any* axis of the test is degenerate, not only the axis that caught you last time.** Every phase-10 task shipped one.
- **A test that passes for the wrong reason is worth less than none.** For any negative test: delete the guard it exists to pin and confirm the test then **compiles**.
- **A new test should kill something nothing else kills.** Show the mutation fails your test *and nothing else*.
- **A claim about whether something compiles, made by inspection, is not verification.** Compile it.
- **A measurement against a different tool than the one that runs is not a measurement.** This project was wrong three times in one day: tectonic vs MathJax 3.2.2, local Doxygen vs CI's 1.9.8, CMake 4.3 vs CI's 3.28.
- **A comment nobody can break is not a decision, it is a wish.**
- **Catch2 splits test filters on commas** — a filter matching nothing still exits non-zero, indistinguishable from a kill. Prove selection counts on the unmutated build first.

---

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/method.hpp` *(new)* | `variant<Tag>`, `Variants<...>`, `Method<...>`, `method(...)`, tag selection, the no-match outcome |
| `include/formula-cpp/overlay.hpp` *(new)* | `Overlay`, `overlay(...)`, `apply(overlay, method)`, the jurisdiction pack and its runtime index |
| `include/formula-cpp/vocabulary.hpp` *(new)* | `ScopedVocabulary`, `symbol_of<Q>(vocabulary)`, the default vocabulary that changes nothing |
| `include/formula-cpp/trace.hpp` *(modify)* | new `StepKind` enumerators; sink entry points for method decisions |
| `include/formula-cpp/trace_render.hpp` *(modify)* | a rendered case per new `StepKind` |
| `include/formula-cpp/render.hpp` *(modify)* | render a `Method`; accept a vocabulary |
| `include/formula-cpp/document.hpp` *(modify)* | walk a `Method`; accept a vocabulary |
| `include/formula-cpp/sink.hpp` *(modify)* | correct the degradation promise (C2) |
| `docs/tracing.md` *(modify)* | correct the same promise (C2) |
| `test/method_tests.cpp`, `test/overlay_tests.cpp`, `test/vocabulary_tests.cpp` *(new)* | per-surface tests |
| `test/negative/*.cpp` *(new)* | one per author error a method or overlay can contain |
| `examples/methods_and_overlays.cpp` *(new)* | runnable, quoted verbatim by the guide |
| `docs/methods-and-overlays.md` *(new)* | the guide |
| `tools/gallery/main.cpp` *(modify)* | a method and an overlaid method on the gallery page |

`method.hpp` and `overlay.hpp` are split because a reviewer can reject one while approving the other: a method that selects correctly is useful without any overlay, and the spec's §9.1 sketch shows methods used with no jurisdiction at all.

---

## Task 1: `variant<Tag>` and the variants pack

**Files:**
- Create: `include/formula-cpp/method.hpp`
- Create: `test/method_tests.cpp`
- Create: `test/negative/method_variants_disagree.cpp`
- Modify: `include/formula-cpp/formula.hpp` (add the include)
- Modify: `test/CMakeLists.txt` (register the new test TU and negative case)

**Interfaces:**
- Produces: `template <typename Tag, Node Expr> struct VariantCase { using tag = Tag; Expr expression; };`
- Produces: `template <typename Tag, Node Expr> [[nodiscard]] constexpr VariantCase<Tag, Expr> variant(Expr expression) noexcept;`
- Produces: `template <typename... Cs> struct Variants { std::tuple<Cs...> cases {}; };`
- Produces: `template <typename... Cs> [[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept;`

**Why `variant<Tag>` and not the spec's `when<Tag>` — ruling V2, and the implementer must not "correct" it back:** `conditional.hpp:93` already ships `when(P predicate, Then thenBranch, Else elseBranch)`. The spike measured that `when<P>(pred, then, else)` **binds to the shipped ternary and compiles silently**, and that every mis-spelling gives the same uninformative "no matching function for call to 'when'" on all five compilers. Put that reason in `method.hpp`'s file comment so nobody re-opens it.

- [ ] **Step 1: Write the failing test**

In `test/method_tests.cpp`:

```cpp
#include <formula-cpp/formula.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube { };
struct Cylinder { };

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton> { };
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre> { };
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre> { };
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal> { };
} // namespace

TEST_CASE("a variants pack holds structurally different expressions", "[method]")
{
    constexpr auto pack = formula::variants(
        formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>)),
        formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>)));

    STATIC_REQUIRE(std::tuple_size_v<decltype(pack.cases)> == 2);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, decltype(pack.cases)>::tag, Cube>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, decltype(pack.cases)>::tag, Cylinder>);
}
```

- [ ] **Step 2: Run it and confirm it fails to build**

Run: `cmake --build --preset cl-debug` (never redirected to `/dev/null`).
Expected: FAIL — `variant` and `variants` are not declared.

- [ ] **Step 3: Write `method.hpp`'s variants half**

```cpp
/// One variant of a method: an expression that applies when the specimen,
/// apparatus or product matches @p Tag.
///
/// Spelled `variant<Tag>` rather than the spec's `when<Tag>` deliberately.
/// `conditional.hpp` already ships `when(predicate, then, else)`, a runtime
/// ternary; measured on cl, clang-cl, clang++, g++ and g++-14,
/// `when<Tag>(pred, then, else)` binds to that overload and compiles
/// SILENTLY, and every mis-spelling reports only "no matching function for
/// call to 'when'" without naming which one was meant. Two concepts, one
/// name, no diagnostic separating them.
template <typename Tag, Node Expr>
struct VariantCase
{
    using tag = Tag;
    Expr expression;
};

template <typename Tag, Node Expr>
[[nodiscard]] constexpr VariantCase<Tag, Expr> variant(Expr expression) noexcept
{
    return VariantCase<Tag, Expr> { expression };
}

template <typename... Cs>
struct Variants
{
    std::tuple<Cs...> cases {};
};

template <typename... Cs>
[[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept
{
    return Variants<Cs...> { std::tuple<Cs...> { cases... } };
}
```

- [ ] **Step 4: Run the test and confirm it passes**

Run: `ctest --preset cl-debug -R "variants pack"` — confirm it selects **1** test before believing any result.

- [ ] **Step 5: Add the quantity-agreement guard (ruling D2)**

Variants differ in *expression type* but must agree in **reported quantity** — dimension and declared unit. Name both offenders, in the `RequireAddendsAgree` style already in `expression.hpp`:

```cpp
template <typename First, typename Other>
struct RequireVariantsAgree
{
    static_assert(First::dimension == Other::dimension,
                  "formula: two variants of this method measure different dimensions; every "
                  "variant must report the same quantity, because a method reports one");
    static constexpr bool value = true;
};
```

- [ ] **Step 6: Write the negative test**

`test/negative/method_variants_disagree.cpp`, with the disagreeing variant in the **middle** of three — task 1 of phase 10 proved middle placement strictly stronger, because it defeats a first-only and a last-only mutation at once:

```cpp
// EXPECT: two variants of this method measure different dimensions
inline constexpr auto broken = formula::variants(
    formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>)),
    formula::variant<Cylinder>(var<EdgeX>),                          // wrong dimension, middle
    formula::variant<Prism>(var<Force> / (var<EdgeX> * var<EdgeY>)));
```

- [ ] **Step 7: Apply the deletion check to the negative test**

Delete `RequireVariantsAgree`'s `static_assert` and rebuild the negative case. It **must now compile**. If it still fails, it is pinning something else and is worthless — a negative test did exactly that in phase 10 and survived a full review.

- [ ] **Step 8: Verify on all seven presets, plus Doxygen and mkdocs**

- [ ] **Step 9: Commit**

```bash
git add include/formula-cpp/method.hpp test/method_tests.cpp test/negative/method_variants_disagree.cpp test/CMakeLists.txt include/formula-cpp/formula.hpp
git commit -m "feat(method): add variant<Tag> and the variants pack"
```

---

## Task 2: `Method`, tag selection, and the no-match outcome

**Files:**
- Modify: `include/formula-cpp/method.hpp`
- Modify: `test/method_tests.cpp`
- Create: `test/negative/method_no_matching_variant.cpp`

**Interfaces:**
- Consumes: `Variants<Cs...>`, `variant<Tag>`, `VariantCase<Tag, Expr>::tag`
- Produces: `template <typename Vs, typename Rounding, typename Constraints> struct Method { Vs variantSet; Rounding rounding; Constraints constraintSet; };`
- Produces: `[[nodiscard]] constexpr auto method(Vs variantSet, Rounding rounding, Constraints constraintSet) noexcept;`
- Produces: `template <typename Tag, typename Rep = Rational, typename M, typename Env, typename Sink = NullSink> [[nodiscard]] constexpr Evaluated<Rep> evaluate_method(M const& m, Env const& environment, Sink sink = {}) noexcept;`

**Bound by C3 — already decided, do not re-open:** `constraint.hpp:253-273` states that `Method` holds a `ConstraintSet` as an **ordinary member** and passes it straight to `check_all()` without unpacking, and says why. Honour it.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("a method selects the variant matching the tag", "[method]")
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>)),
                          formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>))),
        formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 },
                               formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());

    constexpr auto inputs = formula::environment(
        formula::Measured<Force> { formula::Rational { 90'000 } },
        formula::Measured<EdgeX> { formula::Rational { 150 } },
        formula::Measured<EdgeY> { formula::Rational { 100 } });

    // Cube: 90000 N / (150 mm * 100 mm) = 6 MPa
    constexpr auto cube = formula::evaluate_method<Cube>(m, inputs);
    STATIC_REQUIRE(cube.has_value());
    STATIC_REQUIRE(cube->has_value());
    STATIC_REQUIRE(cube->value() == formula::Rational { 6 });

    // Cylinder uses EdgeX twice: 90000 / (150*150) = 4 MPa -- a DIFFERENT
    // number, so this test cannot pass if selection picked the wrong variant.
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(m, inputs);
    STATIC_REQUIRE(cylinder->value() == formula::Rational { 4 });
}
```

**The two expected values differ deliberately.** A fixture where both variants produce the same number cannot distinguish "selected the right one" from "selected any one" — the degenerate-axis failure every phase-10 task shipped at least once.

- [ ] **Step 2: Run it and confirm it fails to build**

- [ ] **Step 3: Implement `Method`, `method()` and `rounding_rule`**

```cpp
/// A method's rounding rule, declared rather than applied after the fact, so
/// the trace can say which rule fired and where it came from (spec section
/// 9.1). Carries no operand: it is applied to whichever variant is selected.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
struct RoundingRule
{
    static constexpr Unit unit = U;
    static constexpr DecimalPlaces places = Places;
    static constexpr RoundingMode mode = Mode;
};

template <Unit U, DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr RoundingRule<U, Places, Mode> rounding_rule() noexcept
{
    return {};
}

template <typename Vs, typename Rounding, typename Constraints>
struct Method
{
    Vs variantSet {};
    Rounding rounding {};
    Constraints constraintSet {};
};

template <typename Vs, typename Rounding, typename Constraints>
[[nodiscard]] constexpr Method<Vs, Rounding, Constraints>
method(Vs variantSet, Rounding rounding, Constraints constraintSet) noexcept
{
    return Method<Vs, Rounding, Constraints> { variantSet, rounding, constraintSet };
}
```

- [ ] **Step 4: Implement tag selection with a named refusal**

```cpp
/// Refuses a tag no variant declares. There is no fallback variant and no
/// "first match wins": a specimen matching no variant has no result, the
/// same ruling phase 9 made for `bool satisfied()` and phase 10 made for a
/// lookup miss. An author who wants a catch-all writes one.
template <typename Tag, typename... Cs>
struct RequireVariantForTag
{
    static_assert((std::is_same_v<Tag, typename Cs::tag> || ...),
                  "formula: this method declares no variant for that tag; a method that matches "
                  "nothing has no result, so add a variant for it or select a tag it declares");
    static constexpr bool value = true;
};
```

- [ ] **Step 5: Run the test and confirm it passes; confirm the filter selected 1 test**

- [ ] **Step 6: Write the negative test, then apply the deletion check**

`test/negative/method_no_matching_variant.cpp` selects a `Prism` from a method declaring only `Cube` and `Cylinder`. Then delete `RequireVariantForTag`'s `static_assert` and confirm the case **compiles**.

- [ ] **Step 7: Verify on all seven presets, plus Doxygen and mkdocs**

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/method.hpp test/method_tests.cpp test/negative/method_no_matching_variant.cpp test/CMakeLists.txt
git commit -m "feat(method): select a variant by tag, and refuse a tag no variant declares"
```

---

## Task 3: Duplicate-tag refusal, and the selection boundary the spike drew

**Files:**
- Modify: `include/formula-cpp/method.hpp`
- Modify: `test/method_tests.cpp`
- Create: `test/negative/method_duplicate_tag.cpp`

**Bound by ruling V5 — the spike established a sharp boundary and the guide must state it:** tag **overlap** is detectable unconditionally; tag **completeness** only against a closed case set the author writes down; predicate overlap is **not a property of the type**, because `ConstantNode::number` is a runtime member. **So this task implements the tag-overlap refusal only.** Do not attempt a general predicate-overlap `static_assert`; the spike proved two guards over two *variables* have nothing in the type to compare.

**Bound by C4 — reuse, do not rebuild:** `band.hpp:169-220` already performs compile-time gap and overlap detection for *numeric intervals*. If a later task wants numeric-range variant selection, it calls that. This task is about **tags**, which are types, and needs its own check.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("duplicate variant tags are refused", "[method]")
{
    // A well-formed pack is accepted -- the control, so this test is shown
    // able to pass as well as able to fail.
    constexpr auto fine = formula::variants(
        formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>)),
        formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>)));
    STATIC_REQUIRE(formula::detail::tags_are_distinct(fine));
}
```

- [ ] **Step 2: Run it and confirm it fails to build**

- [ ] **Step 3: Implement the distinctness check**

```cpp
/// True when no two variants declare the same tag. A method with two
/// variants for one tag has no answer to "which one applies", and picking
/// the first would make the second silently dead code.
template <typename... Cs>
[[nodiscard]] constexpr bool tags_are_distinct(Variants<Cs...> const&) noexcept
{
    return detail::all_distinct<typename Cs::tag...>();
}
```

- [ ] **Step 4: Write the negative test with the duplicate in the MIDDLE, non-adjacent**

Phase 10's task 3 put a duplicate at rows 1 and 3 of 5 and **killed three mutations with one test** — first-pair-only, last-pair-only and adjacent-only all die against it. Do the same:

```cpp
// EXPECT: this method declares two variants for the same tag
inline constexpr auto broken = formula::variants(
    formula::variant<Cube>(...),      // 0
    formula::variant<Cylinder>(...),  // 1  <-- duplicate pair, non-adjacent
    formula::variant<Prism>(...),     // 2
    formula::variant<Cylinder>(...),  // 3  <-- with this one
    formula::variant<Slab>(...));     // 4
```

- [ ] **Step 5: Apply the deletion check, then run three mutations**

Delete the guard: the case must **compile**. Then mutate the check to compare only the first pair, only the last pair, and only adjacent pairs — each must fail this one negative test **and nothing else**. Report which test each mutation killed, not merely that the suite went red.

- [ ] **Step 6: Verify on all seven presets, plus Doxygen and mkdocs**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/method.hpp test/method_tests.cpp test/negative/method_duplicate_tag.cpp test/CMakeLists.txt
git commit -m "feat(method): refuse two variants declaring the same tag"
```

---

## Task 4: The variant-selection trace step, and the promise `sink.hpp` does not keep

**Files:**
- Modify: `include/formula-cpp/trace.hpp` (the `StepKind` enum at `:37`, and `detail::StepKindOf` at `:569-570`)
- Modify: `include/formula-cpp/trace_render.hpp`
- Modify: `include/formula-cpp/sink.hpp:75-79`
- Modify: `docs/tracing.md:385-406`
- Modify: `test/trace_tests.cpp`, `test/trace_render_tests.cpp`

**Bound by C2 — a documented promise the code does not keep, and this task is standing next to it:** `detail::StepKindOf` is a **closed, undefined-primary registry**, so a sink-aware consumer node **fails to compile** under `RecordingSink` — that is how the spike's first overlay build died on `cl`. But `sink.hpp:75-79` and `docs/tracing.md:385-406` promise graceful degradation that **only holds for the two-parameter overload**. Adding a `StepKind` here does not fix that; it makes the registry one entry longer. **Correct the claim in both places while you are editing the registry.** Do not open the extension point — that is a separate change.

**Bound by D5 — this is the phase's acceptance criterion, not a nicety.** §9.1: the trace records "which variant fired and on what discriminator". A step saying *a variant was selected* without naming **which** and **why** is the euphemism phase 10 spent four tasks removing.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("the trace names which variant fired and on what discriminator", "[trace][method]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::evaluate_method<Cylinder>(compressiveStrength, inputs, sink);

    std::string const rendered = formula::render_trace(trace, { .maxSteps = 20 });
    // Names the variant AND the discriminator, not merely that selection happened.
    CHECK(rendered.find("variant Cylinder") != std::string::npos);
    CHECK(rendered.find("Cube") == std::string::npos);  // the one NOT taken is not claimed
}
```

- [ ] **Step 2: Run it and confirm it fails**

- [ ] **Step 3: Add the enumerator**

In `trace.hpp`'s `StepKind`, after `Documented`:

```cpp
    /// A method selecting one of its variants. Carries the tag's name and
    /// the discriminator it matched, because "a variant was selected" is
    /// true of every outcome and therefore answers nothing.
    VariantSelected,
```

**Check the name on GCC specifically.** `StepKind::Pi` once shadowed `formula::Pi` and broke GCC alone, which is why the enumerator at `trace.hpp:45` is spelled `PiConstant`. Four Windows presets did not flag it; the Linux leg did, with warnings as errors.

- [ ] **Step 4: Render it in `trace_render.hpp`, and correct the two overclaims**

- [ ] **Step 5: Run the test; confirm the filter selected the expected count first**

- [ ] **Step 6: Mutate to prove the test kills something nothing else kills**

Change the recorded tag to always be the first variant's. The new test must fail **and nothing else**.

- [ ] **Step 7: Verify on all seven presets, plus Doxygen and mkdocs**

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/trace.hpp include/formula-cpp/trace_render.hpp include/formula-cpp/sink.hpp docs/tracing.md test/trace_tests.cpp test/trace_render_tests.cpp
git commit -m "feat(trace): name the variant that fired, and correct the degradation promise"
```

---

## Task 5: The overlay — constants, pruning and pinning

**Files:**
- Create: `include/formula-cpp/overlay.hpp`
- Create: `test/overlay_tests.cpp`
- Create: `test/negative/overlay_pins_absent_variant.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `test/CMakeLists.txt`

**Interfaces:**
- Produces: `template <typename... Ops> struct Overlay { std::tuple<Ops...> operations {}; };`
- Produces: `[[nodiscard]] constexpr auto overlay(Ops... operations) noexcept;`
- Produces: `[[nodiscard]] constexpr auto apply(Overlay<Ops...> const& o, Method<...> const& m) noexcept;` — **returns a Method**
- Produces: `with_constant<Q>(value)`, `pin_variant<Tag>()`, `prune_variant<Tag>()`

**Bound by ruling V3 — the architecture, settled by measurement:** an overlay is applied at **compile time** and yields a new method type. The spike built both shapes: the compile-time overlay satisfies all seven of §16.7's demands, keeps the trace intact through wholesale replacement, and costs **literally nothing** — `clang++ -O2` emits a byte-identical 39-mnemonic sequence with and without it. The runtime overlay cannot do wholesale replacement without type erasure that **freezes one concrete sink type into a virtual signature**, because `RecordingSink`'s `entered` and `produced` are member templates and cannot cross a virtual call.

**The jurisdiction set is closed and lives in the type; which jurisdiction applies is a runtime index.** That is §16.1's "registered per customer, per region, per contract": the registry is data, the set of possibilities is code.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("an overlay overrides a constant and yields a method", "[overlay]")
{
    constexpr auto national = formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }));
    constexpr auto overlaid = formula::apply(national, baseMethod);

    // The base method's factor is 1; the overlay's is 97/100, so the two
    // results differ. A fixture where they agreed could not tell an applied
    // overlay from an ignored one.
    constexpr auto base = formula::evaluate_method<Cube>(baseMethod, inputs);
    constexpr auto after = formula::evaluate_method<Cube>(overlaid, inputs);
    STATIC_REQUIRE(base->value() != after->value());
    STATIC_REQUIRE(after->value() == formula::Rational { 582, 100 });
}
```

- [ ] **Step 2: Run it and confirm it fails to build**

- [ ] **Step 3: Implement `Overlay`, `overlay()`, `with_constant` and `apply`**

- [ ] **Step 4: Implement `pin_variant<Tag>` and `prune_variant<Tag>`, each refusing an absent tag**

```cpp
static_assert((std::is_same_v<Tag, typename Cs::tag> || ...),
              "formula: this overlay pins or prunes a variant the method does not declare; an "
              "overlay that names a variant by mistake would silently do nothing");
```

- [ ] **Step 5: Measure the scale cost the spike did not (its unestablished item 3)**

`with_constant` is a full structural rewrite instantiating a new type per node per overlay, and the spike measured only **two variants, one constant, one small tree**. Build **five jurisdictions over a ten-node tree** and record compile time and instantiation depth against the un-overlaid baseline, on `cl` and `clang++`. **This is the one place ruling V3 could turn out expensive; report the numbers whatever they say.**

- [ ] **Step 6: Negative test, then the deletion check**

- [ ] **Step 7: Verify on all seven presets, plus Doxygen and mkdocs**

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/overlay.hpp test/overlay_tests.cpp test/negative/overlay_pins_absent_variant.cpp test/CMakeLists.txt include/formula-cpp/formula.hpp
git commit -m "feat(overlay): override constants, pin and prune variants"
```

---

## Task 6: Rounding override, and where the rule came from

**Files:**
- Modify: `include/formula-cpp/overlay.hpp`, `include/formula-cpp/trace.hpp`, `include/formula-cpp/trace_render.hpp`
- Modify: `test/overlay_tests.cpp`, `test/trace_render_tests.cpp`

**Interfaces:**
- Produces: `with_rounding<U, Places, Mode>()`
- Produces: `StepKind::RoundingRuleApplied`

**Bound by D5, and this is the bullet most easily got wrong.** §9.1 requires the trace to record "which rounding rule applied **and where it came from** — the method's own default or a jurisdiction overlay". A step that names only the granularity is true either way and therefore answers nothing — exactly the defect phase 10 found when `Conversion` was pinned only for the one kind where it could not be confused.

- [ ] **Step 1: Write the failing test — both provenances, in one test**

```cpp
TEST_CASE("the trace says where the rounding rule came from", "[trace][overlay]")
{
    auto const fromMethod = traceOf(baseMethod);
    CHECK(fromMethod.find("rounded to 1 dp (method default)") != std::string::npos);

    auto const fromOverlay = traceOf(formula::apply(tighterRounding, baseMethod));
    CHECK(fromOverlay.find("rounded to 2 dp (jurisdiction overlay)") != std::string::npos);

    // The two must differ. A test asserting only one provenance passes
    // whether or not the distinction exists.
    CHECK(fromMethod != fromOverlay);
}
```

- [ ] **Step 2: Run it and confirm it fails**
- [ ] **Step 3: Implement `with_rounding` and the provenance-carrying step**
- [ ] **Step 4: Run the test; confirm the filter selected the expected count**
- [ ] **Step 5: Mutate the provenance field to a constant and confirm this test alone fails**
- [ ] **Step 6: Verify on all seven presets, plus Doxygen and mkdocs**
- [ ] **Step 7: Commit**

```bash
git commit -m "feat(overlay): override rounding, and trace which rule applied and whence"
```

---

## Task 7: Wholesale replacement, and the declared-unit ruling

**Files:**
- Modify: `include/formula-cpp/overlay.hpp`
- Modify: `test/overlay_tests.cpp`
- Create: `test/negative/overlay_replacement_changes_quantity.cpp`

**Interfaces:**
- Produces: `replace_variant<Tag>(expression)`
- Produces: `add_derived<Q>(expression)`

**Bound by C1 — the design proposed something `measured.hpp` rules out, and the ruling is not obvious:** §16.7 requires an overlay to "change the declared unit". `measured.hpp:19,27` says a `Measured<Q>` is "a value of quantity Q, **in Q's declared unit**… It carries **no unit of its own**." The spike demonstrated the collision: jurisdiction A's 46 855 215.25 Pa came back in a `Measured<Strength>` labelled MPa.

**The ruling: a declared unit is a property of the quantity, so an overlay that changes it reports a *different quantity*** — a type-level fact the caller sees, not a hidden relabelling. **Do not add a unit field to `Measured<Q>`**; that invariant is load-bearing everywhere and exists precisely so a number can never disagree with its own label.

**This interacts with D2 and the plan must not leave it to be discovered:** variants must agree in quantity, so an overlay that changes the reported quantity changes it for **every** variant at once, or is refused. Implement that refusal and pin it.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("an overlay replaces a formula wholesale and the trace still explains it", "[overlay]")
{
    constexpr auto nationalB = formula::overlay(
        formula::replace_variant<Cylinder>(var<Force> / (formula::Pi * formula::pow<2>(var<Dm>) / formula::Rational { 4 })));
    constexpr auto overlaid = formula::apply(nationalB, baseMethod);

    auto const rendered = traceOf(overlaid);
    CHECK(rendered.find("variant Cylinder") != std::string::npos);
    CHECK(rendered.find("replaced by jurisdiction overlay") != std::string::npos);
}
```

- [ ] **Step 2: Run it and confirm it fails**
- [ ] **Step 3: Implement `replace_variant<Tag>` and `add_derived<Q>`**
- [ ] **Step 4: Implement the quantity-change refusal, with a message naming both quantities**
- [ ] **Step 5: Negative test, then the deletion check**
- [ ] **Step 6: Verify on all seven presets, plus Doxygen and mkdocs**
- [ ] **Step 7: Commit**

```bash
git commit -m "feat(overlay): replace a variant wholesale, and refuse a silent quantity change"
```

---

## Task 8: The jurisdiction-scoped vocabulary

**Files:**
- Create: `include/formula-cpp/vocabulary.hpp`
- Create: `test/vocabulary_tests.cpp`
- Modify: `include/formula-cpp/render.hpp`, `include/formula-cpp/document.hpp`, `include/formula-cpp/trace.hpp`

**Interfaces:**
- Produces: `template <typename... Es> struct ScopedVocabulary`
- Produces: `vocabulary(Es... entries)`, `renames<Q>("symbol")`
- Produces: `DefaultVocabulary` — changes nothing, and is the default argument everywhere

**Bound by F2 and ruling V4:** §16.7 requires symbols to be jurisdiction-scoped "because the same word denotes different quantities in different countries", **with the two meanings crossed over** in the observed case. This is a correctness requirement about what a rendered page *means*, not cosmetics. `Describe<T>::symbol` (`quantity.hpp:70`) stays unchanged: the vocabulary **wraps** it.

**The spike's finding that will catch you if you skip it:** the trace's symbol is written at **evaluation** time by the sink, not at render time. **Threading only the renderers yields correct pages and wrong traces** — a defect every page hides. `render.hpp`, `document.hpp` **and** `trace.hpp`'s `RecordingSink` all need it; `trace_render.hpp` needs none.

**The one real cost, already found and solved:** threading breaks the `render_node` ADL extension point — verified by compiling a consumer node both ways. `sink.hpp`'s own two-step `if constexpr (requires …)` idiom restores it. Use that idiom; do not invent a second one.

- [ ] **Step 1: Write the failing test — the crossed-over case, end to end**

```cpp
TEST_CASE("two jurisdictions cross over one pair of symbols", "[vocabulary]")
{
    constexpr auto north = formula::vocabulary(formula::renames<Strength>("R"), formula::renames<Modulus>("E"));
    constexpr auto south = formula::vocabulary(formula::renames<Strength>("E"), formula::renames<Modulus>("R"));

    CHECK(formula::render(f, north) == "R / E");
    CHECK(formula::render(f, south) == "E / R");
    // Crossed over, not merely different: each symbol means the other's
    // quantity. A test with two unrelated renames would pass even if the
    // vocabulary were applied to the wrong quantity.
}
```

- [ ] **Step 2: Run it and confirm it fails**
- [ ] **Step 3: Implement `vocabulary`, `renames<Q>`, `DefaultVocabulary` and `symbol_of<Q>`**
- [ ] **Step 4: Thread it through `render.hpp`, `document.hpp` and `RecordingSink`, defaulted so every existing call site is unchanged**
- [ ] **Step 5: Prove the existing call sites really are unchanged**

The spike compiled all 11 examples and 13 test TUs unchanged — **but on `clang++ 20.1.8` only**, and it **compiled them without running them** (its unestablished items 7 and 8). Do both properly: build **and run** the full suite on all seven presets.

- [ ] **Step 6: Prove the trace is right, not only the page**

Render a trace under a non-default vocabulary and assert the *trace's* symbols changed too. Then mutate the sink to ignore the vocabulary: the page tests must still pass and **this test must fail**. That mutation is the spike's finding made executable.

- [ ] **Step 7: Confirm the `render_node` ADL extension point still works**

Compile a consumer node defining `render_node` out of line, exactly as the spike did, both with and without a vocabulary argument.

- [ ] **Step 8: Verify on all seven presets, plus Doxygen and mkdocs**
- [ ] **Step 9: Commit**

```bash
git commit -m "feat(vocabulary): resolve symbols through a jurisdiction-scoped vocabulary"
```

---

## Task 9: The join — an overlaid method, a selected variant, a scoped vocabulary

**Files:**
- Modify: `test/overlay_tests.cpp`, `test/vocabulary_tests.cpp`
- Create: `test/method_cross_tu.hpp`, `test/method_cross_tu_b.cpp`

**This task exists because the spike said what it had not shown.** Its first unestablished item: *"I never combined the three probes… In particular I never rendered an overlaid method's selected variant through a `ScopedVocabulary`. Phase 10's lesson applies exactly here: that combination is an assignment, not an assumption."*

Phase 10's spike made the same disclosure about bands and runtime contents, and the task that joined them had a real assignment instead of an assumption to trip over. **Two separately verified things do not verify their join.**

- [ ] **Step 1: Write the failing test — all three at once**

```cpp
TEST_CASE("an overlaid method's selected variant renders in the jurisdiction's vocabulary", "[join]")
{
    constexpr auto overlaid = formula::apply(nationalB, compressiveStrength);
    auto const rendered = formula::render(overlaid, southernVocabulary);
    auto const traced = traceOf(overlaid, southernVocabulary);

    CHECK(rendered.find("E") != std::string::npos);            // the overlay's vocabulary
    CHECK(traced.find("variant Cylinder") != std::string::npos); // the selected variant
    CHECK(traced.find("replaced by jurisdiction overlay") != std::string::npos);
    CHECK(traced.find("E") != std::string::npos);               // and the trace agrees with the page
}
```

- [ ] **Step 2–4: Run, fix whatever the join breaks, re-run**
- [ ] **Step 5: Add the cross-TU check the spike did not do (its unestablished item 4)**

Everything the spike compiled was **one translation unit**. Given §13's `decltype([]{})` finding and this repo's existing `*_cross_tu` tests, an `inline constexpr` overlaid method shared across translation units gets the same check: declare it in a header, use it from two TUs, and **link**. Mangling defects only appear at link time — phase 10 shipped one that no compile caught.

- [ ] **Step 6: Verify on all seven presets, plus Doxygen and mkdocs**
- [ ] **Step 7: Commit**

```bash
git commit -m "test(method): join variant selection, overlay and vocabulary, and check across TUs"
```

---

## Task 10: Constraints on a method, and an overlay's own acceptance logic

**Files:**
- Modify: `include/formula-cpp/method.hpp`, `include/formula-cpp/overlay.hpp`
- Modify: `test/method_tests.cpp`, `test/overlay_tests.cpp`

**This quadrant is entirely unprobed** — the spike's unestablished item 5: *"I did not put a `ConstraintSet` on a method, did not replace one from an overlay, and did not test differing arity at all."* §16.7 requires an overlay to "supply its own acceptance logic **of a different arity**" — one jurisdiction comparing a pair of determinations where another requires a rolling mean, and a third replacing numeric limits with category codes.

**Bound by C3:** `Method` holds the `ConstraintSet` as an ordinary member and passes it to `check_all()` without unpacking. `check_all` returns `std::array<ConstraintOutcome, sizeof...(Ps)>`, one per constraint **at the matching index** — `constraint.hpp:277-280` says that index correspondence is part of the contract.

- [ ] **Step 1: Write the failing test — differing arity, which is the hard half**

```cpp
TEST_CASE("an overlay supplies acceptance logic of a different arity", "[overlay][constraint]")
{
    // Base: one constraint. Overlay: three, including one the base has no
    // counterpart for. The RESULT ARRAY SIZE differs, which is the point.
    constexpr auto overlaid = formula::apply(threeConstraints, baseMethod);
    constexpr auto baseOutcomes = formula::check_method(baseMethod, inputs);
    constexpr auto afterOutcomes = formula::check_method(overlaid, inputs);
    STATIC_REQUIRE(baseOutcomes.size() == 1);
    STATIC_REQUIRE(afterOutcomes.size() == 3);
}
```

- [ ] **Step 2: Run it and confirm it fails**
- [ ] **Step 3: Implement `check_method` and `with_constraints(...)`**
- [ ] **Step 4: Assert every verdict reaches the trace (D5's third bullet)**
- [ ] **Step 5: Mutate `check_all`'s index correspondence and confirm a named test fails**
- [ ] **Step 6: Verify on all seven presets, plus Doxygen and mkdocs**
- [ ] **Step 7: Commit**

```bash
git commit -m "feat(method): check a method's constraints, and let an overlay replace them"
```

---

## Task 11: Guide, example and gallery

**Files:**
- Create: `examples/methods_and_overlays.cpp`, `docs/methods-and-overlays.md`
- Modify: `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md`, `mkdocs.yml`, `README.md`, `docs/index.md`

**The guide is this project's cheapest reachability probe.** Phase 9 shipped a `collect` overload that worked, was tested, and **no user could call** — found only when a guide author tried the feature by analogy and the analogy would not compile. Phase 10 had its own: `document()` did not compile for any lookup kind while a reviewer had reported verifying it by hand. **If an example you write will not compile, that is a finding about the library.**

**Things a reader is owed, each recorded by an earlier task:**

1. **Why `variant<Tag>` and not the spec's `when<Tag>`** — the shipped ternary, and the silent mis-binding the spike measured.
2. **Which kinds of selection are checked when** — tag overlap always; tag completeness only against a closed case set; predicate overlap not at all, because it is not in the type.
3. **That a method matching no variant has no result** — no fallback, no first-match-wins.
4. **That an overlay changing the declared unit reports a different quantity**, because `Measured<Q>` carries no unit of its own.
5. **That the jurisdiction set is closed at compile time and indexed at runtime**, and why the alternative costs type erasure that freezes a sink type into a virtual signature.

**Gallery rules, bought this phase:** use `write_worked_formula(out, node)` for any worked section — every "Worked…" section once rendered a trace and never the expression it came from. **Never hand-type a formula, a rendering or an expected output**; regenerate `docs/gallery.md` and confirm `gallery.is-current` passes.

**Markdown rule:** a guard test asserts no `](` and no bare `[` in any Markdown *rendering*. Phase 8 published a broken line because `round[to 1 dp of mm](d)` is CommonMark link syntax.

**Quoted compiler diagnostics are now guarded** (`ea441f0`): a test checks the line numbers in quoted diagnostics against the real build. If you quote one, it must be captured from a real compile, and the guard will catch you if a later edit shifts it.

- [ ] **Step 1: Write `examples/methods_and_overlays.cpp`, ending with "all checks passed: yes"**
- [ ] **Step 2: Register it in `examples/CMakeLists.txt` via `formula_add_example`**
- [ ] **Step 3: Run it and capture its real output**
- [ ] **Step 4: Write `docs/methods-and-overlays.md`, quoting only captured output**
- [ ] **Step 5: Add a method and an overlaid method to the gallery; regenerate; confirm `gallery.is-current`**
- [ ] **Step 6: Wire the guide into `mkdocs.yml`, `README.md` and `docs/index.md`**
- [ ] **Step 7: Verify on all seven presets, plus the Doxygen target at 1.9.8 and `mkdocs build --strict`**
- [ ] **Step 8: Commit**

```bash
git commit -m "docs(method): add the methods-and-overlays guide, example and gallery entries"
```

---

## Self-Review

**1. Spec coverage.** §9.1's four patterns: specimen-dependent formula (tasks 1–2), apparatus-dependent correction (task 5's prune/pin), jurisdiction-dependent constants and rounding (tasks 5–6), input from another test — **not covered, and correctly so: that is phase 14's cross-sample context**, and this plan must not pre-empt it. §9.1's three required trace steps: variant (task 4), rounding with provenance (task 6), constraint verdicts (task 10). §16.7's seven overlay powers: constants (5), prune and pin (5), rounding (6), declared unit (7), derived quantities (7), wholesale replacement (7), acceptance logic of different arity (10). §16.7's jurisdiction-scoped symbols: task 8.

**2. Placeholder scan.** No "TBD", no "handle edge cases", no "similar to Task N". Task 5 step 3 and task 7 step 3 give interfaces rather than full bodies — those are structural rewrites whose shape the spike measured but whose code it did not keep, and the interface plus the ruling is what an implementer needs. Every test step carries real assertions.

**3. Type consistency.** `VariantCase<Tag, Expr>::tag` is used by tasks 2, 3, 5. `Method`'s three members are named `variantSet`, `rounding`, `constraintSet` in tasks 2, 5, 6, 7, 10. `apply(overlay, method)` returns a `Method` throughout — never an "OverlaidMethod". `ScopedVocabulary` and `DefaultVocabulary` are consistent between tasks 8 and 9. `check_all` returns `std::array<ConstraintOutcome, sizeof...(Ps)>`, matching `constraint.hpp:323`.

**One gap I am recording rather than papering over:** task 5 step 5's scale measurement could invalidate ruling V3. If five jurisdictions over a ten-node tree prove expensive to compile, the compile-time overlay is still correct but may need a bounded form. The plan does not pre-decide that, because the spike did not measure it — and inventing a mitigation for a cost nobody has seen is how a plan acquires a feature nothing needs.
