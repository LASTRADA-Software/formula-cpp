# formula-cpp Foundation and Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn a repository holding one prototype header into a vcpkg-consumable, header-only C++23 library that builds and tests green on MSVC `cl`, `clang-cl` and `clang++`.

**Architecture:** A CMake `INTERFACE` library exporting `formula-cpp::formula-cpp`, with headers installed through `FILE_SET HEADERS` and a generated package config that mentions no test or dependency machinery. Tests are Catch2 (fetched by a hash-pinned CPM bootstrap) in three flavours: runtime assertions, `STATIC_REQUIRE` compile-time assertions, and a must-not-compile harness that asserts both that a build failed *and* that it failed with the library's own `static_assert` text.

**Tech Stack:** C++23 · CMake ≥ 3.23 · Ninja · Catch2 v3.6.0 via CPM 0.40.5 · GitHub Actions · MkDocs Material + Doxygen (scaffolded here, populated later)

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md`

## Global Constraints

Copied verbatim from the spec. Every task's requirements implicitly include these.

- **Licence:** Apache-2.0. Every `.hpp`, `.cpp`, `.cmake`, `CMakeLists.txt` and `*.cmake.in` starts with `SPDX-License-Identifier: Apache-2.0` (`#` comment for CMake, `//` for C++).
- **C++23 everywhere.** `target_compile_features(... INTERFACE cxx_std_23)`.
- **Compilers that must work:** MSVC `cl`, `clang-cl`, `clang++`. GCC welcome if free.
- **No third-party dependencies in the core.** Specifically **no glaze**. Catch2 is a test-only dependency and must not appear in the installed package config.
- **No norm content in this repository, and no citations either.** Never name a standard, and never transcribe its text, equations, tables, clause numbers or threshold values. Published standards are copyrighted and sold, and this repository is public. Public examples use **generic physics only**, with fictional `Example Standard` citations where a citation's *shape* has to be demonstrated.
- **No macros for traceability.**
- **`NOLINT` is banned in-tree.** Suppressions belong in `.clang-tidy`, where they are reviewable.
- **Every public `static_assert` message begins `formula: `** and is stable, unique and greppable. These strings are tested API; renaming one is a deliberate test change.
- **Version is a committed literal**, never derived from `git describe` — `vcpkg_from_github` extracts a tarball with no `.git`.
- **Naming:** CMake package, target and repo are `formula-cpp`; the exported alias is `formula-cpp::formula-cpp`; the C++ namespace is `formula`.
- **Attribution:** every commit ends with `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`.

## File Structure

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The `INTERFACE` target, options, header list, install/export rules |
| `CMakePresets.json` | Configure/build/test presets selecting `cl`, `clang-cl`, `clang++`, `g++` |
| `cmake/CPM.cmake` | Hash-pinned CPM bootstrap (test dependencies only) |
| `cmake/PedanticCompiler.cmake` | Warning flags, keyed on compiler **frontend variant** |
| `cmake/RunNegativeCompileTest.cmake` | Driver asserting a build failed *for the right reason* |
| `cmake/formula-cpp-config.cmake.in` | Installed package config template |
| `include/formula-cpp/version.hpp` | Hand-written version macros |
| `include/formula-cpp/detail/type_list.hpp` | `index_in_tuple` and friends — pure pack utilities |
| `include/formula-cpp/evaluation.hpp` | The ported prototype: `Overloader`, `EvaluationFunctors`, `Evaluation` |
| `include/formula-cpp/formula.hpp` | Umbrella header |
| `test/CMakeLists.txt` | Catch2 wiring, test target, negative-test registration |
| `test/runtime_tests.cpp` | Runtime Catch2 assertions |
| `test/compile_time_tests.cpp` | `STATIC_REQUIRE` assertions |
| `test/negative/duplicate_producer.cpp` | Must-not-compile case |
| `test/package/` | Standalone consumer project used by the install-and-consume CI job |
| `examples/simple.cpp` | Ported prototype example |
| `.github/workflows/build.yml` | Build matrix |
| `.github/workflows/package.yml` | Install-and-consume regression guard |

Header split rationale: `type_list.hpp` holds reusable pack utilities that survive every later phase; `evaluation.hpp` holds the prototype, which phase 5 of the spec replaces. Keeping them apart means that replacement deletes one file rather than editing a mixed one.

---

### Task 1: Trim the inherited lint configuration

`.clang-format` and `.clang-tidy` were copied from another project. They are not merely stale — `.clang-tidy`'s `HeaderFilterRegex` matches no path in this repository, so clang-tidy would analyse **zero** files and report success.

**Files:**
- Modify: `.clang-tidy`
- Modify: `.clang-format`

**Interfaces:**
- Consumes: nothing
- Produces: a `.clang-tidy` whose `HeaderFilterRegex` is `.*/include/formula-cpp/.*\.hpp$`; a `.clang-format` with `Standard: Latest`

- [ ] **Step 1: Verify the bug before fixing it**

Run:
```bash
grep -n 'HeaderFilterRegex' .clang-tidy
```
Expected: a regex mentioning `CowTree|FastCache|apps|tests` — none of which exist here. Confirm with:
```bash
ls src 2>/dev/null || echo "no src/ directory -- the regex matches nothing"
```

- [ ] **Step 2: Delete the inherited rationale block**

Delete lines 1–28 of `.clang-tidy` (the comment block about POSIX `open()`/`fcntl()`, Win32 `SOCKET`, `Base64.cpp` and "376 translation units"). Every claim in it is about a different program. Replace with:

```yaml
# Suppressions live here, not at call sites: NOLINT is banned in this tree so that
# every exception is reviewable in one place.
#
# Note `Checks:` below is a `>-` block scalar, in which `#` is NOT a comment --
# a "commented out" row there is silently spliced into the check list.
```

- [ ] **Step 3: Fix the header filter**

Replace the `HeaderFilterRegex:` line with:

```yaml
# A header-only library has no .cpp files of its own, so a filter that matches
# nothing makes clang-tidy report success having analysed zero code.
HeaderFilterRegex: '.*/include/formula-cpp/.*\.hpp$'
```

- [ ] **Step 4: Re-enable checks whose exclusions have no cause here**

Delete these four rows from the `Checks:` list — there are no variadic C APIs, no handle casts, and no `std::expected` in this tree yet:

```
  -cppcoreguidelines-pro-type-vararg,
  -performance-no-int-to-ptr,
  -clang-analyzer-core.uninitialized.Assign,
  -clang-analyzer-optin.cplusplus.UninitializedObject,
```

- [ ] **Step 5: Fix the include categories and language standard in `.clang-format`**

Replace the `IncludeCategories:` block with:

```yaml
IncludeCategories:
  - { Regex: '^".*"',              Priority: 0 }
  - { Regex: '^<formula-cpp/',     Priority: 10 }
  - { Regex: '^<catch2/',          Priority: 70 }
  - { Regex: '^<[[:alnum:]_]+>',   Priority: 81 }
  - { Regex: '<[[:alnum:]_]+\.h>', Priority: 83 }
  - { Regex: '.*',                 Priority: 99 }
```

Then change `Standard: Cpp11` to `Standard: Latest`. This one is a real defect for this repository: `Cpp11` changes how clang-format parses nested `>>`, concepts and requires-clauses — exactly what a template metaprogramming library is made of.

- [ ] **Step 6: Verify the config parses**

Run:
```bash
clang-format --style=file --dump-config > /dev/null && echo "clang-format config OK"
clang-tidy --dump-config > /dev/null && echo "clang-tidy config OK"
```
Expected: both print OK with no error.

- [ ] **Step 7: Commit**

```bash
git add .clang-format .clang-tidy
git commit -m "chore: trim inherited lint configuration to this repository

The clang-tidy HeaderFilterRegex matched no path here, so the linter would
have analysed zero files and reported success. Its 28-line rationale block
described a different program -- POSIX ioctl, Win32 sockets, Base64.cpp,
376 translation units -- none of which exist in this tree.

clang-format declared Standard: Cpp11, which mis-parses the concepts and
nested >> this library is made of.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 2: CMake INTERFACE library, version header and presets

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/formula-cpp/version.hpp`
- Create: `CMakePresets.json`
- Create: `cmake/PedanticCompiler.cmake`

**Interfaces:**
- Consumes: nothing
- Produces: target `formula-cpp` (INTERFACE) and alias `formula-cpp::formula-cpp`; options `FORMULA_BUILD_TESTS`, `FORMULA_BUILD_EXAMPLES`, `FORMULA_INSTALL`, `FORMULA_PEDANTIC`, `FORMULA_WERROR`; macros `FORMULA_VERSION_MAJOR/MINOR/PATCH/STRING`; presets `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`, `clang-release`, `gcc-release`

- [ ] **Step 1: Write the version header**

Create `include/formula-cpp/version.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Version macros. Hand-written on purpose: a generated header would live in the
/// build directory and break the library's most valuable fallback property --
/// copy include/ into a tree and it works. `ctest -R hygiene.version` asserts
/// that these agree with project(VERSION) in CMakeLists.txt.

#define FORMULA_VERSION_MAJOR 0
#define FORMULA_VERSION_MINOR 1
#define FORMULA_VERSION_PATCH 0
#define FORMULA_VERSION_STRING "0.1.0"
```

- [ ] **Step 2: Write the top-level CMakeLists**

Create `CMakeLists.txt`:

```cmake
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

# The version is a committed literal, not derived from `git describe`.
# vcpkg_from_github extracts a tarball with no .git, where a derived version
# silently becomes 0.0.0 and every find_package(formula-cpp 0.1.0) then fails
# with an error the git tag does not explain.
project(formula-cpp
    VERSION 0.1.0
    DESCRIPTION "Declarative, traceable, self-documenting formulas for C++23"
    HOMEPAGE_URL "https://github.com/LASTRADA-Software/formula-cpp"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FORMULA_BUILD_TESTS    "Build formula-cpp tests"      ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_BUILD_EXAMPLES "Build formula-cpp examples"   ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_INSTALL        "Generate install rules"       ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_PEDANTIC       "Compile with strict warnings" ON)
option(FORMULA_WERROR         "Treat warnings as errors"     OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(formula-cpp INTERFACE)
add_library(formula-cpp::formula-cpp ALIAS formula-cpp)

target_compile_features(formula-cpp INTERFACE cxx_std_23)

target_sources(formula-cpp INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/formula.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/version.hpp")

# These three are workarounds for cl's non-conformance and are REQUIRED to
# compile this library's headers at all, so they are INTERFACE, not PRIVATE:
# a consumer using cl needs them too. $<CXX_COMPILER_ID:MSVC> matches the real
# cl driver only -- clang-cl reports "Clang" and needs none of them, being
# conforming, UTF-8 by default, and correct about __cplusplus already.
target_compile_options(formula-cpp INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/permissive-;/utf-8;/Zc:__cplusplus>")

if(FORMULA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(FORMULA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

- [ ] **Step 3: Write the warning module**

Create `cmake/PedanticCompiler.cmake`:

```cmake
# SPDX-License-Identifier: Apache-2.0
#
# Warning flags for the project's OWN targets (tests, examples) -- never for the
# exported INTERFACE, which must not impose a warning policy on consumers.
#
# Keyed on CMAKE_CXX_COMPILER_FRONTEND_VARIANT, not CMAKE_CXX_COMPILER_ID: for
# clang-cl the ID is "Clang" but the frontend is "MSVC", so guarding on the ID
# alone hands clang-cl a GCC-style -Werror it will reject.

include_guard(GLOBAL)

function(formula_apply_warnings target)
    if(NOT FORMULA_PEDANTIC)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
        if(FORMULA_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        # /bigobj: template-heavy test translation units exceed the default
        # section limit long before they look large.
        target_compile_options(${target} PRIVATE /bigobj)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wconversion -Wpedantic -Wshadow -Wnon-virtual-dtor)
        if(FORMULA_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
```

- [ ] **Step 4: Write the presets**

Create `CMakePresets.json`:

```json
{
    "version": 4,
    "cmakeMinimumRequired": { "major": 3, "minor": 23, "patch": 0 },
    "configurePresets": [
        {
            "name": "base",
            "hidden": true,
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/out/build/${presetName}",
            "installDir": "${sourceDir}/out/install/${presetName}",
            "cacheVariables": {
                "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                "FORMULA_PEDANTIC": "ON",
                "FORMULA_WERROR": "ON"
            }
        },
        {
            "name": "windows", "hidden": true, "inherits": "base",
            "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
        },
        {
            "name": "posix", "hidden": true, "inherits": "base",
            "condition": { "type": "notEquals", "lhs": "${hostSystemName}", "rhs": "Windows" }
        },

        { "name": "cl-debug",   "displayName": "MSVC cl Debug",   "inherits": "windows",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug",   "CMAKE_C_COMPILER": "cl",       "CMAKE_CXX_COMPILER": "cl" } },
        { "name": "cl-release", "displayName": "MSVC cl Release", "inherits": "windows",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_C_COMPILER": "cl",       "CMAKE_CXX_COMPILER": "cl" } },
        { "name": "clangcl-debug",   "displayName": "clang-cl Debug",   "inherits": "windows",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug",   "CMAKE_C_COMPILER": "clang-cl", "CMAKE_CXX_COMPILER": "clang-cl" } },
        { "name": "clangcl-release", "displayName": "clang-cl Release", "inherits": "windows",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_C_COMPILER": "clang-cl", "CMAKE_CXX_COMPILER": "clang-cl" } },

        { "name": "clang-debug",   "displayName": "clang++ Debug",   "inherits": "posix",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug",   "CMAKE_C_COMPILER": "clang",    "CMAKE_CXX_COMPILER": "clang++" } },
        { "name": "clang-release", "displayName": "clang++ Release", "inherits": "posix",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_C_COMPILER": "clang",    "CMAKE_CXX_COMPILER": "clang++" } },
        { "name": "gcc-release",   "displayName": "g++ Release",     "inherits": "posix",
          "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_C_COMPILER": "gcc",      "CMAKE_CXX_COMPILER": "g++" } }
    ],
    "buildPresets": [
        { "name": "cl-debug", "configurePreset": "cl-debug" },
        { "name": "cl-release", "configurePreset": "cl-release" },
        { "name": "clangcl-debug", "configurePreset": "clangcl-debug" },
        { "name": "clangcl-release", "configurePreset": "clangcl-release" },
        { "name": "clang-debug", "configurePreset": "clang-debug" },
        { "name": "clang-release", "configurePreset": "clang-release" },
        { "name": "gcc-release", "configurePreset": "gcc-release" }
    ],
    "testPresets": [
        { "name": "cl-debug", "configurePreset": "cl-debug", "output": { "outputOnFailure": true } },
        { "name": "cl-release", "configurePreset": "cl-release", "output": { "outputOnFailure": true } },
        { "name": "clangcl-debug", "configurePreset": "clangcl-debug", "output": { "outputOnFailure": true } },
        { "name": "clangcl-release", "configurePreset": "clangcl-release", "output": { "outputOnFailure": true } },
        { "name": "clang-debug", "configurePreset": "clang-debug", "output": { "outputOnFailure": true } },
        { "name": "clang-release", "configurePreset": "clang-release", "output": { "outputOnFailure": true } },
        { "name": "gcc-release", "configurePreset": "gcc-release", "output": { "outputOnFailure": true } }
    ]
}
```

- [ ] **Step 5: Verify configure fails for the right reason**

Tests and examples do not exist yet, so configure must fail on the missing subdirectory — proving the options are wired.

Run (from a Visual Studio developer prompt, or after `ilammy/msvc-dev-cmd` in CI):
```bash
cmake --preset cl-debug
```
Expected: FAIL with `add_subdirectory given source "test" which is not an existing directory`.

- [ ] **Step 6: Configure with tests and examples off**

Run:
```bash
cmake --preset cl-debug -DFORMULA_BUILD_TESTS=OFF -DFORMULA_BUILD_EXAMPLES=OFF
```
Expected: PASS — `-- Configuring done` and `-- Generating done`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt CMakePresets.json cmake/PedanticCompiler.cmake include/formula-cpp/version.hpp
git commit -m "build: header-only INTERFACE target with presets for cl, clang-cl and clang++

The version is a committed literal rather than derived from git describe,
because vcpkg_from_github extracts a tarball with no .git and a derived
version would silently install a config claiming 0.0.0.

Warning flags key on CMAKE_CXX_COMPILER_FRONTEND_VARIANT rather than
CMAKE_CXX_COMPILER_ID: clang-cl reports the ID Clang but the MSVC frontend,
so guarding on the ID hands it a GCC-style -Werror it rejects.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 3: Install, export and a package-consumer test

This task defines the contract the vcpkg port and downstream projects consume. It is deliberately done before any test machinery exists, so the consumable surface is the stable part of the repository from the beginning.

**Files:**
- Create: `cmake/formula-cpp-config.cmake.in`
- Modify: `CMakeLists.txt` (append install rules)
- Create: `test/package/CMakeLists.txt`
- Create: `test/package/main.cpp`

**Interfaces:**
- Consumes: target `formula-cpp` from Task 2
- Produces: an install tree containing `include/formula-cpp/*.hpp` and `lib/cmake/formula-cpp/{formula-cpp-config.cmake, formula-cpp-config-version.cmake, formula-cpp-targets.cmake}`, satisfying `find_package(formula-cpp CONFIG REQUIRED)`

- [ ] **Step 1: Write the package config template**

Create `cmake/formula-cpp-config.cmake.in`:

```cmake
# SPDX-License-Identifier: Apache-2.0
@PACKAGE_INIT@

# Guarded so that two consumers in one build tree do not re-include the targets.
if(NOT TARGET formula-cpp::formula-cpp)
    include("${CMAKE_CURRENT_LIST_DIR}/formula-cpp-targets.cmake")
endif()

check_required_components(formula-cpp)
```

Note what is absent and must stay absent: any mention of Catch2, CPM, a test target, or an absolute path from the build machine. CI asserts this mechanically in Task 9.

- [ ] **Step 2: Append install rules to `CMakeLists.txt`**

Insert immediately before the `if(FORMULA_BUILD_TESTS)` block:

```cmake
if(FORMULA_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS formula-cpp
        EXPORT formula-cpp-targets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT formula-cpp-targets
        FILE formula-cpp-targets.cmake
        NAMESPACE formula-cpp::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    # ARCH_INDEPENDENT: a header-only package must not refuse a consumer whose
    # pointer width differs from whatever configured the install.
    # SameMinorVersion, not SameMajorVersion: before 1.0 the breaking changes
    # land on the minor, so 0.1 must not silently satisfy a request for 0.2.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
        ARCH_INDEPENDENT)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/formula-cpp-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/formula-cpp")
endif()
```

- [ ] **Step 3: Write the consumer project**

Create `test/package/CMakeLists.txt`. Every line here is load-bearing; the comments say why.

```cmake
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

# Deliberately NO project-level CMAKE_CXX_STANDARD: if cxx_std_23 ever stops
# propagating from the exported target, main.cpp must fail to compile.
project(formula-cpp-package-test LANGUAGES CXX)

enable_testing()

# CONFIG, so that a stray Findformula-cpp.cmake module cannot satisfy this.
find_package(formula-cpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE formula-cpp::formula-cpp)

add_test(NAME consumer COMMAND consumer)
```

- [ ] **Step 4: Write the consumer source**

Create `test/package/main.cpp`. The angle-bracket include is deliberate: if the installed include directory is wrong, this fails.

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/version.hpp>

#include <cstdio>

// Proves cxx_std_23 propagated from the exported target: `if consteval` is C++23.
constexpr int probe(int value) noexcept
{
    if consteval
    {
        return value + 1;
    }
    else
    {
        return value;
    }
}

static_assert(probe(1) == 2, "C++23 did not propagate from formula-cpp::formula-cpp");

int main()
{
    std::printf("formula-cpp %s consumed successfully\n", FORMULA_VERSION_STRING);
    return 0;
}
```

- [ ] **Step 5: Install to a staging prefix**

Run:
```bash
cmake --preset cl-release -DFORMULA_BUILD_TESTS=OFF -DFORMULA_BUILD_EXAMPLES=OFF
cmake --install out/build/cl-release --prefix "$PWD/stage"
find stage -type f | sort
```
Expected: exactly these files —
```
stage/include/formula-cpp/formula.hpp
stage/include/formula-cpp/version.hpp
stage/lib/cmake/formula-cpp/formula-cpp-config-version.cmake
stage/lib/cmake/formula-cpp/formula-cpp-config.cmake
stage/lib/cmake/formula-cpp/formula-cpp-targets.cmake
stage/share/licenses/formula-cpp/LICENSE
```
(`formula.hpp` does not exist yet — it arrives in Task 6. Until then expect the `version.hpp` row only, and a configure error naming the missing file. If so, temporarily drop `formula.hpp` from the `FILES` list, finish this task, and restore it in Task 6.)

- [ ] **Step 6: Consume the installed package**

Run:
```bash
cmake -S test/package -B out/consume -G Ninja -DCMAKE_PREFIX_PATH="$PWD/stage"
cmake --build out/consume
ctest --test-dir out/consume --output-on-failure
```
Expected: PASS, printing `formula-cpp 0.1.0 consumed successfully`.

- [ ] **Step 7: Assert the config leaks nothing**

Run:
```bash
grep -REn 'Catch2|CPM|_deps|FORMULA_BUILD_' stage/lib/cmake/formula-cpp/ && echo "LEAK" || echo "clean: no test machinery in the installed config"
grep -REn "$PWD" stage/lib/cmake/formula-cpp/ && echo "LEAK" || echo "clean: no build-machine paths"
```
Expected: both print `clean: ...`.

- [ ] **Step 8: Commit**

```bash
git add cmake/formula-cpp-config.cmake.in CMakeLists.txt test/package
git commit -m "build: install, export, and a package-consumer test

This is the contract the vcpkg port and Lastrada consume, so it lands before
any test machinery exists -- the consumable surface is the part of the
repository that must stay stable, and the test wiring is added around it
rather than tangled into it.

The consumer project sets no CMAKE_CXX_STANDARD of its own and includes by
angle brackets, so a regression in either the propagated C++ standard or the
installed include directory fails the build rather than passing quietly.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 4: CPM bootstrap and Catch2

**Files:**
- Create: `cmake/CPM.cmake`
- Create: `test/CMakeLists.txt`
- Create: `test/runtime_tests.cpp`

**Interfaces:**
- Consumes: target `formula-cpp` (Task 2), `formula_apply_warnings()` (Task 2)
- Produces: test target `formula-cpp-tests`, linking `Catch2::Catch2WithMain`; `include(Catch)` available via `CMAKE_MODULE_PATH`

- [ ] **Step 1: Write the CPM bootstrap**

Create `cmake/CPM.cmake`. The version and hash are pinned; do not "update" them casually — the hash is what makes the download trustworthy.

```cmake
# SPDX-License-Identifier: Apache-2.0
# CPM.cmake bootstrap -- downloads CPM on first use and caches it.
# https://github.com/cpm-cmake/CPM.cmake
#
# Used for TEST dependencies only. Nothing fetched here may reach the installed
# package config.

include_guard(GLOBAL)

set(CPM_DOWNLOAD_VERSION 0.40.5)
set(CPM_HASH_SUM "c46b876ae3b9f994b4f05a4c15553e0485636862064f1fcc9d8b4f832086bc5d")
set(CPM_DOWNLOAD_URL
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake")

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Quoted, and ABSOLUTE: a cache directory under a path containing a space would
# otherwise split here and land the ABSOLUTE keyword in the wrong argument slot.
get_filename_component(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_LOCATION}" ABSOLUTE)

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    # INACTIVITY_TIMEOUT, not TIMEOUT: the bound is on silence, so a slow but
    # progressing download still completes however long it takes.
    file(DOWNLOAD
        "${CPM_DOWNLOAD_URL}"
        "${CPM_DOWNLOAD_LOCATION}"
        EXPECTED_HASH "SHA256=${CPM_HASH_SUM}"
        INACTIVITY_TIMEOUT 120
        STATUS cpmDownloadStatus)
    list(GET cpmDownloadStatus 0 cpmDownloadCode)
    if(NOT cpmDownloadCode EQUAL 0)
        message(FATAL_ERROR
            "could not download the CPM.cmake bootstrap: ${cpmDownloadStatus}\n"
            "  from: ${CPM_DOWNLOAD_URL}\n"
            "  into: ${CPM_DOWNLOAD_LOCATION}\n"
            "Re-run the configure, or point CPM_SOURCE_CACHE at a directory that "
            "already holds the bootstrap.")
    endif()
endif()

# Prefer a system/vcpkg Catch2 over fetching source when one is present.
set(CPM_USE_LOCAL_PACKAGES ON)

include("${CPM_DOWNLOAD_LOCATION}")
```

- [ ] **Step 2: Write the failing test**

Create `test/runtime_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("version macros agree with each other", "[version]")
{
    CHECK(FORMULA_VERSION_MAJOR == 0);
    CHECK(FORMULA_VERSION_MINOR == 1);
    CHECK(FORMULA_VERSION_PATCH == 0);
    CHECK(std::string_view { FORMULA_VERSION_STRING } == "0.1.0");
}
```

- [ ] **Step 3: Write the test CMakeLists**

Create `test/CMakeLists.txt`:

```cmake
# SPDX-License-Identifier: Apache-2.0
include(CPM)
include(PedanticCompiler)

CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.6.0)

# Catch.cmake (which provides catch_discover_tests) reaches us by one of two
# routes: a CPM source build puts it in ${Catch2_SOURCE_DIR}/extras, a system
# find_package puts it next to Catch2Config.cmake in ${Catch2_DIR}.
# CPM_USE_LOCAL_PACKAGES makes the second the default wherever Catch2 is
# installed, so both must be handled.
if(Catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
elseif(Catch2_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
endif()
include(Catch)

add_executable(formula-cpp-tests runtime_tests.cpp)
target_link_libraries(formula-cpp-tests PRIVATE formula-cpp::formula-cpp Catch2::Catch2WithMain)
formula_apply_warnings(formula-cpp-tests)

catch_discover_tests(formula-cpp-tests)
```

- [ ] **Step 4: Run the test**

Run:
```bash
cmake --preset cl-debug
cmake --build --preset cl-debug
ctest --preset cl-debug
```
Expected: PASS, `1 test from 1 test case` (the version test).

- [ ] **Step 5: Verify the fetch is bounded and hash-checked**

Delete the cached bootstrap and confirm it re-downloads and verifies:
```bash
rm -rf out/build/cl-debug/cmake/CPM_0.40.5.cmake
cmake --preset cl-debug 2>&1 | tail -5
```
Expected: configure succeeds. A corrupted download would fail with `HASH mismatch`.

- [ ] **Step 6: Commit**

```bash
git add cmake/CPM.cmake test/CMakeLists.txt test/runtime_tests.cpp
git commit -m "test: Catch2 via a hash-pinned CPM bootstrap

CPM is pinned by version and SHA256 and fetched with INACTIVITY_TIMEOUT
rather than TIMEOUT, so a slow but progressing download still completes
while a stalled one ends.

Catch2 is a test-only dependency; nothing fetched here may reach the
installed package config, which CI asserts.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 5: Port the prototype off the internal-linkage lambda idiom

The prototype identifies quantity types with `decltype([]{})` as a default template argument. A spike compiled and linked this on all three compilers: **the closure type receives internal linkage, so a type built on it is a different type in every translation unit.** In a header-only library this fails at link time with a message that never mentions the cause.

Evidence, reproduced on cl 19.51, clang-cl 22.1.3 and clang++ 22.1.3:

```
cl:      warning C5046: 'consume': Symbol involving type with internal linkage not defined
         error LNK2019: unresolved external symbol ... Tagged<int,class <lambda_1_> >
clang++: warning: function 'consume' has internal linkage but is not defined
         lld-link: error: undefined symbol: ... Tagged<int, class <lambda_1>>
```

**Files:**
- Create: `include/formula-cpp/detail/type_list.hpp`
- Create: `include/formula-cpp/evaluation.hpp`
- Create: `include/formula-cpp/formula.hpp`
- Delete: `include/formula-cpp/formula.hpp` (old prototype content — replaced)
- Modify: `examples/simple.cpp`
- Create: `examples/CMakeLists.txt`
- Modify: `CMakeLists.txt` (header `FILES` list)

**Interfaces:**
- Consumes: nothing
- Produces: `formula::detail::index_in_tuple_v<T, Tuple>`; `formula::Overloader`; `formula::EvaluationFunctors`; `formula::EvaluationArguments`; `formula::Evaluation<Args, Funcs>` with `.set<T>()` / `.calculate(T)`; `formula::get<T>(ctx)`

- [ ] **Step 1: Write a failing cross-translation-unit test**

Create `test/cross_tu_a.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Proves quantity types survive a translation-unit boundary. This is the
// regression guard for the internal-linkage lambda idiom.
#include "cross_tu.hpp"

int consumeFirst(First value)
{
    return value.value;
}
```

Create `test/cross_tu.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <formula-cpp/formula.hpp>

template <typename T, typename Tag>
struct Quantity
{
    T value {};
};

struct FirstTag;
struct SecondTag;
struct ThirdTag;

using First = Quantity<int, FirstTag>;
using Second = Quantity<int, SecondTag>;
using Third = Quantity<int, ThirdTag>;

int consumeFirst(First value);
```

Append to `test/runtime_tests.cpp`:

```cpp
#include "cross_tu.hpp"

TEST_CASE("quantity types survive a translation unit boundary", "[linkage]")
{
    CHECK(consumeFirst(First { 7 }) == 7);
}
```

- [ ] **Step 2: Run it to make sure it fails**

Add `cross_tu_a.cpp` to the test target in `test/CMakeLists.txt`:

```cmake
add_executable(formula-cpp-tests runtime_tests.cpp cross_tu_a.cpp)
```

Run:
```bash
cmake --build --preset cl-debug
```
Expected: FAIL — `formula.hpp` does not exist yet under that name with that content.

- [ ] **Step 3: Extract the pack utilities**

Create `include/formula-cpp/detail/type_list.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace formula::detail
{

/// Index of @p T within @p Tuple, as a compile-time constant.
template <typename T, typename Tuple>
struct index_in_tuple;

template <typename T, typename... Ts>
struct index_in_tuple<T, std::tuple<Ts...>>
{
  private:
    static constexpr std::size_t compute()
    {
        constexpr bool matches[] = { std::is_same_v<T, Ts>..., false };
        for (std::size_t i = 0; i < sizeof...(Ts); ++i)
            if (matches[i])
                return i;
        return sizeof...(Ts);
    }

  public:
    static constexpr std::size_t value = compute();
    static_assert(value < sizeof...(Ts), "formula: type not found in the argument list");
};

template <typename T, typename Tuple>
inline constexpr std::size_t index_in_tuple_v = index_in_tuple<T, Tuple>::value;

} // namespace formula::detail
```

Note the `static_assert` message now begins `formula: ` per the global constraint. It is tested API.

- [ ] **Step 4: Move the evaluator into its own header**

Create `include/formula-cpp/evaluation.hpp` with the prototype's `Overloader`, `EvaluationFunctors`, `EvaluationArguments` and `Evaluation`, unchanged except that it includes `detail/type_list.hpp`, uses `detail::index_in_tuple_v`, and its `static_assert` messages are prefixed. The two messages to change:

```cpp
        static_assert(count_producers<T, Ctx>() == 1,
                      "formula: exactly one functor must return the requested type");
```

```cpp
    static_assert(Funcs.template producers_are_unique<Context const&>(),
                  "formula: two or more functors return the same type");
```

This header is replaced wholesale by the expression layer in a later phase; keeping it separate means that replacement deletes a file rather than editing a mixed one.

- [ ] **Step 5: Write the umbrella header**

Create `include/formula-cpp/formula.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Umbrella header. Deliberately pulls in no <string>, <vector> or <format>:
/// a consumer that only evaluates numbers must not compile them in every
/// translation unit.

#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/evaluation.hpp>
#include <formula-cpp/version.hpp>
```

- [ ] **Step 6: Register the new headers**

In `CMakeLists.txt`, replace the `FILES` list with:

```cmake
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/formula.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/evaluation.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/version.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/type_list.hpp")
```

- [ ] **Step 7: Run the test to verify it passes**

Run:
```bash
cmake --preset cl-debug && cmake --build --preset cl-debug && ctest --preset cl-debug
```
Expected: PASS, including `quantity types survive a translation unit boundary`.

- [ ] **Step 8: Port the example**

Replace `examples/simple.cpp` entirely:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <iostream>

// Named tag types, NOT `decltype([]{})`. A lambda in a default template argument
// gives the closure internal linkage, so the quantity type differs in every
// translation unit -- verified to fail at link time on cl, clang-cl and clang++.
template <typename T, typename Tag>
struct Quantity
{
    T value {};
};

struct FirstTag;
struct SecondTag;
struct ThirdTag;

using First = Quantity<int, FirstTag>;
using Second = Quantity<int, SecondTag>;
using Third = Quantity<int, ThirdTag>;

using Calculation = formula::Evaluation<
    formula::EvaluationArguments<First, Second, Third>,
    formula::EvaluationFunctors {
        [](auto const& ctx) -> Second { return Second { formula::get<First>(ctx).value + 1 }; },
        [](auto const& ctx) -> Third {
            return Third { formula::get<First>(ctx).value + formula::get<Second>(ctx).value };
        } }>;

int main()
{
    auto const result = Calculation().set(First { 1 }).calculate(Third {});
    std::cout << "Third = " << result.value << '\n'; // prints 3
    return 0;
}
```

`std::print` is gone: the library must not require a bleeding-edge standard library on the Linux CI leg.

- [ ] **Step 9: Build and run the example**

Create `examples/CMakeLists.txt`:

```cmake
# SPDX-License-Identifier: Apache-2.0
include(PedanticCompiler)

add_executable(formula-cpp-example-simple simple.cpp)
target_link_libraries(formula-cpp-example-simple PRIVATE formula-cpp::formula-cpp)
formula_apply_warnings(formula-cpp-example-simple)

add_test(NAME example.simple COMMAND formula-cpp-example-simple)
set_tests_properties(example.simple PROPERTIES PASS_REGULAR_EXPRESSION "Third = 3")
```

Run:
```bash
cmake --build --preset cl-debug && ctest --preset cl-debug -R example.simple
```
Expected: PASS, output `Third = 3`.

- [ ] **Step 10: Commit**

```bash
git add include examples test CMakeLists.txt
git commit -m "refactor: identify quantity types by named tags, not by a lambda

decltype([]{}) as a default template argument gives the closure type internal
linkage, so a quantity built on it is a DIFFERENT type in every translation
unit. Verified on cl 19.51, clang-cl 22.1.3 and clang++ 22.1.3: all three fail
at link time, and the diagnostic never mentions the cause.

    cl:      warning C5046: Symbol involving type with internal linkage not defined
    clang++: lld-link: error: undefined symbol: ... Tagged<int, class <lambda_1>>

The prototype survived this only because it lived in one translation unit.
test/cross_tu_a.cpp is the regression guard.

Also splits the prototype into detail/type_list.hpp (pack utilities, which
survive every later phase) and evaluation.hpp (the prototype, which a later
phase replaces wholesale), and drops std::print from the example so the Linux
CI leg does not require a bleeding-edge standard library.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 6: Compile-time tests

Most of this library's behaviour is compile-time, so most of its tests must be too.

**Files:**
- Create: `test/compile_time_tests.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `formula::detail::index_in_tuple_v`, `formula::Evaluation` (Task 5)
- Produces: nothing consumed by later tasks

- [ ] **Step 1: Write the failing test**

Create `test/compile_time_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <type_traits>

namespace
{
struct Alpha
{
    int value {};
};
struct Beta
{
    int value {};
};
struct Gamma
{
    int value {};
};

using Bag = std::tuple<Alpha, Beta, Gamma>;
} // namespace

TEST_CASE("index_in_tuple locates each type", "[type_list]")
{
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Alpha, Bag> == 0);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Beta, Bag> == 1);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Gamma, Bag> == 2);
}

TEST_CASE("a dependent quantity is derived, not supplied", "[evaluation]")
{
    using Calculation =
        formula::Evaluation<formula::EvaluationArguments<Alpha, Beta>,
                            formula::EvaluationFunctors { [](auto const& ctx) -> Beta {
                                return Beta { formula::get<Alpha>(ctx).value * 2 };
                            } }>;

    auto const result = Calculation().set(Alpha { 21 }).calculate(Beta {});
    CHECK(result.value == 42);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run:
```bash
cmake --build --preset cl-debug
```
Expected: FAIL — `compile_time_tests.cpp` is not in the target yet, so the new test cases do not appear in `ctest --preset cl-debug -N`.

- [ ] **Step 3: Add the file to the test target**

In `test/CMakeLists.txt`:

```cmake
add_executable(formula-cpp-tests runtime_tests.cpp compile_time_tests.cpp cross_tu_a.cpp)
```

- [ ] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build --preset cl-debug && ctest --preset cl-debug
```
Expected: PASS, four test cases.

- [ ] **Step 5: Commit**

```bash
git add test/compile_time_tests.cpp test/CMakeLists.txt
git commit -m "test: compile-time assertions for the pack utilities and evaluator

STATIC_REQUIRE rather than CHECK wherever the property is a compile-time one,
so a regression is a build failure rather than a runtime report.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 7: Must-not-compile test harness

The library's headline feature is rejecting wrong code, so "this does not compile" must be tested. `WILL_FAIL TRUE` alone is not enough: it passes when the file fails to compile *for the wrong reason*, including a typo. The driver therefore asserts both halves — the build failed, **and** the failure text contains the library's own `static_assert` message.

**Files:**
- Create: `cmake/RunNegativeCompileTest.cmake`
- Create: `test/negative/duplicate_producer.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `formula::Evaluation` (Task 5)
- Produces: CMake function `formula_add_negative_test(<name> <expected-regex>)`

- [ ] **Step 1: Write the must-not-compile case**

Create `test/negative/duplicate_producer.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// MUST NOT COMPILE: two functors produce the same type, so the evaluator cannot
// decide which one derives it.
#include <formula-cpp/formula.hpp>

namespace
{
struct Alpha
{
    int value {};
};
struct Beta
{
    int value {};
};
} // namespace

using Broken =
    formula::Evaluation<formula::EvaluationArguments<Alpha, Beta>,
                        formula::EvaluationFunctors {
                            [](auto const& ctx) -> Beta { return Beta { formula::get<Alpha>(ctx).value }; },
                            [](auto const& ctx) -> Beta { return Beta { formula::get<Alpha>(ctx).value + 1 }; } }>;

int main()
{
    return Broken().set(Alpha { 1 }).calculate(Beta {}).value;
}
```

- [ ] **Step 2: Write the driver**

Create `cmake/RunNegativeCompileTest.cmake`:

```cmake
# SPDX-License-Identifier: Apache-2.0
#
# Passes only when BOTH hold: the build failed, AND the failure text contains the
# library's own static_assert message. Either half alone is a test that lies --
# "it failed" also passes on a typo, and "the text appeared" also passes on a
# warning.

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}" --target "${TARGET}"
    RESULT_VARIABLE buildResult
    OUTPUT_VARIABLE buildOut
    ERROR_VARIABLE buildErr)

set(combined "${buildOut}${buildErr}")

if(buildResult EQUAL 0)
    message(FATAL_ERROR
        "negative test ${TARGET}: the code COMPILED, and it must not.\n"
        "Expected the compiler to report: ${EXPECT}")
endif()

if(NOT combined MATCHES "${EXPECT}")
    message(FATAL_ERROR
        "negative test ${TARGET}: the build failed, but for the WRONG reason.\n"
        "Expected to find: ${EXPECT}\n"
        "--- compiler output ---\n${combined}")
endif()
```

- [ ] **Step 3: Register the test**

Append to `test/CMakeLists.txt`:

```cmake
# Each negative case is its own EXCLUDE_FROM_ALL target, so a normal build stays
# green while `ctest` drives the failing compile deliberately.
function(formula_add_negative_test name expected)
    add_executable(negative-${name} EXCLUDE_FROM_ALL "negative/${name}.cpp")
    target_link_libraries(negative-${name} PRIVATE formula-cpp::formula-cpp)
    add_test(NAME negative.${name}
        COMMAND "${CMAKE_COMMAND}"
                -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
                -D "CONFIG=$<CONFIG>"
                -D "TARGET=negative-${name}"
                -D "EXPECT=${expected}"
                -P "${PROJECT_SOURCE_DIR}/cmake/RunNegativeCompileTest.cmake")
endfunction()

# The expected text is the library's OWN static_assert message. All three
# compilers echo it verbatim (MSVC wraps it as `error C2338: static assertion
# failed: '<message>'`), so one regex matches on every leg.
formula_add_negative_test(duplicate_producer "formula: two or more functors return the same type")
```

- [ ] **Step 4: Run it to verify it passes**

Run:
```bash
cmake --preset cl-debug && cmake --build --preset cl-debug
ctest --preset cl-debug -R negative.duplicate_producer
```
Expected: PASS.

- [ ] **Step 5: Verify the test can actually fail**

This is the positive control — a test that cannot fail is not a test. Temporarily change the expected text in `test/CMakeLists.txt` to `"formula: this message does not exist"`, reconfigure, and run:
```bash
cmake --preset cl-debug && ctest --preset cl-debug -R negative.duplicate_producer
```
Expected: FAIL with `the build failed, but for the WRONG reason`, followed by the real compiler output. **Restore the correct text afterwards** and re-run to confirm PASS.

- [ ] **Step 6: Commit**

```bash
git add cmake/RunNegativeCompileTest.cmake test/negative test/CMakeLists.txt
git commit -m "test: a must-not-compile harness that checks the reason, not just the failure

WILL_FAIL alone passes when a file fails to compile for the wrong reason,
including a typo, so the driver asserts both that the build failed and that
the output contains the library's own static_assert text.

That places a contract on the library: every public static_assert message
begins 'formula: ' and is stable, unique and greppable. Those strings are
tested API; renaming one is a deliberate test change.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 8: Hygiene tests

**Files:**
- Create: `cmake/CheckVersionConsistency.cmake`
- Create: `cmake/CheckSpdxHeaders.cmake`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `FORMULA_VERSION_*` macros (Task 2)
- Produces: CTest cases `hygiene.version`, `hygiene.spdx`

- [ ] **Step 1: Write the version-drift check**

Create `cmake/CheckVersionConsistency.cmake`:

```cmake
# SPDX-License-Identifier: Apache-2.0
# version.hpp is hand-written, so something must assert it has not drifted from
# project(VERSION). This is that something.

file(READ "${VERSION_HEADER}" header)

foreach(part MAJOR MINOR PATCH)
    if(NOT header MATCHES "#define FORMULA_VERSION_${part} ([0-9]+)")
        message(FATAL_ERROR "version.hpp: FORMULA_VERSION_${part} not found")
    endif()
    set(header_${part} "${CMAKE_MATCH_1}")
endforeach()

set(fromHeader "${header_MAJOR}.${header_MINOR}.${header_PATCH}")

if(NOT fromHeader STREQUAL PROJECT_VERSION)
    message(FATAL_ERROR
        "version drift: version.hpp says ${fromHeader}, project(VERSION) says ${PROJECT_VERSION}")
endif()

if(NOT header MATCHES "#define FORMULA_VERSION_STRING \"([^\"]+)\"")
    message(FATAL_ERROR "version.hpp: FORMULA_VERSION_STRING not found")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL fromHeader)
    message(FATAL_ERROR
        "version drift: FORMULA_VERSION_STRING is \"${CMAKE_MATCH_1}\", macros say ${fromHeader}")
endif()

message(STATUS "version consistent: ${fromHeader}")
```

- [ ] **Step 2: Write the SPDX check**

Create `cmake/CheckSpdxHeaders.cmake`:

```cmake
# SPDX-License-Identifier: Apache-2.0
# Every source and build file carries an SPDX identifier on its first line.

file(GLOB_RECURSE sources
    "${SOURCE_DIR}/include/*.hpp"
    "${SOURCE_DIR}/test/*.cpp"
    "${SOURCE_DIR}/test/*.hpp"
    "${SOURCE_DIR}/examples/*.cpp"
    "${SOURCE_DIR}/cmake/*.cmake"
    "${SOURCE_DIR}/cmake/*.cmake.in")

set(offenders "")
foreach(file ${sources})
    file(READ "${file}" contents LIMIT 200)
    if(NOT contents MATCHES "SPDX-License-Identifier: Apache-2\\.0")
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
        list(APPEND offenders "${rel}")
    endif()
endforeach()

list(LENGTH sources total)
if(offenders)
    list(JOIN offenders "\n  " pretty)
    message(FATAL_ERROR "files without an SPDX identifier:\n  ${pretty}")
endif()

message(STATUS "SPDX identifier present in all ${total} files")
```

- [ ] **Step 3: Register both as tests**

Append to `test/CMakeLists.txt`:

```cmake
add_test(NAME hygiene.version
    COMMAND "${CMAKE_COMMAND}"
            -D "VERSION_HEADER=${PROJECT_SOURCE_DIR}/include/formula-cpp/version.hpp"
            -D "PROJECT_VERSION=${PROJECT_VERSION}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckVersionConsistency.cmake")

add_test(NAME hygiene.spdx
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckSpdxHeaders.cmake")
```

- [ ] **Step 4: Run them**

Run:
```bash
cmake --preset cl-debug && ctest --preset cl-debug -R hygiene
```
Expected: PASS, both cases.

- [ ] **Step 5: Verify each can fail**

Temporarily change `FORMULA_VERSION_PATCH` to `9` in `version.hpp` and run `ctest --preset cl-debug -R hygiene.version`.
Expected: FAIL with `version drift: version.hpp says 0.1.9, project(VERSION) says 0.1.0`. **Restore it.**

Temporarily delete the SPDX line from `include/formula-cpp/version.hpp` and run `ctest --preset cl-debug -R hygiene.spdx`.
Expected: FAIL listing `include/formula-cpp/version.hpp`. **Restore it.**

- [ ] **Step 6: Commit**

```bash
git add cmake/CheckVersionConsistency.cmake cmake/CheckSpdxHeaders.cmake test/CMakeLists.txt
git commit -m "test: assert version consistency and SPDX coverage

version.hpp is hand-written on purpose -- copying include/ into a tree must
be enough to use the library -- so something has to assert it has not drifted
from project(VERSION). Both checks were confirmed able to fail before being
committed.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 9: Continuous integration

**Files:**
- Create: `.github/workflows/build.yml`
- Create: `.github/workflows/package.yml`

**Interfaces:**
- Consumes: presets (Task 2), install rules (Task 3), tests (Tasks 4–8)
- Produces: nothing consumed by later tasks

These workflows cannot be validated until the repository has a remote. They are written here and exercised in Task 11.

- [ ] **Step 1: Write the build matrix**

Create `.github/workflows/build.yml`:

```yaml
# SPDX-License-Identifier: Apache-2.0
name: Build
on:
  push:
    branches: [ master ]
  pull_request:
  merge_group:

concurrency:
  group: build-${{ github.ref }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}

permissions:
  contents: read

env:
  CPM_SOURCE_CACHE: ${{ github.workspace }}/.cache/CPM

jobs:
  build:
    name: ${{ matrix.name }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - { name: "Windows-cl",       os: windows-2025, preset: cl-release }
          - { name: "Windows-clang-cl", os: windows-2025, preset: clangcl-release }
          - { name: "Linux-clang",      os: ubuntu-24.04, preset: clang-release }
          - { name: "Linux-gcc",        os: ubuntu-24.04, preset: gcc-release }
          - { name: "macOS-clang",      os: macos-15,     preset: clang-release }
    steps:
      - uses: actions/checkout@v4

      # Split restore/save rather than a combined actions/cache: in a fan-out
      # matrix a combined step makes every leg a writer of the same key at once.
      - name: Restore CPM cache
        uses: actions/cache/restore@v4
        with:
          path: ${{ env.CPM_SOURCE_CACHE }}
          key: cpm-${{ runner.os }}-${{ hashFiles('test/CMakeLists.txt') }}
          restore-keys: cpm-${{ runner.os }}-

      - uses: seanmiddleditch/gha-setup-ninja@v6

      # ubuntu-24.04 defaults to GCC 13, whose libstdc++ lacks parts of C++23.
      - name: Install GCC 14
        if: matrix.preset == 'gcc-release'
        run: sudo apt-get update && sudo apt-get install -y g++-14

      - name: Set up MSVC environment
        if: runner.os == 'Windows'
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64

      - name: Configure
        shell: bash
        run: |
          extra=()
          [[ "${{ matrix.preset }}" == gcc-* ]] && extra+=(-DCMAKE_CXX_COMPILER=g++-14)
          cmake --preset ${{ matrix.preset }} "${extra[@]}"

      - name: Save CPM cache
        if: github.event_name == 'push'
        uses: actions/cache/save@v4
        with:
          path: ${{ env.CPM_SOURCE_CACHE }}
          key: cpm-${{ runner.os }}-${{ hashFiles('test/CMakeLists.txt') }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}

      - name: Test
        run: ctest --preset ${{ matrix.preset }}
```

- [ ] **Step 2: Write the install-and-consume guard**

Create `.github/workflows/package.yml`. This is the only mechanical protection for the Lastrada consumption path.

```yaml
# SPDX-License-Identifier: Apache-2.0
name: Package
on: [push, pull_request, merge_group]

permissions:
  contents: read

jobs:
  install-and-consume:
    name: install+consume (${{ matrix.os }})
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ windows-2025, ubuntu-24.04, macos-15 ]
    steps:
      - uses: actions/checkout@v4
      - uses: seanmiddleditch/gha-setup-ninja@v6
      - if: runner.os == 'Windows'
        uses: ilammy/msvc-dev-cmd@v1
        with: { arch: x64 }

      # Configured exactly as the vcpkg port does: tests and examples OFF. If the
      # library ever needs Catch2 in order to CONFIGURE, this step fails -- which
      # is the point, because that failure is the port breaking, caught here.
      - name: Configure as the port does
        shell: bash
        run: |
          cmake -S . -B out/pkg -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DFORMULA_BUILD_TESTS=OFF \
            -DFORMULA_BUILD_EXAMPLES=OFF \
            -DFORMULA_INSTALL=ON \
            -DCMAKE_INSTALL_PREFIX="$PWD/stage"

      - name: Install
        run: cmake --install out/pkg

      - name: The installed package leaks nothing
        shell: bash
        run: |
          set -euo pipefail
          cfgdir=stage/lib/cmake/formula-cpp
          find stage -type f | sort
          test -f "$cfgdir/formula-cpp-config.cmake"
          test -f stage/include/formula-cpp/formula.hpp
          if grep -REn 'Catch2|CPM|_deps|FORMULA_BUILD_' "$cfgdir"; then
            echo "::error::installed package references test or dependency machinery"; exit 1
          fi
          if grep -RFn "$GITHUB_WORKSPACE" "$cfgdir"; then
            echo "::error::installed package bakes in a build-machine path"; exit 1
          fi

      - name: Consume via find_package(CONFIG)
        shell: bash
        run: |
          cmake -S test/package -B out/consume -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_PREFIX_PATH="$PWD/stage"
          cmake --build out/consume
          ctest --test-dir out/consume --output-on-failure

      # A Debug consumer against a Release install. Trivially fine for a
      # header-only library -- which is exactly the claim being checked.
      - name: Consume in a different configuration
        if: runner.os == 'Windows'
        shell: bash
        run: |
          cmake -S test/package -B out/consume-dbg -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$PWD/stage"
          cmake --build out/consume-dbg
```

- [ ] **Step 3: Validate the YAML parses**

Run:
```bash
python -c "import yaml,sys; [yaml.safe_load(open(f)) for f in ['.github/workflows/build.yml','.github/workflows/package.yml']]; print('workflows parse OK')"
```
Expected: `workflows parse OK`.

- [ ] **Step 4: Reproduce the package job locally**

Run the same sequence the workflow runs:
```bash
cmake -S . -B out/pkg -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFORMULA_BUILD_TESTS=OFF -DFORMULA_BUILD_EXAMPLES=OFF -DFORMULA_INSTALL=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage"
cmake --install out/pkg
cmake -S test/package -B out/consume -G Ninja -DCMAKE_PREFIX_PATH="$PWD/stage"
cmake --build out/consume && ctest --test-dir out/consume --output-on-failure
```
Expected: PASS, printing `formula-cpp 0.1.0 consumed successfully`.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows
git commit -m "ci: build matrix and an install-and-consume regression guard

The package job is the only mechanical protection for the Lastrada
consumption path: it configures the library exactly as a vcpkg port does
(tests and examples OFF), installs to a staging prefix, asserts the installed
config names no Catch2, CPM or build-machine path, and then consumes it
through find_package(CONFIG REQUIRED).

CPM caching uses split restore/save rather than a combined actions/cache,
because in a fan-out matrix the combined form makes every leg a writer of the
same key at once.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 10: README, CONTRIBUTING and documentation scaffold

**Files:**
- Create: `README.md`
- Create: `CONTRIBUTING.md`
- Create: `mkdocs.yml`
- Create: `docs/index.md`

**Interfaces:**
- Consumes: everything above
- Produces: nothing consumed by later tasks

- [ ] **Step 1: Write the README**

Create `README.md`. Keep it free of GitHub-only Markdown (no `[!NOTE]` callouts) — it doubles as the documentation site's front page.

```markdown
# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only.

A formula is written once with ordinary operators, carries its own documentation
and provenance, and from that single declaration you can get a number, an audit
trail, a rendering, and a documentation page.

## Status

Early. The foundation and packaging are in place; the expression layer described
in `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` is being built
phase by phase.

## Requirements

- C++23
- MSVC 19.4x (`cl` or `clang-cl`), or Clang 17+
- CMake 3.23 or newer

## Installation

### vcpkg

The primary consumption path.

### CMake, from an install tree

    cmake -S . -B build -DFORMULA_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/your/prefix
    cmake --install build

Then, in the consuming project:

    find_package(formula-cpp CONFIG REQUIRED)
    target_link_libraries(your_target PRIVATE formula-cpp::formula-cpp)

### Copy the headers

`include/` is self-contained and depends on nothing outside the standard library.

## Build options

| Option | Default | Effect |
|---|---|---|
| `FORMULA_BUILD_TESTS` | ON when top-level | Build the test suite (fetches Catch2) |
| `FORMULA_BUILD_EXAMPLES` | ON when top-level | Build the examples |
| `FORMULA_INSTALL` | ON when top-level | Generate install and export rules |
| `FORMULA_PEDANTIC` | ON | Strict warnings on the project's own targets |
| `FORMULA_WERROR` | OFF | Treat warnings as errors |

## Licence

Apache-2.0. See `LICENSE`.
```

- [ ] **Step 2: Write CONTRIBUTING**

Create `CONTRIBUTING.md`:

```markdown
# Contributing

## Building

    cmake --preset clang-debug
    cmake --build --preset clang-debug
    ctest --preset clang-debug

Presets exist for `cl`, `clang-cl`, `clang++` and `g++`. All three of the first
three must stay green.

## Invariants

These are not style preferences. Each one is load-bearing, and most exist
because the alternative was measured and failed.

1. **Every file starts with `SPDX-License-Identifier: Apache-2.0`.**
   `ctest -R hygiene.spdx` enforces it.

2. **No `NOLINT` anywhere.** Suppressions belong in `.clang-tidy`, where they
   are reviewable in one place.

3. **The exported target stays minimal.** The installed package config must
   never name Catch2, CPM, a test target, or an absolute path. The `Package`
   CI workflow asserts this.

4. **Public headers pull in no `<string>`, `<vector>`, `<format>` or
   `<iostream>`.** A consumer that only evaluates numbers must not compile them
   in every translation unit. Those belong in opt-in headers.

5. **Every public `static_assert` message begins `formula: `** and is stable,
   unique and greppable. The must-not-compile tests match on that text, so
   these strings are tested API; renaming one is a deliberate test change.

6. **Never identify a type with `decltype([]{})`.** A lambda in a default
   template argument gives the closure internal linkage, so the type differs in
   every translation unit. Verified to fail at link time on `cl`, `clang-cl`
   and `clang++`. Use a named tag type.

7. **The version is a committed literal.** Never derive it from `git describe`:
   `vcpkg_from_github` extracts a tarball with no `.git`.

## Tests

Three kinds, and new behaviour usually needs more than one:

- **Runtime** — ordinary Catch2 assertions.
- **Compile-time** — `STATIC_REQUIRE`, for anything that is a compile-time
  property. A regression should be a build failure, not a runtime report.
- **Must-not-compile** — `formula_add_negative_test(<name> <expected-text>)`
  in `test/CMakeLists.txt`, with the case in `test/negative/`. The harness
  asserts both that the build failed and that it failed with the expected
  message, so add the case with a deliberately wrong expected string first and
  watch it fail before committing the right one.
```

- [ ] **Step 3: Scaffold the documentation site**

Create `mkdocs.yml`:

```yaml
# SPDX-License-Identifier: Apache-2.0
site_name: formula-cpp
site_description: Declarative, traceable, self-documenting formulas for C++23
repo_url: https://github.com/LASTRADA-Software/formula-cpp
theme:
  name: material
  features:
    - navigation.sections
    - content.code.copy
markdown_extensions:
  - admonition
  - pymdownx.highlight
  - pymdownx.superfences
nav:
  - Home: index.md
```

Create `docs/index.md`:

```markdown
# formula-cpp

Declarative, traceable, self-documenting formulas for C++23.

The guides described in the design specification -- quantities and units,
writing formulas, composition, citations, tracing, rendering dialects, exact
numbers and rounding, and series -- are added as each phase lands. The
specification itself is in the repository under
`docs/superpowers/specs/`.

All examples on this site use generic physics with fictional `Example Standard`
citations. Real DIN/EN content lives in the downstream norm libraries.
```

- [ ] **Step 4: Verify the site builds if MkDocs is available**

Run:
```bash
python -m mkdocs build --strict 2>&1 | tail -3 || echo "mkdocs not installed -- skipping (CI will build it)"
```
Expected: either a successful build, or the skip message.

- [ ] **Step 5: Full verification pass**

Run the whole suite on all three compilers before committing:
```bash
for p in cl-debug clangcl-debug clang-debug; do
  echo "=== $p ==="
  cmake --preset $p && cmake --build --preset $p && ctest --preset $p || echo "FAILED: $p"
done
```
Expected: all three green.

- [ ] **Step 6: Commit**

```bash
git add README.md CONTRIBUTING.md mkdocs.yml docs/index.md
git commit -m "docs: README, contributing invariants, and the documentation scaffold

CONTRIBUTING records the invariants that are load-bearing rather than
stylistic -- the exported target staying minimal, public headers not pulling
in <format>, static_assert messages being tested API, and the ban on
decltype([]{}) as a type identity, which was measured to break linkage across
translation units on all three supported compilers.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"
```

---

### Task 11: Create the remote and push

Per the owner's decision, the repository is created only once the build and tests are green locally on all three compilers.

**Files:** none

**Interfaces:**
- Consumes: everything above
- Produces: `github.com/LASTRADA-Software/formula-cpp`

- [ ] **Step 1: Confirm green locally on all three compilers**

Run:
```bash
for p in cl-release clangcl-release clang-release; do
  echo "=== $p ==="
  cmake --preset $p && cmake --build --preset $p && ctest --preset $p || { echo "STOP: $p is not green"; break; }
done
```
Expected: all three green. **Do not continue otherwise** — the point of this gate is that the first public CI run is not red.

- [ ] **Step 2: Confirm the working tree is clean and nothing stray is tracked**

Run:
```bash
git status --short
git ls-files | grep -E '^(out/|stage/|\.cache/)' && echo "STRAY BUILD ARTEFACTS TRACKED" || echo "clean"
```
Expected: empty status, and `clean`.

- [ ] **Step 3: Create the repository**

The GitHub MCP server may be unavailable; `gh` is authenticated and has `LASTRADA-Software` access.

```bash
gh repo create LASTRADA-Software/formula-cpp \
  --public \
  --source=. \
  --remote=origin \
  --description "Declarative, traceable, self-documenting formulas for C++23"
```

- [ ] **Step 4: Push**

```bash
git push -u origin master
```

- [ ] **Step 5: Verify CI**

```bash
gh run list --limit 10
gh run watch
```
Expected: the `Build` and `Package` workflows both go green. If a leg fails, fix it before declaring the task done — a red default branch is worse than an unpushed one.

---

## Self-Review

**Spec coverage.** This plan implements spec §18 (project, packaging, CI) and the phase-1 row of §17, plus the §13 finding that retires `decltype([]{})`. Sections 6–12 and 14–16 (numbers, dimensions, units, quantities, expressions, provenance, tracing, series, methods) are explicitly **out of scope** and belong to later plans — see below. §19 (documentation) is scaffolded here and populated in the phase that gives it something to document.

**Follow-on plans**, one per subsystem, each producing working testable software:

| Plan | Spec phases | Deliverable |
|---|---|---|
| 2 — Exact numbers | 2 | Rational arithmetic, decimal places, rounding modes incl. round-up |
| 3 — Dimensions and units | 3 | The dimensional algebra consumers otherwise supply themselves; exact conversion |
| 4 — Quantities and metadata | 4 | `Describe<T>`, the CRTP declaration, empty propagation |
| 5 — Expression layer | 5 | Operators, `Value<T>`, evaluation returning the sum type |
| 6 — Provenance and docs | 6 | Citations, documentation generation, the populated site |
| 7 — Tracing | 7 | Composable sinks, trace arena, bounded rendering |
| 8+ | 8–15 | Rounding nodes, constraints, tables, methods, series, statistics |

**Placeholder scan.** No `TBD`, `TODO`, "similar to Task N", or "add error handling" remains. Every code step contains the actual content. Task 3 Step 5 carries a conditional instruction about `formula.hpp` not yet existing; that is a real ordering note with a stated remedy, not a placeholder.

**Type consistency.** Checked across tasks: the target is `formula-cpp` and the alias `formula-cpp::formula-cpp` everywhere (Tasks 2, 3, 4, 5, 7, 10); `formula_apply_warnings()` is defined in Task 2 and called in Tasks 4 and 5; `formula_add_negative_test(name expected)` is defined and called in Task 7; `formula::detail::index_in_tuple_v` is defined in Task 5 Step 3 and used in Task 6 Step 1; the `static_assert` text `"formula: two or more functors return the same type"` set in Task 5 Step 4 is exactly the string matched in Task 7 Step 3; `FORMULA_VERSION_STRING` is defined in Task 2 Step 1 and consumed in Tasks 3, 4 and 8. The header `FILES` list in Task 2 Step 2 is superseded by Task 5 Step 6, which is called out in both places.
