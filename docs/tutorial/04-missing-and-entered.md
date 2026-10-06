# 4. Missing and entered values

This chapter evaluates the strength three times: with every measurement
taken, with one side of the specimen never measured, and with the strength
typed in by hand. It shows what a missing measurement makes of a result, and
how a result records where its number came from.

The program is chapter 3's, with whole-number measurements, without the
printed area and the rounding for reading, and with two more environments.

## Every measurement taken

The specimen measures 150 mm by 150 mm and carried 675 kN:

```cpp
--8<-- "examples/tutorial/04_missing_and_entered.cpp:measured"
```

675 kN over 22500 mm² is exactly 30 MPa. The library calculated it, so its
source is `derived`.

## A measurement nobody took

Side b was never measured. `formula::Measured<SideB>::absent()` says so:

```cpp
--8<-- "examples/tutorial/04_missing_and_entered.cpp:absent"
```

The evaluation still succeeds: nothing went wrong in the arithmetic, so the
`std::expected` holds an outcome, not an error. That outcome holds no number.
An absent input makes the result of every operator it reaches absent, so the
area is absent, and with it the strength. `formula::number_of` returns the
number an outcome holds as a `std::optional`, here an empty one, and the
outcome's `kind()` prints as `empty`.

Empty is not zero. Zero is a measurement, and a side of zero millimetres
would make the strength a division by zero, or, in another formula, a
confident wrong number. An empty result says that the number cannot be
known from the measurements given.

## A value entered by hand

Sometimes a person states a result instead of letting the formula calculate
it: a value taken from an earlier report, say. `formula::entered` marks a
measurement as typed in, and the environment carries it next to the
measurements:

```cpp
--8<-- "examples/tutorial/04_missing_and_entered.cpp:entered"
```

The sides and the load still give 30 MPa, but the strength entered by hand,
31 MPa, overrides the formula, and the result is 31 MPa. Its `source()` is
`formula::ValueSource::ManuallyEntered`, printed as `manually entered`, and
`is_overridden()` is true. A report can therefore show which numbers the
library derived and which a person asserted.

## Output

```text
--8<-- "examples/tutorial/04_missing_and_entered.expected.txt"
```

## Summary

- `formula::Measured<Q>::absent()` -- a measurement that was not taken; every
  result that needs it is empty, never zero.
- `formula::entered(measurement)` -- marks a value as typed in by a person;
  it overrides the formula that would calculate it.
- `formula::number_of(outcome)` -- the number an outcome holds, as a
  `std::optional`; empty when it holds none.
- `formula::ValueSource` -- where a result's number came from: `Derived`,
  `Measured` or `ManuallyEntered`.
- `is_overridden()` -- true when the result is a value entered by hand.

## Further reading

- [Quantities and measurements](../quantities.md#measurements-that-may-be-absent)
- [Expressions and evaluation](../expressions.md#absence-propagates)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
