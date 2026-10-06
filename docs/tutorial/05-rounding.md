# 5. Rounding

This chapter rounds the strength to a tenth of a megapascal as part of the
formula, the way a test method prescribes it. It shows how a rounding names
its unit, its places and its mode, and what the mode decides for a value
exactly halfway.

The program is chapter 4's first case, with the strength formula rounded,
a second rounding that differs only in its mode, and two loads.

## Rounding is part of the formula

A test method states its rounding: here, the strength to 0.1 MPa, with a
value exactly halfway rounded away from zero. A `formula::DecimalRounding`
names such a rule once: the unit the places are counted in, how many places,
and the rounding mode. The program names two rules that differ only in the
mode:

```cpp
--8<-- "examples/tutorial/05_rounding.cpp:roundings"
```

Always name the mode: no single mode is right for every method, so the
method's own rule belongs in the declaration. Places count in a unit,
because one decimal place of a megapascal and one decimal place of a pascal
are different roundings.

`formula::rounded<R>(operand)` rounds its operand by the rule `R`, at that
position in the formula. The strength formula rounds the quotient:

```cpp
--8<-- "examples/tutorial/05_rounding.cpp:formulas"
```

The quotient is calculated exactly, converted to megapascals, rounded to one
place, and the rounded value is the result. A rounding inside a formula is a
step of the calculation, unlike the rounding for reading in chapter 3, which
only changes the text printed.

The specimen carried 675.4 kN:

```cpp
--8<-- "examples/tutorial/05_rounding.cpp:evaluate"
```

675.4 kN over 22500 mm² is 30.0177… MPa, which rounds to 30.0. The result is
the exact number 30, and prints as `30`: `{}` writes an exact value without
trailing zeros. A format spec that asks for one place in a named mode,
`{:.1HalfAwayFromZero}`, would print `30.0`
([Displaying numbers](../display.md#exact-fraction-or)).

## The mode matters

A second specimen carried 676.125 kN. Its strength is 30.05 MPa exactly,
halfway between 30.0 and 30.1, and the two rules decide it differently:

```cpp
--8<-- "examples/tutorial/05_rounding.cpp:halfway"
```

Half away from zero rounds the half up to 30.1. Half to even rounds it to
the neighbour whose last digit is even, 30.0. Only a value exactly halfway
tells the two modes apart, which is why the mode a method prescribes
belongs in its rounding.

## Output

```text
--8<-- "examples/tutorial/05_rounding.expected.txt"
```

## Summary

- `formula::DecimalRounding` -- a rounding rule named once: a unit, a number
  of decimal places and a rounding mode.
- `formula::DecimalPlaces` -- how many decimal places of the unit to keep.
- `formula::RoundingMode` -- which way to round, including what to do with a
  value exactly halfway; name the one the method prescribes.
- `formula::rounded<R>(operand)` -- rounds the operand by the rule `R`, at
  that position in the formula.

## Further reading

- [Rounding and conditionals](../rounding-and-conditionals.md#naming-a-rounding-once)
- [Exact numbers and rounding](../numbers.md#rounding)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
