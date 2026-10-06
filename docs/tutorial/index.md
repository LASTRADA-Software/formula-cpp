# Tutorial

This tutorial teaches formula-cpp step by step. It assumes you know modern
C++ -- C++20 and C++23, `constexpr`, `std::expected`, class-type template
arguments -- and nothing about this library.

## Before you start

You need a C++23 compiler (MSVC, clang-cl, Clang, GCC 14 or AppleClang),
CMake 3.23 or newer, and [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake)
to fetch the library. Chapter 1 shows the CMake lines.

## Two tracks

**The core track** builds one program: the calculation of a concrete
specimen's compressive strength, from the load that crushed it and the
specimen's size. Each chapter starts from the previous chapter's program, so
read the core track in order.

**The advanced track** covers the rest of the library in independent
chapters. Read the ones you need, in any order, once you have finished the
core track.

Every program in this tutorial is built and run by the library's test suite,
and every output shown is what that program prints. Every standard cited is
an invented `Example Standard`.

## Core track

1. [First formula](01-first-formula.md) -- one formula, evaluated and checked.
2. [Units and dimensions](02-units-and-dimensions.md) -- sides, areas and unit conversion; a mistake that does not compile.
3. [Exact numbers](03-exact-numbers.md) -- why the results are exact, and how to round them for reading.
4. [Missing and entered values](04-missing-and-entered.md) -- a measurement nobody took, and a value typed in.
5. [Rounding](05-rounding.md) -- rounding where the method says, in the mode it names.
6. [Constraints](06-constraints.md) -- rules a specimen must meet, and the four outcomes of checking one.
7. [Citations, rendering and documentation](07-documentation.md) -- where a formula comes from, written out as text, LaTeX and a symbol table.
8. [Tracing](08-tracing.md) -- how each number was reached, step by step.
9. [Calculations and worksheets](09-worksheets.md) -- the whole test as one calculation, recalculated as inputs change.

## Advanced track

- Chapter 10: [Lookup tables](10-lookup-tables.md) -- values from a published table, and what happens outside it.
- Chapter 11: [Methods and overlays](11-methods-and-overlays.md) -- one method, several variants, and a jurisdiction's changes.
- Chapter 12: [Series](12-series.md) -- one quantity at several ages, calculated element by element.
- Chapter 13: [Statistics](13-statistics.md) -- means and spreads of several specimens, and outliers rejected.
- Chapter 14: [Records](14-records.md) -- values from a reference sample or an earlier test.
- Chapter 15: [Opaque operations and bounded retry](15-opaque-and-retry.md) -- a least-squares fit, and a retest repeated at most a fixed number of times.
