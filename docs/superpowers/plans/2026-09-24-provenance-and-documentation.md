# Provenance and Documentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a formula carry its own citation, render itself as text, and
generate the documentation that describes it — so the audit trail and the manual
come from the same source as the arithmetic.

**Architecture:** `documented(expr, {...})` wraps any expression in a
`DocumentedNode` that forwards dimension and evaluation and is otherwise
invisible, so wrapping changes neither the arithmetic nor the result. A
rendering walk turns a tree into text in one of three dialects, bracketing by
precedence rather than by guesswork. A documentation walk collects the citations
and the symbol table reachable from a root. A generator runs the library over
the example formulas and writes the gallery page, which CI regenerates and
diffs, so the headline feature is exercised on every commit.

**Tech Stack:** C++23 header-only; `std::string`, `std::vector`, `std::string_view`;
Catch2 v3 via CPM; CMake ≥ 3.23 + Ninja; cl, clang-cl, clang++, g++; MkDocs
Material and Doxygen on GitHub Pages.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — this plan
implements **§10 (Provenance)** and **§19 (Documentation)**. It is **spec phase 6
of §17**. Tracing (§11) is phase 7 and must not appear here.

## Global Constraints

- **C++23, header-only, no macros.** Every header compiles standalone under
  `cl`, `clang-cl`, `clang++` and `g++`.
- **Four presets before any commit:** `cl-debug`, `cl-release`, `clangcl-debug`,
  `clangcl-release` must all configure, build and test green.
- **No norm content in this repository, and no citations either.** Never name a
  real published standard, and never transcribe its text, equations, tables,
  clause numbers or threshold values. Published standards are copyrighted and
  sold, and this repository is public. Examples use **generic physics only**,
  with fictional `Example Standard` citations where a citation's *shape* has to
  be demonstrated. This constraint bites harder in this phase than in any
  previous one, because this phase is *about* citations: every sample citation
  in a test, an example, the gallery or the documentation must be invented.
- **The umbrella header stays lean.** `formula.hpp` deliberately pulls in no
  `<string>`, `<vector>` or `<format>`. Rendering and documentation need all
  three, so they live in headers a consumer opts into and the umbrella does
  **not** include. A consumer who only evaluates numbers must not compile a
  string formatter in every translation unit.
- **Two-layer error model:** `checked_<name>` returns
  `std::expected<T, ArithmeticError>` and is `noexcept`; the bare `<name>`
  throws `ArithmeticException`.
- **Every task enumerates the public surface it adds** and names, for each
  entity, the test that fails when that entity is broken. This is a required
  step, not a courtesy: in phase 5 every coverage gap was found by a reviewer
  and none by an implementer, because the plan's tests demonstrated each feature
  instead of covering each surface.
- **A measured claim in a doc comment gets a test, or it is not made.** Two
  documented numbers were wrong in phase 5 and both survived review because
  nothing tests prose.
- **No test may pass for the wrong reason.** Every new test is mutation-tested:
  break the code it covers, confirm *that* test fails with *that* reason, then
  restore. A mutation that makes nothing fail means the suite is incomplete —
  add the missing test, observe it fail, restore, confirm it passes.
- **Examples use `formula_add_example(name source expectedOutput)`.** Never add
  an executable by hand; that function links `support/fail_without_dialogs.cpp`,
  without which a failed assertion on Windows raises a modal dialog.
- **British spelling** in prose and doc comments; `snake_case` free functions,
  `PascalCase` types, `camelCase` locals, leading underscore on private data
  members. Run `clang-format -i` before committing.

## What the spike already settled

A throwaway spike of this design was compiled and **run** on **cl 19.51**,
**clang-cl 22** and **g++ 13.3** before this plan was written. All three printed
byte-identical output:

```
plain    w/c      : V_w / V_c
plain    trap A   : (V_w + V_c) / V_c
plain    trap B   : V_w - (V_c - V_w)
plain    trap C   : V_w / (V_c * V_c)
plain    circle   : pi * d^2 / 4
latex    circle   : \frac{\pi \cdot d^{2}}{4}
plain    adjusted : V_w / V_c * 100
citations reachable from the composed root: 2
w/c evaluates to 3/5
```

| Question | Measured answer |
|---|---|
| Does a designated initialiser bind to the non-deduced `Citation` parameter? | **Yes** on all three. `documented(expr, {.title = "…", .section = "…"})` is a real spelling. The spec claimed this from an older scaffold; it is now re-verified against the current headers. |
| Does wrapping change the arithmetic or the dimension? | No. The wrapped ratio still evaluates to exactly `3/5` and still reports `dim::Scalar`. |
| Does precedence bracketing work, including the traps? | Yes. `(a + b) / c` keeps its brackets, `a - (b - c)` keeps its, `a / (b * c)` keeps its, and `a / b * 100` correctly has none. |
| Do nested citations survive composition? | Yes — two citations reachable from a doubly-wrapped root, outermost first. |
| Is a second dialect a walk or a rewrite? | A walk. LaTeX falls out of the same traversal as `\frac{\pi \cdot d^{2}}{4}`. |

---

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/citation.hpp` | **Create.** `Citation`, `DocumentedNode`, `documented()`, and the evaluation overload that forwards through it. Header-only, no `<string>`. |
| `include/formula-cpp/render.hpp` | **Create.** `Dialect`, precedence, `render<Dialect>(expr)`. Pulls `<string>`; **not** in the umbrella. |
| `include/formula-cpp/document.hpp` | **Create.** `CitationEntry`, `SymbolEntry`, `Documentation`, `document(expr)`. Pulls `<vector>`; **not** in the umbrella. |
| `include/formula-cpp/formula.hpp` | **Modify.** Add `citation.hpp` only. Explain in the file comment why `render.hpp` and `document.hpp` are excluded. |
| `tools/gallery/main.cpp` | **Create.** Renders the example formulas into `docs/gallery.md`. |
| `tools/gallery/CMakeLists.txt` | **Create.** |
| `test/citation_tests.cpp`, `test/render_tests.cpp`, `test/document_tests.cpp` | **Create.** |
| `test/negative/documented_dimension_mismatch.cpp` | **Create.** |
| `examples/citations.cpp` | **Create.** |
| `docs/citations.md` | **Create.** Guide: citations, rendering, documentation generation. |
| `docs/gallery.md` | **Generated.** Checked in, and CI fails if regenerating it changes it. |
| `.github/workflows/pages.yml` | **Create.** MkDocs + Doxygen to GitHub Pages. |
| `.github/workflows/build.yml` | **Modify.** Add a gallery-is-current check. |
| `Doxyfile.in`, `mkdocs.yml`, `CMakeLists.txt` | **Create/modify.** |

---

## Task 1: Citations and the documented wrapper

**Files:**
- Create: `include/formula-cpp/citation.hpp`
- Modify: `include/formula-cpp/formula.hpp`, `CMakeLists.txt`
- Test: `test/citation_tests.cpp`
- Create: `test/negative/documented_dimension_mismatch.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Node`, `NodeBase` from `expression.hpp`; `Evaluated`,
  `checked_evaluate_si` from `evaluate.hpp`; `Dimension` from `dimension.hpp`.
- Produces:
  ```cpp
  struct Citation { std::string_view title, reference, section, equation, text; };
  template <Node Inner> struct DocumentedNode;              // .inner, .citation, ::dimension
  template <Node Inner> constexpr DocumentedNode<Inner> documented(Inner, Citation) noexcept;
  template <typename Rep = Rational, Node Inner, typename Env>
  constexpr Evaluated<Rep> checked_evaluate_si(DocumentedNode<Inner> const&, Env const&) noexcept;
  ```

**Public surface this task adds, and what covers each entity:**

| Entity | Covered by |
|---|---|
| `Citation` and each of its five fields | "a citation carries every field it was given" |
| `Citation::operator==` | "two citations with the same fields compare equal" |
| `DocumentedNode::dimension` | "wrapping does not change the dimension" |
| `DocumentedNode::inner` | "the wrapper keeps the expression it wrapped" |
| `documented()` | every test in the file |
| `checked_evaluate_si(DocumentedNode)` | "a wrapped formula evaluates to what it wrapped" |
| nesting | "a citation survives being wrapped again" |

**Ruling: `Citation` holds `std::string_view`, not owning strings.** A citation
is written as string literals at the point a formula is declared, and literals
have static storage, so a view is sound and keeps `Citation` usable in a
`constexpr` tree. The cost is that a citation built from a runtime `std::string`
must outlive the node; the header says so plainly. Cost if wrong: an owning
variant is additive and needs no change to the node.

**Ruling: every field defaults.** A caller naming only `.title` is the common
case, and a designated initialiser cannot skip a field that has no default.

- [ ] **Step 1: Write the failing test**

Create `test/citation_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/environment.hpp>
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
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Every citation in this repository is invented. Naming a real standard would
// put copyrighted material in a public repository.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });

} // namespace

TEST_CASE("citation: a citation carries every field it was given", "[citation]")
{
    STATIC_REQUIRE(ratio.citation.title == std::string_view { "Water/cement ratio" });
    STATIC_REQUIRE(ratio.citation.reference == std::string_view { "Example Standard 1:2020" });
    STATIC_REQUIRE(ratio.citation.section == std::string_view { "5.4.2" });
    STATIC_REQUIRE(ratio.citation.equation == std::string_view { "(3)" });
    STATIC_REQUIRE(ratio.citation.text == std::string_view { "Ratio of water content to cement content." });
}

TEST_CASE("citation: a field not named is empty, not absent", "[citation]")
{
    constexpr auto sparse = formula::documented(var<WaterVolume>, { .title = "A volume" });

    STATIC_REQUIRE(sparse.citation.title == std::string_view { "A volume" });
    STATIC_REQUIRE(sparse.citation.reference.empty());
    STATIC_REQUIRE(sparse.citation.section.empty());
    STATIC_REQUIRE(sparse.citation.equation.empty());
    STATIC_REQUIRE(sparse.citation.text.empty());
}

TEST_CASE("citation: two citations with the same fields compare equal", "[citation]")
{
    constexpr formula::Citation left { .title = "A", .section = "1" };
    constexpr formula::Citation right { .title = "A", .section = "1" };
    constexpr formula::Citation other { .title = "A", .section = "2" };

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("citation: wrapping does not change the dimension", "[citation]")
{
    STATIC_REQUIRE(decltype(ratio)::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::documented(var<WaterVolume>, {}))::dimension == formula::dim::Volume);
}

TEST_CASE("citation: the wrapper keeps the expression it wrapped", "[citation]")
{
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.inner),
                                  formula::BinaryNode<formula::BinaryOperator::Divide,
                                                      formula::VarNode<WaterVolume>,
                                                      formula::VarNode<CementVolume>>>);
}

TEST_CASE("citation: a documented expression is still an expression", "[citation]")
{
    STATIC_REQUIRE(formula::Node<decltype(ratio)>);

    // It composes like any other node, and the composite has the right dimension.
    constexpr auto scaled = ratio * rat(100);
    STATIC_REQUIRE(formula::is_dimensionless(decltype(scaled)::dimension));
}

TEST_CASE("citation: a wrapped formula evaluates to what it wrapped", "[citation]")
{
    constexpr auto inputs = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                 formula::Measured<CementVolume> { rat(300) });

    constexpr auto wrapped = formula::checked_evaluate<Ratio>(ratio, inputs);
    constexpr auto bare = formula::checked_evaluate<Ratio>(var<WaterVolume> / var<CementVolume>, inputs);

    STATIC_REQUIRE(wrapped.has_value());
    STATIC_REQUIRE(wrapped->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(wrapped->measurement() == bare->measurement());
}

TEST_CASE("citation: an absent input still propagates through a wrapper", "[citation]")
{
    constexpr auto partial = formula::environment(formula::Measured<WaterVolume>::absent(),
                                                  formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("citation: a citation survives being wrapped again", "[citation]")
{
    constexpr auto outer = formula::documented(ratio, { .title = "Water/cement ratio, per cent" });

    STATIC_REQUIRE(outer.citation.title == std::string_view { "Water/cement ratio, per cent" });
    STATIC_REQUIRE(outer.inner.citation.title == std::string_view { "Water/cement ratio" });
    STATIC_REQUIRE(decltype(outer)::dimension == formula::dim::Scalar);
}
```

Create `test/negative/documented_dimension_mismatch.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
#include <formula-cpp/citation.hpp>

struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// Wrapping must not smuggle a dimensional error past the check: the wrapper
// forwards the dimension, so the addition is still refused.
inline constexpr auto broken =
    formula::documented(formula::var<Volume>, { .title = "A volume" }) + formula::var<Length>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
```

- [ ] **Step 2: Run to verify it fails**

```
cmake --preset cl-debug && cmake --build --preset cl-debug
```
Expected: `Cannot open include file: 'formula-cpp/citation.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/citation.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Where a formula comes from.
///
/// `documented(expr, {...})` wraps an expression in a node that carries a
/// citation and is otherwise invisible: it forwards the dimension, it forwards
/// evaluation, and it composes like any other node. Wrapping therefore changes
/// neither the arithmetic nor the result, and a citation stays reachable from a
/// composed root however deeply the formula is nested.
///
/// This header deliberately holds no rendering and no string building. A
/// consumer who only evaluates numbers includes it through the umbrella and
/// pays nothing for the documentation layer.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>

#include <string_view>

namespace formula
{

/// Where a formula came from, in the words of whoever wrote it down.
///
/// The fields are `std::string_view`, not owning strings, because a citation is
/// written as string literals at the point the formula is declared and literals
/// have static storage. That keeps a `Citation` usable inside a `constexpr`
/// tree. A citation built from a runtime `std::string` is legal, but that
/// string must outlive every node holding the view.
///
/// Every field defaults, so a caller may name only the ones that apply -- a
/// designated initialiser cannot skip a field that has no default.
struct Citation
{
    /// What the formula is called, in prose: "Water/cement ratio".
    std::string_view title {};
    /// The document it comes from, however the citing organisation writes it.
    std::string_view reference {};
    /// The clause or section within that document.
    std::string_view section {};
    /// The equation number within that section.
    std::string_view equation {};
    /// The definition in full, for a reader who has not got the document.
    std::string_view text {};

    [[nodiscard]] constexpr bool operator==(Citation const&) const noexcept = default;
};

/// An expression with a citation attached.
///
/// It forwards `dimension` rather than declaring one of its own, so a
/// dimensional error inside a documented formula is still caught where the
/// formula is written -- wrapping is not a way to smuggle one past the check.
template <Node Inner>
struct DocumentedNode: NodeBase
{
    Inner inner {};
    Citation citation {};

    static constexpr Dimension dimension = Inner::dimension;
};

/// Attaches a citation: `documented(var<A> / var<B>, { .title = "…" })`.
///
/// The `Citation` parameter is deliberately **not** deduced. That is what lets
/// the call site write a braced designated initialiser, which was verified on
/// cl 19.51, clang-cl 22 and g++ 13.3 before this was written.
template <Node Inner>
[[nodiscard]] constexpr DocumentedNode<Inner> documented(Inner inner, Citation citation) noexcept
{
    return DocumentedNode<Inner> { {}, inner, citation };
}

/// Evaluating a documented expression evaluates what it documents. The wrapper
/// is invisible to arithmetic; only the documentation walk and, from phase 7,
/// the trace sink will notice it.
template <typename Rep = Rational, Node Inner, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(DocumentedNode<Inner> const& node,
                                                           Env const& environment) noexcept
{
    return checked_evaluate_si<Rep>(node.inner, environment);
}

} // namespace formula
```

Add `#include <formula-cpp/citation.hpp>` to `include/formula-cpp/formula.hpp`,
and extend that file's comment to say why `render.hpp` and `document.hpp` are
**not** included: they pull `<string>` and `<vector>`, and a consumer who only
evaluates numbers must not compile them in every translation unit.

Add the header to `FILE_SET HEADERS` in the top-level `CMakeLists.txt`, and
`citation_tests.cpp` plus the negative test to `test/CMakeLists.txt`.

- [ ] **Step 4: Run the tests to verify they pass**

- [ ] **Step 5: Mutation-test the suite**

Apply each on its own, rebuild, record the failing test and its verbatim message,
restore:

1. `DocumentedNode::dimension` set to `dim::Scalar` rather than forwarded.
   Expected: "wrapping does not change the dimension" fails, and the negative
   test stops failing to compile.
2. `checked_evaluate_si(DocumentedNode)` returns `detail::nothing<Rep>()`.
   Expected: "a wrapped formula evaluates to what it wrapped" fails.
3. `documented()` discards its citation argument and stores `Citation {}`.
   Expected: "a citation carries every field it was given" fails.
4. `Citation::operator==` replaced with `return true`.
   Expected: "two citations with the same fields compare equal" fails on the
   inequality.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/citation.hpp include/formula-cpp/formula.hpp CMakeLists.txt \
        test/citation_tests.cpp test/negative/documented_dimension_mismatch.cpp test/CMakeLists.txt
git commit -m "feat(citation): a formula carries where it came from"
```
---

## Task 2: Rendering a formula as text

**Files:**
- Create: `include/formula-cpp/render.hpp`
- Test: `test/render_tests.cpp`
- Modify: `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: every node type from `expression.hpp`, `function.hpp` and
  `citation.hpp`; `Describe<Q>::symbol`; `view(Symbol)` from `unit.hpp`.
- Produces:
  ```cpp
  enum class Dialect { Plain, Markdown, LaTeX };
  template <Dialect D, Node N> std::string render(N const& node);
  template <Node N> std::string render(N const& node);      // Plain
  ```

**This header is not in the umbrella.** It pulls `<string>`, and `formula.hpp`
promises a consumer who only evaluates numbers will not compile a string
formatter in every translation unit.

**Public surface this task adds, and what covers each entity:**

| Entity | Covered by |
|---|---|
| `Dialect::Plain` | every plain-dialect test |
| `Dialect::Markdown` | "the Markdown dialect emphasises the symbols" |
| `Dialect::LaTeX` | "LaTeX renders a quotient as a fraction" and the `\pi`, `\sqrt`, `^{}` tests |
| `render` of `VarNode` | "a variable renders as its own symbol" |
| `render` of `ConstantNode` | "a constant renders with its unit" and "a dimensionless constant renders bare" |
| `render` of `UnaryNode` | "negation brackets a sum but not a variable" |
| `render` of `BinaryNode`, each of four operators | "the four operators render as themselves" |
| precedence bracketing | the six bracketing tests |
| right-associativity of `-` and `/` | "subtraction and division bracket their right operand" |
| `render` of `PowerNode` | "a power renders its exponent" |
| `render` of `RootNode` | "a square root and a general root render differently" |
| `render` of `PiNode` | "pi renders per dialect" |
| `render` of `DocumentedNode` | "a citation does not appear in the rendered formula" |
| the one-argument `render` | "the default dialect is plain" |

**Ruling: precedence is a property of the node type, read from a trait, not a
number passed down the walk.** `detail::PrecedenceOf<N>` is specialised per node
kind, so adding a node kind later is a specialisation rather than an edit to
every renderer. Cost if wrong: one more specialisation than strictly needed.

**Ruling: a child is bracketed when it binds *more loosely* than its context,
and the right operand of a subtraction or a division raises that context by
one.** This is the only correct rule and the spike proved all three traps:
`(a + b) / c` keeps its brackets, `a - (b - c)` keeps its, `a / (b * c)` keeps
its, and `a / b * 100` correctly has none. A renderer that bracketed by equal
precedence everywhere would produce `(a / b) * 100`, which is noise; one that
never bracketed at equal precedence would produce `a - b - c` for `a - (b - c)`,
which is **wrong**.

**Ruling: `Markdown` differs from `Plain` only in emphasis, and LaTeX is the
one that restructures.** Markdown wraps each variable symbol in backticks so a
symbol containing `_` survives a Markdown renderer; LaTeX uses `\frac`,
`\cdot`, `\sqrt` and `^{}`. Cost if wrong: a dialect is one `if constexpr` away.

- [ ] **Step 1: Write the failing test**

Create `test/render_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/citation.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/render.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", formula::unit::SquareMetre>
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
using formula::Dialect;

} // namespace

TEST_CASE("render: a variable renders as its own symbol", "[render]")
{
    CHECK(formula::render<Dialect::Plain>(var<WaterVolume>) == "V_w");
    CHECK(formula::render<Dialect::Plain>(var<Diameter>) == "d");
}

TEST_CASE("render: the default dialect is plain", "[render]")
{
    CHECK(formula::render(var<WaterVolume> / var<CementVolume>) == "V_w / V_c");
}

TEST_CASE("render: the four operators render as themselves", "[render]")
{
    CHECK(formula::render(var<WaterVolume> + var<CementVolume>) == "V_w + V_c");
    CHECK(formula::render(var<WaterVolume> - var<CementVolume>) == "V_w - V_c");
    CHECK(formula::render(var<WaterVolume> * var<CementVolume>) == "V_w * V_c");
    CHECK(formula::render(var<WaterVolume> / var<CementVolume>) == "V_w / V_c");
}

TEST_CASE("render: a sum inside a quotient keeps its brackets", "[render]")
{
    CHECK(formula::render((var<WaterVolume> + var<CementVolume>) / var<CementVolume>) == "(V_w + V_c) / V_c");
}

TEST_CASE("render: subtraction and division bracket their right operand", "[render]")
{
    // a - (b - c) is not (a - b) - c, so the brackets are not decoration.
    CHECK(formula::render(var<WaterVolume> - (var<CementVolume> - var<WaterVolume>)) == "V_w - (V_c - V_w)");
    CHECK(formula::render(var<WaterVolume> / (var<CementVolume> * var<CementVolume>)) == "V_w / (V_c * V_c)");
}

TEST_CASE("render: equal precedence on the left needs no brackets", "[render]")
{
    CHECK(formula::render((var<WaterVolume> - var<CementVolume>) - var<WaterVolume>) == "V_w - V_c - V_w");
    CHECK(formula::render((var<WaterVolume> / var<CementVolume>) * rat(100)) == "V_w / V_c * 100");
}

TEST_CASE("render: a product inside a sum needs no brackets", "[render]")
{
    CHECK(formula::render(var<WaterVolume> + var<CementVolume> * rat(2)) == "V_w + V_c * 2");
}

TEST_CASE("render: negation brackets a sum but not a variable", "[render]")
{
    CHECK(formula::render(-var<WaterVolume>) == "-V_w");
    CHECK(formula::render(-(var<WaterVolume> + var<CementVolume>)) == "-(V_w + V_c)");
}

TEST_CASE("render: a constant renders with its unit", "[render]")
{
    CHECK(formula::render(formula::constant<formula::unit::Millimetre>(rat(150))) == "150 mm");
}

TEST_CASE("render: a dimensionless constant renders bare", "[render]")
{
    CHECK(formula::render(formula::number(rat(4))) == "4");
    CHECK(formula::render(formula::number(rat(1, 4))) == "1/4");
}

TEST_CASE("render: a power renders its exponent", "[render]")
{
    CHECK(formula::render(formula::pow<2>(var<Diameter>)) == "d^2");
    CHECK(formula::render(formula::pow<-1>(var<Diameter>)) == "d^-1");
    // A sum raised to a power must keep its brackets.
    CHECK(formula::render(formula::pow<2>(var<WaterVolume> + var<CementVolume>)) == "(V_w + V_c)^2");
}

TEST_CASE("render: a square root and a general root render differently", "[render]")
{
    CHECK(formula::render(formula::sqrt(var<Area>)) == "sqrt(A)");
    CHECK(formula::render(formula::cbrt(var<Area>)) == "root3(A)");
    CHECK(formula::render(formula::root<4>(var<Area>)) == "root4(A)");
}

TEST_CASE("render: pi renders per dialect", "[render]")
{
    CHECK(formula::render<Dialect::Plain>(formula::pi) == "pi");
    CHECK(formula::render<Dialect::LaTeX>(formula::pi) == "\\pi");
}

TEST_CASE("render: LaTeX renders a quotient as a fraction", "[render]")
{
    CHECK(formula::render<Dialect::LaTeX>(var<WaterVolume> / var<CementVolume>) == "\\frac{V_w}{V_c}");
    // A fraction brackets nothing: \frac already groups both sides.
    CHECK(formula::render<Dialect::LaTeX>((var<WaterVolume> + var<CementVolume>) / var<CementVolume>)
          == "\\frac{V_w + V_c}{V_c}");
}

TEST_CASE("render: LaTeX spells multiplication, powers and roots its own way", "[render]")
{
    CHECK(formula::render<Dialect::LaTeX>(var<WaterVolume> * var<CementVolume>) == "V_w \\cdot V_c");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(var<Diameter>)) == "d^{2}");
    CHECK(formula::render<Dialect::LaTeX>(formula::sqrt(var<Area>)) == "\\sqrt{A}");
    CHECK(formula::render<Dialect::LaTeX>(formula::root<4>(var<Area>)) == "\\sqrt[4]{A}");
}

TEST_CASE("render: the Markdown dialect emphasises the symbols", "[render]")
{
    // A symbol containing an underscore would otherwise be read as emphasis by
    // a Markdown renderer, which is exactly why this dialect exists.
    CHECK(formula::render<Dialect::Markdown>(var<WaterVolume> / var<CementVolume>) == "`V_w` / `V_c`");
}

TEST_CASE("render: a citation does not appear in the rendered formula", "[render]")
{
    constexpr auto documented = formula::documented(var<WaterVolume> / var<CementVolume>,
                                                    { .title = "Water/cement ratio" });

    CHECK(formula::render(documented) == "V_w / V_c");
    // And wrapping does not change how the result is bracketed in a larger tree.
    CHECK(formula::render(documented * rat(100)) == "V_w / V_c * 100");
}

TEST_CASE("render: a deep tree renders without losing a bracket", "[render]")
{
    constexpr auto circularArea = formula::pi * formula::pow<2>(var<Diameter>) / rat(4);

    CHECK(formula::render(circularArea) == "pi * d^2 / 4");
    CHECK(formula::render<Dialect::LaTeX>(circularArea) == "\\frac{\\pi \\cdot d^{2}}{4}");
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: `Cannot open include file: 'formula-cpp/render.hpp'`.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/render.hpp`. The shape below was compiled and run on
all three toolchains; transcribe it and fill in the dialect differences the tests
demand.

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula back into something a person reads.
///
/// The walk brackets a child exactly when it binds more loosely than the
/// context it sits in, and raises that context by one for the right operand of
/// a subtraction or a division -- because `a - (b - c)` is not `(a - b) - c`.
/// Bracketing on equal precedence everywhere would produce `(a / b) * 100`,
/// which is noise; never bracketing on equal precedence would produce
/// `a - b - c` for `a - (b - c)`, which is wrong.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<string>`, and a consumer who only evaluates numbers must not compile a
/// string formatter in every translation unit. Include it when you want text.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

#include <string>
#include <string_view>

namespace formula
{

/// How a formula should be spelled.
enum class Dialect
{
    /// Plain text, for a terminal or a log.
    Plain,
    /// Markdown: symbols in backticks, so an underscore is not read as emphasis.
    Markdown,
    /// LaTeX: fractions, radicals and braced exponents.
    LaTeX,
};

namespace detail
{
    /// How tightly a node binds. A trait rather than a number threaded through
    /// the walk, so a new node kind is a specialisation rather than an edit to
    /// every renderer.
    enum class Precedence
    {
        Additive = 1,
        Multiplicative = 2,
        Unary = 3,
        Atom = 4,
    };

    template <typename N>
    struct PrecedenceOf
    {
        static constexpr Precedence value = Precedence::Atom;
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct PrecedenceOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr Precedence value = (Op == BinaryOperator::Add || Op == BinaryOperator::Subtract)
                                                ? Precedence::Additive
                                                : Precedence::Multiplicative;
    };

    template <UnaryOperator Op, Node Operand>
    struct PrecedenceOf<UnaryNode<Op, Operand>>
    {
        static constexpr Precedence value = Precedence::Unary;
    };

    /// A wrapper binds exactly as tightly as what it wraps, so a citation never
    /// changes where a bracket falls.
    template <Node Inner>
    struct PrecedenceOf<DocumentedNode<Inner>>
    {
        static constexpr Precedence value = PrecedenceOf<Inner>::value;
    };

    /// An exact rational as text: `4`, or `1/4` when it is not whole.
    [[nodiscard]] inline std::string number_text(Rational value)
    {
        if (value.denominator() == 1)
            return std::to_string(value.numerator());
        return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
    }
} // namespace detail

template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node);

namespace detail
{
    template <Dialect D, Node Child>
    [[nodiscard]] std::string render_operand(Child const& child, Precedence context)
    {
        std::string text = render<D>(child);
        if (static_cast<int>(PrecedenceOf<Child>::value) < static_cast<int>(context))
            return "(" + text + ")";
        return text;
    }
} // namespace detail

// One overload per node kind. Each is found by argument-dependent lookup from
// `render` below, exactly as the evaluator's overloads are.

template <Dialect D, Described Q>
[[nodiscard]] std::string render_node(VarNode<Q> const&)
{
    std::string symbol { Describe<Q>::symbol };
    if constexpr (D == Dialect::Markdown)
        return "`" + symbol + "`";
    else
        return symbol;
}

template <Dialect D, Unit U>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node)
{
    std::string const text = detail::number_text(node.number);
    constexpr Unit unit = U;
    std::string_view const symbol = view(unit.symbolText);
    return symbol.empty() ? text : text + " " + std::string { symbol };
}

template <Dialect D, UnaryOperator Op, Node Operand>
[[nodiscard]] std::string render_node(UnaryNode<Op, Operand> const& node)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary);
}

template <Dialect D, BinaryOperator Op, Node Left, Node Right>
[[nodiscard]] std::string render_node(BinaryNode<Op, Left, Right> const& node)
{
    constexpr detail::Precedence here = detail::PrecedenceOf<BinaryNode<Op, Left, Right>>::value;
    constexpr detail::Precedence rightContext =
        (Op == BinaryOperator::Subtract || Op == BinaryOperator::Divide)
            ? static_cast<detail::Precedence>(static_cast<int>(here) + 1)
            : here;

    if constexpr (D == Dialect::LaTeX && Op == BinaryOperator::Divide)
        // \frac groups both sides itself, so neither operand needs a bracket.
        return "\\frac{" + render<D>(node.lhs) + "}{" + render<D>(node.rhs) + "}";
    else
    {
        std::string const lhs = detail::render_operand<D>(node.lhs, here);
        std::string const rhs = detail::render_operand<D>(node.rhs, rightContext);

        if constexpr (Op == BinaryOperator::Add)
            return lhs + " + " + rhs;
        else if constexpr (Op == BinaryOperator::Subtract)
            return lhs + " - " + rhs;
        else if constexpr (Op == BinaryOperator::Multiply)
            return D == Dialect::LaTeX ? lhs + " \\cdot " + rhs : lhs + " * " + rhs;
        else
            return lhs + " / " + rhs;
    }
}

template <Dialect D, int Exponent, Node Operand>
[[nodiscard]] std::string render_node(PowerNode<Exponent, Operand> const& node)
{
    std::string const base = detail::render_operand<D>(node.operand, detail::Precedence::Atom);
    if constexpr (D == Dialect::LaTeX)
        return base + "^{" + std::to_string(Exponent) + "}";
    else
        return base + "^" + std::to_string(Exponent);
}

template <Dialect D, int Degree, Node Operand>
[[nodiscard]] std::string render_node(RootNode<Degree, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    if constexpr (D == Dialect::LaTeX)
    {
        if constexpr (Degree == 2)
            return "\\sqrt{" + inner + "}";
        else
            return "\\sqrt[" + std::to_string(Degree) + "]{" + inner + "}";
    }
    else
    {
        if constexpr (Degree == 2)
            return "sqrt(" + inner + ")";
        else
            return "root" + std::to_string(Degree) + "(" + inner + ")";
    }
}

template <Dialect D>
[[nodiscard]] inline std::string render_node(PiNode const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\pi";
    else
        return "pi";
}

/// A citation is documentation, not arithmetic: it does not appear in the
/// rendered formula. `document()` is what surfaces it.
template <Dialect D, Node Inner>
[[nodiscard]] std::string render_node(DocumentedNode<Inner> const& node)
{
    return render<D>(node.inner);
}

/// Renders @p node in dialect @p D.
template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render<Dialect::Plain>(node);
}

} // namespace formula
```

Add the header to `FILE_SET HEADERS`, and `render_tests.cpp` to the test sources.
**Do not** add it to `formula.hpp`.

- [ ] **Step 4: Run the tests to verify they pass**

- [ ] **Step 5: Mutation-test the suite**

Apply each alone, rebuild, record the failing test and its message, restore:

1. In `render_operand`, change `<` to `<=`.
   Expected: "equal precedence on the left needs no brackets" fails, showing
   `(V_w - V_c) - V_w`.
2. In `render_node(BinaryNode)`, use `here` for `rightContext` unconditionally.
   Expected: "subtraction and division bracket their right operand" fails,
   showing `V_w - V_c - V_w` for `a - (b - c)` — the wrong-answer case.
3. In `PrecedenceOf<BinaryNode>`, return `Precedence::Atom` always.
   Expected: "a sum inside a quotient keeps its brackets" fails.
4. In `PrecedenceOf<DocumentedNode>`, return `Precedence::Atom`.
   Expected: no test fails on the plain path — **so add one**: a documented
   *sum* inside a quotient, which must keep its brackets. Add it, confirm it
   fails under the mutation, restore.
5. In `render_node(VarNode)` for Markdown, drop the backticks.
   Expected: "the Markdown dialect emphasises the symbols" fails.
6. In `render_node(ConstantNode)`, always return the bare number.
   Expected: "a constant renders with its unit" fails.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/render.hpp CMakeLists.txt test/render_tests.cpp test/CMakeLists.txt
git commit -m "feat(render): a formula spells itself, bracketed by precedence"
```
---

## Task 3: The documentation walk

**Files:**
- Create: `include/formula-cpp/document.hpp`
- Test: `test/document_tests.cpp`
- Modify: `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: every node type; `Citation` from `citation.hpp`; `render` from
  `render.hpp`; `Describe<Q>` from `quantity.hpp`.
- Produces:
  ```cpp
  struct SymbolEntry { std::string_view symbol, description; Unit unit; };
  struct Documentation
  {
      std::string formula;                    // rendered in the requested dialect
      std::vector<Citation> citations;        // outermost first
      std::vector<SymbolEntry> symbols;       // first appearance order, deduplicated
  };
  template <Dialect D = Dialect::Plain, Node N> Documentation document(N const& node);
  ```

**This header is not in the umbrella either.** It pulls `<vector>`.

**Public surface this task adds, and what covers each entity:**

| Entity | Covered by |
|---|---|
| `SymbolEntry` and its three fields | "the symbol table carries each variable's symbol, description and unit" |
| `SymbolEntry::operator==` | "two entries for the same quantity compare equal" |
| `Documentation::formula` | "the documentation carries the rendered formula" |
| `Documentation::citations` ordering | "citations come back outermost first" |
| `Documentation::symbols` ordering | "symbols come back in first-appearance order" |
| symbol deduplication | "a quantity used twice appears once" |
| `document()` dialect parameter | "the rendered formula follows the requested dialect" |
| a tree with no citations | "an undocumented formula yields an empty citation list" |

**Ruling: the symbol table is deduplicated by quantity type and ordered by first
appearance.** Alphabetical order would put `A` before `d` in a formula that
reads `pi * d^2 / 4`, which is not how anyone reads it. First-appearance order
matches the formula as written, which is the whole point of the page. Cost if
wrong: one comparator.

**Ruling: `document()` takes the dialect as a template parameter with a default,
matching `render`.** One spelling for both, so a caller who has learned one has
learned the other.

- [ ] **Step 1: Write the failing test**

Create `test/document_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Invented, as every citation in this repository must be.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2" });

constexpr auto perCent = formula::documented(ratio * rat(100),
                                             { .title = "Water/cement ratio, per cent",
                                               .reference = "Example Standard 1:2020",
                                               .section = "5.4.3" });

} // namespace

TEST_CASE("document: the documentation carries the rendered formula", "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    CHECK(documentation.formula == "V_w / V_c");
}

TEST_CASE("document: the rendered formula follows the requested dialect", "[document]")
{
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(ratio);

    CHECK(latex.formula == "\\frac{V_w}{V_c}");
}

TEST_CASE("document: citations come back outermost first", "[document]")
{
    formula::Documentation const documentation = formula::document(perCent);

    REQUIRE(documentation.citations.size() == 2);
    CHECK(documentation.citations[0].title == std::string_view { "Water/cement ratio, per cent" });
    CHECK(documentation.citations[0].section == std::string_view { "5.4.3" });
    CHECK(documentation.citations[1].title == std::string_view { "Water/cement ratio" });
    CHECK(documentation.citations[1].section == std::string_view { "5.4.2" });
}

TEST_CASE("document: an undocumented formula yields an empty citation list", "[document]")
{
    formula::Documentation const documentation = formula::document(var<WaterVolume> + var<CementVolume>);

    CHECK(documentation.citations.empty());
    CHECK(documentation.formula == "V_w + V_c");
}

TEST_CASE("document: the symbol table carries each variable's symbol, description and unit",
          "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[0].description == std::string_view { "effective water content" });
    CHECK(documentation.symbols[0].unit == formula::unit::Litre);
}

TEST_CASE("document: symbols come back in first-appearance order", "[document]")
{
    // Alphabetical order would put A before d in a formula that reads
    // pi * d^2 / 4, which is not how anyone reads it.
    formula::Documentation const documentation =
        formula::document(formula::pi * formula::pow<2>(var<Diameter>) / var<WaterVolume>);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a quantity used twice appears once", "[document]")
{
    formula::Documentation const documentation =
        formula::document(var<WaterVolume> + var<WaterVolume> * rat(2));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: two entries for the same quantity compare equal", "[document]")
{
    constexpr formula::SymbolEntry left { .symbol = "d",
                                          .description = "specimen diameter",
                                          .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry right { .symbol = "d",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry other { .symbol = "D",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("document: a constant contributes no symbol", "[document]")
{
    formula::Documentation const documentation =
        formula::document(var<Diameter> * formula::constant<formula::unit::Millimetre>(rat(2)));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}
```

- [ ] **Step 2: Run to verify it fails**

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/document.hpp`. Structure:

- `SymbolEntry` — `std::string_view symbol`, `std::string_view description`,
  `Unit unit`, defaulted `operator==`.
- `Documentation` — the three members above.
- A `detail::collect` overload per node kind, taking `Documentation&`:
  - `VarNode<Q>` appends a `SymbolEntry` **unless one with the same symbol is
    already present**;
  - `ConstantNode`, `PiNode` append nothing;
  - `UnaryNode`, `PowerNode`, `RootNode` recurse into their operand;
  - `BinaryNode` recurses left then right, which is what makes first-appearance
    order match reading order;
  - `DocumentedNode` pushes its citation, then recurses into `inner` — pushing
    first is what makes the list outermost-first.
- `document<D>(node)` renders, collects, and returns.

Include `<vector>`, `<string>`, `<string_view>`, `render.hpp` and
`citation.hpp`. Head the file with a comment saying it is deliberately not in
the umbrella and why.

Add to `FILE_SET HEADERS`; **not** to `formula.hpp`.

- [ ] **Step 4: Run the tests to verify they pass**

- [ ] **Step 5: Mutation-test the suite**

1. `DocumentedNode`'s collector recurses before pushing its citation.
   Expected: "citations come back outermost first" fails with the order reversed.
2. `BinaryNode`'s collector recurses right then left.
   Expected: "symbols come back in first-appearance order" fails.
3. The deduplication check is dropped.
   Expected: "a quantity used twice appears once" fails with two entries.
4. `VarNode`'s collector stores `Describe<Q>::symbol` for the description too.
   Expected: "the symbol table carries each variable's symbol, description and
   unit" fails.
5. `ConstantNode`'s collector appends an entry.
   Expected: "a constant contributes no symbol" fails.

- [ ] **Step 6: Run the other three presets**

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/document.hpp CMakeLists.txt test/document_tests.cpp test/CMakeLists.txt
git commit -m "feat(document): a formula describes itself, symbols and citations"
```

---

## Task 4: The generated gallery

**Files:**
- Create: `tools/gallery/main.cpp`, `tools/gallery/CMakeLists.txt`
- Create: `docs/gallery.md` (generated, checked in)
- Modify: `CMakeLists.txt`, `mkdocs.yml`, `.github/workflows/build.yml`

**Why this exists.** §19 asks for a gallery produced by *running* the library
over the example formulas, so the headline feature is exercised on every commit
rather than described. A page written by hand would drift from the code the
first time a symbol changed; a generated one cannot.

**Ruling: the gallery is generated, checked in, and CI fails when regenerating
it produces a different file.** Not generated at build time and left out of the
repository: a reader browsing GitHub should see the page, and a diff should show
when a formula's rendering changes. Cost if wrong: one regeneration step in a
contributor's workflow, which the failure message states.

- [ ] **Step 1: Write the generator**

`tools/gallery/main.cpp` declares a handful of **invented** formulas over
generic physics — a density, a circular area, a flow rate, a water/cement ratio
— each `documented(...)` with a fictional `Example Standard` citation, then
writes Markdown to the path given as `argv[1]`:

- a title and a sentence saying the page is generated by
  `tools/gallery/main.cpp` and must not be edited by hand;
- for each formula: its title as a heading, the plain rendering in a fenced
  block, the LaTeX rendering in a `$$` block, a symbol table as a Markdown table
  (symbol, description, unit), and the citation fields that are non-empty;
- a worked evaluation for at least one formula, showing the inputs and the exact
  result, so the page proves the numbers as well as the text.

Use `document<Dialect::Markdown>` for the symbol table and
`document<Dialect::LaTeX>` for the display form.

- [ ] **Step 2: Wire it into CMake**

`formula-cpp-gallery` is an executable built only when `FORMULA_TOOLS` is `ON`
(default `ON` for a top-level build, `OFF` when consumed as a subproject —
a consumer must not build our tooling). Add a test, `gallery.is-current`, that
runs the generator to a temporary file and compares it with `docs/gallery.md`,
failing with a message naming the exact command to regenerate.

- [ ] **Step 3: Generate the page and check it in**

- [ ] **Step 4: Add the page to `mkdocs.yml` navigation**

- [ ] **Step 5: Verify the check actually fails when the page is stale**

Edit one character of `docs/gallery.md`, run `ctest -R gallery.is-current`,
confirm it fails and that the message names the regeneration command. Restore,
confirm it passes. Quote both.

- [ ] **Step 6: All four presets**

- [ ] **Step 7: Commit**

```bash
git commit -m "feat(gallery): the documentation page the library writes itself"
```

---

## Task 5: The published site

**Files:**
- Create: `.github/workflows/pages.yml`, `Doxyfile.in`
- Modify: `mkdocs.yml`, `CMakeLists.txt`, `README.md`

**What to build.** MkDocs Material for the prose guides, Doxygen for the API
reference under `/api`, both published to GitHub Pages from `master` only.

- [ ] **Step 1: Doxygen configuration**

`Doxyfile.in` configured through CMake, with `WARN_AS_ERROR = FAIL_ON_WARNINGS`
so an undocumented public entity fails the build. Exclude `detail` namespaces.
Generate into the MkDocs site under `api/`.

- [ ] **Step 2: The workflow**

`.github/workflows/pages.yml`: on push to `master`, build the Doxygen API
reference and the MkDocs site, upload as a Pages artifact, deploy. Use the
standard `actions/deploy-pages` flow with `permissions: pages: write,
id-token: write`, and a `concurrency` group so two pushes cannot race.

**Stop and report rather than acting** if the repository's Pages settings are
not already configured for GitHub Actions deployment — that is a repository
setting, not a file in the tree, and it is the owner's to change.

- [ ] **Step 3: Verify the site builds locally**

`mkdocs build --strict` must pass, which catches broken internal links. Record
the output. Do not deploy from a developer machine.

- [ ] **Step 4: Commit**

```bash
git commit -m "docs: publish the guides and the API reference"
```

---

## Task 6: The citations guide and the example

**Files:**
- Create: `examples/citations.cpp`, `docs/citations.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `README.md`

- [ ] **Step 1: The example**

`examples/citations.cpp`: declare a documented formula, print its plain
rendering, its LaTeX rendering, its symbol table and its citation, then evaluate
it and print the result — showing that the citation travels with the formula and
the number is unaffected. Register with `formula_add_example`, pinning the exact
output.

- [ ] **Step 2: The guide**

`docs/citations.md` covering: attaching a citation; why the wrapper is invisible
to arithmetic; nesting and what order citations come back in; the three
dialects, with the same formula rendered three ways; the symbol table and its
ordering rule; generating a page; and the repository's own rule that every
citation here is invented because real standards are copyrighted.

Take every snippet from a test or the example so it cannot drift.

- [ ] **Step 3: All four presets, then commit**

```bash
git commit -m "docs: a guide to citations, rendering and generated documentation"
```

---

## Self-review

**Spec coverage.** §10's `documented()` wrapper, its dimension forwarding and
its invisibility to arithmetic are task 1. §19's MkDocs + Doxygen site is task
5, the generated gallery is task 4, the guides are tasks 4 and 6, and
`WARN_AS_ERROR` on undocumented entities is task 5 step 1. §19's "every fenced
C++ block compiles" is **deliberately not implemented**: it needs a doc-code
extractor that is a project of its own, and the same guarantee is obtained more
cheaply here by taking every snippet from a test or an example, which tasks 4
and 6 require. That is a narrowing of the spec and is called out rather than
hidden. Spelling checks are likewise omitted; link checking comes free with
`mkdocs build --strict`.

**Placeholder scan.** Tasks 3 to 6 describe their headers structurally rather
than giving every line, because their content is mechanical once the shape is
fixed; tasks 1 and 2, which carry the design risk, give complete code.

**Type consistency.** `Dialect` is defined in `render.hpp` and used by
`document.hpp` and the gallery. `Citation` is defined in `citation.hpp` and
consumed by both. `render<D>(node)` and `document<D>(node)` take the dialect the
same way.

**What was measured before this plan was committed.** The task 1 header and all
nine of its tests compile on cl 19.51, clang-cl 22 and g++ 13.3, and its
negative test fails with the expected phrase — naming
`DocumentedNode<VarNode<Volume>>`, which proves a wrapper cannot smuggle a
dimensional error past the check. The task 2 header and **all eighteen of its
test cases were compiled and run** on cl and g++: every expected string in this
plan is what the code actually produces. That last step is new, and it exists
because phase 5 shipped a plan containing a runtime test that asserted the
opposite of the truth — it was never run, only compiled.
