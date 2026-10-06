# 10. Lookup tables

A test method sometimes gives a number no formula calculates: it publishes a
table and says which row to read. This chapter corrects the strength of the
test for the size of the specimen, with a factor read from a table by the
specimen's edge length, and shows what happens to an edge the table does not
cover.

The program is self-contained: it declares its own quantities, and takes the
strength as a measured 30 MPa rather than calculating it.

## Why a published table belongs in the formula

The size correction factor is a quantity of its own, without a unit, read by
the edge length of the specimen; the corrected strength is the strength
multiplied by that factor:

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:quantities"
```

The table is part of the method, so it is part of the formula. A lookup
renders, evaluates and traces like any other part of a formula, and a reader
of the formula sees the table it reads.

## Band boundaries

The invented table has three bands of edge length, each with its own factor:
0 to under 100 mm gives 1.05, 100 to under 200 mm gives 1.00, and 200 to
under 300 mm gives 0.95. `formula::BandTable<N>` holds the bands, each
written `formula::band(low, high)`:

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:bands"
```

A band includes its low bound and stops under its high bound. A value on a
boundary belongs to the band it starts, so an edge of exactly 100 mm is in
the second band, never the first. The library writes a band the same way,
"0 to under 100 mm", wherever it shows one.

`formula::banded_lookup<KeyUnit, Bands, ValueUnit>(key, values)` reads the
row whose band holds `key`:

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:lookup"
```

The template arguments are the table's structure: the unit its bands are
stated in, the bands, and the unit its values are stated in. The braced list
holds one value per band, in the order the bands are declared. The corrected
strength uses the factor's `.expression`, as any formula that reads a bound
formula does.

Rendered, the lookup shows every band and the value it gives, each value as
an exact fraction: 21/20 is 1.05, and 19/20 is 0.95.

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:render"
```

## Reading the table

The program reads the table for one edge at a time, and prints the factor
and the corrected strength:

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:report"
```

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:evaluate"
```

An edge of 150 mm is in the second band: the factor is 1 and the strength
stays 30 MPa. An edge of 100 mm sits on the boundary between the first two
bands and so is in the second: again 1, and 30 MPa. An edge of 250 mm is in
the third band: 30 MPa times 0.95 is 28.5 MPa.

## A miss is not a number

An edge of 300 mm is in no band: the last band stops under 300 mm. The
lookup does not answer with zero, with the nearest band, or with the last
row. It fails with `formula::ArithmeticError::DomainError`, through the
`std::expected` that `checked_evaluate` returns, and the program prints the
error:

```cpp
--8<-- "examples/tutorial/10_lookup_tables.cpp:miss"
```

A number for an edge the table does not cover would be a number the method
never states, and nothing after it could tell it apart from one the method
does state. A formula that reads the lookup, such as the corrected strength,
fails the same way.

## A gap between bands does not compile

The high bound of each band must be the low bound of the next. A table with
a gap between two bands, or two bands that overlap, does not compile, and
the compiler's message names the two bands. The guide shows the message in
[A table with a gap does not compile](../lookup-tables.md#a-table-with-a-gap-does-not-compile-and-the-message-says-where).

## Output

```text
--8<-- "examples/tutorial/10_lookup_tables.expected.txt"
```

## Summary

- `formula::BandTable<N>` -- a table of `N` bands, each from its low bound
  to under its high bound.
- `formula::band(low, high)` -- one band; a value on `low` is in it, a value
  on `high` is not.
- `formula::banded_lookup<KeyUnit, Bands, ValueUnit>(key, values)` -- the
  value of the band that holds `key`; an error, not a number, when no band
  holds it.

## Further reading

- [Banded: a measured value falls in an interval](../lookup-tables.md#banded-a-measured-value-falls-in-an-interval)
- [A miss is not a value](../lookup-tables.md#a-miss-is-not-a-value)
- [Lookup tables](../lookup-tables.md), which also covers exact and interpolating tables
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
