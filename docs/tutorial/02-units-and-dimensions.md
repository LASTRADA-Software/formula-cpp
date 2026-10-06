# 2. Units and dimensions

This chapter calculates the loaded area from the specimen's two sides instead
of measuring it, states the strength in a second unit, and shows what happens
to a formula that adds quantities of different dimensions.

The program is chapter 1's, with the area calculated by a formula of its own,
and the strength also evaluated in newtons per square millimetre.

## Compute the area from the sides

The program now opens its anonymous namespace with three declarations that
shorten what follows:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:names"
```

`var<Q>` can now be written without `formula::`, and the units as
`unit::Millimetre` rather than `formula::unit::Millimetre`.
`formula::literals` holds the `_r` literal, which chapter 3 uses.

The specimen's loaded face is a rectangle, so the area is the product of its
two sides. Each side is a quantity of its own, stated in millimetres:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:sides"
```

The area becomes a formula, and the strength formula uses it:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:formulas"
```

A formula can use another formula. `loadedArea` is bound to its result,
`Area`, by `yields`, and a bound formula is the top of a formula rather than
a part of one; the formula it holds, `loadedArea.expression`, is an operand
like any other. `var<SideA> * var<SideB>` multiplies millimetres by
millimetres, so its unit is mm², the unit `Area` declares, and its dimension
is an area. `yields<Area>` checks that dimension when the program compiles.

The measurements are now the two sides and the load:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:environment"
```

Both formulas are evaluated against them, and each result is checked before
it is read:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:evaluate"
```

## Units convert themselves

A second quantity states the same strength in newtons per square millimetre:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:other-unit"
```

The program evaluates the strength formula's expression for this quantity:

```cpp
--8<-- "examples/tutorial/02_units_and_dimensions.cpp:evaluate-other-unit"
```

`formula::checked_evaluate<StrengthInNewtons>` names the quantity to
evaluate for, as `yields` does for a bound formula. 30 MPa and 30 N/mm² are
one value: a megapascal is a newton per square millimetre. The declared unit
of the result quantity decides how the value is stated, and the program
converts nothing itself. The conversion factors between units are exact
ratios, so a conversion never adds a rounding error.

## A dimensional mistake does not compile

A load and an area measure different dimensions, so their sum measures
nothing. A formula that adds them is refused:

```cpp
--8<-- "test/negative/tutorial_load_plus_area.cpp:mistake"
```

The program does not compile. g++'s report begins:

```
static assertion failed: formula: the two sides of this addition or subtraction measure different dimensions
```

The library's test suite compiles this line and checks that the compiler
refuses it with this message. Every compiler reports it where the formula is
written, before the program can run.

## Output

```text
--8<-- "examples/tutorial/02_units_and_dimensions.expected.txt"
```

## Summary

- `.expression` -- the formula a bound formula holds; it is how one formula
  uses another.
- `formula::checked_evaluate<Q>` -- evaluates a formula for the quantity `Q`,
  stated in `Q`'s declared unit.
- A quantity's declared unit -- decides how its value is stated; conversion
  between units is exact and needs no code.
- Adding or subtracting quantities of different dimensions -- refused when
  the program compiles.

## Further reading

- [Dimensions and units](../dimensions.md#exact-conversion)
- [Expressions and evaluation](../expressions.md#composing-a-formula-from-other-formulas)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
