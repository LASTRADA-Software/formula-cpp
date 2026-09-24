# Quantities and Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every variable in a formula a type that carries its own symbol, description and unit, readable through one access point, and let a value be honestly absent rather than silently wrong.

**Architecture:** A CRTP base `Quantity<Tag, Symbol, Description, Unit>` makes each variable a distinct type whose metadata lives in the type itself. `Describe<T>` is the single place anything reads that metadata, and can be specialised for foreign types nobody owns. `Measured<Q>` is the runtime value: a possibly-absent `Rational` in `Q`'s declared unit. The type is compile-time and structural; the value is neither, and nothing forces them together.

**Tech Stack:** C++23, header-only, standard library only. Catch2 via CPM. cl.exe, clang-cl, clang++ and GCC.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — §8 (units, quantities and metadata), §9's empty-propagation paragraph, and the phase 4 row of §17.

## Global Constraints

- **C++23**, header-only, no dependencies beyond the standard library in public headers.
- Must compile clean under **cl.exe, clang-cl, clang++ and GCC**.
- **Open source, carrying no company IP, as generic as possible**: no DIN/EN/ISO citations, no transcribed standard text, no real threshold values from any standard, no LASTRADA references. Examples use generic physics only.
- Every new file starts with `// SPDX-License-Identifier: Apache-2.0`.
- **Every new header is registered in `FILE_SET HEADERS`** in the top-level `CMakeLists.txt` as it is created, never deferred. Ten headers ship today; this plan adds three, and Task 6 verifies all thirteen install.
- Catch2 `TEST_CASE` names must contain **balanced square brackets**. An unbalanced `[` or `]` makes CMake silently drop that test and every test discovered after it.
- Public headers must not include `<string>`, `<vector>`, `<format>` or `<iostream>`.
- Naming: types and constants `CamelCase`, functions `snake_case`.
- **Runtime assertions must never dereference a `std::expected` without checking it first.** Use `REQUIRE`/`REQUIRE_FALSE`, or the `unwrapped()` and `error_of()` helpers already in the test files. A `CHECK` does not stop the case, and the following dereference is undefined behaviour that this STL turns into a process abort — the assertion written to catch a regression becomes the thing that hides it.

---

## Design Rulings — all measured by a spike before this plan was written

**Ruling A — the five-parameter spelling is wrong; the dimension comes from the unit.**
The spec originally passed a dimension *and* a unit. A `Unit` already carries its dimension, so that states it twice and lets the two contradict. Measured on all three compilers, for a quantity declared `dim::Mass` with `unit::Litre`:

| Spelling | Result |
|---|---|
| dimension and unit, unchecked | **compiles silently on all three** |
| dimension and unit, `static_assert` they agree | fails loudly |
| unit only | cannot be written |

The spec has been amended (commit `5b2a8d6`). `Quantity` takes four parameters. Prefer the design where the invalid state cannot be expressed over the one where it is merely diagnosed.

**Ruling B — the CRTP tag is load-bearing and proven so.**
Two quantity types with *identical* symbol, description and unit but different tags are different types; the same tag in two translation units is one type. Verified by linking, not merely compiling, on cl, clang-cl and clang++. Without the tag, two variables whose documentation coincides collapse into one — and the failure mode is a wrong number in a report.

**Ruling C — the base-detection idiom for `Describe<T>` works on cl.**
An undefined function taking `Quantity<Tag, Symbol, Description, Unit> const&`, deduced through the derived-to-base conversion, compiled first try on cl. This was the risk expected to fight and did not.

**Ruling D — an empty value is `std::optional<Rational>`, and that does not constrain the type.**
`std::optional<Rational>` is fully usable in constant expressions on all three compilers, and cannot be a non-type template parameter on any of them — the MSVC STL's `optional` privately inherits a control-block base, so it is not structural. That does not matter: a spike built a runtime value wrapper alongside the quantity type while the type stayed usable as an NTTP. **The type must be structural; the value need not be; the two are independent axes.**

**Ruling E — cl prints string NTTPs as raw bytes, which the negative-test harness must account for.**
In a `static_assert` diagnostic, cl renders a `fixed_string` argument as a byte list (`char88,0`) where clang and clang-cl print `{"X"}`. The existing negative-compile tests assert on diagnostic text. Any new negative test whose expected substring would fall inside a `fixed_string` argument must instead assert on the library's own message, which all three compilers echo verbatim. **Write the assertion against our words, never against how a compiler renders a template argument.**

**Ruling H — the primary `Describe` is empty, and the diagnostic lives elsewhere.**
Found while running the pre-flight scan on this plan, by compiling it rather than reading it. The
first design gave the primary template a `static_assert` so an undeclared type got a helpful
message. That is wrong: a `static_assert` failure is not in the immediate context, so it is a hard
error rather than a substitution failure, and the `Described` concept — whose entire job is to
answer *is this type described?* — then fails to compile for every type that is not. Measured on
clang:

```
static_assert(!Described<int>);
  error: static assertion failed ... 'formula: this type does not declare any quantity metadata'
  note: in instantiation of template class 'Describe<int>' requested here
```

So the primary is empty, `Described` works by member detection, and the helpful message moves to
`RequireDescribed<T>`, which a caller asks for deliberately. Verified on cl, clang-cl and clang++
that the corrected shape gives `Described<Declared>` true and `Described<int>` false with no error.

**Ruling F — `Measured<Q>` stores its value in `Q`'s declared unit, always.**
The unit is part of the quantity type, so a value needs no unit of its own. Converting is an explicit operation that produces a value of a *different* quantity or a bare `Rational`, never a silently re-scaled `Measured<Q>`.

---

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/detail/fixed_string.hpp` | **Create.** A structural string usable as a non-type template parameter. |
| `include/formula-cpp/quantity.hpp` | **Create.** `Quantity` CRTP base and `Describe<T>`. |
| `include/formula-cpp/measured.hpp` | **Create.** `Measured<Q>`, the possibly-absent value, and its propagation. |
| `include/formula-cpp/formula.hpp` | **Modify.** Add the two new public headers to the umbrella. |
| `CMakeLists.txt` | **Modify.** Register all three in `FILE_SET HEADERS`. |
| `test/fixed_string_tests.cpp` | **Create.** |
| `test/quantity_tests.cpp` | **Create.** |
| `test/quantity_cross_tu.hpp`, `test/quantity_cross_tu_b.cpp` | **Create.** Cross-TU identity, proven by linking. |
| `test/measured_tests.cpp` | **Create.** |
| `test/negative/quantity_wrong_type.cpp` | **Create.** Must-not-compile: one quantity used where another is expected. |
| `test/negative/describe_undeclared_type.cpp` | **Create.** Must-not-compile: `Describe<T>` on a type that declares nothing. |
| `test/CMakeLists.txt` | **Modify.** Register the new sources and the two negative cases. |
| `examples/quantities.cpp` | **Create.** Generic-physics example. |
| `examples/CMakeLists.txt` | **Modify.** Register it via `formula_add_example`. |
| `docs/quantities.md` | **Create.** Human-readable page. |
| `mkdocs.yml`, `README.md` | **Modify.** |

## Task Overview

| Task | Deliverable |
|---|---|
| 1 | `FixedString` — a structural string for template parameters |
| 2 | `Quantity` — the CRTP base, with cross-TU identity proven by linking |
| 3 | `Describe<T>` — the single access point, specialisable for foreign types |
| 4 | `Measured<Q>` — the possibly-absent value |
| 5 | Empty propagation, unit conversion, bounds and declared precision |
| 6 | Umbrella, example, documentation, packaging verification |

---

### Task 1: `FixedString` — a structural string for template parameters

**Files:**
- Create: `include/formula-cpp/detail/fixed_string.hpp`
- Create: `test/fixed_string_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `fixed_string_tests.cpp` to `add_executable(formula-cpp-tests ...)`)
- Modify: `CMakeLists.txt` (register `detail/fixed_string.hpp` in `FILE_SET HEADERS`)

**Interfaces produced**, in `namespace formula::detail`:
- `template <std::size_t N> struct FixedString { char characters[N] {}; ... };`
- a deduction guide from a string literal
- `constexpr std::string_view view() const noexcept`
- `constexpr bool operator==(FixedString<A> const&, FixedString<B> const&) noexcept` — heterogeneous, so two different capacities holding the same text compare equal

**Why it is in `detail`:** a caller writes `Quantity<Tag, "V_w", "…", unit::Litre>` and never names this type. It exists so a string literal can be a template argument at all. It is still registered in `FILE_SET HEADERS`, because a public header includes it.

**Why not `Symbol` from `unit.hpp`:** that one is a fixed 16-byte buffer that silently belongs to a `Unit`, sized for unit symbols. A quantity's *description* is a sentence. `FixedString<N>` sizes itself to its literal, so nothing is truncated and nothing is padded.

- [ ] **Step 1: Write the failing test**

Create `test/fixed_string_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/fixed_string.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

using formula::detail::FixedString;

// ---- it carries exactly the text it was given ----

static_assert(FixedString { "V_w" }.view() == std::string_view { "V_w" });
static_assert(FixedString { "V_w" }.view().size() == 3);
static_assert(FixedString { "" }.view().empty());
static_assert(FixedString { "a sentence with spaces" }.view().size() == 22);

// The capacity includes the terminator; the view does not.
static_assert(std::is_same_v<decltype(FixedString { "abc" }), FixedString<4>>);

// ---- the property it exists for: usable as a template argument ----

template <FixedString S>
struct Tagged
{
    static constexpr std::string_view text = S.view();
};

static_assert(Tagged<"V_w">::text == std::string_view { "V_w" });

// Same text means the same type; different text means a different type. This is
// what makes a quantity's symbol part of its identity rather than a field
// somebody can change without the type noticing.
static_assert(std::is_same_v<Tagged<"V_w">, Tagged<"V_w">>);
static_assert(!std::is_same_v<Tagged<"V_w">, Tagged<"V_c">>);

// ---- equality across capacities ----
//
// Two literals of different lengths produce two different FixedString TYPES, so
// a defaulted member operator== would not compare them at all. Text equality has
// to work regardless of capacity, or every comparison silently depends on how
// long the literals happened to be.
static_assert(FixedString { "abc" } == FixedString { "abc" });
static_assert(!(FixedString { "abc" } == FixedString { "abcd" }));
static_assert(!(FixedString { "abc" } == FixedString { "abd" }));

TEST_CASE("a fixed string reports exactly the text it was built from", "[fixed-string]")
{
    constexpr FixedString symbol { "rho" };
    CHECK(symbol.view() == std::string_view { "rho" });
    CHECK(symbol.view().size() == 3);

    // Embedded UTF-8 survives byte for byte; the type counts bytes, not
    // characters, and says so.
    constexpr FixedString degrees { "\xc2\xb0" "C" };
    CHECK(degrees.view().size() == 3);
    CHECK(degrees.view() == std::string_view { "\xc2\xb0" "C" });
}
```

- [ ] **Step 2: Run to verify the RED signal**

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `formula-cpp/detail/fixed_string.hpp` does not exist (`C1083` on cl).

- [ ] **Step 3: Write the implementation**

Create `include/formula-cpp/detail/fixed_string.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A string that can be a non-type template parameter.

#include <cstddef>
#include <string_view>

namespace formula::detail
{

/// A compile-time string, sized to the literal it was built from.
///
/// Exists because a string literal cannot itself be a template argument, and a
/// quantity's symbol and description are part of its type. `N` counts the
/// terminator, so `FixedString { "abc" }` is a `FixedString<4>` whose `view()`
/// is three characters long.
///
/// All members public, deliberately: a class used as a non-type template
/// parameter must be *structural*, which means every non-static data member
/// public, recursively. Do not make `characters` private to "protect" it --
/// that would make this type unusable for its only purpose. The same reasoning,
/// and the same measurement, as `Exponent` in `dimension.hpp`.
///
/// Byte-oriented: `view().size()` is a count of bytes, not of characters, so a
/// multi-byte UTF-8 symbol reports its encoded length.
template <std::size_t N>
struct FixedString
{
    char characters[N] {};

    constexpr FixedString(char const (&text)[N]) noexcept
    {
        for (std::size_t index = 0; index < N; ++index)
            characters[index] = text[index];
    }

    /// The text without its terminator.
    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return std::string_view { characters, N - 1 };
    }
};

template <std::size_t N>
FixedString(char const (&)[N]) -> FixedString<N>;

/// Compares the text, not the capacity.
///
/// Heterogeneous on purpose. `FixedString<4>` and `FixedString<5>` are different
/// types, so a defaulted member `operator==` would not compare them at all and
/// every comparison would silently depend on how long the literals happened to
/// be. Template-argument equivalence does not use this -- it compares members
/// directly -- so this is for callers, not for the language.
template <std::size_t A, std::size_t B>
[[nodiscard]] constexpr bool operator==(FixedString<A> const& lhs, FixedString<B> const& rhs) noexcept
{
    return lhs.view() == rhs.view();
}

} // namespace formula::detail
```

Register it in the top-level `CMakeLists.txt` `FILE_SET HEADERS` list, beside the other `detail/` headers.

- [ ] **Step 4: Run the tests on both compilers**

```
cmake --build --preset cl-debug        && ctest --preset cl-debug --output-on-failure
cmake --build --preset clangcl-debug   && ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS, with matching counts.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/detail/fixed_string.hpp test/fixed_string_tests.cpp test/CMakeLists.txt CMakeLists.txt
git commit -m "feat(quantity): add a structural string for template parameters"
```

---

### Task 2: `Quantity` — the CRTP declaration

**Files:**
- Create: `include/formula-cpp/quantity.hpp`
- Create: `test/quantity_tests.cpp`
- Create: `test/quantity_cross_tu.hpp`
- Create: `test/quantity_cross_tu_b.cpp`
- Modify: `test/CMakeLists.txt`
- Modify: `CMakeLists.txt` (register `quantity.hpp`)

**Interfaces produced**, in `namespace formula`:
- `template <typename Tag, detail::FixedString Symbol, detail::FixedString Description, Unit U> struct Quantity`
  with `static constexpr std::string_view symbol`, `description`; `static constexpr Unit unit`; `static constexpr Dimension dimension`.

**Consumes:** `FixedString` (Task 1), `Unit` and `Dimension` (phase 3).

**Ruling A applies:** four parameters, not five. The dimension is `U.dimension`, never a separate argument.

- [ ] **Step 1: Write the failing tests**

Create `test/quantity_cross_tu.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Two quantity types shared by two translation units.
///
/// A quantity type is only useful if it means the same thing everywhere. That is
/// a statement about NAME MANGLING, and a mangling bug shows at link time, not
/// at compile time -- so the functions below are DEFINED in
/// quantity_cross_tu_b.cpp and CALLED from quantity_tests.cpp. If the two
/// translation units disagreed about what `WaterVolume` is, this would fail to
/// link rather than fail a check.

#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

namespace cross
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", formula::unit::Litre>
{
};

/// Same symbol, same description, same unit -- different tag. The tag is the
/// whole reason these do not collapse into one type.
struct CementVolume: formula::Quantity<CementVolume, "V_w", "volume of water added", formula::unit::Litre>
{
};

/// Defined in quantity_cross_tu_b.cpp.
[[nodiscard]] std::string_view symbol_of_water_volume();
[[nodiscard]] bool water_and_cement_are_distinct();

} // namespace cross
```

Create `test/quantity_cross_tu_b.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include "quantity_cross_tu.hpp"

#include <type_traits>

namespace cross
{

std::string_view symbol_of_water_volume()
{
    return WaterVolume::symbol;
}

bool water_and_cement_are_distinct()
{
    return !std::is_same_v<WaterVolume, CementVolume>
           && !std::is_same_v<formula::Quantity<WaterVolume, "V_w", "volume of water added", formula::unit::Litre>,
                              formula::Quantity<CementVolume, "V_w", "volume of water added", formula::unit::Litre>>;
}

} // namespace cross
```

Create `test/quantity_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include "quantity_cross_tu.hpp"

#include <formula-cpp/quantity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

namespace dim = formula::dim;
namespace unit = formula::unit;

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of the effective mixing water", unit::Litre>
{
};

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

} // namespace

// ---- the metadata is in the type ----

static_assert(WaterVolume::symbol == std::string_view { "V_w" });
static_assert(WaterVolume::description == std::string_view { "volume of the effective mixing water" });
static_assert(WaterVolume::unit == unit::Litre);

// Ruling A: the dimension is DERIVED from the unit, never declared beside it, so
// there is no second place for it to disagree with.
static_assert(WaterVolume::dimension == dim::Volume);
static_assert(SpecimenMass::dimension == dim::Mass);
static_assert(WaterVolume::dimension == unit::Litre.dimension);

// ---- identity ----

static_assert(!std::is_same_v<WaterVolume, SpecimenMass>);

// Two quantities alike in EVERYTHING but their tag are still different types.
// Without this the library would happily let a report label one measurement with
// another's name.
static_assert(!std::is_same_v<cross::WaterVolume, cross::CementVolume>);
static_assert(cross::WaterVolume::symbol == cross::CementVolume::symbol);
static_assert(cross::WaterVolume::unit == cross::CementVolume::unit);

TEST_CASE("a quantity type means the same thing in every translation unit", "[quantity]")
{
    // Defined in quantity_cross_tu_b.cpp. If the two translation units disagreed
    // about what cross::WaterVolume is, this would not link.
    CHECK(cross::symbol_of_water_volume() == std::string_view { "V_w" });
    CHECK(cross::water_and_cement_are_distinct());
}

TEST_CASE("a quantity reports its own metadata", "[quantity]")
{
    CHECK(WaterVolume::symbol == std::string_view { "V_w" });
    CHECK(WaterVolume::description == std::string_view { "volume of the effective mixing water" });
    CHECK(formula::view(WaterVolume::unit.symbolText) == std::string_view { "l" });
    CHECK(WaterVolume::unit.decimals == 1);
    CHECK(SpecimenMass::unit.decimals == 3);
}
```

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — `formula-cpp/quantity.hpp` does not exist.

- [ ] **Step 3: Write the implementation**

Create `include/formula-cpp/quantity.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Quantities: a variable's identity, carrying its own documentation.

#include <formula-cpp/detail/fixed_string.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/unit.hpp>

#include <string_view>

namespace formula
{

/// The base a quantity type derives from, carrying that quantity's metadata.
///
/// Declared like this:
///
///     struct WaterVolume:
///         formula::Quantity<WaterVolume,                    // the type's own name
///                           "V_w",
///                           "volume of the effective mixing water",
///                           formula::unit::Litre>
///     {
///     };
///
/// **The tag is essential, not decorative.** It is the first parameter and it is
/// the type's own name. Without it, two variables whose symbol, description and
/// unit all coincide would be the SAME type, and a report could label one
/// measurement with another's name -- a wrong number with no signal. Measured:
/// with the tag, two otherwise-identical declarations are distinct types and
/// stay distinct across translation units, verified by linking.
///
/// **There is no dimension parameter.** A `Unit` already carries its dimension,
/// so passing both would state it twice and let the two contradict each other.
/// A spike compiled that spelling with `dim::Mass` against `unit::Litre` and all
/// three compilers accepted it in silence. `dimension` below is derived, so the
/// contradiction cannot be written.
template <typename Tag, detail::FixedString Symbol, detail::FixedString Description, Unit U>
struct Quantity
{
    /// The quantity's own type, for anything that needs to name it.
    using QuantityTag = Tag;

    /// How the quantity is written in a formula.
    static constexpr std::string_view symbol = Symbol.view();

    /// What the quantity means, in words, for generated documentation.
    static constexpr std::string_view description = Description.view();

    /// The unit its values are expressed in.
    static constexpr Unit unit = U;

    /// Derived from the unit -- see the note above about why it is not a
    /// parameter of its own.
    static constexpr Dimension dimension = U.dimension;
};

} // namespace formula
```

Register `quantity.hpp` in the top-level `CMakeLists.txt` `FILE_SET HEADERS`, and add `quantity_tests.cpp` and `quantity_cross_tu_b.cpp` to the test executable.

- [ ] **Step 4: Run the tests on both compilers**

Expected: PASS with matching counts.

- [ ] **Step 5: Prove the cross-TU test is a LINK test, not a compile test**

Change `quantity_cross_tu_b.cpp`'s definition of `symbol_of_water_volume` so it is declared for a *different* quantity than the header declares — for instance by defining it inside a `WaterVolume` declared with a different unit. Rebuild.

Expected: a real **unresolved external symbol** (`LNK2019` on the MSVC family), not a type error. Restore, rebuild, confirm green. Quote both in your report.

If your first attempt produces an ordinary type mismatch instead of a link error, that attempt proves nothing — the mangling is what is under test. Break the *defining* side, not the calling side.

- [ ] **Step 6: Commit**

```bash
git add include/formula-cpp/quantity.hpp test/quantity_tests.cpp test/quantity_cross_tu.hpp test/quantity_cross_tu_b.cpp test/CMakeLists.txt CMakeLists.txt
git commit -m "feat(quantity): add the CRTP quantity declaration"
```

---

### Task 3: `Describe<T>` — the single access point

**Files:**
- Modify: `include/formula-cpp/quantity.hpp` (append)
- Modify: `test/quantity_tests.cpp` (append)
- Create: `test/negative/describe_undeclared_type.cpp`
- Create: `test/negative/quantity_wrong_type.cpp`
- Modify: `test/CMakeLists.txt` (register both negative cases)

**Interfaces produced**, in `namespace formula`:
- `template <typename T> struct Describe` — the primary template, deliberately EMPTY (see Ruling H)
- a constrained partial specialisation for anything deriving from `Quantity`
- `template <typename T> concept Described`
- `template <typename T> struct RequireDescribed` — the named helper that refuses an undeclared type in our own words
- in `namespace formula::detail`: `quantity_base_of`, `DeclaresQuantity`

**Why one access point:** nothing above this layer may know how metadata was declared. A caller writes `Describe<T>::symbol`, and whether `T` is a quantity of ours or a `double` somebody specialised is not its business. That is what lets a foreign type — a `double`, a type from a vendor SDK — be used in a formula without owning its source.

**Ruling C applies:** the base-detection idiom below was measured on cl and compiled first try.

**Ruling E applies to the negative tests:** assert on *our* message, never on how a compiler renders a `FixedString` template argument. cl prints one as a byte list (`char88,0`) where clang prints `{"X"}`.

- [ ] **Step 1: Write the failing tests**

Append to `test/quantity_tests.cpp`:

```cpp
// ---- Describe: one way in, for our types and foreign ones alike ----

using formula::Describe;

static_assert(Describe<WaterVolume>::symbol == std::string_view { "V_w" });
static_assert(Describe<WaterVolume>::description
              == std::string_view { "volume of the effective mixing water" });
static_assert(Describe<WaterVolume>::unit == unit::Litre);
static_assert(Describe<WaterVolume>::dimension == dim::Volume);

// Reading through Describe must agree with reading the type directly. If these
// ever diverge, every consumer above this layer is reading something else.
static_assert(Describe<WaterVolume>::symbol == WaterVolume::symbol);
static_assert(Describe<SpecimenMass>::unit == SpecimenMass::unit);

static_assert(formula::Described<WaterVolume>);
static_assert(!formula::Described<int>);
```

and, still in `test/quantity_tests.cpp`, a specialisation for a type nobody owns:

```cpp
// A foreign type, standing in for a `double` or a vendor SDK type: no base, no
// cooperation, not ours to change. Specialising Describe is the sanctioned way
// to bring it into a formula.
struct ForeignTemperature
{
    double celsius {};
};

template <>
struct formula::Describe<ForeignTemperature>
{
    static constexpr std::string_view symbol = "theta";
    static constexpr std::string_view description = "a temperature from somebody else's library";
    static constexpr Unit unit = formula::unit::Celsius;
    static constexpr Dimension dimension = formula::unit::Celsius.dimension;
};

static_assert(formula::Described<ForeignTemperature>);
static_assert(Describe<ForeignTemperature>::symbol == std::string_view { "theta" });
static_assert(Describe<ForeignTemperature>::dimension == dim::Temperature);

TEST_CASE("metadata reads the same through Describe as off the type", "[quantity]")
{
    CHECK(Describe<WaterVolume>::symbol == WaterVolume::symbol);
    CHECK(Describe<WaterVolume>::description == WaterVolume::description);
    CHECK(Describe<WaterVolume>::unit == WaterVolume::unit);
    CHECK(Describe<WaterVolume>::dimension == WaterVolume::dimension);
}

TEST_CASE("a foreign type joins on the same terms as ours", "[quantity]")
{
    // The point: nothing here knows that one of these was declared with the CRTP
    // base and the other by specialisation.
    CHECK(Describe<ForeignTemperature>::symbol == std::string_view { "theta" });
    CHECK(formula::view(Describe<ForeignTemperature>::unit.symbolText)
          == formula::view(unit::Celsius.symbolText));
    CHECK(Describe<ForeignTemperature>::dimension == Describe<ForeignTemperature>::unit.dimension);
}
```

Create `test/negative/describe_undeclared_type.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// A type that declares no metadata must be refused in the library's own words,
// not accepted with empty strings and not buried in template noise.
// This must not compile.
#include <formula-cpp/quantity.hpp>

struct PlainStruct
{
    int value {};
};

int main()
{
    // RequireDescribed, not Describe<PlainStruct>::symbol: the primary Describe
    // is deliberately empty, so that spelling would give the compiler's own "no
    // member" error rather than ours. `::value` is required -- the assertion is
    // in the class body and a bare alias would instantiate nothing.
    return formula::RequireDescribed<PlainStruct>::value ? 1 : 0;
}
```

Create `test/negative/quantity_wrong_type.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Two quantities alike in symbol, description and unit are still different
// types, so one must not be usable where the other is expected. That is the
// whole reason for the CRTP tag. This must not compile.
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V", "a volume", formula::unit::Litre>
{
};

struct CementVolume: formula::Quantity<CementVolume, "V", "a volume", formula::unit::Litre>
{
};

void takes_water(WaterVolume);

int main()
{
    takes_water(CementVolume {});
    return 0;
}
```

Register both in `test/CMakeLists.txt`:

```cmake
formula_add_negative_test(describe_undeclared_type
    "formula: this type does not declare any quantity metadata")
formula_add_negative_test(quantity_wrong_type "CementVolume")
```

Note the second expects a *type name*, not a library message, because the compiler's own conversion error is the diagnostic here and all three name the offending type. Prove it discriminates anyway — see Step 5.

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — no `formula::Describe`, no `formula::Described`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/quantity.hpp`:

```cpp
namespace detail
{
    /// Never defined, never called. Its only job is to let template argument
    /// deduction find a `Quantity` base through the derived-to-base conversion,
    /// which is how `Describe` learns a type's metadata without the type having
    /// to repeat it. Measured on cl, clang-cl and clang++.
    template <typename Tag, FixedString Symbol, FixedString Description, Unit U>
    auto quantity_base_of(Quantity<Tag, Symbol, Description, U> const&)
        -> Quantity<Tag, Symbol, Description, U>;

    template <typename T>
    concept DeclaresQuantity = requires(T const& value) { quantity_base_of(value); };
} // namespace detail

/// The one place anything reads a quantity's metadata.
///
/// Nothing above this layer knows how the metadata was declared. A type of ours
/// gets it from its `Quantity` base; a type nobody owns -- a `double`, something
/// from a vendor SDK -- gets it from an explicit specialisation of this
/// template. Both are read the same way, which is what lets a foreign type be
/// used in a formula without owning its source.
///
/// Specialise it like this:
///
///     template <>
///     struct formula::Describe<TheirType>
///     {
///         static constexpr std::string_view symbol = "theta";
///         static constexpr std::string_view description = "...";
///         static constexpr Unit unit = formula::unit::Celsius;
///         static constexpr Dimension dimension = formula::unit::Celsius.dimension;
///     };
/// **Empty on purpose.** It would be nicer for this to `static_assert` with a
/// helpful message, and that was the first design; it is wrong. A
/// `static_assert` failure is not in the immediate context, so it is a hard
/// error rather than a substitution failure -- which means the `Described`
/// concept below, whose whole job is to answer "is this type described?",
/// would fail to COMPILE for every type that is not, instead of answering no.
/// Measured on clang: `static_assert(!Described<int>)` did not compile.
///
/// So the primary stays empty, `Described` works by member detection, and the
/// helpful diagnostic lives in `RequireDescribed` below, where it can be asked
/// for deliberately.
template <typename T>
struct Describe
{
};

template <detail::DeclaresQuantity T>
struct Describe<T>
{
  private:
    using Base = decltype(detail::quantity_base_of(std::declval<T const&>()));

  public:
    static constexpr std::string_view symbol = Base::symbol;
    static constexpr std::string_view description = Base::description;
    static constexpr Unit unit = Base::unit;
    static constexpr Dimension dimension = Base::dimension;
};

/// True when `T` has metadata, however it was declared.
///
/// Answers rather than explodes for a type that has none -- see the note on the
/// primary template above for why that took a redesign.
template <typename T>
concept Described = requires {
    { Describe<T>::symbol } -> std::convertible_to<std::string_view>;
    { Describe<T>::unit } -> std::convertible_to<Unit>;
};

/// Fails to compile, in our own words, when `T` declares no metadata.
///
/// The counterpart to `Describe` being silent: somewhere has to say what to do
/// about it, and a bare "no member named 'symbol'" does not. Same shape as
/// `RequireSameDimension` in `dimension.hpp`, and the same caveat applies --
/// the assertion is in the class body, so it fires when the type is COMPLETED.
/// Write `RequireDescribed<T>::value`; a bare alias instantiates nothing and
/// checks nothing.
template <typename T>
struct RequireDescribed
{
    static_assert(Described<T>,
                  "formula: this type does not declare any quantity metadata. Either derive it "
                  "from formula::Quantity<TheType, symbol, description, unit>, or -- if it is a "
                  "type you do not own -- specialise formula::Describe for it");

    static constexpr bool value = true;
};
```

Add `#include <concepts>` and `#include <utility>` to the header's include block.

- [ ] **Step 4: Run the tests on both compilers**

Expected: PASS, including both negative cases.

- [ ] **Step 5: Prove both negative tests discriminate**

For **each** of the two, run it once with a deliberately wrong expected substring (must FAIL) and once with the correct one (must PASS), on **both** `cl-debug` and `clangcl-debug`. Quote all eight outcomes.

Measured facts so you do not have to rediscover them: all three compilers echo a `static_assert` message verbatim (MSVC wraps it as `error C2338: static assertion failed: '<message>'`). But cl renders a `FixedString` template argument as a raw byte list, so never put an expected substring inside one — assert on our own wording, or on a plain type name as `quantity_wrong_type` does.

- [ ] **Step 6: Record the diagnostics**

Paste the full diagnostic each compiler gives for both negative cases into your report. Readability is a deliverable here: somebody who forgets to declare metadata should be able to tell what to do from the first line.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/quantity.hpp test/quantity_tests.cpp test/negative/describe_undeclared_type.cpp test/negative/quantity_wrong_type.cpp test/CMakeLists.txt
git commit -m "feat(quantity): add Describe, the single metadata access point"
```

---

### Task 4: `Measured<Q>` — a value that may be absent

**Files:**
- Create: `include/formula-cpp/measured.hpp`
- Create: `test/measured_tests.cpp`
- Modify: `test/CMakeLists.txt`
- Modify: `CMakeLists.txt` (register `measured.hpp`)

**Interfaces produced**, in `namespace formula`:
- `template <Described Q> class Measured`
  - `constexpr Measured() noexcept` — not measured
  - `constexpr explicit Measured(Rational value) noexcept`
  - `static constexpr Measured absent() noexcept`
  - `[[nodiscard]] constexpr bool has_value() const noexcept`
  - `[[nodiscard]] constexpr bool is_absent() const noexcept`
  - `[[nodiscard]] constexpr std::optional<Rational> const& stored() const noexcept`
  - `[[nodiscard]] constexpr Rational value() const` — throws `ArithmeticException{DomainError}` when absent
  - `[[nodiscard]] constexpr Rational value_or(Rational fallback) const noexcept`
  - `[[nodiscard]] constexpr bool operator==(Measured const&) const noexcept = default`

**Ruling D applies:** the payload is `std::optional<Rational>`, which is fully usable in constant expressions and which cannot be a non-type template parameter. Neither fact constrains `Q`, which stays structural. Do not attempt to make `Measured` an NTTP.

**Ruling F applies:** the value is in `Q`'s declared unit, always. `Measured` carries no unit of its own.

**Why a default-constructed `Measured` is absent rather than zero:** zero is a measurement. "Not measured" is not a number at all, and a library that starts an unmeasured quantity at zero produces a confident wrong answer — the failure this whole layer exists to prevent.

- [ ] **Step 1: Write the failing test**

Create `test/measured_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace unit = formula::unit;

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Measured;
using formula::Rational;

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", unit::Litre>
{
};

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", unit::Kilogram>
{
};

consteval Measured<WaterVolume> measured(std::int64_t numerator, std::int64_t denominator)
{
    return Measured<WaterVolume> { *Rational::make(numerator, denominator) };
}

} // namespace

// ---- absent is the default, and it is not zero ----

static_assert(Measured<WaterVolume> {}.is_absent());
static_assert(!Measured<WaterVolume> {}.has_value());
static_assert(Measured<WaterVolume>::absent().is_absent());

// Zero is a measurement; absent is not a number. Collapsing the two is how an
// unmeasured quantity comes to look like a real reading of nothing.
static_assert(!measured(0, 1).is_absent());
static_assert(measured(0, 1) != Measured<WaterVolume> {});

// ---- a present value is exactly what was put in ----

static_assert(measured(9, 2).value() == *Rational::make(9, 2));
static_assert(measured(9, 2).has_value());
static_assert(Measured<WaterVolume> {}.value_or(*Rational::make(7, 1)) == *Rational::make(7, 1));
static_assert(measured(9, 2).value_or(*Rational::make(7, 1)) == *Rational::make(9, 2));

// ---- equality ----

static_assert(measured(9, 2) == measured(9, 2));
static_assert(measured(9, 2) != measured(9, 3));
static_assert(Measured<WaterVolume> {} == Measured<WaterVolume> {});

// ---- two quantities are two types, even with the same payload ----

static_assert(!std::is_same_v<Measured<WaterVolume>, Measured<SpecimenMass>>);

TEST_CASE("an absent measurement refuses to invent a number", "[measured]")
{
    Measured<WaterVolume> const notMeasured {};
    REQUIRE(notMeasured.is_absent());
    CHECK_THROWS_AS(notMeasured.value(), ArithmeticException);

    // value_or is the sanctioned way to get a number out of one, because the
    // caller has to say what an absent reading should count as.
    CHECK(notMeasured.value_or(Rational { 0 }) == Rational { 0 });
}

TEST_CASE("a measurement carries its quantity's own metadata", "[measured]")
{
    // No unit of its own: the unit is part of the quantity type. See Ruling F.
    CHECK(Measured<WaterVolume>::quantity_unit() == unit::Litre);
    CHECK(Measured<WaterVolume>::quantity_symbol() == std::string_view { "V_w" });
    CHECK(Measured<SpecimenMass>::quantity_unit() == unit::Kilogram);
}
```

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — `formula-cpp/measured.hpp` does not exist.

- [ ] **Step 3: Write the implementation**

Create `include/formula-cpp/measured.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A value of a quantity, which may honestly be absent.

#include <formula-cpp/error.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>

#include <optional>
#include <string_view>

namespace formula
{

/// A value of quantity `Q`, in `Q`'s declared unit, which may not have been
/// measured at all.
///
/// A default-constructed `Measured` is ABSENT, not zero. Zero is a measurement;
/// "not entered" is not a number, and a library that starts an unmeasured
/// quantity at zero produces a confident wrong answer -- which is the failure
/// this layer exists to prevent.
///
/// It carries no unit of its own: the unit is part of `Q`. Converting therefore
/// produces a value of a different quantity, or a bare `Rational`, and never a
/// silently re-scaled `Measured<Q>`.
///
/// Unlike `Unit` and `Dimension`, this is NOT a structural type and is not meant
/// to be a template argument -- `std::optional` is not structural in any of the
/// standard libraries we support. That costs nothing: the quantity TYPE is the
/// compile-time thing, and this is the runtime value. Measured on all three
/// compilers; the two are independent.
template <Described Q>
class Measured
{
  public:
    /// Not measured.
    constexpr Measured() noexcept = default;

    constexpr explicit Measured(Rational value) noexcept: _value { value } {}

    [[nodiscard]] static constexpr Measured absent() noexcept { return Measured {}; }

    [[nodiscard]] constexpr bool has_value() const noexcept { return _value.has_value(); }
    [[nodiscard]] constexpr bool is_absent() const noexcept { return !_value.has_value(); }

    /// The stored payload, for code that wants to branch on it directly.
    [[nodiscard]] constexpr std::optional<Rational> const& stored() const noexcept { return _value; }

    /// @throws ArithmeticException with `DomainError` when absent. There is no
    ///         number to return, and returning zero would be a lie.
    [[nodiscard]] constexpr Rational value() const
    {
        if (!_value.has_value())
            throw ArithmeticException { ArithmeticError::DomainError };
        return *_value;
    }

    /// The caller states what an absent reading counts as. Deliberately explicit:
    /// there is no default answer the library could give that is right for every
    /// caller.
    [[nodiscard]] constexpr Rational value_or(Rational fallback) const noexcept
    {
        return _value.value_or(fallback);
    }

    [[nodiscard]] constexpr bool operator==(Measured const&) const noexcept = default;

    /// Convenience readers for the quantity's own metadata, so a caller holding
    /// a value does not have to name `Describe<Q>` to label it.
    [[nodiscard]] static constexpr Unit quantity_unit() noexcept { return Describe<Q>::unit; }
    [[nodiscard]] static constexpr std::string_view quantity_symbol() noexcept { return Describe<Q>::symbol; }
    [[nodiscard]] static constexpr std::string_view quantity_description() noexcept
    {
        return Describe<Q>::description;
    }
    [[nodiscard]] static constexpr Dimension quantity_dimension() noexcept { return Describe<Q>::dimension; }

  private:
    std::optional<Rational> _value {};
};

} // namespace formula
```

Register `measured.hpp` in `FILE_SET HEADERS` and `measured_tests.cpp` in the test executable.

- [ ] **Step 4: Run the tests on both compilers**

Expected: PASS with matching counts.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/measured.hpp test/measured_tests.cpp test/CMakeLists.txt CMakeLists.txt
git commit -m "feat(measured): add a quantity value that may be absent"
```

---

### Task 5: Propagation, conversion, bounds and declared precision

**Files:**
- Modify: `include/formula-cpp/unit.hpp` (add one `BoundsCheck` value and its `describe()` case)
- Modify: `include/formula-cpp/measured.hpp` (append)
- Modify: `test/unit_tests.cpp` (append — the new enum value)
- Modify: `test/measured_tests.cpp` (append)

**Interfaces produced**, in `namespace formula`:
- `BoundsCheck::NotMeasured` — a new enumerator
- `template <Described Q, typename F> constexpr auto transform(Measured<Q>, F) -> Measured<Q>`
- `template <Described Q, Described R, typename F> constexpr Measured<R> combine(Measured<Q>, Measured<R>, F)`
- `template <Described R, Described Q> constexpr std::expected<Measured<R>, ArithmeticError> checked_convert_to(Measured<Q>)`
- `template <Described Q> constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(Measured<Q>)`
- `template <Described Q> constexpr std::expected<Measured<Q>, ArithmeticError> checked_round_to_declared(Measured<Q>, RoundingMode)`

**The rule this task exists for:** absence propagates; it never becomes a number and never becomes an error. An absent input gives an absent output, every time, and that is asserted for every operation here rather than for one of them.

**Why `BoundsCheck` gains a value rather than the function returning an optional:** "the unit declares no bounds" and "there was no value to check" are different facts, and a report that shows them as the same one is exactly the collapse `NotChecked` was introduced to prevent in phase 3. Adding the enumerator keeps one honest vocabulary instead of two overlapping ones.

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_tests.cpp`:

```cpp
// A fourth outcome, added with Measured: a value that was never taken is not the
// same fact as a unit that declares no range.
static_assert(!formula::describe(formula::BoundsCheck::NotMeasured).empty());
static_assert(formula::describe(formula::BoundsCheck::NotMeasured)
              != formula::describe(formula::BoundsCheck::NotChecked));

TEST_CASE("every bounds outcome still has its own distinct wording", "[unit]")
{
    BoundsCheck const all[] = { BoundsCheck::WithinBounds,
                                BoundsCheck::BelowMinimum,
                                BoundsCheck::AboveMaximum,
                                BoundsCheck::NotChecked,
                                BoundsCheck::NotMeasured };
    for (std::size_t i = 0; i < std::size(all); ++i)
    {
        CHECK_FALSE(formula::describe(all[i]).empty());
        for (std::size_t j = i + 1; j < std::size(all); ++j)
            CHECK(formula::describe(all[i]) != formula::describe(all[j]));
    }
}
```

Append to `test/measured_tests.cpp`:

```cpp
// ---- absence propagates through everything ----

namespace
{

struct VolumeInCubicMetres:
    formula::Quantity<VolumeInCubicMetres, "V", "volume of water added", unit::CubicMetre>
{
};

constexpr auto doubled = [](Rational value) { return value + value; };

} // namespace

static_assert(formula::transform(measured(3, 1), doubled).value() == *Rational::make(6, 1));
static_assert(formula::transform(Measured<WaterVolume> {}, doubled).is_absent());

// Both present, one absent, the other absent, both absent -- every combination,
// because propagation that works in three cases out of four is not propagation.
static_assert(formula::combine(measured(3, 1), Measured<SpecimenMass> { *Rational::make(2, 1) },
                               [](Rational a, Rational b) { return a * b; })
                  .value()
              == *Rational::make(6, 1));
static_assert(formula::combine(Measured<WaterVolume> {}, Measured<SpecimenMass> { *Rational::make(2, 1) },
                               [](Rational a, Rational b) { return a * b; })
                  .is_absent());
static_assert(formula::combine(measured(3, 1), Measured<SpecimenMass> {},
                               [](Rational a, Rational b) { return a * b; })
                  .is_absent());
static_assert(formula::combine(Measured<WaterVolume> {}, Measured<SpecimenMass> {},
                               [](Rational a, Rational b) { return a * b; })
                  .is_absent());

TEST_CASE("absence survives a conversion instead of becoming a number", "[measured]")
{
    // 450 litres is exactly 9/20 of a cubic metre -- the same exact conversion
    // phase 3 proved, now carrying a quantity's identity with it.
    auto const present = formula::checked_convert_to<VolumeInCubicMetres>(measured(450, 1));
    REQUIRE(present.has_value());
    REQUIRE(present->has_value());
    CHECK(present->value() == *Rational::make(9, 20));

    auto const absent = formula::checked_convert_to<VolumeInCubicMetres>(Measured<WaterVolume> {});
    REQUIRE(absent.has_value());
    CHECK(absent->is_absent());
}

TEST_CASE("converting to a quantity of another dimension is refused", "[measured]")
{
    auto const wrong = formula::checked_convert_to<SpecimenMass>(measured(450, 1));
    REQUIRE_FALSE(wrong.has_value());
    CHECK(wrong.error() == ArithmeticError::DomainError);

    // And it is refused for an ABSENT value too. A conversion nobody could
    // perform must not look like it succeeded merely because there was no number
    // to get wrong.
    auto const wrongAndAbsent = formula::checked_convert_to<SpecimenMass>(Measured<WaterVolume> {});
    REQUIRE_FALSE(wrongAndAbsent.has_value());
    CHECK(wrongAndAbsent.error() == ArithmeticError::DomainError);
}

TEST_CASE("an unmeasured value is not judged against bounds", "[measured]")
{
    auto const absent = formula::checked_within_bounds(Measured<WaterVolume> {});
    REQUIRE(absent.has_value());
    CHECK(*absent == formula::BoundsCheck::NotMeasured);

    // Litre declares no bounds, which is a DIFFERENT fact from having no value.
    auto const present = formula::checked_within_bounds(measured(1000000, 1));
    REQUIRE(present.has_value());
    CHECK(*present == formula::BoundsCheck::NotChecked);
}

TEST_CASE("rounding to declared precision leaves an absent value absent", "[measured]")
{
    // Litre declares one decimal place.
    auto const rounded =
        formula::checked_round_to_declared(measured(123456, 1000), formula::RoundingMode::HalfAwayFromZero);
    REQUIRE(rounded.has_value());
    REQUIRE(rounded->has_value());
    CHECK(rounded->value() == *Rational::from_decimal(1235, -1));

    auto const stillAbsent =
        formula::checked_round_to_declared(Measured<WaterVolume> {}, formula::RoundingMode::HalfAwayFromZero);
    REQUIRE(stillAbsent.has_value());
    CHECK(stillAbsent->is_absent());
}
```

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — no `formula::transform`, no `formula::combine`, no `BoundsCheck::NotMeasured`.

- [ ] **Step 3: Correct a naming inconsistency phase 3 shipped**

`unit.hpp:387` declares

```cpp
constexpr std::expected<Rational, ArithmeticError> round_to_declared(Rational, Unit, RoundingMode) noexcept;
```

It returns a `std::expected` but is not called `checked_`. Everywhere else in this library that
prefix is the signal: `checked_add` returns an expected, `operator+` throws; `checked_convert`
returns an expected, `convert` throws; `checked_within_bounds` returns an expected. One function
breaking that pattern makes the pattern unreliable, and a caller cannot tell from a name whether
it must handle an error or catch one.

**Ruling G — rename it, and give it the throwing sibling the convention implies.** The library is
pre-1.0 and unreleased, so this costs nothing now and would be a breaking change later.

- Rename the existing function to `checked_round_to_declared`.
- Add `constexpr Rational round_to_declared(Rational, Unit, RoundingMode)` that throws, written as
  `detail::or_throw(checked_round_to_declared(...))`, exactly as `convert` is written against
  `checked_convert`.
- Update the existing call sites and tests in `test/unit_tests.cpp`.
- Add one test that the throwing form throws where the checked form returns an error, so the pair
  is pinned the way `convert`/`checked_convert` is.

Do this as its own commit, before the rest of Task 5, so the rename is separable from the new
behaviour:

```bash
git add include/formula-cpp/unit.hpp test/unit_tests.cpp
git commit -m "refactor(unit): name round_to_declared by the convention it follows"
```

- [ ] **Step 4: Write the implementation**

In `include/formula-cpp/unit.hpp`, add the enumerator **last**, so no existing value's meaning changes, and add its `describe()` case:

```cpp
    /// There was no value to check. Distinct from NotChecked, which says the
    /// unit declares no range: a measurement nobody took and a range nobody
    /// declared are different facts, and a report that shows them as one is the
    /// collapse NotChecked exists to prevent.
    NotMeasured,
```

```cpp
        case BoundsCheck::NotMeasured: return "no value was measured";
```

Append to `include/formula-cpp/measured.hpp`:

```cpp
/// Applies @p function to a present value; leaves an absent one absent.
///
/// This is the whole propagation rule in one place. Everything below is written
/// in terms of it or repeats it exactly.
template <Described Q, typename F>
[[nodiscard]] constexpr Measured<Q> transform(Measured<Q> value, F function)
{
    if (value.is_absent())
        return Measured<Q> {};
    return Measured<Q> { function(value.value()) };
}

/// Combines two measurements. Absent if EITHER is absent.
///
/// Not "absent if both": a formula with one missing input has no answer, and
/// producing one from the inputs that happen to be present is precisely the
/// wrong number this layer exists to prevent.
template <Described Q, Described R, typename F>
[[nodiscard]] constexpr Measured<R> combine(Measured<Q> lhs, Measured<R> rhs, F function)
{
    if (lhs.is_absent() || rhs.is_absent())
        return Measured<R> {};
    return Measured<R> { function(lhs.value(), rhs.value()) };
}

/// Converts a measurement of `Q` into one of `R`, exactly.
///
/// The dimensions are checked even when the value is absent: a conversion nobody
/// could perform must not look like it succeeded merely because there was no
/// number to get wrong.
template <Described R, Described Q>
[[nodiscard]] constexpr std::expected<Measured<R>, ArithmeticError> checked_convert_to(Measured<Q> value) noexcept
{
    if (!(Describe<Q>::dimension == Describe<R>::dimension))
        return std::unexpected { ArithmeticError::DomainError };
    if (value.is_absent())
        return Measured<R> {};

    std::expected<Rational, ArithmeticError> const converted =
        checked_convert(value.value(), Describe<Q>::unit, Describe<R>::unit);
    if (!converted)
        return std::unexpected { converted.error() };
    return Measured<R> { *converted };
}

/// Checks a measurement against its quantity's unit's declared bounds.
template <Described Q>
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(
    Measured<Q> value) noexcept
{
    if (value.is_absent())
        return BoundsCheck::NotMeasured;
    return checked_within_bounds(value.value(), Describe<Q>::unit);
}

/// Rounds a measurement to the precision its quantity's unit declares.
template <Described Q>
[[nodiscard]] constexpr std::expected<Measured<Q>, ArithmeticError> checked_round_to_declared(
    Measured<Q> value, RoundingMode mode) noexcept
{
    if (value.is_absent())
        return Measured<Q> {};

    std::expected<Rational, ArithmeticError> const rounded =
        round_to_declared(value.value(), Describe<Q>::unit, mode);
    if (!rounded)
        return std::unexpected { rounded.error() };
    return Measured<Q> { *rounded };
}
```

Add `#include <formula-cpp/rounding.hpp>` and `#include <expected>` to `measured.hpp`'s include block.

- [ ] **Step 5: Run the tests on both compilers**

Expected: PASS with matching counts.

- [ ] **Step 6: Prove the propagation tests are not vacuous**

Break `transform` so it returns `Measured<Q> { Rational { 0 } }` for an absent input instead of an absent value, rebuild, and confirm the suite goes **red**. Restore and confirm green. Do the same for `combine`'s either-absent rule by changing `||` to `&&`. Quote both cycles.

A zero returned for an absent input is exactly the bug this task prevents, so a test suite that stays green under that change is not testing it.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/unit.hpp include/formula-cpp/measured.hpp test/unit_tests.cpp test/measured_tests.cpp
git commit -m "feat(measured): propagate absence through conversion, bounds and rounding"
```

---

### Task 6: Umbrella, example, documentation, packaging

**Files:**
- Modify: `include/formula-cpp/formula.hpp`
- Create: `examples/quantities.cpp`
- Modify: `examples/CMakeLists.txt`
- Create: `docs/quantities.md`
- Modify: `mkdocs.yml`, `README.md`

- [ ] **Step 1: Add the public headers to the umbrella**

Add `#include <formula-cpp/quantity.hpp>` and `#include <formula-cpp/measured.hpp>`, keeping the list alphabetical. `detail/fixed_string.hpp` does **not** go in the umbrella — it arrives through `quantity.hpp`, and a caller never names it.

- [ ] **Step 2: Write the example**

Create `examples/quantities.cpp`, registered with `formula_add_example(quantities quantities.cpp "<a line that only appears when the numbers are right>")`. It must include the umbrella `<formula-cpp/formula.hpp>`, not the individual headers.

Cover, in this order, printing the computed values:

1. Declaring two quantities and reading their metadata through `Describe`.
2. Two quantities alike in symbol, description and unit but distinct in type — printed so the reader can see the tag is what separates them.
3. A foreign type joining by `Describe` specialisation.
4. A present measurement converted exactly between quantities (450 l to m³).
5. An absent measurement surviving conversion, rounding and a bounds check without becoming a number.
6. `combine` of a present and an absent measurement, showing the result is absent.

Follow phase 3's structure: compute a `bool allChecksPassed` from every claim the output makes, print it, and return non-zero when false. **Every printed number must be covered by one of those checks** — a value that is printed but not checked is a documentation claim nothing protects, and phase 3 shipped exactly that until a reviewer changed a rounding mode and the suite stayed green.

- [ ] **Step 3: Run the example and paste its real output into the report**

Do not write the expected output from reasoning; run it and record what it prints.

- [ ] **Step 4: Write the documentation page**

Create `docs/quantities.md`. Required sections:

1. **Why a variable is a type** — the metadata travels with the value, and a dimensional mistake is a compile error where the formula is written.
2. **Declaring a quantity** — the four-parameter form, and why the tag is first and essential. State plainly that there is no dimension parameter and why: a `Unit` already carries one, and the spelling that passes both compiles a `dim::Mass`-with-`unit::Litre` contradiction in silence on all three compilers.
3. **`Describe<T>`, and foreign types** — the single access point, with the specialisation shown.
4. **Measurements that may be absent** — absent is not zero; `value()` throws, `value_or` makes the caller decide; the propagation rule, including that `combine` is absent if *either* input is.
5. **Bounds, precision and conversion** — and why `NotMeasured` and `NotChecked` are different answers.
6. **Limits** — `FixedString` counts bytes, not characters; `Measured` is deliberately not a structural type and cannot be a template argument, while the quantity type is and can; a link to `docs/dimensions.md` and `docs/numbers.md` rather than restating their limits.

**Every numeric claim on this page must come from running code.** Compile and run the snippet, then paste what it printed. Phase 2 shipped six wrong claims in one prose section by writing prose from reasoning, and phase 3 shipped two more — one of them a sentence saying an invalid exponent "is a compile error" when that holds only in constant evaluation. If you catch yourself writing a number you worked out, stop and run something.

- [ ] **Step 5: Add the page to `mkdocs.yml` nav and one accurate line to `README.md`**

The README may now claim quantities and metadata alongside dimensions and units. It must **not** claim expression trees, traceability, provenance or documentation generation — those are phases 5 to 7 and do not exist.

- [ ] **Step 6: Run the full four-preset matrix**

```
cmake --build --preset cl-debug        && ctest --preset cl-debug --output-on-failure
cmake --build --preset cl-release      && ctest --preset cl-release --output-on-failure
cmake --build --preset clangcl-debug   && ctest --preset clangcl-debug --output-on-failure
cmake --build --preset clangcl-release && ctest --preset clangcl-release --output-on-failure
```

Pass counts must be identical across all four.

- [ ] **Step 7: Verify the installed package**

Configure with `-DFORMULA_BUILD_TESTS=OFF -DFORMULA_INSTALL=ON`, install to a staging directory, confirm **all thirteen** headers land there, and compile a consumer that includes only `<formula-cpp/formula.hpp>` against `stage/include` — with no path into the repository's own `include/`. That last condition is what catches a header that builds in-tree but is missing from the install manifest; this repository has shipped that defect once.

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/formula.hpp examples/quantities.cpp examples/CMakeLists.txt docs/quantities.md mkdocs.yml README.md
git commit -m "docs(quantity): document and demonstrate quantities and measurements"
```

---

## Self-Review

**1. Spec coverage.**

| Spec requirement | Task |
|---|---|
| §8 a quantity type is the identity of a variable | 2 |
| §8 the CRTP tag is essential; two coinciding declarations must not collapse | 2 (cross-TU, proven by linking) |
| §8 symbol, description, unit carried by the type | 2 |
| §8 all metadata read through a single `Describe<T>` access point | 3 |
| §8 `Describe` specialisable for foreign types you do not own | 3 |
| §9 a quantity may be "not entered / not measured" | 4 |
| §9 emptiness propagates rather than producing a wrong number | 5 |
| §17 phase 4 row | 1–6 |

Deliberately **not** here: the `FORMULA_QUANTITY(...)` macro spelling §8 mentions. The project's standing requirement is traceability without macros, and a convenience macro for declaration would be the only macro in the library, earning its keep only by saving one line. If it is wanted later it is additive and costs nothing to add then. Recorded so the omission is visible rather than accidental.

Also not here: the evaluation sum type (`value | verdict | invalid | empty`). That is §9 and phase 5; this plan delivers the `empty` case's representation, which phase 5 will fold into the sum.

**2. Placeholder scan.** No "TBD", no "add error handling", no "similar to Task N". Every code step carries its code. Task 6's example and documentation are specified by required content rather than verbatim text, deliberately: their numbers must come from running the code, and a plan that dictated the output would invite transcription instead of measurement.

**3. Type consistency.**
- `FixedString` is defined once (Task 1) and used by `Quantity` (Task 2) only; nothing else names it.
- `Quantity` takes four parameters everywhere in this plan. The five-parameter form appears nowhere — Ruling A, and the spec was amended to match rather than left to contradict it.
- `Describe<T>` exposes exactly `symbol`, `description`, `unit`, `dimension`, and the same four are readable off the `Quantity` base. Task 3 asserts the two agree.
- `Measured<Q>` is constrained on `Described Q`, so a quantity that declares nothing cannot have a value — the diagnostic arrives at the declaration, not deep inside an operation.
- `checked_convert_to<R>(Measured<Q>)` puts the destination first so it can be written `checked_convert_to<CubicMetres>(v)` with `Q` deduced.
- `checked_within_bounds` is overloaded on `Measured<Q>` alongside phase 3's `(Rational, Unit)`. Distinct parameter types, so no ambiguity; Task 5's implementation calls the phase 3 one.
- `BoundsCheck` gains `NotMeasured` **last**, so no existing enumerator's value changes.

**4. Ordering hazards.** `FixedString` must exist before `Quantity` names it; `Quantity` and `Describe` before `Measured` constrains on `Described`; phase 3's `checked_convert`, `checked_within_bounds` and `round_to_declared` all exist already. Task order respects each. `measured.hpp` includes `quantity.hpp`, which includes `unit.hpp` and `detail/fixed_string.hpp`; no cycle.

**5. The thing most likely to go wrong.** Task 3's `Describe` primary template refuses an undeclared type via `static_assert`, which fires only when the template is **instantiated** — the same completion trap documented on `RequireSameDimension`. A caller who merely names `Describe<T>` without touching a member may get no error. Task 3's negative test uses `::symbol`, which does instantiate. If the reviewer finds a spelling that stays silent, that is a real finding and the header must document it the way `dimension.hpp` documents its own.
