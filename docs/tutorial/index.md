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
specimen's size. Each chapter extends the previous chapter's program, so read
the core track in order.

**The advanced track** covers the rest of the library in independent
chapters. Read the ones you need, in any order, once you have finished the
core track.

Every program in this tutorial is built and run by the library's test suite,
and every output shown is what that program prints. Every standard cited is
an invented `Example Standard`.

## Core track

## Advanced track
