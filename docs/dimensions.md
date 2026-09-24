# Dimensions and units

`formula-cpp` provides a compile-time dimension vector, `formula::Dimension`,
and a unit descriptor built on top of it, `formula::Unit`. This page explains
why a dimension is a type rather than a runtime tag, how to compose one, why
its exponents are rational rather than integer, what a `Unit` carries, how
conversion between units stays exact, and where the declared-precision and
bounds machinery sits. The worked example below is
`examples/dimensions_and_units.cpp`; every block on this page that is
formatted as program output is copied verbatim from that program's actual
output, not worked out by hand. A couple of numeric facts that the example
does not itself print are given as plain rationals instead, each naming the
`static_assert` in the test suite that pins it -- never formatted as if a
program had printed them.

## Why dimensions are types

A `Dimension` is an exponent vector over the seven SI base quantities --
length, mass, time, current, temperature, amount and luminosity -- and it is
*structural*: every member public, recursively, which is what lets it be used
as a non-type template parameter. That is the point of the design, not an
implementation detail. A quantity's unit names its dimension as part of the
quantity's *type*, so a mismatch between two dimensions is something the
compiler catches while reading the declaration, not something a running
program has to notice and report.

The mismatch diagnostic, `formula::RequireSameDimension<Left, Right>`, is a
`static_assert` inside a class template, instantiated on the two dimensions'
actual values rather than on their (opaque) types. That is deliberate: naming
the specialisation is not instantiating it, so a bare alias checks nothing,
but writing `RequireSameDimension<Left, Right>::value` forces the
instantiation and the compiler prints the two exponent vectors themselves as
part of the error -- not two anonymous type names, the actual numbers, in the
order length, mass, time, current, temperature, amount, luminosity. Adding a
volume to a mass (`RequireSameDimension<dim::Volume, dim::Mass>::value`) fails
to compile with both vectors spelled out in the diagnostic. `formula::Unit`
has the analogous `RequireSameUnitDimension<From, To>` for conversions between
units of different dimensions.

**The guard fires only when the type is completed.** `using Checked =
RequireSameDimension<A, B>;` and a function parameter of that type compile
silently even when `A` and `B` differ -- the class template was named, not
instantiated. Only `::value`, `sizeof(...)`, or a variable of that type forces
completion and runs the `static_assert`. Where a plain `bool` is enough and no
diagnostic text is needed, `formula::SameDimension<Left, Right>` is a variable
template and is always evaluated.

## Composing dimensions

Nobody spells out `Dimension{.length = exponent(2)}` for an area. The
`dim::` namespace supplies the seven base dimensions and a handful of derived
ones (`Area`, `Volume`, `Density`, `Velocity`, `Acceleration`, `Force`,
`Pressure`, `Energy`, `Frequency`), and the arithmetic operators build the
rest: `operator*` adds two dimensions' exponents (composing quantities that
multiply), `operator/` subtracts them, `power` scales by an integer exponent,
and `nth_root` divides them by the degree of the root. From the example:

```cpp
Dimension const area = dim::Length * dim::Length;
Dimension const volume = area * dim::Length;
Dimension const density = dim::Mass / volume;
```

prints, and matches the named constants exactly:

```
area (length * length) = L^2
volume (area * length) = L^3
density (mass / volume) = L^-3 M^1
composed dimensions match the named constants: yes
```

Composing from constants rather than writing exponents by hand keeps the
representation swappable -- if `Dimension` ever grew an eighth base quantity,
every one of these call sites stays correct without being touched.

## Rational exponents

An integer exponent is enough for area (`L^2`) or volume (`L^3`), but not for
every quantity a norm-style formula needs. The square root of an *area* is a
length -- an ordinary integer power, `L^1` -- but the square root of a
*length* is length to the one half, an exponent no integer can name at all.
Formulas that take such roots are common in size and shape calculations, which
is why `Exponent` is a rational (`numerator / denominator`, always reduced,
denominator always positive) rather than a plain integer:

```cpp
Dimension const rootOfLength = formula::nth_root(dim::Length, 2);
```

```
sqrt(length) = L^(1/2)
sqrt(length) has exponent one half: yes
```

The same mechanism covers a fractional power such as two-thirds: `power(
nth_root(dimension, 3), 2)` raises a dimension to the 2/3 power by first
taking a cube root, then squaring -- the ordinary way to build a non-unit
rational exponent out of the two integer operations that already exist.
`Exponent` canonicalises through a single internal function on every
construction, so `exponent(2, 4)` and `exponent(1, 2)` are the same value
(and, as non-type template arguments, name the same type) even though they
were written differently.

## Units

A `formula::Unit` is a small aggregate, and every field earns its place:

| Field | Purpose |
|---|---|
| `dimension` | which physical quantity this unit measures |
| `magnitudeNumerator` / `magnitudeDenominator` | the exact multiplicative factor to the coherent SI unit, as an integer ratio |
| `offsetNumerator` / `offsetDenominator` | the exact additive offset, for an affine scale such as degrees Celsius |
| `symbolText` | a fixed-capacity display symbol (a `Symbol`, not a `std::string_view`) |
| `decimals` | the declared display precision |
| `bounds` | an optional valid range, in the unit's own scale |

Like `Dimension`, `Unit` is structural on purpose: phase 4's quantity type
names its unit as a template argument, so `Unit` has to stay a single
ordinary type usable as one. That rules out `std::string_view` for the symbol
(private members, not structural) and `Rational` for the magnitude and offset
(same reason) -- hence plain fixed-size integer and character-array fields
here, with the convenient types (`Rational`, `std::string_view`) appearing
only at the point of use, via `formula::view()` and the conversion functions
below.

The `formula::unit::` namespace declares twenty of these: the coherent SI
units (`Metre`, `Kilogram`, `Second`, `Kelvin`, `Pascal`, ...) alongside scaled
ones (`Millimetre`, `Tonne`, `Hour`, `Megapascal`, ...) and the two dimensionless
units, `One` and `Percent`. Their `decimals` values are ordinary engineering
defaults, not a requirement taken from any standard -- a caller that needs a
different precision states it at the point of use.

## Exact conversion

Converting between two units multiplies by the source unit's magnitude, adds
its offset, subtracts the target unit's offset, then divides by the target
unit's magnitude -- as exact integer ratios throughout, and always
multiply-then-divide rather than a single precomputed floating-point factor.
That ordering is why 30 MPa converts to *exactly* 30000000 Pa and back to
*exactly* 30, rather than to some binary approximation that happens to print
as 30. As exact rationals, not program output -- pinned by `static_assert`s
in `test/unit_tests.cpp:234-235`, not printed by the example below: 30/1 MPa
converts to 30000000/1 Pa, and converting that back gives 30/1 MPa again.

The worked example does perform this round trip on a volume, though, and its
output below really is copied from the program:

```
450 l = 9/20 m3
... converted back = 450 l
volume round trip exact: yes
```

`formula::checked_convert` returns `std::expected<Rational, ArithmeticError>`
and never produces a wrong number -- a dimension mismatch or an
unrepresentable intermediate is reported, not silently rounded away.
`formula::convert` is its throwing counterpart, for callers who already know
the conversion is well-formed.

### The affine case, and the point-versus-difference caveat

Degrees Celsius is why `Unit` carries an offset at all: converting to Kelvin
is not a plain scaling. `checked_convert`/`convert` move a *point* on a
scale, not a difference between two points -- a distinction that matters
because the two operations give different answers for the same nominal
number. As exact rationals, not program output -- pinned by `static_assert`s
in `test/unit_tests.cpp:243,244,250`:

| Input    | In degC | In Kelvin |
|----------|---------|-----------|
| 0 degC   | 0/1     | 5463/20   |
| 1 degC   | 1/1     | 5483/20   |
| 100 degC | 100/1   | 7463/20   |

5463/20 is 273,15 and 5483/20 is 274,15: 1 degree Celsius converts to
274,15 K, **not** to 1 K. A caller that wants "how much did the temperature
change" needs a difference, which this function does not compute -- it always
applies the offset, because it always converts a point. The worked example
does convert 100 degC, and its output below really is copied from the
program, confirming the round trip holds anyway, offset included:

```
100 degC = 7463/20 K
... converted back = 100 degC
temperature round trip exact: yes
```

## Declared precision and bounds

Every unit declares a display precision (`decimals`) and, optionally, a valid
range (`bounds`), and both apply to a *computed* value, not just to a literal:

```cpp
Rational const computedMass = genericDensity * volumeInCubicMetres;   // 450/7 kg
Rational const roundedMass =
    *formula::checked_round_to_declared(computedMass, unit::Kilogram, RoundingMode::HalfAwayFromZero);
```

```
computed mass = 450/7 kg
rounded to kg's declared precision (3 places) = 64.286 kg
```

`formula::declared_decimals` returns the rounding layer's own `DecimalPlaces`
type, not a bare `int`, so it plugs directly into `formula::round` /
`formula::checked_round` (see [`docs/numbers.md`](numbers.md)).
`checked_round_to_declared` is the two calls composed, and returns a
`std::expected` like every other `checked_` function here;
`round_to_declared` is the same thing spelled to throw, as `convert` is to
`checked_convert`.

`formula::checked_within_bounds` checks a value, in the unit's own scale,
against that unit's declared `bounds`, and returns one of five
`BoundsCheck` values: `WithinBounds`, `BelowMinimum`, `AboveMaximum`,
`NotChecked`, or `NotMeasured`. **`NotChecked` is deliberately not the same thing as
`WithinBounds`.** A unit that declares no bounds at all has not validated
anything, and reporting it as "within bounds" would make an unvalidated value
indistinguishable from one that was actually checked and passed:

```
unbounded unit (litre) reports: no bounds declared for this unit
bounded gauge at 42%: within the declared bounds
```

`unit::Litre` declares no bounds, so it always reports `NotChecked`,
regardless of the value; a unit that does declare bounds (the example builds
one, a generic 0-to-100 gauge) can report `WithinBounds`, `BelowMinimum` or
`AboveMaximum`. `NotMeasured` is the fifth, and it belongs to values rather
than units: it is what `checked_within_bounds` answers for a `Measured` that
holds nothing. A reading nobody took and a range nobody declared are
different facts, for the same reason `NotChecked` is not `WithinBounds`. `formula::describe(BoundsCheck)` gives each outcome its own
non-empty, mutually distinct wording, as shown above.

## Limits

`Exponent`'s numerator and denominator are `std::int32_t`. Building one with a
zero denominator, or one whose reduced form does not fit `std::int32_t`, never
produces a wrapped or truncated value. Every `Exponent` in the library is
constructed through one internal function, and that function calls a
deliberately non-`constexpr` sentinel on either failure. What the sentinel does
depends on where you are:

- **In a constant expression** -- which is how dimensions are normally built,
  and how every `dim::` constant is built -- calling a non-`constexpr` function
  is not allowed, so the compiler rejects it and names the sentinel. This is
  the case the negative-compile tests pin.
- **At run time**, from a value the compiler cannot see, the sentinel aborts.

```cpp
constexpr auto bad = formula::exponent(1, 0);   // does not compile
auto const alsoBad = formula::exponent(1, argc - 1);   // compiles; aborts if argc == 1
```

Both are fail-fast; only the first is a diagnostic. Measured on clang 22: the
second compiles cleanly and terminates at run time. So write dimensions in
constant expressions and you get the error at the point you wrote the mistake;
build one from runtime input and you get a crash instead of a wrong answer.

`SymbolCapacity` is 16 bytes **including the terminator** -- 15 usable
characters, not 16. `Unit`'s `magnitudeNumerator`, `magnitudeDenominator`,
`offsetNumerator`, `offsetDenominator` and the four fields of `Bounds` are all
`std::int64_t`, the same width as `Rational`'s own numerator and denominator.

Conversion is built on `formula::Rational` and the `checked_` arithmetic
functions, so it inherits their overflow behaviour and rounding limits
exactly -- including the ±18-decimal-place ceiling on `DecimalPlaces` and the
numerator-magnitude-dependent limits on rounding described in
[`docs/numbers.md`](numbers.md#limits). Nothing in `dimension.hpp` or
`unit.hpp` widens or narrows those limits; a `Unit`'s magnitude and offset are
exactly the integer pairs `Rational` already knows how to handle exactly.
