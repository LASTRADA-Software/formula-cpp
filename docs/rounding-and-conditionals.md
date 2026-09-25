# Rounding and conditionals

Two additions to the node vocabulary -- rounding a formula at the position
the method specifies, and selecting between two formulas by a numeric
threshold -- plus a deliberately loud escape hatch for the one kind of rule
that cannot be expressed honestly any other way. All three render, document
and trace exactly like every other node this library ships; nothing here is
a special case anywhere except in the one place it has to be.

The worked example is `examples/rounding_and_conditionals.cpp`; every block on
this page formatted as program output is copied verbatim from that program's
actual output, the same way [Tracing and audit trails](tracing.md) does for
`examples/tracing.cpp`.

`RoundingMode`, `DecimalPlaces`, `SignificantDigits` and the plain-`Rational`
`formula::round()` free function already exist -- [Exact numbers](numbers.md)
covers them, including the intermediate-versus-final rounding argument made
there on bare numbers. What is new here is rounding as a **position in a
formula tree**: a node with a unit and a dimension, composable with `+`, `*`,
`when()` and everything else a `Node` supports, rather than a function you
call on a `Rational` you already have in hand.

## Rounding is a node, and it names a unit

`rounded<U, Places, Mode>(operand)` and `rounded_to_digits<U, Digits,
Mode>(operand)` (`rounding_node.hpp`) round `operand` to `Places` decimal
places, or `Digits` significant digits, of the unit `U`, under the tie-break
rule `Mode`:

```cpp
constexpr auto coarseInput =
    formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
constexpr auto toTwoSignificantDigits =
    formula::rounded_to_digits<unit::Millimetre, SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
```

The unit is not decoration. "To one decimal place" means nothing about a
quantity until you say one decimal place *of what*: the evaluator works in
the coherent SI unit of each dimension, so a length is normally carried in
metres, and "one decimal place" of a metre and of a millimetre are different
thresholds by three orders of magnitude. A rounding node converts into the
unit it names, rounds there, and converts back -- and naming a unit that does
not measure the operand's own dimension (rounding a mass "to 0.1 mm", say) is
a compile error, the same way a dimensional mismatch anywhere else in this
library is one.

`RoundingMode` itself is not new -- it has been part of `rounding.hpp` since
phase 2, and a rounding node is simply that mode exposed as a position in the
tree rather than a call you make on a number you already hold.

## The reason this is a node at all

A method may specify "round the diameter to the nearest millimetre before
doubling it" -- a coarse instrument that only ever reads whole millimetres --
or "double the diameter, then round the result to one decimal place". Both
are legitimate, and they are not the same formula. A library that only lets
you round the number you print at the end cannot express the first one at
all; it can only ever give you the second answer, silently, regardless of
which one the method actually calls for.

```cpp
constexpr auto coarseInput =
    formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
constexpr auto roundThenDouble = coarseInput + coarseInput;
constexpr auto doubleThenRound = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(
    var<Diameter> + var<Diameter>);
```

`roundThenDouble` rounds the input to a whole millimetre first and doubles
the rounded value; `doubleThenRound` doubles first and rounds only the final
result. Same measured diameter, same shape of formula -- one rounding node,
one addition -- and, on 12.50 mm:

```
rendered: round(d + d, to 1 dp of mm)
12.50 mm, round to 0 dp then double = 26.000000 mm
12.50 mm, double then round to 1 dp = 25.000000 mm
```

26 and 25 are not close-enough-to-agree; they are two different numbers, from
the same formula and the same input, because 12.50 mm rounds to 13 mm before
doubling (half away from zero) but the doubled value, 25.00 mm, was already
past the point where rounding to one decimal place could move it. Whichever
of the two a specification calls for, this library can say which one it did.

Significant-digit rounding is the same node, spelled `rounded_to_digits`, and
is a genuinely different operation from rounding to a fixed number of
decimal places -- the two can and do disagree, exactly as
[Exact numbers](numbers.md) already shows for plain `Rational` values:

```
12.34 mm to 2 significant digits    = 12.000000 mm
```

## A numeric threshold selects between two formulas

`when(predicate, thenBranch, elseBranch)` (`conditional.hpp`) evaluates
`predicate`; if it holds, it evaluates and returns `thenBranch`, and if it
does not, `elseBranch` -- and it evaluates **only** the branch it takes. A
formula guarded by `when(v != 0, x / v, fallback)` exists precisely because
the other branch is invalid for these inputs, so evaluating it anyway could
raise an arithmetic error that has nothing to do with the answer.

A predicate compares two expressions with `<`, `<=`, `>`, `>=`, `==` or `!=`
(`predicate.hpp`). It is deliberately **not** itself a `Node` -- a truth
value has no unit, and giving a predicate a dimension would mean either
inventing one to lie about or weakening what every other node in this
library promises. Both sides of a comparison must measure the same
dimension, and both branches of a `when()` must too -- each a compile-time
check, the same shape as every other dimensional-agreement check this
library makes.

A specimen reported to the nearest millimetre once it is large enough, and
to one decimal place below that, puts the threshold itself inside the
formula rather than in an `if`/`else` a caller has to remember to apply the
same way every time:

```cpp
constexpr auto sizeAdjustedDiameter =
    formula::when(var<Diameter> > formula::constant<unit::Millimetre>(formula::Rational { 20 }),
                 formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>),
                 formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter>));
```

which renders, and evaluates on both sides of its own threshold, as:

```
rendered: if d > 20 mm then round(d, to 0 dp of mm) else round(d, to 1 dp of mm)
12.34 mm, size-adjusted rounding    = 12.300000 mm
25.40 mm, size-adjusted rounding    = 25.000000 mm
```

One caveat worth knowing before it surprises you: `document()` walks **both**
branches of a `when()` for the symbol table, regardless of which one a given
evaluation actually took. That is deliberate and the opposite of evaluation's
own short-circuiting -- a formula's documentation describes the formula
itself, not the one run that happened to produce it, and a variable read
only in the branch not taken this time still belongs in the symbol table. A
generated page must not depend on which inputs happened to be passed in.

## How rounding, `numeric_value_of` and conditionals render and trace

A rounding node renders as `round(<operand>, to <places> dp of <unit>)`, or
`sf` in place of `dp` for significant digits; `numeric_value_of` (below)
renders as `numeric(<operand>, in <unit>)`. The operand comes first and the
granularity second, comma-separated, deliberately -- not because it looks
tidier, but because the alternative shapes both have a real failure mode a
review actually caught. A trailing suffix with nothing separating it from
the operand (`round(<operand> to 1 dp of mm)`) misattaches to whichever
branch of a `when()` operand happens to render last, with no closing
delimiter of its own to stop it; and the fix that was tried before this one,
a `[...]` prefix (`round[to 1 dp of mm](d)`), collided with CommonMark's
inline-link syntax and made a Markdown renderer drop the operand from the
visible page entirely. The comma form is safe against both at once. See
[Citations and rendering](citations.md) for the other rule every dialect
follows the same way, for the same kind of reason -- wrapping a symbol
containing an underscore in backticks so Markdown does not read it as
emphasis.

A conditional renders as `if <predicate> then <then> else <else>` -- seen
above -- and a trace names which branch it took as a trailing clause, the
same way a citation trails a `Documented` step:

```
1. d = 127/5 mm
2. 20 mm
3. d = 127/5 mm
4. round(#3, to 0 dp of mm) = 25 mm
5. when(#1, #2, #4) = 1/40 [then]
```

25.40 mm is above the 20 mm threshold, so the trace's last line says
`[then]`; it would say `[else]` had the predicate not held, or `[no branch]`
had the diameter never been measured at all -- never "false", which would
misreport a predicate that never resolved. `#1` and `#2` are the predicate's
two sides, recorded and numbered exactly like any other step's operands, even
though `PredicateNode` itself is not a `Node` and never gets a step of its
own. Note too that step 5's own value, `1/40`, carries no `mm` -- a `when()`
step is a computed value like any other, and every computed step is shown in
the coherent SI unit of its dimension with no symbol at all, the same rule
[Tracing and audit trails](tracing.md) explains for `#1 / #2` in a plain
division. For a second worked derivation of a conditional -- a different
formula, a different threshold, still naming the branch it took -- see
[the gallery](gallery.md).

## The traced escape hatch: `numeric_value_of`

Some published rules are fits whose coefficients only work when the input is
expressed in one particular unit -- an empirical formula stated over "the
numeric value of the strength in MPa" rather than over the strength itself.
Read the same physical quantity in a different unit and the rule feeds a
different number into the same coefficients, silently, because it was never
meant to be evaluated in any other unit. That is not a quantity any more; it
is a rule that only happens to be stated using one, and this library cannot
make it dimensionally consistent because it genuinely is not.

`numeric_value_of<U, Justification>(operand)` (`escape.hpp`) is the one place
this library lets you drop a dimension on purpose, and it is built to make
that hole as narrow and as loud as it can be: it names the unit the number
must be read in, it produces a value that is honestly dimensionless rather
than a quantity wearing one, and it demands a `Justification` -- a
compile-time string that cannot be empty, cannot be blank, and cannot be
omitted -- recording *why* this one rule needed a bare number. Leaving it out,
or writing one that says nothing, is a compile error, not a lint warning.

**Reaching for this should feel wrong, every time except the one time it is
actually the answer.** It exists for a rule that is genuinely, unavoidably
stated over a bare number in one unit -- not for a dimensional mismatch you
would rather not fix. If you find yourself adding `numeric_value_of` to make
a `static_assert` about mismatched dimensions go away, stop: that assertion
is the library telling you two things do not measure what you think they
measure, and this escape hatch will happily carry that mistake forward
without ever converting it back, because converting it back is exactly what
it is built not to do.

```cpp
constexpr auto empiricalCorrection =
    formula::numeric_value_of<unit::Megapascal,
                              "Example Standard 9:2020 states this empirical coefficient over the numeric "
                              "value of strength in MPa">(var<Strength>)
        * formula::Rational { 2, 100 }
    - formula::Rational { 1 };
```

which renders and traces as:

```
rendered: numeric(f, in MPa) * 1/50 - 1
empirical correction factor at 70 MPa = 0.400000
1. f = 70 MPa
2. numeric(#1, in MPa) = 70 (Example Standard 9:2020 states this empirical coefficient over the numeric value of strength in MPa)
3. 1/50
4. #2 * #3 = 7/5
5. 1
6. #4 - #5 = 2/5
```

The justification is not merely carried -- it is rendered, on the step
itself, in the trace above. That is the whole point of the escape hatch: a
number left a named unit for a stated reason, and an audit trail that
recorded the reason without ever showing it would be exactly the silent
failure mode this node exists to prevent.
