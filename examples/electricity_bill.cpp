// SPDX-License-Identifier: Apache-2.0
//
// A household's monthly electricity bill, as a calculation and a worksheet.
//
// Every named value of the bill -- the fridge's energy a day, the energy drawn
// from the grid, the tax -- is defined once, by the expression that calculates
// it, and read by name wherever another definition needs it. The calculation
// knows at compile time what reads what; the worksheet holds the inputs,
// calculates each named value once, and after a change recalculates only what
// the change reaches.
//
// Invented appliances, prices and tariffs, and no real standard. It shows:
//
//   - currencies of its own, each a base dimension the SI does not have, and a
//     rounding to cents as a node of the formula;
//   - the dependency graph described, queried and drawn, all known while the
//     program compiles, and the calculation's documentation page;
//   - a first run, then a change of price, the same price again, a new base
//     fee and a fridge that draws twice the power for half the time -- with
//     how many values each recalculated and how many it reused;
//   - the derivation of the daily load, which that last change reused rather
//     than recalculated, still describing the current inputs;
//   - a what-if copy, and a value typed in by hand in place of a calculated
//     one, with the derivation that reads it, and its clearing;
//   - in a second, smaller calculation -- the bill shared among the people who
//     live there, its total converted into that calculation's input -- a
//     division by zero reaching the value that reads it, the same failure
//     again counted as no change, and an input nobody counted.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- Money ----
//
// The library ships no currency. Each is a base dimension of its own, named by
// base_dimension(), so a price cannot be added to a bare number or converted
// into another currency.
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

// Euros and yen measure different things: no factor turns one into the other,
// because an exchange rate is data, not a constant -- a quantity in yen per
// euro, read like any other input.
static_assert(!formula::SameDimension<Euro.dimension, Yen.dimension>);

// ---- The inputs ----
using FridgeW = formula::Quantity<struct FridgeWTag, "fridge_w", "the fridge's power", unit::Watt>;
using FridgeH = formula::Quantity<struct FridgeHTag, "fridge_h", "the fridge's hours a day", unit::Hour>;
using OvenKw = formula::Quantity<struct OvenKwTag, "oven_kw", "the oven's power", unit::Kilowatt>;
using OvenH = formula::Quantity<struct OvenHTag, "oven_h", "the oven's hours a day", unit::Hour>;
using HeaterKw = formula::Quantity<struct HeaterKwTag, "heater_kw", "the heater's power", unit::Kilowatt>;
using HeaterH = formula::Quantity<struct HeaterHTag, "heater_h", "the heater's hours a day", unit::Hour>;
using Solar = formula::Quantity<struct SolarTag, "solar", "the solar yield of a month", unit::KilowattHour>;
using Price = formula::Quantity<struct PriceTag, "price", "the grid price", EuroPerKilowattHour>;
using FeedIn = formula::Quantity<struct FeedInTag, "feed_in", "the feed-in tariff", EuroPerKilowattHour>;
using BaseFee = formula::Quantity<struct BaseFeeTag, "base_fee", "the monthly base fee", Euro>;

// ---- The calculated values ----
using FridgeKw = formula::Quantity<struct FridgeKwTag, "fridge_kw", "the fridge's power in kilowatts", unit::Kilowatt>;
using FridgeKwh = formula::Quantity<struct FridgeKwhTag, "fridge_kwh", "the fridge's energy a day", unit::KilowattHour>;
using OvenKwh = formula::Quantity<struct OvenKwhTag, "oven_kwh", "the oven's energy a day", unit::KilowattHour>;
using HeaterKwh = formula::Quantity<struct HeaterKwhTag, "heater_kwh", "the heater's energy a day", unit::KilowattHour>;
using DailyLoad = formula::Quantity<struct DailyLoadTag, "daily_load", "the energy used a day", unit::KilowattHour>;
using MonthlyLoad = formula::Quantity<struct MonthlyLoadTag, "monthly_load", "the energy used a month", unit::KilowattHour>;
using SelfUsed = formula::Quantity<struct SelfUsedTag, "self_used", "the solar energy used at home", unit::KilowattHour>;
using Exported = formula::Quantity<struct ExportedTag, "exported", "the solar energy fed into the grid", unit::KilowattHour>;
using NetDraw = formula::Quantity<struct NetDrawTag, "net_draw", "the energy drawn from the grid", unit::KilowattHour>;
using GridCost = formula::Quantity<struct GridCostTag, "grid_cost", "the cost of the energy drawn", Euro>;
using FeedInCredit = formula::Quantity<struct FeedInCreditTag, "feed_in_credit", "the credit for the energy fed in", Euro>;
using EnergyCost = formula::Quantity<struct EnergyCostTag, "energy_cost", "the net cost of energy", Euro>;
using Subtotal = formula::Quantity<struct SubtotalTag, "subtotal", "the bill before tax", Euro>;
using Vat = formula::Quantity<struct VatTag, "vat", "the value-added tax", Euro>;
using Total = formula::Quantity<struct TotalTag, "total", "the bill", Euro>;

// Two numbers the bill states: the share of the solar yield the household
// uses itself, and the rate of the tax. Both are pure numbers.
inline constexpr auto selfUseShare = 0.8_r;
inline constexpr auto vatRate = 0.19_r;

// A bill in whole cents: the euro cent's own decimals, rounded half away from
// zero.
inline constexpr formula::DecimalRounding wholeCents =
    formula::declared_rounding(EuroCent, formula::RoundingMode::HalfAwayFromZero);

// ---- The calculation ----
//
// One definition per named value. The order they are given in does not
// matter: the calculation works out what reads what, and calculates each value
// after the values it reads.
inline constexpr auto bill = formula::calculation(
    formula::define<FridgeKw>(var<FridgeW>),
    formula::define<FridgeKwh>(var<FridgeKw> * var<FridgeH>),
    formula::define<OvenKwh>(var<OvenKw> * var<OvenH>),
    formula::define<HeaterKwh>(var<HeaterKw> * var<HeaterH>),
    formula::define<DailyLoad>(var<FridgeKwh> + var<OvenKwh> + var<HeaterKwh>),
    formula::define<MonthlyLoad>(var<DailyLoad> * 30),
    formula::define<SelfUsed>(var<Solar> * selfUseShare),
    formula::define<Exported>(var<Solar> - var<SelfUsed>),
    formula::define<NetDraw>(var<MonthlyLoad> - var<SelfUsed>),
    formula::define<GridCost>(var<NetDraw> * var<Price>),
    formula::define<FeedInCredit>(var<Exported> * var<FeedIn>),
    formula::define<EnergyCost>(var<GridCost> - var<FeedInCredit>),
    formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>),
    formula::define<Vat>(var<Subtotal> * vatRate),
    formula::define<Total>(formula::rounded<wholeCents>(var<Subtotal> + var<Vat>)));

// What reads what is known while the program compiles.
static_assert(formula::depends_on<Total, Price>(bill));
static_assert(!formula::depends_on<Exported, Price>(bill));

// ---- A second calculation: a cost shared out ----
//
// The bill shared among the people who live there, each share in whole
// cents. With nobody to share it, the share divides by zero, and the share
// in cents, which reads it, fails with it.
using SharedCost = formula::Quantity<struct SharedCostTag, "shared_cost", "the cost to share", Euro>;
using Occupants = formula::Quantity<struct OccupantsTag, "occupants", "the people sharing it", unit::One>;
using Share = formula::Quantity<struct ShareTag, "share", "each one's share", Euro>;
using ShareInCents = formula::Quantity<struct ShareInCentsTag, "share_ct", "each one's share, in whole cents", Euro>;

inline constexpr auto sharing = formula::calculation(
    formula::define<Share>(var<SharedCost> / var<Occupants>),
    formula::define<ShareInCents>(formula::rounded<wholeCents>(var<Share>)));

/// @p names, comma-separated.
template <std::size_t N>
std::string listed(std::array<std::string_view, N> const& names)
{
    std::string joined;
    for (std::string_view const named: names)
        joined += (joined.empty() ? "" : ", ") + std::string { named };
    return joined;
}

/// How many values a worksheet recalculated and reused so far.
struct Counters
{
    std::size_t recomputed;
    std::size_t reused;
};

/// @p sheet's counters.
template <typename Sheet>
Counters counters_of(Sheet const& sheet)
{
    return Counters { .recomputed = sheet.recomputed(), .reused = sheet.reused() };
}
} // namespace

int main()
{
    bool ok = true;
    auto check = [&ok](char const* what, bool condition) {
        std::println("{:<58} {}", what, condition ? "yes" : "NO");
        ok = ok && condition;
    };

    // Every number shown as a decimal where that is its exact value: the
    // self-use share 0.8, the fridge's 4.8 kWh a day.
    auto const decimals = formula::NumberStyle::exact_decimal();

    // ---- 1. The calculation, and what reads what ----
    std::println("the calculation, in the order it calculates:\n{}\n", formula::render(bill, { .numbers = decimals }));
    std::println("its graph:\n{}", formula::describe_graph(bill));

    std::println("inputs               : {}", listed(formula::inputs_of(bill)));
    std::println("calculation order    : {}", listed(formula::calculation_order(bill)));
    std::println("grid_cost reads      : {}", listed(formula::dependencies_of<GridCost>(bill)));
    std::println("affected by price    : {}", listed(formula::affected_by<Price>(bill)));
    std::println("upstream of net_draw : {}", listed(formula::upstream_of<NetDraw>(bill)));
    std::println("read by self_used    : {}", listed(formula::dependents_of<SelfUsed>(bill)));
    check("ten inputs, fifteen calculated, and grid_cost reads two",
          formula::inputs_of(bill).size() == 10 && formula::calculation_order(bill).size() == 15
              && formula::dependencies_of<GridCost>(bill).size() == 2);
    check("a new price reaches five values, and self_used is read by two",
          formula::affected_by<Price>(bill).size() == 5 && formula::dependents_of<SelfUsed>(bill).size() == 2);

    std::string const drawn = formula::to_dot(bill);
    std::println("\nfor Graphviz:\n{}", drawn);

    // Its documentation page: a row per calculated value, with its definition,
    // then a row per input.
    formula::Documentation const page = formula::document(bill, { .numbers = decimals });
    std::println("its symbol table:");
    for (formula::SymbolEntry const& symbolRow: page.symbols)
        std::println("{:<14} {:<35} {}",
                     symbolRow.symbol,
                     symbolRow.description,
                     symbolRow.calculatedAs.has_value() ? "calculated as " + *symbolRow.calculatedAs
                                                        : std::string { "an input" });
    check("25 rows, the self-use share in decimals",
          page.symbols.size() == 25 && page.symbols[6].calculatedAs == "solar * 0.8");

    // ---- 2. A worksheet, and a first run ----
    //
    // The inputs, as an environment of measurements. Nothing is calculated yet.
    auto sheet = formula::worksheet(bill,
                                    formula::environment(formula::Measured<FridgeW> { 200 },
                                                         formula::Measured<FridgeH> { 24 },
                                                         formula::Measured<OvenKw> { 2.5_r },
                                                         formula::Measured<OvenH> { 1 },
                                                         formula::Measured<HeaterKw> { 1.5_r },
                                                         formula::Measured<HeaterH> { 4 },
                                                         formula::Measured<Solar> { 150 },
                                                         formula::Measured<Price> { 0.32_r },
                                                         formula::Measured<FeedIn> { 0.08_r },
                                                         formula::Measured<BaseFee> { 12.5_r }));

    Counters before = counters_of(sheet);
    // Asks for the total and the net draw, and says how many values that
    // recalculated and reused since the last time it asked.
    auto report = [&sheet, &before](char const* step) {
        auto const [total, netDraw] = sheet.calculate<Total, NetDraw>();
        Counters const after = counters_of(sheet);
        Counters const counted { .recomputed = after.recomputed - before.recomputed,
                                 .reused = after.reused - before.reused };
        before = after;
        // The total is in whole cents already; .2 pads it to them: 98.00 EUR.
        std::println("{:<26} total {:.2HalfAwayFromZero}, net draw {}, recomputed {}, reused {}",
                     step,
                     total,
                     netDraw,
                     counted.recomputed,
                     counted.reused);
        return std::pair { formula::number_of(total), counted };
    };

    auto const [firstTotal, firstCount] = report("first run:");
    check("118.26 EUR, every value calculated once",
          firstTotal == 118.26_r && firstCount.recomputed == 15 && firstCount.reused == 0);
    check("279 kWh drawn from the grid", formula::number_of(sheet.calculate<NetDraw>()) == 279_r);

    // ---- 3. Changes, and what each recalculates ----
    //
    // A new price reaches the grid cost and what is built on it: five values.
    sheet.set(formula::Measured<Price> { 0.25_r });
    auto const [cheaperTotal, cheaperCount] = report("price 0.25 EUR/kWh:");
    check("95.02 EUR, five values recalculated",
          cheaperTotal == 95.02_r && cheaperCount.recomputed == 5 && cheaperCount.reused == 0);

    // The same price again changes nothing, and nothing is recalculated.
    sheet.set(formula::Measured<Price> { 0.25_r });
    auto const [samePriceTotal, samePriceCount] = report("the same price again:");
    check("nothing recalculated", samePriceTotal == cheaperTotal && samePriceCount.recomputed == 0
                                      && samePriceCount.reused == 0);

    sheet.set(formula::Measured<BaseFee> { 15 });
    auto const [feeTotal, feeCount] = report("base fee 15 EUR:");
    check("98.00 EUR, three values recalculated",
          feeTotal == 98_r && feeCount.recomputed == 3 && feeCount.reused == 0);

    // Twice the power for half the time: the fridge's energy a day is the
    // same, so what reads it is reused rather than recalculated.
    sheet.set(formula::Measured<FridgeW> { 400 }, formula::Measured<FridgeH> { 12 });
    auto const [fridgeTotal, fridgeCount] = report("fridge 400 W for 12 h:");
    check("two recalculated, eight reused, the total unchanged",
          fridgeTotal == feeTotal && fridgeCount.recomputed == 2 && fridgeCount.reused == 8);

    // ---- 4. The derivation of a value that was reused ----
    //
    // The daily load was not recalculated by that change: what it reads came
    // out the same. Its derivation is recorded afresh from the values the
    // worksheet holds now, so it reads the fridge at 400 W for 12 h all the
    // same -- and recording it calculates nothing.
    Counters const beforeExplaining = counters_of(sheet);
    auto const dailyLoad = formula::explain_worksheet<DailyLoad>(sheet);
    std::string const dailyText = formula::render_derivation(dailyLoad, { .maxSteps = 30, .numbers = decimals });
    std::print("\nhow the daily load was reached:\n{}", dailyText);
    check("the reused daily load reads the fridge's 400 W, and not 200 W",
          dailyText.contains("fridge_w = 400 W") && !dailyText.contains("200 W"));
    check("recording it calculated nothing",
          sheet.recomputed() == beforeExplaining.recomputed && sheet.reused() == beforeExplaining.reused);

    // ---- 5. What if the sun shone more? ----
    //
    // with() answers on a copy; the worksheet itself is left as it was.
    auto sunnier = sheet.with(formula::Measured<Solar> { 200 });
    Counters const copied = counters_of(sunnier);
    auto const [sunnierTotal, sunnierDraw] = sunnier.calculate(var<Total>, var<NetDraw>);
    std::println("\n{:<26} total {:.2HalfAwayFromZero}, net draw {}, recomputed {}",
                 "with 200 kWh of sun:",
                 sunnierTotal,
                 sunnierDraw,
                 sunnier.recomputed() - copied.recomputed);
    check("85.14 EUR and 239 kWh on the copy, nine recalculated",
          formula::number_of(sunnierTotal) == 85.14_r && formula::number_of(sunnierDraw) == 239_r
              && sunnier.recomputed() - copied.recomputed == 9);
    auto const [originalTotal, originalCount] = report("the worksheet itself:");
    check("the worksheet itself unchanged, nothing recalculated",
          originalTotal == feeTotal && originalCount.recomputed == 0 && originalCount.reused == 0);

    // ---- 6. A value typed in by hand, in place of a calculated one ----
    //
    // A meter reading of 250 kWh overrides the calculated net draw: what reads
    // it is recalculated, and what it was calculated from is no longer read.
    sheet.set(formula::entered(formula::Measured<NetDraw> { 250 }));
    auto const [overriddenTotal, overriddenCount] = report("net draw typed in:");
    check("89.37 EUR from 250 kWh typed in",
          overriddenTotal == 89.37_r && sheet.is_overridden<NetDraw>()
              && sheet.calculate<NetDraw>().source() == formula::ValueSource::ManuallyEntered
              && overriddenCount.recomputed == 5);

    // The grid cost reads the value typed in, and its derivation says what
    // that value stands in place of. Cut short at five lines, it says how
    // many it left out.
    auto const gridCost = formula::explain_worksheet<GridCost>(sheet);
    std::string const gridText = formula::render_derivation(gridCost, { .maxSteps = 5, .numbers = decimals });
    std::print("\nhow the grid cost was reached, in five lines:\n{}", gridText);
    check("the override in place of its definition, one line left out",
          gridText.contains("net_draw = 250 kWh, entered by hand in place of monthly_load - self_used\n")
              && gridText.ends_with("... 1 further step not shown\n"));

    sheet.clear_override<NetDraw>();
    auto const [clearedTotal, clearedCount] = report("the override cleared:");
    check("calculated again: 98.00 EUR",
          clearedTotal == feeTotal && !sheet.is_overridden<NetDraw>() && clearedCount.recomputed == 6);

    // ---- 7. One calculation's result, another's input ----
    //
    // The bill shared by the three people who live there, each share in whole
    // cents: the bill's total, read as the second calculation's input. It is
    // converted, not re-wrapped: exactly into the input's unit, an absent
    // total staying absent, and a total of another dimension refused.
    auto const sharedCost = formula::checked_convert_to<SharedCost>(sheet.calculate<Total>().measurement());
    if (!sharedCost.has_value())
    {
        std::println("the total is not a cost to share: {}", sharedCost.error());
        return 1;
    }
    auto shares = formula::worksheet(sharing, formula::environment(*sharedCost, formula::Measured<Occupants> { 3 }));
    auto const eachInCents = shares.calculate<ShareInCents>();
    std::println("\n{:.2HalfAwayFromZero} shared by 3: {} each", *sharedCost, eachInCents);
    check("32.67 EUR each", formula::number_of(eachInCents) == 32.67_r);

    // ---- 8. A failure, and what reads it ----
    //
    // Nobody to share it: the share divides by zero, and the share in cents,
    // which reads it, fails with it.
    shares.set(formula::Measured<Occupants> { 0 });
    auto const [share, shareInCents] = shares.checked_calculate<Share, ShareInCents>();
    std::println("shared by nobody: share: {}, in cents: {}",
                 share.has_value() ? "a value" : formula::describe(share.error()),
                 shareInCents.has_value() ? "a value" : formula::describe(shareInCents.error()));
    check("a division by zero, and the value reading it fails with it",
          !share.has_value() && share.error() == formula::ArithmeticError::DivisionByZero
              && !shareInCents.has_value() && shareInCents.error() == formula::ArithmeticError::DivisionByZero);

    // The throwing form says the same -- and the failure, kept like any
    // answer, is not calculated again: nothing it read has changed.
    Counters const failed = counters_of(shares);
    bool thrown = false;
    try
    {
        static_cast<void>(shares.calculate<ShareInCents>());
    }
    catch (formula::ArithmeticException const& failure)
    {
        std::println("calculate<ShareInCents>() threw: {}", failure.what());
        thrown = failure.code() == formula::ArithmeticError::DivisionByZero;
    }
    std::println("asked again: recomputed {}", shares.recomputed() - failed.recomputed);
    check("calculate() throws what checked_calculate() returns", thrown);
    check("the failure was not calculated again", shares.recomputed() == failed.recomputed);

    auto const failedShare = formula::explain_worksheet<ShareInCents>(shares);
    std::print("\nhow the failure was reached:\n{}",
               formula::render_derivation(failedShare, { .maxSteps = 12, .numbers = decimals }));

    // A new cost, and still nobody to share it: the share is calculated again
    // and fails with the same error, which counts as unchanged, so the share
    // in cents, which reads only the share, is reused.
    Counters const beforeNewCost = counters_of(shares);
    formula::Measured<SharedCost> const newCost { 100 };
    shares.set(newCost);
    auto const stillShared = shares.checked_calculate<ShareInCents>();
    Counters const afterNewCost = counters_of(shares);
    std::println("\n{:.2HalfAwayFromZero}, still shared by nobody: recomputed {}, reused {}",
                 newCost,
                 afterNewCost.recomputed - beforeNewCost.recomputed,
                 afterNewCost.reused - beforeNewCost.reused);
    check("failing again the same way counts as unchanged",
          !stillShared.has_value() && stillShared.error() == formula::ArithmeticError::DivisionByZero
              && afterNewCost.recomputed - beforeNewCost.recomputed == 1
              && afterNewCost.reused - beforeNewCost.reused == 1);

    // ---- 9. An input nobody measured ----
    //
    // The people living there not counted: the occupants are absent, and so
    // is every share -- not zero, and not a failure.
    shares.set(formula::Measured<Occupants>::absent());
    auto const [uncountedShare, uncountedInCents] = shares.calculate<Share, ShareInCents>();
    std::println("\noccupants not counted: share {}, in cents {}", uncountedShare, uncountedInCents);
    check("an absent input leaves what reads it empty", uncountedShare.is_empty() && uncountedInCents.is_empty());

    std::println("\nall checks passed: {}", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
