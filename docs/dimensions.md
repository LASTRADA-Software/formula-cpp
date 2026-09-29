# Dimensions and units

`formula-cpp` provides a compile-time dimension vector, `formula::Dimension`,
and a unit descriptor built on top of it, `formula::Unit`. This page explains
why a dimension is a type rather than a runtime tag, how to compose one, why
its exponents are rational rather than integer, what a `Unit` carries, how
conversion between units stays exact, where the declared-precision and
bounds machinery sits, and how an application declares a base dimension the
SI does not have, such as money.

The worked example is `examples/dimensions_and_units.cpp`. **Program output**
on this page is copied verbatim from that program's output, and
`docs.dimensions-output` fails unless each output block is a run of
consecutive lines the program prints, exactly as quoted
(`cmake/CheckGuideOutput.cmake`). **Code** is copied from the example's
source, and `docs.dimensions-snippets` fails unless each code block appears
there as a run of consecutive lines, compared without their indentation
(`cmake/CheckGuideSnippets.cmake`). A code block deliberately not from the
example carries a `<!-- snippet: not from the example -->` comment directly
above it; one on this page does, the block showing that
`formula::exponent(1, 0)` does not compile. A couple of numeric facts that
the example does not itself print are given as plain rationals instead, each
naming the `static_assert` in the test suite that pins it -- never formatted
as if a program had printed them.

## Why dimensions are types

A `Dimension` is an exponent vector over the seven SI base quantities --
length, mass, time, current, temperature, amount and luminosity -- and up to
four base dimensions the SI does not have, which the application names itself:
money in one currency is the usual one (see
[Base dimensions the SI does not have](#base-dimensions-the-si-does-not-have)).
It is *structural*: every member public, recursively, which is what lets it be
used as a non-type template parameter. That is the point of the design, not an
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
order length, mass, time, current, temperature, amount, luminosity, then the
named base dimensions by name. Adding a volume to a mass
(`RequireSameDimension<dim::Volume, dim::Mass>::value`) fails to compile with
both vectors spelled out in the diagnostic. A named base appears there as its
name and its exponent, and how legibly depends on the compiler: g++ prints
`formula::Symbol{"EUR"}`, while cl, clang-cl and clang++ print the name's
character codes (69, 85, 82 for EUR). `formula::Unit` has the analogous
`RequireSameUnitDimension<From, To>` for conversions between units of
different dimensions.

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
`Pressure`, `Energy`, `Power`, `Frequency`, and `MassPerArea`, `ForcePerLength`,
`DynamicViscosity`, `KinematicViscosity`), and the arithmetic operators build the
rest: `operator*` adds two dimensions' exponents (composing quantities that
multiply), `operator/` subtracts them, `power` scales by an integer exponent,
and `nth_root` divides them by the degree of the root. From the example:

```cpp
Dimension const area = dim::Length * dim::Length;
Dimension const volume = area * dim::Length;
Dimension const density = dim::Mass / volume;
```

prints, and matches the named constants exactly:

```text
area (length * length) = L^2
volume (area * length) = L^3
density (mass / volume) = L^-3 M^1
composed dimensions match the named constants: yes
```

Composing from constants rather than writing exponents by hand keeps the
representation swappable, and that has been put to the test: `Dimension` has
since grown past the seven SI base quantities, to hold the named base
dimensions described below, and not one of these call sites had to change.

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

```text
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
| `dimension` | which quantity this unit measures |
| `magnitudeNumerator` / `magnitudeDenominator` | the exact multiplicative factor to the coherent unit -- the coherent SI unit, times one of each named base dimension -- as an integer ratio |
| `offsetNumerator` / `offsetDenominator` | the exact additive offset, for an affine scale such as degrees Celsius or degrees Fahrenheit |
| `symbolText` | a fixed-capacity display symbol (a `Symbol`, not a `std::string_view`) |
| `decimals` | the declared display precision |
| `bounds` | an optional valid range, in the unit's own scale |

Like `Dimension`, `Unit` is structural on purpose: a quantity (see
[Quantities and measurements](quantities.md)) names its unit as a template
argument, so `Unit` has to stay a single ordinary type usable as one. That
rules out `std::string_view` for the symbol (private members, not structural)
and `Rational` for the magnitude and offset (same reason) -- hence plain
fixed-size integer and character-array fields here, with the convenient types
(`Rational`, `std::string_view`) appearing only at the point of use, via
`formula::view()` and the conversion functions below.

The `formula::unit::` namespace declares fifty-six of these: the coherent SI
units (`Metre`, `Kilogram`, `Second`, `Kelvin`, `Newton`, `Pascal`, `Watt`, ...)
alongside scaled ones (`Millimetre`, `Tonne`, `Hour`, `Megapascal`,
`KilowattHour`, ...), compound ones (`KilogramPerCubicMetre`,
`MillimetrePerMinute`, `PascalSecond`, ...) and five dimensionless ones (`One`,
`Percent`, `PerMille`, `PartsPerMillion`, `MilligramPerKilogram`). Two pairs are
deliberately the same magnitude under two names -- `Megapascal` and
`NewtonPerSquareMillimetre`, `PartsPerMillion` and `MilligramPerKilogram` --
because both spellings are in ordinary use, and a test pins that each pair
converts into the other exactly. None of them is a currency: which currencies
an application deals in, and to how many decimals each is shown, is its own
policy, and it declares those units itself (see
[Base dimensions the SI does not have](#base-dimensions-the-si-does-not-have)).

Power and energy have four of them: `Watt` and `Kilowatt` measure `dim::Power`,
`WattHour` and `KilowattHour` measure `dim::Energy`. A watt-hour is the energy
of one watt sustained for an hour, exactly 3600 joules, so a kilowatt-hour --
the unit an electricity bill is usually written in -- is exactly 3600000
joules, a whole number, and a power times a time converts into kilowatt-hours
without a rounded factor. A kilowatt and a kilowatt-hour differ only by a
factor of time, but they are different dimensions: the type system keeps a
power and an energy apart. The worked example converts one kilowatt-hour, and
its output below really is copied from the program:

```text
1 kWh = 3600000 J
```

Temperature has `Kelvin` and two affine scales, `Celsius` and `Fahrenheit`,
which the section on the affine case below covers.

There is no angle unit. A degree is pi/180 radians, which is not a rational
number, and every conversion here is by exact rational magnitude; a `Degree`
would have to be either inexact or unconvertible, and neither is a choice to
make silently.

Their `decimals` values are ordinary engineering
defaults, not a requirement taken from any standard -- a caller that needs a
different precision states it at the point of use.

## Exact conversion

Converting between two units multiplies by the source unit's magnitude, adds
its offset, subtracts the target unit's offset, then divides by the target
unit's magnitude -- as exact integer ratios throughout, and always
multiply-then-divide rather than a single precomputed floating-point factor.
That ordering is why 30 MPa converts to *exactly* 30000000 Pa and back to
*exactly* 30, rather than to some binary approximation that happens to print
as 30. As exact rationals, not program output, and not printed by the example
below: 30/1 MPa converts to 30000000/1 Pa, and converting that back gives 30/1
MPa again. `static_assert`s in `test/unit_tests.cpp` pin both, the ones that
read `converted(30, 1, unit::Megapascal, unit::Pascal)` and
`converted(30000000, 1, unit::Pascal, unit::Megapascal)`.

The worked example does perform this round trip on a volume, though, and its
output below really is copied from the program:

```text
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
is not a plain scaling. Degrees Fahrenheit is the second unit with an offset,
and everything here holds for both. `checked_convert`/`convert` move a *point*
on a scale, not a difference between two points -- a distinction that matters
because the two operations give different answers for the same nominal
number. As exact rationals, not program output -- pinned by the `static_assert`s
in `test/unit_tests.cpp` that read `converted(0, 1, unit::Celsius, unit::Kelvin)`,
`converted(1, 1, unit::Celsius, unit::Kelvin)` and
`converted(100, 1, unit::Celsius, unit::Kelvin)`:

| Input    | In degC | In Kelvin |
|----------|---------|-----------|
| 0 degC   | 0/1     | 5463/20   |
| 1 degC   | 1/1     | 5483/20   |
| 100 degC | 100/1   | 7463/20   |

5463/20 is 273.15 and 5483/20 is 274.15: 1 degree Celsius converts to
274.15 K, **not** to 1 K. A caller that wants "how much did the temperature
change" needs a difference, which this function does not compute -- it always
applies the offset, because it always converts a point.

Degrees Fahrenheit has a different degree as well as a different offset: one is
exactly 5/9 of a kelvin, and 0 degF is 45967/180 K. Again exact rationals, not
program output, pinned by the `static_assert`s that read
`converted(0, 1, unit::Fahrenheit, unit::Kelvin)`,
`converted(1, 1, unit::Fahrenheit, unit::Kelvin)` and
`converted(32, 1, unit::Fahrenheit, unit::Kelvin)`:

| Input    | In degF | In Kelvin |
|----------|---------|-----------|
| 0 degF   | 0/1     | 45967/180 |
| 1 degF   | 1/1     | 46067/180 |
| 32 degF  | 32/1    | 5463/20   |

The first two rows differ by 100/180 K, which is 5/9 K: a step of one degree
Fahrenheit is 5/9 K, but the *point* 1 degF is 46067/180 K, not 5/9 K, for the
same reason 1 degC is not 1 K. The last row is the freezing point of water,
5463/20 K, which is also where 0 degC sits.

The two scales agree at one point only, and the example prints it: -40 degF is
-40 degC. Converting from Fahrenheit divides by 9, and a ninth is not a
terminating decimal, so a reading in whole degrees Fahrenheit has no finite
decimal form in Celsius unless it lies a multiple of 9 degrees from 32 degF:
100 degF is 340/9 degC, which is 37.777... degC. The conversion returns the
fraction rather than a rounded 37.78, because a conversion that rounded would
stop round-tripping: 37.78 degC converts back to 100.004 degF, not to 100.
Going the other way multiplies by 9/5, which keeps a terminating decimal
terminating -- 37 degC is 493/5 degF, which is 98.6 -- and rounding a result for
display is the job of the unit's declared precision (see below), not of the
conversion.

The worked example converts 100 degC, and its output below really is copied from
the program, confirming the round trip holds anyway, offset included. The last
two lines are the two Fahrenheit conversions above:

```text
100 degC = 7463/20 K
... converted back = 100 degC
temperature round trip exact: yes
-40 degF = -40 degC
100 degF = 340/9 degC
```

## Declared precision and bounds

Every unit declares a display precision (`decimals`) and, optionally, a valid
range (`bounds`), and both apply to a *computed* value, not just to a literal:

```cpp
Rational const computedMass = genericDensity * volumeInCubicMetres;
Rational const roundedMass =
    *formula::checked_round_to_declared(computedMass, unit::Kilogram, RoundingMode::HalfAwayFromZero);
```

```text
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

```text
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

## Base dimensions the SI does not have

The seven SI base quantities describe physics, and formulas are often about
money as well: a tariff in euros per kilowatt-hour, a price per tonne. A
currency is not a bare number. Declared as one -- a `Unit` of `dim::Scalar` --
a price in euros could be added to a ratio, or to a price in yen, and the
dimension system would have nothing to object to. So an application declares a
base dimension of its own for each currency it deals in, with
`formula::base_dimension`, and composes it like any other. From the example:

```cpp
Dimension const euros = formula::base_dimension("EUR");
Dimension const tariff = euros / dim::Energy;
Dimension const tariffTimesEnergy = tariff * dim::Energy;
```

```text
tariff (EUR / energy) = L^-2 M^-1 T^2 EUR^1
tariff * energy = EUR^1
```

The example prints a named base after the seven SI exponents, by its name. A
tariff is euros over an energy -- `L^-2 M^-1 T^2` from the joule, `EUR^1` from
the base -- and times an energy it is euros again: the same value as
`base_dimension("EUR")` itself, which the example checks.

**Identity is the name, byte for byte.** Two parts of a program, or two
libraries, that both write `base_dimension("EUR")` get the same dimension --
the same value, and the same template argument -- so a quantity one of them
declares in euros is a quantity the other accepts. For a three-letter currency
code that is what you want. For a generic word it may not be: another
library's `base_dimension("credit")` would be yours, whatever it meant by it,
so pick a distinctive name ("AcmeCredit" rather than "credit").

Agreeing on the dimension is half of it; agreeing on its units is the other
half, and a convention covers that: **the unit named after a base has
magnitude one.** A euro is the coherent unit of euros, and a cent is a
hundredth of it. The library cannot enforce the convention -- a `Unit` is an
aggregate anyone may fill in -- but every conversion between two units of one
base relies on it, as conversions between lengths rely on the metre having
magnitude one. The coherent unit of any dimension is then the coherent SI
unit times one of each of its named bases, and it is the unit every value is
carried in while a formula is evaluated. The example declares three units:

```cpp
constexpr Unit Euro { .dimension = formula::base_dimension("EUR"),
                      .symbolText = formula::symbol("EUR"),
                      .decimals = 2 };
constexpr Unit EuroCent { .dimension = formula::base_dimension("EUR"),
                          .magnitudeNumerator = 1,
                          .magnitudeDenominator = 100,
                          .symbolText = formula::symbol("ct"),
                          .decimals = 0 };
constexpr Unit Yen { .dimension = formula::base_dimension("JPY"),
                     .symbolText = formula::symbol("JPY"),
                     .decimals = 0 };
```

and converts 250 euros into cents, back into euros, and then into yen:

```text
250 EUR = 25000 ct
... converted back = 250 EUR
250 EUR to JPY: argument outside the domain of the operation
```

**Each currency is a base of its own.** Euros and yen never convert into each
other: `checked_convert` refuses them as it refuses any two units of different
dimensions, with `ArithmeticError::DomainError`, and
`RequireSameUnitDimension<Euro, Yen>::value` does not compile. That is
deliberate. An exchange rate is not a property of two units; it changes from
day to day and is agreed per transaction. It is data -- a quantity in yen per
euro, of dimension `base_dimension("JPY") / base_dimension("EUR")` -- which a
formula multiplies by, and which the trace records like any other input. For
the same reason there is no `dim::Money`: one money dimension could not tell
euros from yen. In a formula the same rules hold at the formula's own source
line; [Expressions and evaluation](expressions.md#where-a-dimensional-error-appears)
shows the two additions it refuses. The library itself declares no currency:
`formula::unit` is generic physics, and the currencies and their decimals are
the application's.

**Names.** A base's name must be an ASCII letter followed by ASCII letters or
digits, at most 15 bytes long -- it is a `Symbol`, as a unit's symbol is --
and not the symbol of an SI base unit: `m`, `kg`, `s`, `A`, `K`, `mol` or `cd`,
since a base named `m` would read as metres wherever it is printed. And it is
printed: it is the symbol of the base's coherent unit, written into a trace
beside spaces, `/`, `^` and parentheses, and into Markdown, where `_`, `*` and
`[` are markup -- hence letters and digits only. `base_dimension` is
`consteval`, so a name that breaks a rule is always a compile error, and the
error names the rule: `formula_base_dimension_name_must_not_be_empty`,
`formula_base_dimension_name_too_long`,
`formula_base_dimension_name_must_be_a_letter_then_letters_or_digits` or
`formula_base_dimension_name_is_an_si_base_unit_symbol`. A helper that passes
a name on to `base_dimension` must be `consteval` as well.

**Capacity, and cancellation.** One dimension holds at most four named bases
(`NamedBaseCapacity`): a tariff needs one, an exchange rate two. Composition
merges the two lists by name. A name both sides carry has its exponents added
or subtracted, and a name whose exponent comes to zero drops out before
anything is counted, so `(EUR / USD) * (USD / JPY)` is `EUR / JPY`: two bases,
not three. A product or quotient that still needs a fifth fails to compile,
naming `formula_dimension_has_too_many_named_bases` -- where a `Dimension` is
composed in a constant expression, and where a formula multiplies quantities
whose dimensions would need it
(`test/negative/expression_too_many_named_bases.cpp`). The bases are kept
sorted by name, so `EUR * JPY` and `JPY * EUR` are one value
and one template argument. Filling `namedBases` by hand bypasses that order,
which is why a dimension should only ever be built with `base_dimension` and
the operators.

**In a coherent unit, the name is the symbol.** A computed step in a trace
carries no unit symbol of its own (see
[Tracing and audit trails](tracing.md#reading-a-derivation)), but where the
trace does spell a coherent unit out -- for an opaque operation's output that
no input's unit fits ([Opaque operations and bounded retry](opaque-and-retry.md))
-- a named base is written by its name, ahead of the SI units on its side of
the slash: `EUR s^2/(m^2 kg)` for euros per joule, then `1/JPY`, `EUR/JPY`,
`EUR^(1/2)`. The money comes first because a tariff is read as money per
energy.

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

<!-- snippet: not from the example -->
```cpp
constexpr auto bad = formula::exponent(1, 0);   // does not compile
auto const alsoBad = formula::exponent(1, argc - 1);   // compiles; aborts if argc == 1
```

Both are fail-fast; only the first is a diagnostic. Measured on clang 22: the
second compiles cleanly and terminates at run time. So write dimensions in
constant expressions and you get the error at the point you wrote the mistake;
build one from runtime input and you get a crash instead of a wrong answer.

`NamedBaseCapacity` is 4: a `Dimension` holds at most four named bases at
once, counted after cancellation. A fifth is refused through a sentinel of
the same kind, `formula_dimension_has_too_many_named_bases` -- a compile error
in a constant expression, an abort at run time -- never by dropping a base. A
named base's exponent is an `Exponent`, with the limits above.

`SymbolCapacity` is 16 bytes **including the terminator** -- 15 usable
characters, not 16 -- and a base's name is a `Symbol` too, so it is at most 15
bytes long. The name is checked only where it is made, and `base_dimension` is
`consteval`, so its four sentinels are always compile errors, never aborts:
`formula_base_dimension_name_must_not_be_empty`,
`formula_base_dimension_name_too_long`,
`formula_base_dimension_name_must_be_a_letter_then_letters_or_digits` and
`formula_base_dimension_name_is_an_si_base_unit_symbol`. A `namedBases` array
filled by hand is checked by none of them.

`Unit`'s `magnitudeNumerator`, `magnitudeDenominator`,
`offsetNumerator`, `offsetDenominator` and the four fields of `Bounds` are all
`std::int64_t`, the same width as `Rational`'s own numerator and denominator.

Conversion is built on `formula::Rational` and the `checked_` arithmetic
functions, so it inherits their overflow behaviour and rounding limits
exactly -- including the ±18-decimal-place ceiling on `DecimalPlaces` and the
numerator-magnitude-dependent limits on rounding described in
[`docs/numbers.md`](numbers.md#limits). Nothing in `dimension.hpp` or
`unit.hpp` widens or narrows those limits; a `Unit`'s magnitude and offset are
exactly the integer pairs `Rational` already knows how to handle exactly.
