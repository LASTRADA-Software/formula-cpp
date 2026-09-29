// SPDX-License-Identifier: Apache-2.0
#pragma once

// A household's monthly electricity bill: ten inputs and fifteen calculated
// values, the calculation the worksheet tests share. The units this library
// does not ship are declared here, as any caller may declare one; money is
// dimensionless, as examples/composition.cpp explains.
//
// In an unnamed namespace, as each test's own fixtures are: every test file
// that includes it gets a copy of its own.

#include <formula-cpp/calculation.hpp>

#include <optional>
#include <type_traits>

namespace
{
namespace household
{
    using formula::var;

    inline constexpr formula::Dimension power = formula::dim::Energy / formula::dim::Time;
    inline constexpr formula::Unit Watt { .dimension = power, .symbolText = formula::symbol("W"), .decimals = 0 };
    inline constexpr formula::Unit Kilowatt { .dimension = power,
                                              .magnitudeNumerator = 1'000,
                                              .symbolText = formula::symbol("kW"),
                                              .decimals = 3 };
    inline constexpr formula::Unit KilowattHour { .dimension = formula::dim::Energy,
                                                  .magnitudeNumerator = 3'600'000,
                                                  .symbolText = formula::symbol("kWh"),
                                                  .decimals = 3 };
    inline constexpr formula::Unit Euro { .dimension = formula::dim::Scalar,
                                          .symbolText = formula::symbol("EUR"),
                                          .decimals = 2 };
    inline constexpr formula::Unit EuroPerKilowattHour { .dimension = formula::dim::Scalar / formula::dim::Energy,
                                                         .magnitudeNumerator = 1,
                                                         .magnitudeDenominator = 3'600'000,
                                                         .symbolText = formula::symbol("EUR/kWh"),
                                                         .decimals = 4 };

    // The inputs.
    struct FridgeW: formula::Quantity<FridgeW, "fridge_w", "the fridge's power", Watt>
    {
    };
    struct FridgeH: formula::Quantity<FridgeH, "fridge_h", "the fridge's hours a day", formula::unit::Hour>
    {
    };
    struct OvenKw: formula::Quantity<OvenKw, "oven_kw", "the oven's power", Kilowatt>
    {
    };
    struct OvenH: formula::Quantity<OvenH, "oven_h", "the oven's hours a day", formula::unit::Hour>
    {
    };
    struct HeaterKw: formula::Quantity<HeaterKw, "heater_kw", "the heater's power", Kilowatt>
    {
    };
    struct HeaterH: formula::Quantity<HeaterH, "heater_h", "the heater's hours a day", formula::unit::Hour>
    {
    };
    struct Solar: formula::Quantity<Solar, "solar", "the solar yield of a month", KilowattHour>
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

    // The calculated values.
    struct FridgeKw: formula::Quantity<FridgeKw, "fridge_kw", "the fridge's power in kilowatts", Kilowatt>
    {
    };
    struct FridgeKwh: formula::Quantity<FridgeKwh, "fridge_kwh", "the fridge's energy a day", KilowattHour>
    {
    };
    struct OvenKwh: formula::Quantity<OvenKwh, "oven_kwh", "the oven's energy a day", KilowattHour>
    {
    };
    struct HeaterKwh: formula::Quantity<HeaterKwh, "heater_kwh", "the heater's energy a day", KilowattHour>
    {
    };
    struct DailyLoad: formula::Quantity<DailyLoad, "daily_load", "the energy used a day", KilowattHour>
    {
    };
    struct MonthlyLoad: formula::Quantity<MonthlyLoad, "monthly_load", "the energy used a month", KilowattHour>
    {
    };
    struct SelfUsed: formula::Quantity<SelfUsed, "self_used", "the solar energy used at home", KilowattHour>
    {
    };
    struct Exported: formula::Quantity<Exported, "exported", "the solar energy fed into the grid", KilowattHour>
    {
    };
    struct NetDraw: formula::Quantity<NetDraw, "net_draw", "the energy drawn from the grid", KilowattHour>
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

    // Given in dependency order: the order the graph keeps.
    inline constexpr auto bill =
        formula::calculation(formula::define<FridgeKw>(var<FridgeW>),
                             formula::define<FridgeKwh>(var<FridgeKw> * var<FridgeH>),
                             formula::define<OvenKwh>(var<OvenKw> * var<OvenH>),
                             formula::define<HeaterKwh>(var<HeaterKw> * var<HeaterH>),
                             formula::define<DailyLoad>(var<FridgeKwh> + var<OvenKwh> + var<HeaterKwh>),
                             formula::define<MonthlyLoad>(var<DailyLoad> * formula::Rational { 30 }),
                             formula::define<SelfUsed>(var<Solar> * formula::Rational { 4, 5 }),
                             formula::define<Exported>(var<Solar> - var<SelfUsed>),
                             formula::define<NetDraw>(var<MonthlyLoad> - var<SelfUsed>),
                             formula::define<GridCost>(var<NetDraw> * var<Price>),
                             formula::define<FeedInCredit>(var<Exported> * var<FeedIn>),
                             formula::define<EnergyCost>(var<GridCost> - var<FeedInCredit>),
                             formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>),
                             formula::define<Vat>(var<Subtotal> * formula::Rational { 19, 100 }),
                             formula::define<Total>(var<Subtotal> + var<Vat>));

    using BillGraph = formula::detail::CalculationGraphOf<std::remove_cv_t<decltype(bill)>>::type;

    /// The bill's ten inputs, each given or not.
    struct BillValues
    {
        std::optional<formula::Rational> fridgeW;
        std::optional<formula::Rational> fridgeH;
        std::optional<formula::Rational> ovenKw;
        std::optional<formula::Rational> ovenH;
        std::optional<formula::Rational> heaterKw;
        std::optional<formula::Rational> heaterH;
        std::optional<formula::Rational> solar;
        std::optional<formula::Rational> price;
        std::optional<formula::Rational> feedIn;
        std::optional<formula::Rational> baseFee;
    };

    /// The fixture's inputs.
    inline constexpr BillValues billValues { .fridgeW = formula::Rational { 200 },
                                             .fridgeH = formula::Rational { 24 },
                                             .ovenKw = formula::Rational { 5, 2 },
                                             .ovenH = formula::Rational { 1 },
                                             .heaterKw = formula::Rational { 3, 2 },
                                             .heaterH = formula::Rational { 4 },
                                             .solar = formula::Rational { 150 },
                                             .price = formula::Rational { 8, 25 },
                                             .feedIn = formula::Rational { 2, 25 },
                                             .baseFee = formula::Rational { 25, 2 } };

    /// @p value measured, or absent when it is not given.
    template <typename Q>
    constexpr formula::Measured<Q> given(std::optional<formula::Rational> const& value)
    {
        return value.has_value() ? formula::Measured<Q> { *value } : formula::Measured<Q>::absent();
    }

    /// An environment of the bill's inputs, measured as @p values says, with
    /// @p extra after them.
    template <typename... Extra>
    constexpr auto bill_environment(BillValues const& values, Extra... extra)
    {
        return formula::environment(given<FridgeW>(values.fridgeW),
                                    given<FridgeH>(values.fridgeH),
                                    given<OvenKw>(values.ovenKw),
                                    given<OvenH>(values.ovenH),
                                    given<HeaterKw>(values.heaterKw),
                                    given<HeaterH>(values.heaterH),
                                    given<Solar>(values.solar),
                                    given<Price>(values.price),
                                    given<FeedIn>(values.feedIn),
                                    given<BaseFee>(values.baseFee),
                                    extra...);
    }
} // namespace household
} // namespace
