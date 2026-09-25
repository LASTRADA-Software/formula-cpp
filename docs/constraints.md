# Constraints and verdicts

A standard routinely states a relationship purely to validate a result, not
to compute one -- "the specimen shall be rejected below 30 MPa", "the two
replicates shall agree within 5%". `formula::constraint()` pairs a predicate
(`predicate.hpp`, also covered in [Rounding and conditionals](rounding-and-conditionals.md))
with what to do when it does not hold, and `formula::check()` reports what
happened as one of **four** outcomes, not two. This page explains why there
are four, works through the one case that is the entire reason this feature
exists, and shows how a constraint renders and traces.

The worked example is `examples/constraints.cpp`; every block on this page
formatted as program output is copied verbatim from that program's actual
output, the same way [Rounding and conditionals](rounding-and-conditionals.md)
does for `examples/rounding_and_conditionals.cpp`.

A `Constraint` is deliberately **not** a `Node`, for the same reason a
predicate is not one: checking it produces a verdict, which has no
dimension, and giving it one would mean inventing a dimension to lie about.
That has real consequences below -- a constraint cannot be wrapped by
`documented()`, cannot be passed to `document()`, and cannot sit inside a
formula's own operand tree. It carries its own `Citation` directly instead.

## A predicate paired with a verdict

`formula::constraint(predicate, verdict, citation = {})` builds a
`Constraint<P>`: a plain aggregate holding the predicate that must **hold**,
the `Verdict` that applies when it does not, and where the rule comes from.
The predicate is stated the way the standard states it -- as the condition
that must hold, not as the failure -- so the declaration reads the way the
standard reads:

```cpp
constexpr auto minimumStrength =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" },
                        formula::Citation { .title = "Minimum compressive strength",
                                            .reference = "Example Standard 7:2020",
                                            .section = "5.1" });
```

There is deliberately no separate verdict for success: a satisfied
constraint is silent by nature, and a slot for a pass verdict would only
invite noise an auditor then has to read past.

## Four outcomes, not two

Checking a constraint (`formula::check(constraint, environment)`) returns a
`ConstraintOutcome`, whose `kind()` is one of four `ConstraintOutcomeKind`
values:

- **`Satisfied`** -- the predicate held.
- **`Violated`** -- the predicate did not hold; `outcome.verdict()` carries
  the `Verdict` the constraint was declared with.
- **`NotChecked`** -- the predicate never resolved, because an input it
  needed was never measured.
- **`Invalid`** -- evaluating the predicate raised an arithmetic error;
  `outcome.error()` carries it.

`ConstraintOutcome` has no `operator bool()` and no plain `bool satisfied()`
convenience accessor, and that absence is deliberate rather than an
oversight. Either answer such an accessor could give for `NotChecked` or
`Invalid` is a lie: `true` collapses "held" together with "never checked" or
"broke while checking", and `false` says the specimen failed the check when
in fact no check ran at all. A caller who "just wants to know if it passed"
is exactly the caller who must instead read `kind()`, or `is_satisfied()`
*and* `is_violated()` *and* `is_not_checked()` *and* `is_invalid()`, and
decide what an unresolved check means for their own report.

## The case this phase exists for: a measurement nobody took

Here is the same constraint checked against three environments -- one where
strength was measured at 45 MPa, one at 20 MPa, and one where strength was
never measured at all:

```cpp
constexpr auto satisfied = formula::check(minimumStrength, strengthOf(45));
constexpr auto violated = formula::check(minimumStrength, strengthOf(20));
constexpr auto notChecked = formula::check(minimumStrength, nothingMeasured());
```

which report:

```
45 MPa: satisfied
20 MPa: violated (reject the specimen)
no strength measured: not checked
```

The third line is the one this whole phase exists for. Nobody measured the
strength, so the predicate `f >= 30 MPa` never resolved -- it is not true and
it is not false, because there is no `f` to compare against 30 MPa at all.
`check()` reports `NotChecked`, never `Satisfied`.

**Consider what a library that reported `Satisfied` there would be
producing.** This library's intended use is a laboratory or a compliance
report: a constraint like `minimumStrength` exists so that someone, reading
the generated report, can trust that "satisfied" means a measured specimen
was checked against 30 MPa and passed. If an unmeasured input silently
reported `Satisfied`, that trust would be misplaced every time a
measurement was skipped, forgotten, or arrived late -- the report would read
identically whether the specimen was tested and passed, or never tested at
all. That is not a rounding error or a display quirk; it is a false record
of verification, and a false record of verification is precisely what a
constraint exists to prevent, not produce.

The code backs this up directly, not only the printed word:

```cpp
bool const notCheckedIsHonest = notChecked.is_not_checked() && !notChecked.is_satisfied() && !notChecked.is_violated();
```

`NotChecked` is not a failure either. `is_violated()` is false for it too --
a specimen that was never tested has not failed a test, and reporting it as
though it had would send someone chasing a rejection that never happened.
It is its own, honest, third thing.

## A fourth state: arithmetic can break while checking, too

`NotChecked` is not the only way a predicate can fail to resolve. Checking a
constraint whose predicate divides by a measured zero raises an arithmetic
error instead of ever comparing anything:

```cpp
constexpr auto dividesByZero =
    formula::constraint((var<Strength> / formula::number(formula::Rational { 0 }))
                             > formula::constant<unit::Megapascal>(formula::Rational { 1 }),
                        formula::Verdict { "specimen result is unusable" });
```

```
divides by zero: invalid (division by zero)
```

`NotChecked` and `Invalid` stay distinct states rather than collapsing into
one "could not check" bucket, because they call for different action from
whoever reads the report: "nobody measured the input" is a data-collection
problem, and "we tried to check and the arithmetic broke" is a defect in the
rule or the data feeding it. A reader who sees only `Satisfied` and
`Violated` in a report will end up writing code that treats every other
state as one of those two -- which is exactly the failure this four-state
design exists to prevent.

## How a constraint renders -- and how it doesn't

A constraint renders as its rule alone, `require <lhs> <comparison> <rhs>`,
never its verdict:

```cpp
std::printf("rendered: %s\n", formula::render(minimumStrength).c_str());
std::printf("rendered (LaTeX): %s\n", formula::render<formula::Dialect::LaTeX>(minimumStrength).c_str());
```

```
rendered: require f >= 30 MPa
rendered (LaTeX): \text{require } f \geq 30 MPa
```

`require` names no public function -- `constraint(predicate, verdict,
citation)` is a three-argument call, so nothing here reads as a mis-spelled
invocation of it -- and it is the same keyword the trace step below uses, so
a reader who has seen a constraint in one surface recognises it in the
other. The verdict stays out of both renderings on purpose: this text states
the condition a standard asks a reader to check, and a verdict is not part
of that condition, it is what a checker does once the condition is already
decided. `check()` reaches `Satisfied` or `Violated` from the predicate
alone; the verdict is a label attached afterwards, not an ingredient the
predicate needed.

A `Constraint` cannot be wrapped by `documented()` and cannot be passed to
`document()` -- both are built around `Node`, and a constraint deliberately
is not one. It carries a `Citation` as a plain public member instead,
readable directly with no `document()` call at all:

```cpp
formula::Citation const& citation = minimumStrength.citation;
```

```
cited: Minimum compressive strength, Example Standard 7:2020, 5.1
```

## How a constraint traces

Where the verdict *does* appear is the trace. `check()` records a
constraint as its own step, in the same `require #1 >= #2` shape as the
rendering above, with a bracketed suffix naming what checking it concluded --
present for every one of the four outcomes, because nothing else in the
line carries that distinction:

```cpp
template <typename P, typename Env>
[[nodiscard]] std::string tracedCheck(formula::Constraint<P> const& subject, Env const& environment)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    [[maybe_unused]] auto const outcome = formula::check(subject, environment, sink);
    return formula::render_trace(trace, { .maxSteps = 5 });
}
```

Called once per outcome above, this prints:

```
1. f = 45 MPa
2. 30 MPa
3. require #1 >= #2 [satisfied]
```

```
1. f = 20 MPa
2. 30 MPa
3. require #1 >= #2 [reject the specimen]
```

```
1. f = (not measured)
2. 30 MPa
3. require #1 >= #2 [not checked]
```

```
1. f = 0 MPa
2. 0
3. #1 / #2 = division by zero
4. require #3 [division by zero]
```

`#1` and `#2` are the predicate's own left and right sides, recorded and
numbered exactly like any other step's operands even though `Constraint` is
not a `Node` and never gets a step of its own from `entered`/`produced` --
it goes through the parallel `constraint_entered`/`constraint_produced` hook
`RecordingSink` defines for it. `[satisfied]` and `[not checked]` are fixed
words; `[reject the specimen]` is the constraint's own `Verdict` label,
carried into the trace only once it actually applies; and `[division by
zero]` is the arithmetic error that made checking impossible, the same text
`formula::describe(ArithmeticError::DivisionByZero)` returns elsewhere in
this library. The last trace also shows why a constraint's step can have
only **one** operand rather than two: the left side, `f / 0`, itself raised
the division-by-zero error, so the right side was never dispatched and
nothing was ever compared -- `require #3`, with no comparison token at all,
for the same reason a `Conditional` step drops its own comparison in the
matching one-operand case (see [Rounding and conditionals](rounding-and-conditionals.md)).

## Checking a set: no short-circuit

A method rarely states one constraint in isolation. `formula::constraints(a,
b, ...)` bundles several into a `ConstraintSet`, and `formula::check_all()`
checks every one of them against the same environment, reporting one
`ConstraintOutcome` per constraint at the same index it was declared at.
Alongside `minimumStrength`, a second, independent constraint over a
different quantity:

```cpp
constexpr auto maximumDiameter =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 100 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" });
```

```cpp
constexpr auto setOutcomes =
    formula::check_all(formula::constraints(minimumStrength, maximumDiameter), strengthOnly(20));
```

Here strength (20 MPa) violates `minimumStrength`, and diameter was never
measured, so `maximumDiameter` cannot resolve:

```
set[0] (minimumStrength): violated
set[1] (maximumDiameter): not checked
```

**Every constraint is evaluated. There is no short-circuit.** Both outcomes
above are reported; neither suppresses the other, and the outcome at index 1
being unresolved has no effect on index 0 being reported as violated, or
vice versa. This is deliberately the opposite of `when()`
([Rounding and conditionals](rounding-and-conditionals.md)), which evaluates
**only** the branch its predicate selects -- and the reason differs rather
than the rule being inconsistent. `when()` skips a branch because evaluating
it could raise an arithmetic error that has nothing to do with the answer a
caller asked for; every constraint in a set, by contrast, *is* about the
answer, so skipping one to save work would be discarding a check the caller
actually asked for, not avoiding a meaningless one. A specimen can fail two
checks at once, and a report naming only the first sends someone back for a
second round of testing they should not have needed.

## Every citation here is invented

Every citation used to demonstrate constraints on this page and in
`examples/constraints.cpp` names a fictional `Example Standard`, never a
real one, for the reason [Citations and rendering](citations.md) gives in
full: a real standard's clause numbers and thresholds are copyrighted
material, and this is a public repository.
