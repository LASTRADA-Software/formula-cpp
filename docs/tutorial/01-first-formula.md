# 1. First formula

This chapter calculates one specimen's compressive strength from the maximum
load it carried and the area that carried it. Along the way it introduces the
four ideas everything else in the library builds on: a quantity, a formula, an
environment of measurements, and a checked evaluation.

## Add the library to a project

formula-cpp is header-only. With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake)
in `cmake/CPM.cmake`, these lines fetch it and link a program against it:

```cmake
include(cmake/CPM.cmake)
CPMAddPackage("gh:LASTRADA-Software/formula-cpp@0.4.0")

add_executable(strength main.cpp)
target_compile_features(strength PRIVATE cxx_std_23)
target_link_libraries(strength PRIVATE formula-cpp::formula-cpp)
```

The program starts with these includes:

```cpp
--8<-- "examples/tutorial/01_first_formula.cpp:includes"
```

`formula.hpp` is the library. `format.hpp` lets `std::format` and
`std::print` write its values. The headers for rendering, documentation and
tracing are separate, and chapters 7 and 8 introduce them.

## Declare the quantities

```cpp
--8<-- "examples/tutorial/01_first_formula.cpp:quantities"
```

A quantity is a type. The four template arguments used here are a tag that
makes it unique, its symbol, its description, and the unit its values are
stated in. `Load` is stated in kilonewtons, `Area` in square millimetres
and `Strength` in megapascals. Because the tag makes each quantity its own
type, two quantities with the same unit are still different types: a load
can never be passed where another kilonewton quantity is expected.

## Write the formula

```cpp
--8<-- "examples/tutorial/01_first_formula.cpp:formula"
```

`formula::var<Q>` stands for the value of `Q`, and ordinary operators combine
such values into a formula. `formula::yields<Strength>` names the quantity
the formula calculates. The formula is a compile-time object: declaring it
computes nothing, and no value exists until it is evaluated.

## Provide the measurements

```cpp
--8<-- "examples/tutorial/01_first_formula.cpp:environment"
```

`formula::Measured<Q>` holds one value of `Q`, stated in `Q`'s declared unit:
675 is a load in kilonewtons and 22500 an area in square millimetres.
`formula::environment()` collects the measurements a formula is evaluated
against.

## Evaluate, and check the result

```cpp
--8<-- "examples/tutorial/01_first_formula.cpp:evaluate"
```

`formula::checked_evaluate` returns a `std::expected`. An arithmetic failure,
such as a division by zero, is an error value: never an exception, and never
a silent zero. The program checks the result before reading it, and prints
the error and returns 1 if there is one.

The result prints as a number and its unit. `formula::symbol_of<Strength>()`
gives the quantity's symbol, and `source()` says where the value came from:
the library derived it from the formula.

## Output

```text
--8<-- "examples/tutorial/01_first_formula.expected.txt"
```

The formula divides kilonewtons by square millimetres, and the result came
out in megapascals with no conversion written; chapter 2 explains why.

## Summary

- `formula::Quantity` -- declares a quantity as a type; the four template
  arguments used here are tag, symbol, description and unit.
- `formula::var<Q>` -- stands for the value of `Q` in a formula.
- `formula::yields<Q>` -- names the quantity a formula calculates.
- `formula::Measured<Q>` -- one measured value of `Q`, in `Q`'s declared unit.
- `formula::environment()` -- collects the measurements a formula is
  evaluated against.
- `formula::checked_evaluate` -- evaluates a formula, returning a
  `std::expected` that holds the result or the arithmetic error.
- `formula::symbol_of<Q>()` -- the symbol of `Q`.
- `source()` -- says where a result's value came from.

## Further reading

- [Quantities and measurements](../quantities.md#declaring-a-quantity)
- [Expressions and evaluation](../expressions.md#writing-a-formula)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
