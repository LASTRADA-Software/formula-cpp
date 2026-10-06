# 14. Records

A strength is sometimes reported against another specimen's: as a share of a
reference sample's strength, or compared with an earlier test of the same
sample. The formula then reads a value from a record other than the one being
evaluated, and a trace that lists only numbers cannot say whose each one was.
This chapter divides a specimen's strength by a reference sample's, read from
that sample's record, and shows what the trace says about each value and what
happens when the reference has not been tested yet.

The program is self-contained: it declares its own quantities and does not
build on the previous chapters' programs.

## A value from another sample or an earlier test

The specimen's strength and the reference sample's are the same quantity,
each held by its own record. The ratio of the two is a pure number. A formula
never names a sample: it names a **role**, a type the program declares, here
`Reference`. Which sample plays that role is data, decided when the records
are put together:

```cpp
--8<-- "examples/tutorial/14_records.cpp:quantities"
```

`formula::from_record<Reference>(expression)` evaluates `expression` against
the record that plays `Reference`. Outside it, the formula reads the record
being evaluated:

```cpp
--8<-- "examples/tutorial/14_records.cpp:ratio"
```

Each record holds its own environment. `formula::record<Role>(key, environment)`
makes one, and its key is a sample and a test, `formula::sample_id` and
`formula::test_id`: two separate types, so a swapped pair does not compile.
`formula::ThisRecord` is the library's role for the record being evaluated.
`formula::record_context` holds the records by role:

```cpp
--8<-- "examples/tutorial/14_records.cpp:records"
```

The context is this record's environment as well, so `formula::checked_explain`
and everything else that takes an environment takes it. A formula that reads
from a role the context does not bind does not compile. An earlier test of
the same sample is read the same way: a second role, played by a record with
the same sample key and another test key.

Rendered, the read from the other record names its role:

```cpp
--8<-- "examples/tutorial/14_records.cpp:render"
```

## Where each value came from

`report` explains the ratio over a context, prints the trace, then the ratio:

```cpp
--8<-- "examples/tutorial/14_records.cpp:report"
```

The specimen's strength is 30 MPa and the reference sample's 32 MPa:

```cpp
--8<-- "examples/tutorial/14_records.cpp:evaluate"
```

The trace keeps, on every step, the record it was read from. The specimen's
own strength is line 1, with no record named. Line 2 is the reference
sample's strength, and names the role and both keys, sample 23, test 3: two
tests of one sample share the sample key, so a sample alone could name
either. Line 3 is the read from the other record, and line 4 divides the
two: 30/32 is 15/16, the exact decimal 0.9375.

## A record not yet made

The reference sample may not have been tested yet. Its record then keeps its
role, and has no key and no values. `Record<Role, Environment>::unbound()`
makes it, and has the same type as the record
`formula::record<Reference>(...)` makes from `referenceSample`, so one
formula takes one context type whether the reference has been tested or not:

```cpp
--8<-- "examples/tutorial/14_records.cpp:not-yet-made"
```

The read gives no value: line 2 says no record is bound, and the ratio is
`(not measured)`, never zero. A ratio of zero would read as a specimen with
no strength.

## Output

```text
--8<-- "examples/tutorial/14_records.expected.txt"
```

## Summary

- `formula::from_record<Role>(expression)` -- `expression` evaluated against
  the record that plays `Role`.
- `formula::record<Role>(formula::record_key(sample, test), environment)` --
  a record, its key and its values.
- `formula::record_context(records...)` -- the records by role; it is this
  record's environment, so everything that takes an environment takes it.
- `formula::Record<Role, Environment>::unbound()` -- a record not yet made: a
  read from it gives no value.

## Further reading

- [A role is code, a record is data](../records.md#a-role-is-code-a-record-is-data)
- [What the trace says](../records.md#what-the-trace-says)
- [A record not yet made](../records.md#a-record-not-yet-made)
- [Other samples and other tests](../records.md), which also covers computing over another specimen, series, and lineage
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
