# Exact numbers and rounding

`formula-cpp` provides an exact rational number, `formula::Rational`, and a
rounding layer built on top of it. This page explains why that exists, how to
construct and use it, and where its limits are.

## Why not `double`

Binary floating point cannot represent most decimal fractions exactly, and it
cannot accumulate them exactly either. Summing `0.1` ten times in `double`:

```cpp
double sum = 0.0;
for (int i = 0; i < 10; ++i)
    sum += 0.1;
// sum is 0.99999999999999988898, not 1.0
```

That is not a rare edge case; it is what binary floating point does with
decimal input in general. A quantity such as 450 millilitres, stored as
0,45 litres in a `double` and converted back, is not reliably 450 again --
the round trip is lossy because 0,45 is not exactly representable in base 2.
`Rational` makes that round trip exact:

```cpp
Rational const volumeInMillilitres = *Rational::from_decimal(45, 1);       // 450/1
Rational const volumeInLitres = volumeInMillilitres / Rational { 1000 };   // 9/20
Rational const roundTripped = volumeInLitres * Rational { 1000 };
// roundTripped == volumeInMillilitres, exactly
```

The second reason is more fundamental than accumulated error: rounding rules
in measurement and reporting methods are *specified behaviour*, not
formatting. "Round the result to two decimal places, ties away from zero" is
part of the method that produced the number, not a choice made when printing
it. A number type that cannot represent decimal values exactly cannot apply
such a rule faithfully -- it is already carrying error before the rounding
rule is even applied. `Rational` is exact so that rounding, when it happens,
is the only place precision is deliberately given up.

## Constructing a `Rational`

| You want | Write | You get |
|---|---|---|
| an integer | `Rational { 7 }` | 7/1 |
| a fraction | `Rational { 3, 4 }` | 3/4 |
| an exact decimal | `Rational::from_decimal(45, -2)` | 9/20 |
| a whole number of tens | `Rational::from_decimal(45, 1)` | 450/1 |
| the exact value of a `double` | `Rational::from_double_exact(0.45)` | a power-of-two denominator |
| a measured `double` on a known scale | `rational_from_double(0.45, DecimalPlaces { 2 }, mode)` | 9/20 |

Spelled out, as runnable code:

```cpp
Rational const a { 7 };                                              // 7/1
Rational const b { 3, 4 };                                           // 3/4
Rational const c = *Rational::from_decimal(45, -2);                  // 9/20
Rational const d = *Rational::from_decimal(45, 1);                   // 450/1
Rational const e = *Rational::from_double_exact(0.45);               // a power-of-two denominator
Rational const f =
    *formula::rational_from_double(0.45, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero); // 9/20
```

`from_decimal`, `from_double_exact` and `rational_from_double` all return
`std::expected<Rational, ArithmeticError>` because the conversion can fail --
`from_decimal`'s scale factor can overflow, and `from_double_exact` can be
asked for a NaN or an infinity. `*` unwraps a value known to be present; use
the `checked_` layer described below when the input is not already known-good.

`Rational r = 0.45;` does **not** compile. `Rational` has a constructor
template for floating-point types whose body is a `static_assert` that always
fires, with a message pointing at `from_decimal`, `from_double_exact` and
`rational_from_double`. This is deliberate: a `double` is a binary fraction,
so silently converting one to a `Rational` would make `0.45` mean
8106479329266893 / 2^54, not 9/20 -- exactly the confusion this type exists to
prevent.

## Exact or nothing

`checked_` means exactly one thing in this library: the function returns
`std::expected<T, ArithmeticError>` and never throws. Every `checked_`
function that has a natural infallible-looking spelling also has a throwing
counterpart -- but not every operation has both forms, and this section says
which.

`formula::checked_add`, `checked_sub`, `checked_mul`, `checked_div`,
`checked_pow`, `checked_negate` and `checked_abs` return
`std::expected<Rational, ArithmeticError>`:

```cpp
std::expected<Rational, ArithmeticError> const result = formula::checked_add(a, b);
if (!result)
{
    // result.error() is an ArithmeticError; formula::describe(result.error())
    // gives a lowercase noun phrase such as "overflow in exact arithmetic"
    std::string_view const message = formula::describe(result.error());
}
```

Their throwing counterparts -- `operator+`, `operator-` (binary and unary),
`operator*`, `operator/`, their `+=` family, `pow` and `abs` -- are thin
wrappers that throw `formula::ArithmeticException` on failure:

```cpp
Rational const sum = a + b; // throws formula::ArithmeticException on overflow
```

Three operations are checked-only, deliberately without a throwing form.
`checked_reciprocal` has no throwing counterpart because `operator/` already
covers the throwing case: dividing into `Rational { 1 }` is a reciprocal.
`checked_decimal_exponent` (rounding.hpp) and `rational_from_double` are
checked-only because their failure is not the rare case a throw is meant
for -- a zero value or an out-of-range `double` is an ordinary input a
caller should expect to handle, not an exceptional one.

Because a throw is not a core constant expression, a failure that happens
inside a `constexpr` context is not a runtime exception at all -- it is a
compile error. Code that divides by a `constexpr` zero, or overflows a
`constexpr` computation, fails to compile rather than failing at run time.

Nothing in this library saturates and nothing truncates silently. An
operation either produces the exact result or reports why it could not; there
is no near-miss value standing in for a result that does not exist.

## Rounding

`formula::RoundingMode` has seven values. The four non-half modes are
unconditional -- they ignore how close the value is and always move the same
way. The three half modes move to the nearer result and differ only on an
exact tie. Worked on `7/4 = 1,75`, `-7/4 = -1,75`, `3/2 = 1,5` and
`5/2 = 2,5`:

| Mode | Meaning | 1,75 | -1,75 | 1,5 | 2,5 |
|---|---|---|---|---|---|
| `HalfAwayFromZero` | nearest, ties away from zero | 2 | -2 | 2 | 3 |
| `HalfTowardZero` | nearest, ties toward zero | 2 | -2 | 1 | 2 |
| `HalfEven` | nearest, ties to even | 2 | -2 | 2 | 2 |
| `Ceiling` | toward positive infinity | 2 | -1 | 2 | 3 |
| `Floor` | toward negative infinity | 1 | -2 | 1 | 2 |
| `TowardZero` | toward zero, plain truncation | 1 | -1 | 1 | 2 |
| `AwayFromZero` | always away from zero | 2 | -2 | 2 | 3 |

```cpp
Rational::Int const nearest = formula::round_to_int(Rational { 7, 4 }, RoundingMode::HalfAwayFromZero); // 2
```

`round_to_int` returns the integer itself; `round_to_integer` returns it as a
`Rational` for use in further exact arithmetic.

Beyond rounding to a whole number, three forms round to a place:

```cpp
// Decimal places: 45,67 rounded to one decimal place is 45,7, i.e. 457/10.
Rational const value = *Rational::from_decimal(4567, -2);
Rational const toOneDecimal = formula::round(value, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero);

// Significant digits: the same 45,67 rounded to two significant digits is 46 -- a
// different operation from decimal places, and the two can and do disagree.
Rational const toTwoSignificant = formula::round(value, SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero);

// Rounding to an arbitrary step, the primitive the two forms above are built on.
Rational const snapped = formula::round_to_multiple(Rational { 7 }, Rational { 5 }, RoundingMode::HalfAwayFromZero); // 5
```

## Rounding is part of the calculation

Because rounding is an operation over exact values rather than a formatting
step, the order in which it is applied is part of the method and changes the
result. Rounding an intermediate value before using it produces a different
answer from rounding only at the end -- both are correct for their respective
methods, and a library that rounds only on output cannot express the
difference:

```cpp
Rational const mean = Rational { 302, 3 }; // 100,666...

Rational const roundedFirst =
    formula::round(mean, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero) * Rational { 2 }; // 202

Rational const roundedLast =
    formula::round(mean * Rational { 2 }, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero); // 201

// roundedFirst != roundedLast
```

`roundedFirst` rounds the mean to a whole number first and doubles the
rounded value; `roundedLast` doubles first and rounds only the final result.
Both are legitimate methods, and which one a specification calls for changes
the answer.

## Limits

`Rational`'s numerator and denominator are `std::int64_t`. `DecimalPlaces`
and the decimal-place form of `round` are limited to ±18 places, because the
scale factor `10^places` must itself fit in `Int`; an out-of-range
`DecimalPlaces` reports `Overflow`, while an out-of-range `SignificantDigits`
(fewer than 1) reports `DomainError` -- both mean "argument outside the
domain of the operation", but a caller switching on the code should expect
either one. That ±18 ceiling is rarely the one actually hit, though.

Rounding to `N` decimal places scales the value by `10^N`. Common factors of
two cancel against the denominator first, so what must fit in `Int` is

```
|numerator| * (10^N / gcd(10^N, denominator))
```

which for a power-of-two denominator is `|numerator| * 5^N`. **The limit is set
by the numerator's magnitude**, not by the denominator and not by the size of
the value:

| numerator (over `2^54`) | max decimal places |
|---|---|
| `1` | 18 |
| `10^9` | 18 |
| `10^12` | 15 |
| `8106479329266893` (a `double`'s mantissa) | 4 |

Holding the numerator at 53 bits and varying the denominator from `2^10` to
`2^62` leaves the answer at 4 places throughout; `1 / 2^60` rounds correctly at
all 18.

This is why `from_decimal` and `rational_from_double` behave so differently for
the same nominal value. `from_decimal(45, -2)` is `9/20` -- numerator 9, so all
18 places work. The same 0,45 as a `double` is exactly
`8106479329266893 / 2^54`: a `double`'s mantissa is always about 53 bits
whatever its exponent, so **any** value from `from_double_exact` or
`rational_from_double` caps out at 4 decimal places, large or small alike.
Asking for more reports `Overflow`, never a wrong number.

`from_double_exact` additionally refuses a `double` whose exact value would need
a denominator above `2^63`. Measured, that rules out a full-mantissa value below
`2^-10` (about 0,00098): `0.0009765625` converts, `0.0001` is refused outright,
before rounding is even reached.

For an exact decimal, prefer `from_decimal`: its numerator is whatever you
passed -- usually a handful of significant digits -- so the limit above does not
bite. Its *denominator* need not be small at all: `from_decimal(1, -18)` is
`1/10^18`, and it still rounds correctly at every decimal place from 0 to 18.
That is the clearest demonstration that the denominator is not what constrains
rounding to decimals.

Rounding to a *negative* number of places -- to whole tens, hundreds, thousands
-- scales the other way: the step is an integer, so it multiplies the
denominator rather than the numerator, and there the denominator is what
constrains you. `1/10^18` is refused at every negative place for exactly that
reason, while `1/3` handles them all.

Reach for `rational_from_double` only when the input is a genuinely measured
`double`, and only at modest decimal precision.

Overflow is always reported, never absorbed -- with one nuance worth knowing.
`checked_add` and `checked_sub` can report overflow for a result that would,
once reduced, actually fit: if both operands' numerators are near `2^63` and
their denominators share a large common factor, the intermediate numerator
sum can exceed `int64_t` even though the reduced answer is representable --
for example `IntMax/3037000500 + IntMax/3037000500`, whose exact value
`IntMax/1518500250` fits easily. Multiplication does not have this problem,
because it cross-reduces before multiplying. Measured over 473,984 operand
pairs against 128-bit ground truth: no wrong values were ever produced, and
no false overflows occurred at all for numerators below roughly 10^6, which
covers realistic use. The failure direction is always the safe one -- a
reported error, never a wrong number.
