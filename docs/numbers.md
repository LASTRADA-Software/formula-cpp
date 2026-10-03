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
`Rational` makes that round trip exact. From `examples/exact_numbers.cpp`
(`_r` is the exact-decimal literal of [Writing an exact
decimal](#writing-an-exact-decimal)):

```cpp
using namespace formula::literals;

// 450 millilitres, written exactly. 450_r is the number 450, never a double.
Rational const volumeInMillilitres = 450_r;

// Convert to litres by an exact integer factor: multiply, then divide.
// 450 ml -> 9/20 l, and back to 450 ml with nothing lost.
Rational const volumeInLitres = volumeInMillilitres / 1000;
Rational const roundTripped = volumeInLitres * 1000;
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
| an exact decimal | `0.45_r` | 9/20 |
| an exact decimal from digits known only at run time | `Rational::from_decimal(45, -2)` | 9/20 |
| a whole number of tens, the same way | `Rational::from_decimal(45, 1)` | 450/1 |
| the exact value of a `double` | `Rational::from_double_exact(0.45)` | a power-of-two denominator |
| a measured `double` on a known scale | `rational_from_double(0.45, DecimalPlaces { 2 }, mode)` | 9/20 |

Spelled out, as runnable code:

```cpp
using namespace formula::literals;

Rational const a { 7 };                                              // 7/1
Rational const b { 3, 4 };                                           // 3/4
Rational const c = 0.45_r;                                           // 9/20
Rational const d = *Rational::from_decimal(45, -2);                  // 9/20
Rational const e = *Rational::from_decimal(45, 1);                   // 450/1
Rational const f = *Rational::from_double_exact(0.45);               // a power-of-two denominator
Rational const g =
    *formula::rational_from_double(0.45, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero); // 9/20
```

`0.45_r` is read from its spelling at compile time
([Writing an exact decimal](#writing-an-exact-decimal)). `from_decimal`
writes the same number from a mantissa and a power of ten that may be known
only at run time.

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

## Writing an exact decimal

`Rational::from_decimal(273, -1)` is exact but hard to read. The literal `_r`
writes the same value the way it is written on paper:

```cpp
using namespace formula::literals;

Rational const a = 27.3_r;   // 273/10, not the double nearest 27.3
Rational const b = 0.47_r;   // 47/100
Rational const c = 1.5e-3_r; // 3/2000
Rational const d = -27.3_r;  // -273/10
```

A literal has to be a `_r` literal rather than a `double` for the reason given
under *Why not `double`*: `0.47` as a `double` is not 47/100, and a `Rational`
made from it would carry that error. `0.47_r` is read from its spelling, so
nothing is rounded on the way in. An exponent scales exactly, digit separators
(`1'000.5_r`) are ignored, and a minus sign is `Rational`'s own negation.

The literal is evaluated at compile time, so a spelling it cannot honour does not
compile. The diagnostic names the function that was reached:

| Spelling | Why it is refused | Named in the diagnostic |
|---|---|---|
| `9'223'372'036'854'775'808_r` | one more than the largest 64-bit integer, 9'223'372'036'854'775'807, which bounds the literal's mantissa -- though a `Rational` holds the value | `formula_rational_literal_out_of_range` |
| `0.0000000000000000001_r` | a denominator of 10^19: the literal's scale, like `from_decimal`'s, stops at 10^18 | `formula_rational_literal_out_of_range` |
| `0x1F_r`, `0b101_r` | not a decimal | `formula_rational_literal_not_a_decimal` |
| `017_r` | C++ reads a leading zero as octal, so it is not the decimal 17 | `formula_rational_literal_not_a_decimal` |

Trailing zeros after the point cost nothing: `4.210_r` is 421/100, and a
literal with two dozen places still works when most of them are zeros.

The literal is for decimals. A fraction such as one third is still
`Rational { 1, 3 }`, or `1_r / 3`.

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
using namespace formula::literals;

// Decimal places: 45,67 rounded to one decimal place is 45,7, i.e. 457/10.
Rational const value = 45.67_r;
Rational const toOneDecimal = formula::round(value, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero);

// Significant digits: the same 45,67 rounded to two significant digits is 46 -- a
// different operation from decimal places, and the two can and do disagree.
Rational const toTwoSignificant = formula::round(value, SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero);

// Rounding to an arbitrary step, the primitive the two forms above are built on.
Rational const snapped = formula::round_to_multiple(7, 5, RoundingMode::HalfAwayFromZero); // 5
```

A `Rational` is written as text as a fraction by default. To write it as a
decimal -- exactly where it has one, rounded in a mode you name and marked `≈`
where it does not -- in a trace, a rendered formula or `number_text()`, see
[Displaying numbers](display.md). `std::format` is the other way round: `{}`
writes the exact decimal where there is one and the fraction where there is
not, and `{:/}` always the fraction.

## Rounding is part of the calculation

Because rounding is an operation over exact values rather than a formatting
step, the order in which it is applied is part of the method and changes the
result. Rounding an intermediate value before using it produces a different
answer from rounding only at the end -- both are correct for their respective
methods, and a library that rounds only on output cannot express the
difference:

```cpp
Rational const mean { 302, 3 }; // 100,666...

Rational const roundedFirst = formula::round(mean, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero) * 2; // 202

Rational const roundedLast = formula::round(mean * 2, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero); // 201

// roundedFirst != roundedLast
```

`roundedFirst` rounds the mean to a whole number first and doubles the
rounded value; `roundedLast` doubles first and rounds only the final result.
Both are legitimate methods, and which one a specification calls for changes
the answer.

## Limits

`Rational`'s numerator and denominator are `formula::Int128`, signed 128-bit
integers: each holds up to 2^127 − 1, and a numerator down to −2^127 -- up to
39 decimal digits. That is the integer width only. `DecimalPlaces` and the decimal-place form of
`round` stay limited to ±18 places, as `from_decimal`'s exponent and the `_r`
literal's 18 places are; an out-of-range `DecimalPlaces` reports `Overflow`,
while an out-of-range `SignificantDigits` (fewer than 1) reports `DomainError`
-- both mean "argument outside the domain of the operation", but a caller
switching on the code should expect either one.

Rounding to `N` decimal places scales the value by `10^N`. Common factors of
two cancel against the denominator first, so what must fit in `Rational::Int`
is

```
|numerator| * (10^N / gcd(10^N, denominator))
```

which for a power-of-two denominator is `|numerator| * 5^N`. **The limit is set
by the numerator's magnitude**, not by the denominator and not by the size of
the value: `1 / 2^121` rounds correctly at all 18 places, while a 100-bit
numerator over the same denominator is refused at 18.

A `double` below 2^53 in magnitude has a numerator of at most 53 bits, and
rounding it at up to 18 places forms at most 2^53 · 5^18 · 2^18, below 2^113,
so such a value from `from_double_exact` or `rational_from_double` rounds at
every place from 0 to 18: 0,45 as a `double` is exactly
`8106479329266893 / 2^54`, and rounds to 18 places as 0.450000000000000011.
Past that the limit returns: a whole `double` such as 1e21 has a numerator of
its own magnitude and nothing to cancel, so it is refused at 18 places, 1e38
even at 1, and `2^-100` at -18 places multiplies its denominator past 2^127. `from_decimal(45, -2)` is `9/20` -- the same nominal
value, and what a method that writes 0,45 means.

`from_double_exact` refuses a `double` whose exact value would need a
denominator of `2^127` or more, before rounding is even reached. That limit is
set by the value's magnitude: `0.0001` converts, over `2^66`, while `1e-30`,
over `2^147`, is refused.

For an exact decimal, prefer `from_decimal`: its numerator is whatever you
passed -- usually a handful of significant digits -- so the limit above does not
bite. Its *denominator* need not be small at all: `from_decimal(1, -18)` is
`1/10^18`, and it still rounds correctly at every decimal place from 0 to 18.
That is the clearest demonstration that the denominator is not what constrains
rounding to decimals.

Rounding to a *negative* number of places -- to whole tens, hundreds, thousands
-- scales the other way: the step is an integer, so it multiplies the
denominator rather than the numerator, and there the denominator is what
constrains you.

Reach for `rational_from_double` only when the input is a genuinely measured
`double`.

Overflow is always reported, never absorbed -- with one nuance worth knowing.
`checked_add` and `checked_sub` can report overflow for a result that would,
once reduced, actually fit: if both operands' numerators are near `2^127` and
their denominators share a large common factor, the intermediate numerator
sum can exceed `Rational::Int` even though the reduced answer is
representable -- for example `IntMax/3037000500 + IntMax/3037000500`, with
`IntMax` the largest `Rational::Int`, whose exact value `IntMax/1518500250`
fits easily. Multiplication does not have this problem, because it
cross-reduces before multiplying. The failure direction is always the safe
one -- a reported error, never a wrong number.
