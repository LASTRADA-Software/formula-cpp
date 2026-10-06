# 9. Calculations and worksheets

This chapter puts the whole test into one calculation: the loaded area, the
volume and density of the specimen, and its strength, each defined once and
read by name wherever another definition needs it. A worksheet holds the
measurements, calculates each value when it is asked for, and after a change
recalculates only what the change reaches.

The program is chapter 8's without the tracing, with the two bound formulas
replaced by one calculation, and the specimen's height and mass added.

## One calculation, several steps

The specimen's height and mass are measured, and its volume and density are
calculated, so each is a quantity of its own:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:quantities"
```

`formula::define<Q>(expression)` says that the quantity `Q` is calculated by
`expression`, and `formula::calculation(...)` puts the definitions together:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:calculation"
```

`var<Area>` in the volume's and the strength's definitions reads the value
the area's definition calculates; it is not the area's formula written into
theirs, so the area is calculated once. A quantity that is read and never
defined is an input: here the two sides, the height, the mass and the load.

The order the definitions are given in does not matter. The calculation works
out, while the program compiles, what each definition reads, and calculates
every value after the values it reads. Each definition is checked where it is
written: a definition whose expression does not measure what its quantity
measures does not compile.

## A worksheet

`formula::worksheet(calculation, environment)` holds a calculation's inputs,
given as an environment of measurements, as for a formula. Every input must
be given. Nothing is calculated when the worksheet is made:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:worksheet"
```

The program asks for values in one function, which it calls after each
change:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:report"
```

`sheet.checked_calculate(var<Density>, var<Strength>)` calculates what the
two answers need, each value once, and keeps every result. It returns a
`std::tuple` with one `std::expected` per value asked for, in the order
asked, and each is checked before it is read. `calculate` is the same
question in its throwing form.

`recomputed()` is how many values the worksheet has calculated since it was
made, and `reused()` how many it found still up to date after a change, and
did not calculate again. Both are running totals, so the function reads them
before it asks and after, and prints how far each moved.

The first question calculates four values: the area, 22500 mm²; the volume,
3375000 mm3; the density, 8.1 kg in 0.003375 m3, which is 2400 kg/m3; and
the strength, 30 MPa. `{:~.1HalfEven}` writes the density as its exact
decimal where it has one, and rounds it to one decimal place, marked `≈`,
where it has none.

## Change an input

`sheet.set(...)` gives an input a new value:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:change"
```

Setting an input calculates nothing. The worksheet marks the values the
change reaches, and the next question recalculates those it needs. A new
load reaches only the strength: 676.125 kN over 22500 mm² is 30.05 MPa,
rounded half away from zero to 30.1 MPa. One value is recalculated. The
area, the volume and the density read nothing that changed, so they are kept
as they are; `reused()` counts only values a change reached, so it does not
move.

## What if

`sheet.with(...)` answers a what-if question on a copy, and leaves the
worksheet it was asked of as it was:

```cpp
--8<-- "examples/tutorial/09_worksheets.cpp:what-if"
```

The copy holds every value the worksheet has calculated, so it recalculates
only what the change reaches: the density, 8.25 kg in 0.003375 m3, which is
22000/9 kg/m3. That number has no exact decimal, so it is written rounded,
`≈2444.4 kg/m3`. Asked again, the worksheet itself still reports
2400 kg/m3, and calculates nothing.

## Output

```text
--8<-- "examples/tutorial/09_worksheets.expected.txt"
```

## Summary

- `formula::calculation(...)` -- several definitions put together; what
  reads what is worked out while the program compiles.
- `formula::define<Q>(expression)` -- the quantity `Q` is calculated by
  `expression`, and read elsewhere as `var<Q>`.
- `formula::worksheet(calculation, environment)` -- a calculation's inputs,
  and each value calculated once and kept.
- `checked_calculate(var<Q>...)` -- the values asked for, each a
  `std::expected`; `calculate(var<Q>...)` is the throwing form.
- `set(...)` -- gives an input a new value; only what the change reaches is
  recalculated.
- `with(...)` -- a copy with the change made, the worksheet left unchanged.
- `recomputed()` -- how many values the worksheet has calculated.
- `reused()` -- how many values a change reached that were still up to date.

## Further reading

- [Defining named values](../calculations.md#defining-named-values)
- [A worksheet](../calculations.md#a-worksheet)
- [A change, and what it reaches](../calculations.md#a-change-and-what-it-reaches)
- [What if?](../calculations.md#what-if)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
