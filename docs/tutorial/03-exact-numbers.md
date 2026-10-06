# 3. Exact numbers

This chapter measures the specimen as it really is, to a tenth of a
millimetre and a tenth of a kilonewton. It shows why `checked_evaluate`'s
results are exact, what an exact result looks like when it has no decimal,
and how to round one for reading.

The program is chapter 2's, without the second strength quantity, with
decimal measurements, and with the strength also printed rounded.

## Exact decimal literals

The specimen's sides measure 150.2 mm and 149.8 mm, and it carried 675.4 kN:

```cpp
--8<-- "examples/tutorial/03_exact_numbers.cpp:measured"
```

The `_r` literal, from `formula::literals`, makes an exact `formula::Rational`
from its spelling: `150.2_r` is 751/5, exactly 150.2, never the `double`
nearest it. `checked_evaluate` calculates in `Rational`, a fraction of two
128-bit integers, so nothing is rounded on the way in or during the
calculation. A value too large for a `Rational` is reported as an error,
never rounded ([Limits](../numbers.md#limits)). The usual example of binary
floating point going wrong holds exactly:

```cpp
--8<-- "examples/tutorial/03_exact_numbers.cpp:exact-sum"
```

With `double`, `0.1 + 0.2 == 0.3` is false. With `Rational` it is true.

## An exact result that has no decimal

The formulas and their evaluation are chapter 2's:

```cpp
--8<-- "examples/tutorial/03_exact_numbers.cpp:evaluate"
```

The area, 150.2 mm × 149.8 mm, is 22499.96 mm², which has an exact decimal
and prints as one. The strength, 675.4 kN over 22499.96 mm², is
16885000/562499 MPa. That fraction has no terminating decimal, so the result
prints as the fraction. A rounded decimal would be a different number, and
the library does not print one unless asked to.

## Rounding for reading

A reader wants a decimal. The format spec asks for one:

```cpp
--8<-- "examples/tutorial/03_exact_numbers.cpp:for-reading"
```

`{:~.2HalfEven}` names the number of decimal places, 2, and the rounding
mode, `HalfEven`. There is no default mode: the spec always names one. The
`~` writes the exact decimal where the value has one, and otherwise rounds it
and marks the rounded value with `≈`, so it cannot pass for the exact
result. The rounding happens only in the text printed; the result itself
stays exact. Rounding that is part of a method, where a standard prescribes
it, comes in chapter 5.

## Output

```text
--8<-- "examples/tutorial/03_exact_numbers.expected.txt"
```

## Summary

- `_r` -- an exact decimal literal, read from its spelling; in
  `formula::literals`.
- `formula::Rational` -- an exact fraction of two integers, the number type
  `checked_evaluate` calculates in.
- `{:~.NMode}` -- formats a value rounded to N places in the rounding mode
  named and marked `≈`; a value with an exact decimal prints unrounded.

## Further reading

- [Exact numbers and rounding](../numbers.md#writing-an-exact-decimal)
- [Displaying numbers](../display.md#the-spec)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
