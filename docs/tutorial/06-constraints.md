# 6. Constraints

This chapter checks the specimen's size against a tolerance before its
strength is trusted. It shows how a rule pairs a condition with what to do
when it fails, the four outcomes of checking one, and how to check several
at once.

The program keeps chapter 5's two sides, and adds four rules on them and
three specimens to check; it calculates no strength.

## A rule the result must meet

The test method requires each side of the loaded face to measure 150 mm
within 1 mm. A `formula::constraint` pairs a predicate, the condition that
must hold, with a `formula::Verdict`, what to do when it does not.
`formula::constant<unit::Millimetre>(149)` is a fixed length in the
predicate, with its unit, so it is compared with a side in the same
dimension:

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:constraints"
```

A predicate compares two values; it does not combine comparisons. The
tolerance on each side is therefore two rules, a lower and an upper limit,
with the same verdict.

The program checks three specimens. The third has no measurement of side b:

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:specimens"
```

`formula::check(constraint, environment)` checks one rule and returns a
`formula::ConstraintOutcome`. The program checks the upper limit on side b
against the second specimen, whose side b measures 152.5 mm:

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:check"
```

The rule does not hold, so the outcome is `violated` and carries the rule's
verdict.

## Four outcomes, not two

Checking a rule has four outcomes, and `kind()` names which one it reached:

- `satisfied` -- the predicate held.
- `violated` -- the predicate did not hold; `verdict()` returns the rule's
  verdict.
- `not checked` -- the predicate never resolved, because a value it reads
  was never measured. An unmeasured side is never reported as satisfied: a
  record saying the specimen was verified when nothing verified it is worse
  than no check at all.
- `invalid` -- checking itself failed, because evaluating the predicate
  raised an arithmetic error; `error()` returns it. See
  [A fourth state](../constraints.md#a-fourth-state-arithmetic-can-break-while-checking-too)
  in the guide.

`ConstraintOutcome` has no conversion to `bool`, because any answer it gave
for `not checked` or `invalid` would be wrong. The program prints each
outcome by asking for the verdict and the error, which are present only for
`violated` and `invalid`:

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:print"
```

None of the rules here can fail to evaluate, so an `invalid` outcome would
be a fault: the program prints it and exits with an error.

## Checking several rules

`formula::constraints(...)` bundles the rules into a set, and the program
names each rule for printing, in the same order:

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:set"
```

`formula::check_all(set, environment)` checks every rule in the set and
returns one outcome per rule, at the index the rule was given. It checks
every rule without stopping at the first failure: a specimen can fail two
rules at once, and a report naming only the first would send it back for a
second round of testing.

```cpp
--8<-- "examples/tutorial/06_constraints.cpp:check-all"
```

The first specimen meets all four rules. The second fails the upper limit
on side b, and every other rule is still reported. The third meets both
rules on side a; both rules on side b are not checked, since nobody measured
it.

## Output

```text
--8<-- "examples/tutorial/06_constraints.expected.txt"
```

## Summary

- `formula::constraint(predicate, verdict)` -- a rule: a condition that
  must hold, and what to do when it does not.
- `formula::Verdict` -- what to do when a rule is violated, in words.
- `formula::constant<Unit>(value)` -- a fixed value with its unit, for use
  in a predicate.
- `formula::check(constraint, environment)` -- checks one rule; the outcome
  is satisfied, violated, not checked or invalid.
- `formula::check_all(set, environment)` -- checks every rule in a set,
  without stopping at the first failure.
- `formula::constraints(...)` -- bundles rules into a set for `check_all`.

## Further reading

- [Constraints and verdicts](../constraints.md#a-predicate-paired-with-a-verdict)
- [Checking a set: no short-circuit](../constraints.md#checking-a-set-no-short-circuit)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
