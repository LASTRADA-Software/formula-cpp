// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/statistics.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
namespace unit = formula::unit;
using formula::var;
using formula::detail::CalculationReads;
using formula::detail::CalculationReadsOf;
using formula::detail::QuantityList;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented quantities; every value below is invented too.
struct Load: formula::Quantity<Load, "F", "an invented load", unit::Newton>
{
};
struct Width: formula::Quantity<Width, "b", "an invented width", unit::Millimetre>
{
};
struct Depth: formula::Quantity<Depth, "d", "an invented depth", unit::Millimetre>
{
};
struct Stress: formula::Quantity<Stress, "sigma", "an invented stress", unit::Megapascal>
{
};
struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
struct FirstMass: formula::Quantity<FirstMass, "m_1", "an invented first determination", unit::Gram>
{
};
struct SecondMass: formula::Quantity<SecondMass, "m_2", "an invented second determination", unit::Gram>
{
};
struct Tolerance: formula::Quantity<Tolerance, "t", "an invented tolerance", unit::Gram>
{
};
struct Retained: formula::Quantity<Retained, "m_r", "an invented retained mass", unit::Gram>
{
};
struct Doubled: formula::Quantity<Doubled, "s_2", "an invented share, doubled", unit::One>
{
};
struct Halved: formula::Quantity<Halved, "s_h", "an invented share, halved", unit::One>
{
};
struct Start: formula::Quantity<Start, "x_0", "an invented start", unit::One>
{
};
struct Low: formula::Quantity<Low, "x_l", "an invented low point", unit::One>
{
};
struct High: formula::Quantity<High, "x_h", "an invented high point", unit::One>
{
};
struct Apex: formula::Quantity<Apex, "x_a", "an invented apex", unit::One>
{
};

constexpr formula::Citation clause { .reference = "Example Standard 7:2024", .section = "2" };

struct Plain
{
};

inline constexpr auto shareMethod = formula::method(
    formula::variants(formula::variant<Plain>(var<Factor> * var<Other>)),
    formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto derivedFactor =
    formula::apply(formula::overlay(formula::add_derived<Factor>(var<Width> / var<Depth>, clause)), shareMethod);
inline constexpr auto fixedFactor =
    formula::apply(formula::overlay(formula::with_constant<Factor>(rat(103, 100), clause)), shareMethod);
inline constexpr auto replacedShare =
    formula::apply(formula::overlay(formula::replace_variant<Plain>(var<Other> / var<Factor>, clause)), shareMethod);

/// A consumer's operation over two single values: the larger of the two.
struct LargerOfTwo
{
    static constexpr std::string_view name = "larger of two";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "larger" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        if (!(declared[0] == declared[1]))
            return std::nullopt;
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep oneValue,
                                                                                         Rep otherValue) noexcept
    {
        return std::array { otherValue < oneValue ? oneValue : otherValue };
    }
};

inline constexpr formula::BreakpointTable<2> curvePoints { formula::breakpoint(1), formula::breakpoint(2) };
inline constexpr formula::BreakpointTable<1> curveTail { formula::breakpoint(5) };
inline constexpr formula::PlacesTable<3> roundingPlaces { formula::DecimalPlaces { 0 },
                                                          formula::DecimalPlaces { 0 },
                                                          formula::DecimalPlaces { -1 } };
} // namespace

TEST_CASE("define binds a quantity to the expression that calculates it", "[calculation]")
{
    constexpr auto calculatingStress = var<Load> / (var<Width> * var<Depth>) * formula::number(rat(3, 2));
    constexpr auto stress = formula::define<Stress>(calculatingStress);
    using Defined = std::remove_cv_t<decltype(stress)>;
    STATIC_REQUIRE(std::is_same_v<Defined, formula::Definition<Stress, std::remove_cv_t<decltype(calculatingStress)>>>);
    STATIC_REQUIRE(std::is_same_v<Defined::quantity, Stress>);
    STATIC_REQUIRE(Defined::valid);

    // The expression it holds is the one given, its constant included:
    // 13081 N over 103 mm by 127 mm is 1 MPa, times 3/2. An expression whose
    // constant had been lost would give 0.
    constexpr auto specimen = formula::environment(formula::Measured<Load> { rat(13081) },
                                                   formula::Measured<Width> { rat(103) },
                                                   formula::Measured<Depth> { rat(127) });
    STATIC_REQUIRE(formula::checked_evaluate<Stress>(stress.expression, specimen)->measurement().value() == rat(3, 2));
}

TEST_CASE("a definition reads each quantity once, in the order it first reads it", "[calculation]")
{
    // The factor, the other factor, the factor again: listed once each, the
    // factor first. Kept at its last appearance, or walked right to left, the
    // factor would come second.
    using Reads = formula::Definition<Share, decltype(var<Factor> + var<Other> * var<Factor>)>::reads;
    STATIC_REQUIRE(std::is_same_v<Reads, QuantityList<Factor, Other>>);

    // A constant, pi and a bare number read nothing.
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::number(rat(2)) * formula::pi)>, QuantityList<>>);
    STATIC_REQUIRE(
        std::is_same_v<CalculationReadsOf<decltype(formula::constant<unit::Newton>(rat(139)))>, QuantityList<>>);
}

TEST_CASE("a when() reads its condition and both of its branches", "[calculation]")
{
    // Each of the three quantities appears in one place only, so a walk that
    // missed the condition, the branch taken when it holds or the other
    // branch would miss one of them. Both branches are listed although one
    // evaluation reads one: what it may read.
    constexpr auto chosen =
        formula::when(var<Load> > formula::constant<unit::Newton>(rat(139)), var<Factor>, var<Other>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(chosen)>, QuantityList<Load, Factor, Other>>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::define<Share>(chosen))::reads, QuantityList<Load, Factor, Other>>);
}

TEST_CASE("a precision limit reads its level and its limit expression", "[calculation]")
{
    // The second determination is read only in the limit expression, pass
    // 2, which is evaluated against the same environment as the level; the
    // placeholder reads nothing.
    constexpr auto limit = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        var<FirstMass>, rat(1, 50) * formula::precision_level<FirstMass> + var<SecondMass>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(limit)>, QuantityList<FirstMass, SecondMass>>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::define<Tolerance>(limit))::reads, QuantityList<FirstMass, SecondMass>>);
}

TEST_CASE("documented() reads what it documents", "[calculation]")
{
    constexpr auto documentedShare = formula::documented(var<Factor> * var<Other>, clause);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(documentedShare)>, QuantityList<Factor, Other>>);
}

TEST_CASE("an overlay's derived quantity reads its definition, and its fixed constant reads nothing",
          "[calculation][overlay]")
{
    // The factor's derived quantity reads the width and the depth, never the
    // factor it stands for; the fixed factor reads nothing, as its evaluator
    // never asks the environment for its value. Walked as the variables they
    // derive from, both would read the factor.
    constexpr auto derivedShare = std::get<0>(derivedFactor.variantSet.cases).expression;
    constexpr auto fixedShare = std::get<0>(fixedFactor.variantSet.cases).expression;
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(derivedShare)>, QuantityList<Width, Depth, Other>>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(fixedShare)>, QuantityList<Other>>);

    // A replaced formula reads what the replacement reads: the other factor
    // first.
    constexpr auto replaced = std::get<0>(replacedShare.variantSet.cases).expression;
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(replaced)>, QuantityList<Other, Factor>>);

    // A definition of an overlaid formula: 103 mm over 127 mm, times 139/100.
    constexpr auto share = formula::define<Share>(derivedShare);
    STATIC_REQUIRE(std::is_same_v<std::remove_cv_t<decltype(share)>::reads, QuantityList<Width, Depth, Other>>);
    STATIC_REQUIRE(std::remove_cv_t<decltype(share)>::valid);
    constexpr auto specimen = formula::environment(formula::Measured<Width> { rat(103) },
                                                   formula::Measured<Depth> { rat(127) },
                                                   formula::Measured<Other> { rat(139, 100) });
    STATIC_REQUIRE(formula::checked_evaluate<Share>(share.expression, specimen)->measurement().value()
                   == rat(103 * 139, 127 * 100));
}

TEST_CASE("a single value made of a series is walked into, and the scalars it broadcasts are read", "[calculation]")
{
    // A series constant reads nothing; the factor broadcast into it is read.
    constexpr auto scaled = formula::series_constant<unit::Gram>(rat(103), rat(127), rat(197)) * var<Factor>;
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::sum(scaled))>, QuantityList<Factor>>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::sample_mean(scaled) / formula::sample_count(scaled))>,
                                  QuantityList<Factor>>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::sample_variance(scaled)
                                                              / (formula::sample_range(scaled) * var<SecondMass>))>,
                                  QuantityList<Factor, SecondMass>>);

    // A negation, a rounding per element and a running total, summed.
    constexpr auto running = formula::sum(formula::cumulative<formula::CumulativeDirection::FromLast>(
        formula::rounded_elementwise<unit::Gram, roundingPlaces, formula::RoundingMode::HalfAwayFromZero>(-scaled)));
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(running)>, QuantityList<Factor>>);
    STATIC_REQUIRE(formula::Definition<Retained, std::remove_cv_t<decltype(running)>>::valid);

    // A curve over a declared domain, read at the other factor; and two
    // curves spliced, the factor scaling the first.
    constexpr auto alongCurve = formula::interpolate_at(
        formula::curve(formula::domain<unit::One, curvePoints>, formula::series_constant<unit::One>(rat(1, 4), rat(3, 4))),
        var<Other>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(alongCurve)>, QuantityList<Other>>);
    constexpr auto alongSplice = formula::interpolate_at(
        formula::splice<formula::Monotone::NonDecreasing>(
            formula::curve(formula::domain<unit::One, curvePoints>,
                           formula::series_constant<unit::One>(rat(1, 4), rat(3, 4)) * var<Factor>),
            formula::curve(formula::domain<unit::One, curveTail>, formula::series_constant<unit::One>(rat(1)))),
        var<Other>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(alongSplice)>, QuantityList<Factor, Other>>);
    STATIC_REQUIRE(formula::Definition<Share, std::remove_cv_t<decltype(alongSplice)>>::valid);

    // A rejection reads its sample and its limit, and its pass mean is a
    // placeholder, not a read of the retained mass.
    constexpr auto trimmed = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            scaled,
            formula::deviation_from_mean(var<Other> * formula::pass_mean<Retained>),
            formula::Verdict { "repeat the determination" },
            clause);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::sample_count(trimmed))>, QuantityList<Factor, Other>>);
    STATIC_REQUIRE(CalculationReads<std::remove_cv_t<decltype(formula::sample_count(trimmed))>>::accepted);
}

TEST_CASE("an opaque operation's output reads what its inputs read", "[calculation][opaque]")
{
    constexpr auto larger = formula::opaque_output<"larger">(formula::opaque<LargerOfTwo>(clause, var<Factor>, var<Other>));
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(larger)>, QuantityList<Factor, Other>>);
}

TEST_CASE("placeholders and a retry's context are listed as reading nothing", "[calculation][retry]")
{
    // A placeholder stands for a value its construct works out. A retry's
    // context does read, inside a retry -- attempt_input a recorded series --
    // but no definition can hold a retry, and outside one the evaluator
    // refuses these nodes.
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::precision_level<Retained>
                                                              + formula::pass_mean<Retained> * formula::pass_count)>,
                                  QuantityList<>>);
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<decltype(formula::previous_attempt<Retained>
                                                              + formula::this_attempt<Retained>
                                                              + formula::attempt_input<Retained>)>,
                                  QuantityList<>>);
    STATIC_REQUIRE(
        std::is_same_v<CalculationReadsOf<decltype(var<Factor> * formula::attempt_number)>, QuantityList<Factor>>);
}

TEST_CASE("a node refused already reads nothing, and draws no second message", "[calculation]")
{
    // Arithmetic over a retry is refused where it is written, and leaves a
    // node refused already, of no LevelChildren entry: nothing is asked of it.
    // Its dimension, a mass's, is a stand-in, and a definition of a share by
    // it is not refused a second time for measuring something else.
    using OverRetry = formula::detail::RefusedRetryValue<formula::dim::Mass>;
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<OverRetry>, QuantityList<>>);
    STATIC_REQUIRE_FALSE(CalculationReads<OverRetry>::accepted);
    STATIC_REQUIRE_FALSE(formula::Definition<Share, OverRetry>::valid);

    // A series-valued read from another record, refused where it is written:
    // its operand, a series, is not walked -- which would refuse it again.
    using SeriesScope = formula::detail::RefusedSeriesScope<formula::SeriesVarNode<Retained, 3>>;
    STATIC_REQUIRE(std::is_same_v<CalculationReadsOf<SeriesScope>, QuantityList<>>);
    STATIC_REQUIRE_FALSE(CalculationReads<SeriesScope>::accepted);
}

// ---- A calculation's graph: a household's monthly electricity bill -------
//
// Ten inputs and fifteen calculated values. The units this library does not
// ship are declared here, as any caller may declare one; money is
// dimensionless, as examples/composition.cpp explains.

namespace
{
namespace household
{
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
    struct FridgeH: formula::Quantity<FridgeH, "fridge_h", "the fridge's hours a day", unit::Hour>
    {
    };
    struct OvenKw: formula::Quantity<OvenKw, "oven_kw", "the oven's power", Kilowatt>
    {
    };
    struct OvenH: formula::Quantity<OvenH, "oven_h", "the oven's hours a day", unit::Hour>
    {
    };
    struct HeaterKw: formula::Quantity<HeaterKw, "heater_kw", "the heater's power", Kilowatt>
    {
    };
    struct HeaterH: formula::Quantity<HeaterH, "heater_h", "the heater's hours a day", unit::Hour>
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
    inline constexpr auto bill = formula::calculation(formula::define<FridgeKw>(var<FridgeW>),
                                                      formula::define<FridgeKwh>(var<FridgeKw> * var<FridgeH>),
                                                      formula::define<OvenKwh>(var<OvenKw> * var<OvenH>),
                                                      formula::define<HeaterKwh>(var<HeaterKw> * var<HeaterH>),
                                                      formula::define<DailyLoad>(var<FridgeKwh> + var<OvenKwh>
                                                                                 + var<HeaterKwh>),
                                                      formula::define<MonthlyLoad>(var<DailyLoad> * rat(30)),
                                                      formula::define<SelfUsed>(var<Solar> * rat(4, 5)),
                                                      formula::define<Exported>(var<Solar> - var<SelfUsed>),
                                                      formula::define<NetDraw>(var<MonthlyLoad> - var<SelfUsed>),
                                                      formula::define<GridCost>(var<NetDraw> * var<Price>),
                                                      formula::define<FeedInCredit>(var<Exported> * var<FeedIn>),
                                                      formula::define<EnergyCost>(var<GridCost> - var<FeedInCredit>),
                                                      formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>),
                                                      formula::define<Vat>(var<Subtotal> * rat(19, 100)),
                                                      formula::define<Total>(var<Subtotal> + var<Vat>));

    using BillGraph = formula::detail::CalculationGraphOf<std::remove_cv_t<decltype(bill)>>::type;
} // namespace household

template <std::size_t N>
using Names = std::array<std::string_view, N>;

/// A quantity of a chain, one type for each position.
template <std::size_t I>
struct LinkTag
{
};

template <std::size_t I>
using Link = formula::Quantity<LinkTag<I>, "l", "an invented link of a chain", unit::One>;

/// The chain's definition at @p I: 1 at the start, and one more than the
/// link before it after that.
template <std::size_t I>
[[nodiscard]] constexpr auto link_definition()
{
    if constexpr (I == 0)
        return formula::define<Link<0>>(formula::number(rat(1)));
    else
        return formula::define<Link<I>>(var<Link<I - 1>> + formula::number(rat(1)));
}

/// A chain of links given last first, so that the dependency order reverses
/// the order given.
template <std::size_t... Is>
[[nodiscard]] constexpr auto reversed_chain(std::index_sequence<Is...>)
{
    return formula::calculation(link_definition<sizeof...(Is) - 1 - Is>()...);
}

// 64 quantities, the most a calculation holds: 64 definitions, no input.
inline constexpr auto chain = reversed_chain(std::make_index_sequence<64>{});
using ChainGraph = formula::detail::CalculationGraphOf<std::remove_cv_t<decltype(chain)>>::type;
} // namespace

TEST_CASE("a calculation's inputs are what it reads and does not define, in the order first read", "[calculation]")
{
    using namespace household;
    STATIC_REQUIRE(BillGraph::valid);
    STATIC_REQUIRE(formula::inputs_of(bill)
                   == Names<10> { "fridge_w",
                                  "fridge_h",
                                  "oven_kw",
                                  "oven_h",
                                  "heater_kw",
                                  "heater_h",
                                  "solar",
                                  "price",
                                  "feed_in",
                                  "base_fee" });
    // Given in dependency order, the definitions keep the order given.
    STATIC_REQUIRE(formula::calculation_order(bill)
                   == Names<15> { "fridge_kw",
                                  "fridge_kwh",
                                  "oven_kwh",
                                  "heater_kwh",
                                  "daily_load",
                                  "monthly_load",
                                  "self_used",
                                  "exported",
                                  "net_draw",
                                  "grid_cost",
                                  "feed_in_credit",
                                  "energy_cost",
                                  "subtotal",
                                  "vat",
                                  "total" });
}

TEST_CASE("a calculation answers what a quantity reads and what reads it, in dependency order", "[calculation]")
{
    using namespace household;
    // The fridge's energy reads its power, then its hours; in dependency
    // order the hours, an input, come first.
    STATIC_REQUIRE(formula::dependencies_of<FridgeKwh>(bill) == Names<2> { "fridge_h", "fridge_kw" });
    STATIC_REQUIRE(formula::dependents_of<SelfUsed>(bill) == Names<2> { "exported", "net_draw" });
    // An input reads nothing; the total is read by nothing.
    STATIC_REQUIRE(formula::dependencies_of<Solar>(bill).empty());
    STATIC_REQUIRE(formula::dependents_of<Total>(bill).empty());
    STATIC_REQUIRE(formula::dependents_of<Solar>(bill) == Names<2> { "self_used", "exported" });
}

TEST_CASE("a calculation answers what depends on what through any chain of reads", "[calculation]")
{
    using namespace household;
    // The price reaches the total through the grid cost, the energy cost, the
    // subtotal and the tax; the grid cost alone reads it.
    STATIC_REQUIRE(formula::affected_by<Price>(bill)
                   == Names<5> { "grid_cost", "energy_cost", "subtotal", "vat", "total" });
    STATIC_REQUIRE(formula::upstream_of<NetDraw>(bill)
                   == Names<14> { "fridge_w",
                                  "fridge_h",
                                  "oven_kw",
                                  "oven_h",
                                  "heater_kw",
                                  "heater_h",
                                  "solar",
                                  "fridge_kw",
                                  "fridge_kwh",
                                  "oven_kwh",
                                  "heater_kwh",
                                  "daily_load",
                                  "monthly_load",
                                  "self_used" });

    static_assert(formula::depends_on<Total, Price>(bill));
    static_assert(formula::depends_on<NetDraw, FridgeW>(bill));
    static_assert(!formula::depends_on<Price, Total>(bill));
    static_assert(!formula::depends_on<NetDraw, Price>(bill));
    static_assert(!formula::depends_on<Total, Total>(bill));
}

TEST_CASE("definitions given out of order are calculated in dependency order", "[calculation]")
{
    using namespace household;
    // The tax is given before the subtotal it reads, and the total before
    // both. The export reads neither and comes first of the four; in the
    // order given, or reversed, or depth first from the tax, it would not.
    constexpr auto unordered = formula::calculation(formula::define<Vat>(var<Subtotal> * rat(19, 100)),
                                                    formula::define<Exported>(var<Solar> * rat(1, 5)),
                                                    formula::define<Total>(var<Subtotal> + var<Vat>),
                                                    formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>));
    STATIC_REQUIRE(formula::inputs_of(unordered) == Names<3> { "solar", "energy_cost", "base_fee" });
    STATIC_REQUIRE(formula::calculation_order(unordered) == Names<4> { "exported", "subtotal", "vat", "total" });
    STATIC_REQUIRE(formula::dependencies_of<Total>(unordered) == Names<2> { "subtotal", "vat" });
}

TEST_CASE("a when() definition depends on its condition and both of its branches", "[calculation]")
{
    using namespace household;
    // Each input appears in one place only: the solar yield in the condition,
    // the grid price in one branch, the feed-in tariff in the other.
    constexpr auto settled = formula::calculation(formula::define<GridCost>(
        formula::when(var<Solar> > formula::constant<KilowattHour>(rat(0)), var<NetDraw> * var<Price>,
                      var<Exported> * var<FeedIn>)));
    STATIC_REQUIRE(formula::dependencies_of<GridCost>(settled)
                   == Names<5> { "solar", "net_draw", "price", "exported", "feed_in" });
    STATIC_REQUIRE(formula::affected_by<FeedIn>(settled) == Names<1> { "grid_cost" });
}

TEST_CASE("a calculation's queries write each symbol as the vocabulary says", "[calculation][vocabulary]")
{
    using namespace household;
    constexpr auto words = formula::vocabulary(formula::renames<NetDraw>("E_grid"),
                                               formula::renames<Price>("c_grid"),
                                               formula::renames<Total>("C_bill"));
    STATIC_REQUIRE(formula::dependencies_of<GridCost>(bill) == Names<2> { "price", "net_draw" });
    STATIC_REQUIRE(formula::dependencies_of<GridCost>(bill, words) == Names<2> { "c_grid", "E_grid" });
    STATIC_REQUIRE(formula::inputs_of(bill, words)[7] == "c_grid");
    STATIC_REQUIRE(formula::calculation_order(bill, words)[8] == "E_grid");
    STATIC_REQUIRE(formula::calculation_order(bill, words)[14] == "C_bill");
    STATIC_REQUIRE(formula::dependents_of<Subtotal>(bill, words) == Names<2> { "vat", "C_bill" });
    STATIC_REQUIRE(formula::affected_by<Price>(bill, words)
                   == Names<5> { "grid_cost", "energy_cost", "subtotal", "vat", "C_bill" });
    STATIC_REQUIRE(formula::upstream_of<GridCost>(bill, words)[7] == "c_grid");
    STATIC_REQUIRE(formula::upstream_of<GridCost>(bill, words)[15] == "E_grid");
}

TEST_CASE("a chain read against the order given is followed to its end both ways", "[calculation]")
{
    // The apex is given before the high point it reads, which reads the low
    // point, which reads the start: a closure that visited the rows only in
    // the order given would miss the start from the apex.
    constexpr auto againstTheGrain = formula::calculation(formula::define<Low>(var<Start> + rat(1)),
                                                          formula::define<Apex>(var<High> + rat(1)),
                                                          formula::define<High>(var<Low> + rat(1)));
    STATIC_REQUIRE(formula::calculation_order(againstTheGrain) == Names<3> { "x_l", "x_h", "x_a" });
    STATIC_REQUIRE(formula::upstream_of<Apex>(againstTheGrain) == Names<3> { "x_0", "x_l", "x_h" });
    STATIC_REQUIRE(formula::affected_by<Start>(againstTheGrain) == Names<3> { "x_l", "x_h", "x_a" });
    STATIC_REQUIRE(formula::depends_on<Apex, Start>(againstTheGrain));
    STATIC_REQUIRE_FALSE(formula::depends_on<Start, Apex>(againstTheGrain));
}

TEST_CASE("a cycle names exactly the quantities on it", "[calculation]")
{
    // Asked of the graph itself, which states the rules without firing
    // them. The share reads the doubled share, which reads the halved one,
    // which reads the share; the other factor reads the share from off the
    // cycle, and the factor the share reads is defined from the start.
    using DefineShare = decltype(formula::define<Share>(var<Doubled> + var<Factor>));
    using DefineDoubled = decltype(formula::define<Doubled>(var<Halved> * rat(4)));
    using DefineHalved = decltype(formula::define<Halved>(var<Share> * rat(1, 2)));
    using DefineOther = decltype(formula::define<Other>(var<Share>));
    using DefineFactor = decltype(formula::define<Factor>(var<Start> + rat(1)));
    using formula::detail::DefinedCalculationGraph;

    using Circular = DefinedCalculationGraph<DefineShare, DefineDoubled, DefineHalved, DefineOther>;
    STATIC_REQUIRE_FALSE(Circular::valid);
    STATIC_REQUIRE(std::is_same_v<Circular::cycle, QuantityList<Share, Doubled, Halved>>);

    // A definition the cycle reads, given first: still the three, and only
    // they.
    using ReadByTheCycle = DefinedCalculationGraph<DefineFactor, DefineShare, DefineDoubled, DefineHalved, DefineOther>;
    STATIC_REQUIRE(std::is_same_v<ReadByTheCycle::cycle, QuantityList<Share, Doubled, Halved>>);

    // A definition that reads itself is refused as that, before any cycle is
    // judged: none is named.
    using ReadsItself = DefinedCalculationGraph<decltype(formula::define<Share>(var<Share> + var<Factor>))>;
    STATIC_REQUIRE_FALSE(ReadsItself::valid);
    STATIC_REQUIRE(std::is_same_v<ReadsItself::cycle, QuantityList<>>);

    // Without a cycle, none either.
    STATIC_REQUIRE(std::is_same_v<household::BillGraph::cycle, QuantityList<>>);
}

TEST_CASE("a calculation holds up to 64 quantities, and orders them all", "[calculation]")
{
    // The chain was given last first: the dependency order reverses it, the
    // first link first, and the last link depends on the first through all
    // 63 others.
    STATIC_REQUIRE(ChainGraph::valid);
    STATIC_REQUIRE(ChainGraph::slotCount == 64);
    STATIC_REQUIRE(ChainGraph::order[0] == ChainGraph::slot_of<Link<0>>);
    STATIC_REQUIRE(ChainGraph::order[63] == ChainGraph::slot_of<Link<63>>);
    STATIC_REQUIRE(ChainGraph::slot_of<Link<0>> == 63);
    STATIC_REQUIRE(formula::inputs_of(chain).empty());
    STATIC_REQUIRE(formula::calculation_order(chain).size() == 64);
    STATIC_REQUIRE(formula::upstream_of<Link<63>>(chain).size() == 63);
    STATIC_REQUIRE(formula::affected_by<Link<0>>(chain).size() == 63);
    STATIC_REQUIRE(formula::depends_on<Link<63>, Link<0>>(chain));
    STATIC_REQUIRE_FALSE(formula::depends_on<Link<0>, Link<63>>(chain));
}
