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
//     program compiles;
//   - a first run, then a change of price, the same price again, a new base
//     fee and a fridge that draws twice the power for half the time -- with
//     how many values each recalculated and how many it reused;
//   - a derivation of a value that was reused rather than recalculated, which
//     still describes the current inputs;
//   - a what-if copy, and a value typed in by hand in place of a calculated
//     one, and its clearing;
//   - in a second, smaller calculation -- a cost shared among the people who
//     live there -- a division by zero reaching the value that reads it.

#include <formula-cpp/calculation.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

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
struct FridgeW: formula::Quantity<FridgeW, "fridge_w", "the fridge's power", unit::Watt>
{
};
struct FridgeH: formula::Quantity<FridgeH, "fridge_h", "the fridge's hours a day", unit::Hour>
{
};
struct OvenKw: formula::Quantity<OvenKw, "oven_kw", "the oven's power", unit::Kilowatt>
{
};
struct OvenH: formula::Quantity<OvenH, "oven_h", "the oven's hours a day", unit::Hour>
{
};
struct HeaterKw: formula::Quantity<HeaterKw, "heater_kw", "the heater's power", unit::Kilowatt>
{
};
struct HeaterH: formula::Quantity<HeaterH, "heater_h", "the heater's hours a day", unit::Hour>
{
};
struct Solar: formula::Quantity<Solar, "solar", "the solar yield of a month", unit::KilowattHour>
{
};
struct Price: formula::Quantity<Price, "price", "the grid price", EuroPerKilowattHour>
{
};
struct FeedIn: formula::Quantity<FeedIn, "feed_in", "the feed-in tariff", EuroPerKilowattHour>
{
};
struct BaseFee: formula::Quantity<BaseFee, "base_fee", "the monthly base fee", Euro>
{
};

// ---- The calculated values ----
struct FridgeKw: formula::Quantity<FridgeKw, "fridge_kw", "the fridge's power in kilowatts", unit::Kilowatt>
{
};
struct FridgeKwh: formula::Quantity<FridgeKwh, "fridge_kwh", "the fridge's energy a day", unit::KilowattHour>
{
};
struct OvenKwh: formula::Quantity<OvenKwh, "oven_kwh", "the oven's energy a day", unit::KilowattHour>
{
};
struct HeaterKwh: formula::Quantity<HeaterKwh, "heater_kwh", "the heater's energy a day", unit::KilowattHour>
{
};
struct DailyLoad: formula::Quantity<DailyLoad, "daily_load", "the energy used a day", unit::KilowattHour>
{
};
struct MonthlyLoad: formula::Quantity<MonthlyLoad, "monthly_load", "the energy used a month", unit::KilowattHour>
{
};
struct SelfUsed: formula::Quantity<SelfUsed, "self_used", "the solar energy used at home", unit::KilowattHour>
{
};
struct Exported: formula::Quantity<Exported, "exported", "the solar energy fed into the grid", unit::KilowattHour>
{
};
struct NetDraw: formula::Quantity<NetDraw, "net_draw", "the energy drawn from the grid", unit::KilowattHour>
{
};
struct GridCost: formula::Quantity<GridCost, "grid_cost", "the cost of the energy drawn", Euro>
{
};
struct FeedInCredit: formula::Quantity<FeedInCredit, "feed_in_credit", "the credit for the energy fed in", Euro>
{
};
struct EnergyCost: formula::Quantity<EnergyCost, "energy_cost", "the net cost of energy", Euro>
{
};
struct Subtotal: formula::Quantity<Subtotal, "subtotal", "the bill before tax", Euro>
{
};
struct Vat: formula::Quantity<Vat, "vat", "the value-added tax", Euro>
{
};
struct Total: formula::Quantity<Total, "total", "the bill", Euro>
{
};

// Two numbers the bill states: the share of the solar yield the household
// uses itself, and the rate of the tax. Both are pure numbers.
inline constexpr Rational selfUseShare { 4, 5 };
inline constexpr Rational vatRate { 19, 100 };

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

// What reads what is known while the program compiles.
static_assert(formula::depends_on<Total, Price>(bill));
static_assert(!formula::depends_on<Exported, Price>(bill));

// ---- A second calculation: a cost shared out ----
//
// The bill shared among the people who live there, each share in whole
// cents. With nobody to share it, the share divides by zero, and the share
// in cents, which reads it, fails with it.
struct SharedCost: formula::Quantity<SharedCost, "shared_cost", "the cost to share", Euro>
{
};
struct Occupants: formula::Quantity<Occupants, "occupants", "the people sharing it", unit::One>
{
};
struct Share: formula::Quantity<Share, "share", "each one's share", Euro>
{
};
struct ShareInCents: formula::Quantity<ShareInCents, "share_ct", "each one's share, in whole cents", Euro>
{
};

inline constexpr auto sharing = formula::calculation(
    formula::define<Share>(var<SharedCost> / var<Occupants>),
    formula::define<ShareInCents>(
        formula::rounded<EuroCent, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Share>)));

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
        std::printf("%-58s %s\n", what, condition ? "yes" : "NO");
        ok = ok && condition;
    };

    // Every number shown as a decimal where that is its exact value: the
    // self-use share 0.8, the fridge's 4.8 kWh a day.
    formula::NumberStyle const decimals = formula::NumberStyle::exact_decimal();

    // ---- 1. The calculation, and what reads what ----
    std::printf("the calculation, in the order it calculates:\n%s\n\n",
                formula::render(bill, formula::DefaultVocabulary {}, { .numbers = decimals }).c_str());
    std::printf("its graph:\n%s\n", formula::describe_graph(bill).c_str());

    std::printf("affected by price    : %s\n", listed(formula::affected_by<Price>(bill)).c_str());
    std::printf("upstream of net_draw : %s\n", listed(formula::upstream_of<NetDraw>(bill)).c_str());
    std::printf("read by self_used    : %s\n", listed(formula::dependents_of<SelfUsed>(bill)).c_str());
    check("the price reaches the total, and not the energy exported",
          formula::affected_by<Price>(bill).size() == 5 && formula::dependents_of<SelfUsed>(bill).size() == 2);

    std::string const drawn = formula::to_dot(bill);
    std::printf("\nfor Graphviz:\n%s\n", drawn.c_str());

    // ---- 2. A worksheet, and a first run ----
    //
    // The inputs, as an environment of measurements. Nothing is calculated yet.
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
        std::printf("%s\n",
                    std::format("{:<26} total {:.2HalfAwayFromZero}, net draw {}, recomputed {}, reused {}",
                                step,
                                total.measurement(),
                                netDraw.measurement(),
                                counted.recomputed,
                                counted.reused)
                        .c_str());
        return std::pair { total.measurement().value(), counted };
    };

    auto const [firstTotal, firstCount] = report("first run:");
    check("118.26 EUR, every value calculated once",
          firstTotal == Rational { 11826, 100 } && firstCount.recomputed == 15 && firstCount.reused == 0);
    check("279 kWh drawn from the grid", sheet.calculate<NetDraw>().measurement().value() == Rational { 279 });

    // ---- 3. Changes, and what each recalculates ----
    //
    // A new price reaches the grid cost and what is built on it: five values.
    sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
    auto const [cheaperTotal, cheaperCount] = report("price 0.25 EUR/kWh:");
    check("95.02 EUR, five values recalculated",
          cheaperTotal == Rational { 9502, 100 } && cheaperCount.recomputed == 5 && cheaperCount.reused == 0);

    // The same price again changes nothing, and nothing is recalculated.
    sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
    auto const [samePriceTotal, samePriceCount] = report("the same price again:");
    check("nothing recalculated", samePriceTotal == cheaperTotal && samePriceCount.recomputed == 0
                                      && samePriceCount.reused == 0);

    sheet.set(formula::Measured<BaseFee> { Rational { 15 } });
    auto const [feeTotal, feeCount] = report("base fee 15 EUR:");
    check("98.00 EUR, three values recalculated",
          feeTotal == Rational { 9800, 100 } && feeCount.recomputed == 3 && feeCount.reused == 0);

    // Twice the power for half the time: the fridge's energy a day is the
    // same, so what reads it is reused rather than recalculated.
    sheet.set(formula::Measured<FridgeW> { Rational { 400 } }, formula::Measured<FridgeH> { Rational { 12 } });
    auto const [fridgeTotal, fridgeCount] = report("fridge 400 W for 12 h:");
    check("two recalculated, eight reused, the total unchanged",
          fridgeTotal == feeTotal && fridgeCount.recomputed == 2 && fridgeCount.reused == 8);

    // ---- 4. A derivation, after that cutoff ----
    //
    // The fridge's energy a day was recalculated, and its derivation is
    // recorded afresh from the values the worksheet holds now: 400 W for 12 h.
    auto const fridgeEnergy = formula::explain_worksheet<FridgeKwh>(sheet);
    std::string const fridgeText =
        formula::render_derivation(fridgeEnergy, { .maxSteps = 12, .numbers = decimals });
    std::printf("\nhow the fridge's energy a day was reached:\n%s", fridgeText.c_str());
    check("the derivation reads the current 400 W",
          fridgeText.find("fridge_w = 400 W") != std::string::npos
              && fridgeText.find("fridge_h = 12 h") != std::string::npos);

    // ---- 5. What if the sun shone more? ----
    //
    // with() answers on a copy; the worksheet itself is left as it was.
    auto sunnier = sheet.with(formula::Measured<Solar> { Rational { 200 } });
    Counters const copied = counters_of(sunnier);
    auto const [sunnierTotal, sunnierDraw] = sunnier.calculate(var<Total>, var<NetDraw>);
    std::printf("\n%s\n",
                std::format("{:<26} total {:.2HalfAwayFromZero}, net draw {}, recomputed {}",
                            "with 200 kWh of sun:",
                            sunnierTotal.measurement(),
                            sunnierDraw.measurement(),
                            sunnier.recomputed() - copied.recomputed)
                    .c_str());
    check("85.14 EUR and 239 kWh on the copy, nine recalculated",
          sunnierTotal.measurement().value() == Rational { 8514, 100 }
              && sunnierDraw.measurement().value() == Rational { 239 }
              && sunnier.recomputed() - copied.recomputed == 9);
    auto const [originalTotal, originalCount] = report("the worksheet itself:");
    check("the worksheet itself unchanged, nothing recalculated",
          originalTotal == feeTotal && originalCount.recomputed == 0 && originalCount.reused == 0);

    // ---- 6. A value typed in by hand, in place of a calculated one ----
    //
    // A meter reading of 250 kWh overrides the calculated net draw: what reads
    // it is recalculated, and what it was calculated from is no longer read.
    sheet.set(formula::entered(formula::Measured<NetDraw> { Rational { 250 } }));
    auto const [overriddenTotal, overriddenCount] = report("net draw typed in:");
    check("89.37 EUR from 250 kWh typed in",
          overriddenTotal == Rational { 8937, 100 } && sheet.is_overridden<NetDraw>()
              && sheet.calculate<NetDraw>().source() == formula::ValueSource::ManuallyEntered
              && overriddenCount.recomputed == 5);

    sheet.clear_override<NetDraw>();
    auto const [clearedTotal, clearedCount] = report("the override cleared:");
    check("calculated again: 98.00 EUR",
          clearedTotal == feeTotal && !sheet.is_overridden<NetDraw>() && clearedCount.recomputed == 6);

    // ---- 7. A failure, and what reads it ----
    //
    // 98.00 EUR shared by three is 32.67 EUR each, in whole cents.
    auto shares = formula::worksheet(sharing,
                                     formula::environment(formula::Measured<SharedCost> { Rational { 98 } },
                                                          formula::Measured<Occupants> { Rational { 3 } }));
    formula::Measured<ShareInCents> const eachInCents = shares.calculate<ShareInCents>().measurement();
    std::printf("\n%s\n", std::format("98.00 EUR shared by 3: {} each", eachInCents).c_str());
    check("32.67 EUR each", eachInCents.value() == Rational { 3267, 100 });

    // Nobody to share it: the share divides by zero, and the share in cents,
    // which reads it, fails with it.
    shares.set(formula::Measured<Occupants> { Rational { 0 } });
    auto const [share, shareInCents] = shares.checked_calculate<Share, ShareInCents>();
    std::printf("shared by nobody: share: %s, in cents: %s\n",
                share.has_value() ? "a value" : std::string { formula::describe(share.error()) }.c_str(),
                shareInCents.has_value() ? "a value"
                                         : std::string { formula::describe(shareInCents.error()) }.c_str());
    check("a division by zero, and the value reading it fails with it",
          !share.has_value() && share.error() == formula::ArithmeticError::DivisionByZero
              && !shareInCents.has_value() && shareInCents.error() == formula::ArithmeticError::DivisionByZero);

    // The throwing form says the same.
    bool thrown = false;
    try
    {
        static_cast<void>(shares.calculate<ShareInCents>());
    }
    catch (formula::ArithmeticException const& failure)
    {
        std::printf("calculate<ShareInCents>() threw: %s\n", failure.what());
        thrown = failure.code() == formula::ArithmeticError::DivisionByZero;
    }
    check("calculate() throws what checked_calculate() returns", thrown);

    auto const failedShare = formula::explain_worksheet<ShareInCents>(shares);
    std::printf("\nhow the failure was reached:\n%s",
                formula::render_derivation(failedShare, { .maxSteps = 12, .numbers = decimals }).c_str());

    std::printf("\nall checks passed: %s\n", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
