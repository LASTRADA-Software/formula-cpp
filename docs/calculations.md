# Calculations and worksheets

A formula built from other formulas is one larger expression tree
([Composing a formula from other formulas](expressions.md#composing-a-formula-from-other-formulas)):
a named sub-formula is written into every formula that uses it. That is the
right model for one published equation, and the wrong one for a bill, a
report or a mix design of many named values, each built on the ones before
it. Evaluated as a tree, a sub-result two formulas share is evaluated again
for each of them, and nothing remembers it: change one input and everything
is evaluated again, and nothing can say which values the change reached.

formula-cpp has a **calculation** for this. Each named value is defined once,
by the expression that calculates it, and read by name wherever another
definition needs it. What each definition reads is known while the program
compiles, so the calculation's dependency graph is known -- and checked -- at
compile time: a definition that reads itself, two that read each other, or a
quantity defined twice does not compile. A **worksheet** holds a
calculation's inputs. It calculates each named value once, and after a change
recalculates only what the change reaches. It answers what-if questions on a
copy, takes a value a person typed in place of a calculated one, and explains
any result as one derivation per named value, which always describes the
current inputs.

The worked example is `examples/electricity_bill.cpp`: a household's monthly
electricity bill, from what three appliances draw, what the solar panels
yield, the grid price, the feed-in tariff and a base fee, to the total with
tax. The page holds three kinds of quoted block. **Program output** is copied
verbatim from that program's actual output, and `docs.calculations-output`
fails unless each of these blocks is a run of consecutive lines the program
prints, exactly as quoted (`cmake/CheckGuideOutput.cmake`). **Code** is copied
from the example's source, and `docs.calculations-snippets` fails unless each
code block appears there as a run of consecutive lines, compared without their
indentation (`cmake/CheckGuideSnippets.cmake`). A code block that is
deliberately *not* from the example -- each misuse under
[What is refused](#what-is-refused) -- carries a
`<!-- snippet: not from the example -->` comment directly above it, which
that check skips. **Compiler diagnostics** are the library's refusals of
those misuses as g++ 14.2 printed them, each block opening `static assertion
failed` -- and once as cl 19.51 printed one, opening with the header's path
and line. Both compilers print the same message, one per misuse.

## Headers and names

The umbrella header, `formula.hpp`, brings calculations and worksheets: it
includes `calculation.hpp`. It includes nothing that turns them into text,
since each of those pulls in `<string>`, `<vector>` or `<format>`, which a
program that only calculates must not compile: `render.hpp` for `render`,
`describe_graph` and `to_dot`; `document.hpp` for `document`; `trace.hpp` for
`explain_worksheet`; `trace_render.hpp` for `render_derivation`; and
`format.hpp` for `std::format` of a number ([Displaying numbers](display.md)).
Include whichever you need, by name. The example includes them all:

```cpp
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>
```

and spells three names short:

```cpp
namespace unit = formula::unit;
using formula::Rational;
using formula::var;
```

## Money of its own

A bill is in money, and the library ships no currency. Each currency is a
base dimension of its own
([Base dimensions the SI does not have](dimensions.md#base-dimensions-the-si-does-not-have)),
so a price cannot be added to a bare number, and euros are never converted
into yen:

```cpp
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit EuroCent { .dimension = Euro.dimension,
                                          .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 100,
                                          .symbolText = formula::symbol("ct"),
                                          .decimals = 0 };
// One euro per kilowatt-hour is one euro per 3600000 joules.
inline constexpr formula::Unit EuroPerKilowattHour { .dimension = Euro.dimension / formula::dim::Energy,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 3'600'000,
                                                     .symbolText = formula::symbol("EUR/kWh"),
                                                     .decimals = 4 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };
```

An exchange rate is data, not a constant: a quantity in yen per euro, read like
any other input, and never a factor the dimension system applies on its own.
The two currencies measure different things, and the program says so where it
is compiled:

```cpp
static_assert(!formula::SameDimension<Euro.dimension, Yen.dimension>);
```

The power and energy units -- watts, kilowatts, hours and kilowatt-hours --
are the library's own.

## Quantities

Each value the bill names is a quantity: a type with a symbol, a description
and the unit its values are stated in
([Quantities and measurements](quantities.md)). The fridge's power is an
input, stated in watts, and the grid price one in the euros per kilowatt-hour
declared above:

```cpp
struct FridgeW: formula::Quantity<FridgeW, "fridge_w", "the fridge's power", unit::Watt>
{
};
```

```cpp
struct Price: formula::Quantity<Price, "price", "the grid price", EuroPerKilowattHour>
{
};
```

The same power in kilowatts is a calculated value:

```cpp
struct FridgeKw: formula::Quantity<FridgeKw, "fridge_kw", "the fridge's power in kilowatts", unit::Kilowatt>
{
};
```

A value is converted exactly wherever it is read in another unit, so the
definition `define<FridgeKw>(var<FridgeW>)` below is the whole conversion from
watts to kilowatts: 400 W is 0.4 kW.

## Defining named values

`define<Q>(expression)` says that the quantity `Q` is calculated by
`expression`, and `calculation(...)` puts the definitions together. The bill's
two plain numbers -- the share of the solar yield the household uses itself,
and the rate of the tax -- are constants:

```cpp
inline constexpr Rational selfUseShare { 4, 5 };
inline constexpr Rational vatRate { 19, 100 };
```

and every other value is defined once, by what it is calculated from:

```cpp
inline constexpr auto bill = formula::calculation(
    formula::define<FridgeKw>(var<FridgeW>),
    formula::define<FridgeKwh>(var<FridgeKw> * var<FridgeH>),
    formula::define<OvenKwh>(var<OvenKw> * var<OvenH>),
    formula::define<HeaterKwh>(var<HeaterKw> * var<HeaterH>),
    formula::define<DailyLoad>(var<FridgeKwh> + var<OvenKwh> + var<HeaterKwh>),
    formula::define<MonthlyLoad>(var<DailyLoad> * Rational { 30 }),
    formula::define<SelfUsed>(var<Solar> * selfUseShare),
    formula::define<Exported>(var<Solar> - var<SelfUsed>),
    formula::define<NetDraw>(var<MonthlyLoad> - var<SelfUsed>),
    formula::define<GridCost>(var<NetDraw> * var<Price>),
    formula::define<FeedInCredit>(var<Exported> * var<FeedIn>),
    formula::define<EnergyCost>(var<GridCost> - var<FeedInCredit>),
    formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>),
    formula::define<Vat>(var<Subtotal> * vatRate),
    formula::define<Total>(
        formula::rounded<EuroCent, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Subtotal> + var<Vat>)));
```

A definition is an ordinary formula of single values, and may hold what such
a formula may hold: here, a rounding to whole cents
([Rounding and conditionals](rounding-and-conditionals.md)). `var<NetDraw>` in
the grid cost's definition reads the value the net draw's definition
calculates. It is not the net draw's expression written out again.

**The order the definitions are given in does not matter.** The calculation
works out what each one reads, and calculates every value after the values it
reads. A quantity that is read and never defined is an **input**: the bill
has ten, from the fridge's power to the base fee, in the order the
definitions first read them.

`render` writes the calculation out, one `symbol = expression` line per
definition in the order it calculates them, in any of the
[three dialects](citations.md#the-three-dialects). The example asks for its
numbers as decimals where that is their exact value
([Displaying numbers](display.md)):

```cpp
formula::NumberStyle const decimals = formula::NumberStyle::exact_decimal();
```

```cpp
std::printf("the calculation, in the order it calculates:\n%s\n\n",
            formula::render(bill, formula::DefaultVocabulary {}, { .numbers = decimals }).c_str());
```

```text
fridge_kw = fridge_w
fridge_kwh = fridge_kw * fridge_h
oven_kwh = oven_kw * oven_h
heater_kwh = heater_kw * heater_h
daily_load = fridge_kwh + oven_kwh + heater_kwh
monthly_load = daily_load * 30
self_used = solar * 0.8
exported = solar - self_used
net_draw = monthly_load - self_used
grid_cost = net_draw * price
feed_in_credit = exported * feed_in
energy_cost = grid_cost - feed_in_credit
subtotal = energy_cost + base_fee
vat = subtotal * 0.19
total = round(subtotal + vat, to 0 dp of ct)
```

**Each definition is checked where it is written.** Its expression must
measure what its quantity measures: the net draw is an energy, and a
definition that multiplied an energy by a price would be a cost, and does not
compile. The calculation as a whole must be one whose values can be
calculated in some order. The misuses and their messages are under
[What is refused](#what-is-refused).

## The graph, known while the program compiles

Because what each definition reads is part of its type, so is the graph: which
value reads which, directly and through any chain. A program can state a fact
about its own calculation and have it checked while it compiles:

```cpp
static_assert(formula::depends_on<Total, Price>(bill));
static_assert(!formula::depends_on<Exported, Price>(bill));
```

The queries name the values themselves, each as its symbol: the inputs
first, in the order the definitions first read them, then the calculated
values in the order the calculation calculates them:

```cpp
std::printf("affected by price    : %s\n", listed(formula::affected_by<Price>(bill)).c_str());
std::printf("upstream of net_draw : %s\n", listed(formula::upstream_of<NetDraw>(bill)).c_str());
std::printf("read by self_used    : %s\n", listed(formula::dependents_of<SelfUsed>(bill)).c_str());
```

```text
affected by price    : grid_cost, energy_cost, subtotal, vat, total
upstream of net_draw : fridge_w, fridge_h, oven_kw, oven_h, heater_kw, heater_h, solar, fridge_kw, fridge_kwh, oven_kwh, heater_kwh, daily_load, monthly_load, self_used
read by self_used    : exported, net_draw
```

`affected_by<Q>` is every value a change of `Q` reaches, and `upstream_of<Q>`
every value `Q` is reached from; `dependents_of<Q>` and `dependencies_of<Q>`
are the direct readers and reads only, `inputs_of` the inputs, and
`calculation_order` the calculated values. Each returns a `std::array` of
`std::string_view` whose size is known at compile time, so a query is a
constant expression as well. `listed` is the example's own, joining the names
with commas. Each query takes a vocabulary as a second argument, and then
names each value as that vocabulary writes it
([Methods and overlays](methods-and-overlays.md)).

The graph can also be read whole. `describe_graph` lists the inputs, and then
every calculated value with what it reads, the arrows aligned. A value's reads
come in the queries' order -- its inputs first -- so the fridge's energy reads
its hours before its power. `to_dot` writes the graph in the DOT language of
Graphviz, for `dot -Tsvg` to draw, the inputs as boxes and the calculated
values as ellipses:

```cpp
std::printf("its graph:\n%s\n", formula::describe_graph(bill).c_str());
```

```text
inputs: fridge_w, fridge_h, oven_kw, oven_h, heater_kw, heater_h, solar, price, feed_in, base_fee
fridge_kw      <- fridge_w
fridge_kwh     <- fridge_h, fridge_kw
oven_kwh       <- oven_kw, oven_h
heater_kwh     <- heater_kw, heater_h
daily_load     <- fridge_kwh, oven_kwh, heater_kwh
monthly_load   <- daily_load
self_used      <- solar
exported       <- solar, self_used
net_draw       <- monthly_load, self_used
grid_cost      <- price, net_draw
feed_in_credit <- feed_in, exported
energy_cost    <- grid_cost, feed_in_credit
subtotal       <- base_fee, energy_cost
vat            <- subtotal
total          <- subtotal, vat
```

```cpp
std::string const drawn = formula::to_dot(bill);
```

It opens with the inputs,

```text
digraph calculation {
  rankdir=LR;
  node [fontname="Helvetica"];
  q0 [label="fridge_w", shape=box];
  q1 [label="fridge_h", shape=box];
```

and ends with the arrows into the tax and the total:

```text
  q22 -> q23;
  q22 -> q24;
  q23 -> q24;
}
```

`document(bill)` makes the calculation's documentation page
([Citations and rendering](citations.md)), here with its numbers as decimals
too:

```cpp
formula::Documentation const page = formula::document(bill, formula::DefaultVocabulary {}, { .numbers = decimals });
```

Its [symbol table](citations.md#the-symbol-table-and-its-ordering-rule),
`page.symbols`, has a `SymbolEntry` for each of the 25 quantities, whose
`calculatedAs` holds a calculated value's definition as `render` writes it and
is empty for an input. The example prints one line each: first the calculated
values, with their definitions,

```text
self_used      the solar energy used at home       calculated as solar * 0.8
```

and after the last of them, the inputs:

```text
total          the bill                            calculated as round(subtotal + vat, to 0 dp of ct)
fridge_w       the fridge's power                  an input
fridge_h       the fridge's hours a day            an input
```

## A worksheet

A worksheet is made from a calculation and an environment of its inputs,
exactly the environment a formula is evaluated in
([The environment](expressions.md#the-environment)):

```cpp
auto sheet = formula::worksheet(bill,
                                formula::environment(formula::Measured<FridgeW> { Rational { 200 } },
                                                     formula::Measured<FridgeH> { Rational { 24 } },
                                                     formula::Measured<OvenKw> { Rational { 5, 2 } },
                                                     formula::Measured<OvenH> { Rational { 1 } },
                                                     formula::Measured<HeaterKw> { Rational { 3, 2 } },
                                                     formula::Measured<HeaterH> { Rational { 4 } },
                                                     formula::Measured<Solar> { Rational { 150 } },
                                                     formula::Measured<Price> { Rational { 8, 25 } },
                                                     formula::Measured<FeedIn> { Rational { 2, 25 } },
                                                     formula::Measured<BaseFee> { Rational { 25, 2 } }));
```

Every input must be given. One nobody measured is given as
`Measured<Q>::absent()`, which says so, where an input simply left out would
let a missing reading pass for a mistake nobody noticed. An entry the
calculation neither reads nor defines is refused too: it would do nothing,
and most likely names the wrong quantity. Nothing is calculated when the
worksheet is made.

## Asking for values

`calculate<Q>()` answers with an `Outcome<Q>`, as evaluating a formula does
([The outcome](expressions.md#the-outcome)): a value, an absence, or a value
typed in by hand. Asked for several values at once, it answers with a
`std::tuple` of them, and a structured binding takes them apart:

```cpp
auto const [total, netDraw] = sheet.calculate<Total, NetDraw>();
```

The example spells each answer with `std::format` (`format.hpp`).
`.2HalfAwayFromZero` rounds to two places and pads to them: the total is whole
cents already, so only the padding shows, and the mode -- which `std::format`
requires on every rounding
([Displaying numbers](display.md#rounding-modes-and-why-none-is-assumed)) --
is the bill's own. `{}` writes the net draw's exact decimal. Each comes with
its unit:

```cpp
std::format("{:<26} total {:.2HalfAwayFromZero}, net draw {}, recomputed {}, reused {}",
            step,
            total.measurement(),
            netDraw.measurement(),
            counted.recomputed,
            counted.reused)
```

```text
first run:                 total 118.26 EUR, net draw 279 kWh, recomputed 15, reused 0
```

`step` labels the line, and `counted` is how far the worksheet's counters
moved during this one question: the example reads `recomputed()` and
`reused()` before it asks and after, since each is a running total.

The same question can name the variables instead of their types:

```cpp
auto const [sunnierTotal, sunnierDraw] = sunnier.calculate(var<Total>, var<NetDraw>);
```

**`checked_calculate` returns what `calculate` throws.** A calculation can
fail -- a division by zero, an overflow -- and `checked_calculate` answers
with a `std::expected<Outcome<Q>, ArithmeticError>` for each value asked,
where `calculate` throws `ArithmeticException` for the first failure.
[Failure and absence](#failure-and-absence) shows both.

Asking is not `const`. It brings every value the answer is reached through up
to date, calculating each at most once, and counts: `recomputed()` is how
many values the worksheet has calculated since it was made, and `reused()`
how many it found still up to date after a change and did not calculate
again -- running totals, both. The first time the bill is asked for its total
and net draw, it calculates the fifteen values those are reached through, each
once -- 118.26 EUR and 279 kWh -- and reuses none. A value no question reaches
is not calculated at all.

## A change, and what it reaches

`set(...)` gives an input a new value:

```cpp
sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
```

```text
price 0.25 EUR/kWh:        total 95.02 EUR, net draw 279 kWh, recomputed 5, reused 0
```

Nothing is calculated when an input is set. The worksheet marks every value
the change can reach as out of date, and the next question calculates those
of them it needs. After a new price, the grid cost and what is built on it --
the energy cost, the subtotal, the tax and the total -- are calculated again:
five values, and the total is 95.02 EUR.

**The same value again is no change.** Setting the price to 0.25 EUR/kWh a
second time marks nothing, and the next question calculates nothing:

```cpp
sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
```

```text
the same price again:      total 95.02 EUR, net draw 279 kWh, recomputed 0, reused 0
```

A new base fee reaches three values, the subtotal, the tax and the total:

```cpp
sheet.set(formula::Measured<BaseFee> { Rational { 15 } });
```

```text
base fee 15 EUR:           total 98.00 EUR, net draw 279 kWh, recomputed 3, reused 0
```

**A value that comes out the same stops the change there.** Several inputs
can be set at once; here the fridge draws twice the power for half the time:

```cpp
sheet.set(formula::Measured<FridgeW> { Rational { 400 } }, formula::Measured<FridgeH> { Rational { 12 } });
```

```text
fridge 400 W for 12 h:     total 98.00 EUR, net draw 279 kWh, recomputed 2, reused 8
```

The fridge's power in kilowatts is calculated again, and so is its energy a
day -- which comes out at 4.8 kWh, what it was. The eight values built on it,
from the daily load to the total, are then up to date as they are: none of
what they read has changed. The worksheet reuses them rather than calculating
them again, so the change costs two calculations, and `reused()` counts
eight. A value calculated again that comes out equal to its old value, failure
and all, leaves what reads it alone.

## What if?

`with(...)` answers a what-if question on a copy, and leaves the worksheet it
was asked of as it was:

```cpp
auto sunnier = sheet.with(formula::Measured<Solar> { Rational { 200 } });
```

The copy holds everything the worksheet had calculated, so a question to it
calculates only what the change reaches: nine values, for 85.14 EUR and
239 kWh drawn. Its counters start where the worksheet's stood, and the line
says how far the copy's `recomputed()` moved:

```text
with 200 kWh of sun:       total 85.14 EUR, net draw 239 kWh, recomputed 9
```

Asked again, the worksheet itself still answers 98.00 EUR and calculates
nothing:

```text
the worksheet itself:      total 98.00 EUR, net draw 279 kWh, recomputed 0, reused 0
```

## How a value was reached

`explain_worksheet<Q>(sheet)` asks for `Q` and records how the answer was
reached, as one **block** per named value; `render_derivation` writes them
out for a person. The example asks how the daily load was reached, right
after the fridge's change -- which reused the daily load rather than
calculating it again:

```cpp
auto const dailyLoad = formula::explain_worksheet<DailyLoad>(sheet);
std::string const dailyText = formula::render_derivation(dailyLoad, { .maxSteps = 30, .numbers = decimals });
```

`Q`'s block comes first: its definition, evaluated step by step as a trace
states a formula ([Reading a derivation](tracing.md#reading-a-derivation)).
In it, a calculated value it reads is one step, marked `calculated`:

```text
daily_load = fridge_kwh + oven_kwh + heater_kwh = 13.3 kWh
  1. fridge_kwh = 4.8 kWh, calculated
  2. oven_kwh = 2.5 kWh, calculated
  3. #1 + #2 = 26280000
  4. heater_kwh = 6 kWh, calculated
  5. #3 + #4 = 47880000
```

That value's own block, further on, says how it was reached, and the inputs
read come last, one line each:

```text
fridge_kwh = fridge_kw * fridge_h = 4.8 kWh
  1. fridge_kw = 0.4 kW, calculated
  2. fridge_h = 12 h
  3. #1 * #2 = 17280000
fridge_kw = fridge_w = 0.4 kW
  1. fridge_w = 400 W
inputs
  fridge_w = 400 W
  fridge_h = 12 h
  oven_kw = 2.5 kW
  oven_h = 1 h
  heater_kw = 1.5 kW
  heater_h = 4 h
```

A value the evaluation never reached -- read only in a `when()` branch not
taken, or to the right of an operand that failed -- gets no block. One step
limit bounds every line, as `render_trace`'s does, and a last line says how
many were left out ([A value typed in by hand](#a-value-typed-in-by-hand)
shows one). `.numbers` spells every number as it spells a trace's -- here the
decimals the example named, so the heater's power reads 1.5 kW rather than
3/2 kW.

**A derivation is never stale.** It is recorded afresh on every call, from the
values the worksheet holds, never kept from when a value was calculated. The
daily load was not calculated again after the fridge's change -- it was one
of the eight reused -- and a derivation kept from when it was calculated would
read the fridge at 200 W for 24 h. This one reads 400 W for 12 h, because that
is what the worksheet holds, and the example checks that no 200 W is in it. The
rule that lets a value be reused is what makes this sound: a value is reused
only when nothing it reads has changed since it was calculated, so evaluating
its definition again against what it reads now gives the value it holds.

Asking for `Q` brings it up to date, as `checked_calculate` does, and counts as
it does; recording the blocks calculates nothing again. Here nothing was out
of date, and the example checks that neither counter moved.

A computed step states its value in the coherent unit of its dimension, as
every trace does: the fridge's 4.8 kWh reads `17280000` there, in joules,
under a header in kilowatt-hours.

## A value typed in by hand

A person may know a calculated value better than the calculation: a meter
reading, say, in place of the net draw worked out from the appliances. Set it
with `entered(...)`, as a value typed in:

```cpp
sheet.set(formula::entered(formula::Measured<NetDraw> { Rational { 250 } }));
```

```text
net draw typed in:         total 89.37 EUR, net draw 250 kWh, recomputed 5, reused 0
```

The value typed in stands in place of the calculated one: what is built on it
is calculated again from it -- five values, for 89.37 EUR -- its source reads
`ManuallyEntered`, and what the net draw was calculated from is no longer
read. `is_overridden<NetDraw>()` says whether it stands. The grid cost's
derivation reads it as typed in, and the net draw's own block is one line
saying what it stands in place of. Cut short at five lines, the derivation
says how many it left out:

```cpp
auto const gridCost = formula::explain_worksheet<GridCost>(sheet);
std::string const gridText = formula::render_derivation(gridCost, { .maxSteps = 5, .numbers = decimals });
```

```text
grid_cost = net_draw * price = 62.5 EUR
  1. net_draw = 250 kWh, entered by hand
  2. price = 0.25 EUR/kWh
  3. #1 * #2 = 62.5
net_draw = 250 kWh, entered by hand in place of monthly_load - self_used
... 1 further step not shown
```

`clear_override` goes back to calculating it:

```cpp
sheet.clear_override<NetDraw>();
```

```text
the override cleared:      total 98.00 EUR, net draw 279 kWh, recomputed 6, reused 0
```

The net draw is calculated again, 279 kWh, and so are the five values built on
it: six calculations. A calculated value given as a plain measurement,
`set(Measured<NetDraw> { ... })`, is refused: a measurement of a value the
calculation calculates is only ever a person's value in its place, and says
so by being `entered`.

## Failure and absence

A value whose calculation fails is kept like any other answer, and a
definition that reads it fails where it reads it, with the same error, as an
operand's failure fails a formula. The example shows it with a second, small
calculation: the bill shared among the people who live there, each share in
whole cents.

```cpp
inline constexpr auto sharing = formula::calculation(
    formula::define<Share>(var<SharedCost> / var<Occupants>),
    formula::define<ShareInCents>(
        formula::rounded<EuroCent, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Share>)));
```

The bill's total is the second calculation's input -- one calculation's
result read by another as a measurement, which is also how a calculation too
large for one graph is split in two ([Limits](#limits)):

```cpp
formula::Measured<SharedCost> const sharedCost { sheet.calculate<Total>().measurement().value() };
auto shares = formula::worksheet(
    sharing, formula::environment(sharedCost, formula::Measured<Occupants> { Rational { 3 } }));
```

```text
98.00 EUR shared by 3: 32.67 EUR each
```

With nobody to share it, the share divides by zero, and the share in cents,
which reads it, fails with it:

```cpp
shares.set(formula::Measured<Occupants> { Rational { 0 } });
auto const [share, shareInCents] = shares.checked_calculate<Share, ShareInCents>();
```

```text
shared by nobody: share: division by zero, in cents: division by zero
```

`calculate` throws the same failure, as an `ArithmeticException` whose
`code()` is the `ArithmeticError` -- and calculates nothing to do so. The
failure is kept, and is not calculated again until something it read changes:

```cpp
try
{
    static_cast<void>(shares.calculate<ShareInCents>());
}
catch (formula::ArithmeticException const& failure)
{
    std::printf("calculate<ShareInCents>() threw: %s\n", failure.what());
    thrown = failure.code() == formula::ArithmeticError::DivisionByZero;
}
```

```text
calculate<ShareInCents>() threw: division by zero
asked again: recomputed 0
```

A value calculated again that fails with the same error counts as unchanged.
Its derivation is a block like any other, the failure in its header and in the
step that read it:

```cpp
auto const failedShare = formula::explain_worksheet<ShareInCents>(shares);
std::printf("\nhow the failure was reached:\n%s",
            formula::render_derivation(failedShare, { .maxSteps = 12, .numbers = decimals }).c_str());
```

```text
share_ct = round(share, to 0 dp of ct) = division by zero
  1. share = division by zero, calculated
  2. round(#1, to 0 dp of ct) = division by zero [nearest, ties away from zero]
share = shared_cost / occupants = division by zero
  1. shared_cost = 98 EUR
  2. occupants = 0
  3. #1 / #2 = division by zero
inputs
  shared_cost = 98 EUR
  occupants = 0
```

**Absence is not failure.** An input given as `Measured<Q>::absent()` makes
what reads it absent, as it makes a formula's result absent
([Absence propagates](expressions.md#absence-propagates)), never zero:

```cpp
shares.set(formula::Measured<Occupants>::absent());
```

```text
occupants not counted: share (not measured), in cents (not measured)
```

And a `when()` branch not taken reads nothing: a failed value read only there
does not fail the value whose definition holds it.

## What is refused

Each misuse below is refused where it is written, with one message. Each
snippet uses the example's quantities; `ApplianceKwh`, `DryerKw` and
`LastMonth` are quantities and a role the example does not declare.

**A definition whose expression measures something else.** A kilowatt-hour
times a price is money, not energy:

<!-- snippet: not from the example -->
```cpp
formula::define<NetDraw>(var<MonthlyLoad> * var<Price>)
```

```
static assertion failed: formula: this definition's expression measures a different dimension from the quantity it defines; every read of the quantity would be a value it does not measure -- the quantity and the expression appear in this diagnostic as the template arguments Q and Expr of RequireDefinitionMeasuresQuantity
```

**A definition that reads what it defines**, which could never be calculated:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<MonthlyLoad>(var<MonthlyLoad> + var<DailyLoad>))
```

```
static assertion failed: formula: this definition reads the quantity it defines, so it can never be calculated -- the quantity appears in this diagnostic as the template argument Q of RequireDefinitionNotSelfReferential
```

**A quantity defined twice.** Taking the first or the last definition would be
a guess either way:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<Vat>(var<Subtotal> * vatRate),
                     formula::define<Vat>(var<Subtotal> / Rational { 5 }))
```

```
static assertion failed: formula: this calculation defines the same quantity more than once; first-wins and last-wins are equally arbitrary, so neither is guessed -- the quantities defined appear in this diagnostic as the template arguments of RequireDistinctDefinitions
```

**Definitions that read one another in a cycle**, none of which could be
calculated first. The message names every quantity on the cycle:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<Subtotal>(var<Total> - var<Vat>),
                     formula::define<Vat>(var<Subtotal> * vatRate),
                     formula::define<Total>(var<Subtotal> + var<Vat>))
```

```
static assertion failed: formula: these definitions read one another in a cycle, so none of them can be calculated first -- the quantities on the cycle appear in this diagnostic as the template arguments of RequireAcyclicDefinitions
```

cl 19.51 says the same, at the check's own line:

```
include\formula-cpp/calculation.hpp(550): error C2338: static assertion failed: 'formula: these definitions read one another in a cycle, so none of them can be calculated first -- the quantities on the cycle appear in this diagnostic as the template arguments of RequireAcyclicDefinitions'
```

**A worksheet missing an input.** Here the environment has no price:

<!-- snippet: not from the example -->
```cpp
formula::worksheet(bill,
                   formula::environment(formula::Measured<FridgeW> { Rational { 200 } },
                                        formula::Measured<FridgeH> { Rational { 24 } },
                                        formula::Measured<OvenKw> { Rational { 5, 2 } },
                                        formula::Measured<OvenH> { Rational { 1 } },
                                        formula::Measured<HeaterKw> { Rational { 3, 2 } },
                                        formula::Measured<HeaterH> { Rational { 4 } },
                                        formula::Measured<Solar> { Rational { 150 } },
                                        formula::Measured<FeedIn> { Rational { 2, 25 } },
                                        formula::Measured<BaseFee> { Rational { 25, 2 } }))
```

```
static assertion failed: formula: this worksheet's environment provides no value for an input of its calculation; supply one, as Measured<Q>::absent() if it was not measured -- the input and the environment appear in this diagnostic as the template arguments of RequireCalculationInput
```

**An entry nobody reads.** The ten inputs, and a dryer's power no definition
reads:

<!-- snippet: not from the example -->
```cpp
formula::worksheet(bill, bill_inputs(formula::Measured<DryerKw> { Rational { 2 } }))
```

```
static assertion failed: formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines; an entry nobody reads would silently do nothing, most likely because it names the wrong quantity -- the entry appears in this diagnostic as the template argument of RequireEntryReadByCalculation
```

(`bill_inputs` stands for the example's ten measurements, followed by what it
is given.)

**A calculated value given as a measurement**, rather than typed in by hand:

<!-- snippet: not from the example -->
```cpp
sheet.set(formula::Measured<NetDraw> { Rational { 250 } });
```

```
static assertion failed: formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement; override a calculated value by hand with entered(Measured<Q> { ... }) -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedOverriddenByEntry
```

**A question about a quantity the calculation does not hold:**

<!-- snippet: not from the example -->
```cpp
sheet.checked_calculate<DryerKw>()
```

```
static assertion failed: formula: this worksheet's calculation neither defines nor reads the quantity asked for; the quantity and the calculation appear in this diagnostic as the template arguments of RequireWorksheetResult
```

**Clearing an override of an input.** An input is never overridden; it is
simply set again:

<!-- snippet: not from the example -->
```cpp
sheet.clear_override<Price>();
```

```
static assertion failed: formula: clear_override or is_overridden names an input of the calculation; only a calculated quantity is overridden by hand, and an input is simply set again -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedQuantity
```

**The same quantity set twice** in one `set()`: taking the first or the last
would be a guess either way.

<!-- snippet: not from the example -->
```cpp
sheet.set(formula::Measured<Price> { Rational { 1, 4 } }, formula::Measured<Price> { Rational { 1, 5 } });
```

```
static assertion failed: formula: set() was given the same quantity more than once; first-wins and last-wins are equally arbitrary, so neither is guessed -- the entries appear in this diagnostic as the template arguments of RequireDistinctSettings
```

**Setting a quantity the calculation does not hold:**

<!-- snippet: not from the example -->
```cpp
sheet.set(formula::Measured<DryerKw> { Rational { 2 } });
```

```
static assertion failed: formula: set() names a quantity this worksheet's calculation neither reads nor defines; the quantity and the calculation appear in this diagnostic as the template arguments of RequireSettableQuantity
```

**A query about a quantity the calculation does not hold:**

<!-- snippet: not from the example -->
```cpp
formula::affected_by<DryerKw>(bill)
```

```
static assertion failed: formula: this calculation neither defines nor reads this quantity; the quantity and the calculation appear in this diagnostic as the template arguments of RequireCalculationQuantity
```

The rest are refused the same way, each with a message of its own: a series
given to `define<Q>` as its expression, `calculation()` with no definitions,
an argument to `calculation()` that is not a definition, a node kind the
calculation cannot see inside, and a series or raw observations given to a
worksheet.

A refused calculation says nothing more: a query over it, a worksheet of it, a
question to that worksheet, a derivation or a documentation page of it adds no
second message to the one that refused it.

## Limits

**Single values only.** A calculation holds one value per quantity. A
definition that reads a series or raw observations, even to reduce them, is
refused -- read their reduction as an input, calculated elsewhere:

<!-- snippet: not from the example -->
```cpp
formula::define<DailyLoad>(formula::sum(formula::series<ApplianceKwh, 3>))
```

```
static assertion failed: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations; read it with var<Q> -- the quantity appears in this diagnostic as the template argument Q of RequireSingleValueReadsInCalculation
```

**No reads from another record.** A definition reads the worksheet's own
values; a formula that reads another sample or another test
([Other samples and other tests](records.md)) is evaluated against a
`record_context` instead:

<!-- snippet: not from the example -->
```cpp
formula::define<Exported>(var<Solar> - formula::from_record<LastMonth>(var<SelfUsed>))
```

```
static assertion failed: formula: a calculation's definitions read the worksheet's own values, and this one reads from another record with from_record; evaluate that formula against a record_context instead -- the role appears in this diagnostic as the template argument of RequireNoRecordReadInCalculation
```

**At most 64 quantities**, inputs and calculated values together: the graph
is kept as one 64-bit word per quantity. `test/negative/calculation_over_capacity.cpp`
builds a calculation of 65, and is refused with:

```
static assertion failed: formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more; the count appears in this diagnostic as the template argument of RequireCalculationWithinCapacity -- split it into two, the second reading the first's results as inputs
```

**The graph is what a definition may read.** A `when()` counts both its
branches as read, since which one is taken is known only when it is
evaluated. So a value read only on the branch not taken is still brought up
to date when the value holding the `when()` is asked for, and a change to it
still marks that value out of date -- though it is not read, its failure does
not reach, and it has no block in a derivation.

**A calculated value is kept in its declared unit.** It is stored in the unit
its quantity was declared in, and converted back to the coherent unit where a
definition reads it. Both conversions are exact, but either can overflow
where the formula written out as one tree, which makes neither, would not
([Numeric headroom](numeric-headroom.md)).
