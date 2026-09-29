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

#include "household_bill.hpp"

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
#include <vector>

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

namespace
{
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

// ------------------------------------------------------------ worksheets

namespace
{
namespace household
{
    /// Which value a step of a sequence asks for once it has set its inputs.
    enum class Asked
    {
        Nothing,
        NetDraw,
        FeedInCredit,
        Total,
    };

    /// One step of a sequence of changes to the bill's worksheet: every input
    /// set as `values` says, the price typed in or measured, the net draw
    /// overridden or its override cleared, and then one value asked for.
    struct BillStep
    {
        BillValues values;
        bool priceTypedIn = false;
        std::optional<formula::Rational> netDrawOverride = std::nullopt;
        Asked asked = Asked::Total;
    };

    /// Takes @p step on @p sheet, which keeps what it calculated before.
    template <typename Sheet>
    void take_step(Sheet& sheet, BillStep const& step)
    {
        BillValues const& values = step.values;
        auto const setEveryInput = [&](auto priceEntry) {
            sheet.set(given<FridgeW>(values.fridgeW),
                      given<FridgeH>(values.fridgeH),
                      given<OvenKw>(values.ovenKw),
                      given<OvenH>(values.ovenH),
                      given<HeaterKw>(values.heaterKw),
                      given<HeaterH>(values.heaterH),
                      given<Solar>(values.solar),
                      priceEntry,
                      given<FeedIn>(values.feedIn),
                      given<BaseFee>(values.baseFee));
        };
        if (step.priceTypedIn)
            setEveryInput(formula::entered(given<Price>(values.price)));
        else
            setEveryInput(given<Price>(values.price));
        if (step.netDrawOverride.has_value())
            sheet.set(formula::entered(formula::Measured<NetDraw> { *step.netDrawOverride }));
        else
            sheet.template clear_override<NetDraw>();

        if (step.asked == Asked::NetDraw)
            static_cast<void>(sheet.template checked_calculate<NetDraw>());
        else if (step.asked == Asked::FeedInCredit)
            static_cast<void>(sheet.template checked_calculate<FeedInCredit>());
        else if (step.asked == Asked::Total)
            static_cast<void>(sheet.template checked_calculate<Total>());
    }

    /// A worksheet that has calculated nothing yet, holding what @p step
    /// leaves the bill's worksheet holding.
    inline auto from_scratch(BillStep const& step)
    {
        auto fresh = formula::worksheet(bill, bill_environment(step.values));
        if (step.priceTypedIn)
            fresh.set(formula::entered(given<Price>(step.values.price)));
        if (step.netDrawOverride.has_value())
            fresh.set(formula::entered(formula::Measured<NetDraw> { *step.netDrawOverride }));
        return fresh;
    }

    template <typename Q, typename Sheet>
    void check_same_value(Sheet& incremental, Sheet& fresh)
    {
        INFO(formula::symbol_of<Q>(formula::DefaultVocabulary {}));
        CHECK(incremental.template checked_calculate<Q>() == fresh.template checked_calculate<Q>());
    }

    /// Checks that @p incremental and @p fresh hold the same answer for every
    /// one of `Qs`.
    template <typename Sheet, typename... Qs>
    void check_same_values(Sheet& incremental, Sheet& fresh, QuantityList<Qs...> const*)
    {
        (check_same_value<Qs>(incremental, fresh), ...);
    }
} // namespace household

/// Two inputs and four definitions, small enough to run at compile time: the
/// start squared, one more than that, that times the factor, and the factor
/// plus one.
inline constexpr auto squared = formula::calculation(formula::define<Low>(var<Start> * var<Start>),
                                                     formula::define<High>(var<Low> + rat(1)),
                                                     formula::define<Apex>(var<High> * var<Factor>),
                                                     formula::define<Other>(var<Factor> + rat(1)));

/// What `run_squared` found.
struct SquaredRun
{
    formula::Measured<Apex> first;
    formula::Measured<Apex> second;
    std::size_t recomputed = 0;
    std::size_t reused = 0;

    constexpr bool operator==(SquaredRun const&) const = default;
};

/// Asks `squared`'s worksheet for the apex, sets both inputs, and asks again.
constexpr SquaredRun run_squared()
{
    auto sheet = formula::worksheet(
        squared, formula::environment(formula::Measured<Start> { rat(1) }, formula::Measured<Factor> { rat(5) }));
    formula::Measured<Apex> const first = sheet.checked_calculate<Apex>()->measurement();
    sheet.set(formula::Measured<Start> { rat(-1) }, formula::Measured<Factor> { rat(6) });
    formula::Measured<Apex> const second = sheet.checked_calculate<Apex>()->measurement();
    return SquaredRun { first, second, sheet.recomputed(), sheet.reused() };
}

/// A share of two factors, which fails when the other factor is zero; a
/// value that reads the share only where the other factor is not zero, and
/// the factor where it is; and a value that reads the share whatever happens.
inline constexpr auto guarded =
    formula::calculation(formula::define<Share>(var<Factor> / var<Other>),
                         formula::define<Halved>(
                             formula::when(var<Other> > formula::constant<unit::One>(rat(0)), var<Share>, var<Factor>)),
                         formula::define<Doubled>(var<Share> * rat(2)));

/// `guarded`'s worksheet over @p factor and @p other.
inline auto guarded_sheet(formula::Rational factor, formula::Rational other)
{
    return formula::worksheet(
        guarded, formula::environment(formula::Measured<Factor> { factor }, formula::Measured<Other> { other }));
}
} // namespace

TEST_CASE("a worksheet calculates each value once, and recalculates only what a change reaches",
          "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    // Nothing is calculated until something is asked.
    CHECK(sheet.recomputed() == 0);

    formula::Outcome<Total> const total = sheet.calculate<Total>();
    CHECK(total.measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(total.source() == formula::ValueSource::Derived);
    CHECK(sheet.recomputed() == 15);
    CHECK(sheet.reused() == 0);
    // Asked again, or asked for a value calculated on the way: nothing more.
    CHECK(sheet.calculate<Total>() == total);
    CHECK(sheet.calculate<NetDraw>().measurement() == formula::Measured<NetDraw> { rat(279) });
    CHECK(sheet.recomputed() == 15);

    // A new price reaches the grid cost, the energy cost, the subtotal, the
    // tax and the total -- calculated when asked, not when set.
    sheet.set(formula::Measured<Price> { rat(1, 4) });
    CHECK(sheet.recomputed() == 15);
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(950215, 10000) });
    CHECK(sheet.recomputed() == 20);
    CHECK(sheet.reused() == 0);

    // The same price again changes nothing.
    sheet.set(formula::Measured<Price> { rat(1, 4) });
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(950215, 10000) });
    CHECK(sheet.recomputed() == 20);

    // A new base fee reaches the subtotal, the tax and the total.
    sheet.set(formula::Measured<BaseFee> { rat(15) });
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(979965, 10000) });
    CHECK(sheet.recomputed() == 23);
    CHECK(sheet.reused() == 0);

    // Twice the fridge's power for half the hours: its power in kilowatts
    // and its energy a day are calculated again, and the energy comes out
    // the same, so the eight values after it are reused as they stand.
    sheet.set(formula::Measured<FridgeW> { rat(400) }, formula::Measured<FridgeH> { rat(12) });
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(979965, 10000) });
    CHECK(sheet.recomputed() == 25);
    CHECK(sheet.reused() == 8);

    // A copy with more sun: the nine values the solar yield reaches are
    // calculated on the copy, which keeps the counters it started from.
    auto sunnier = sheet.with(formula::Measured<Solar> { rat(200) });
    CHECK(sunnier.calculate<Total>().measurement() == formula::Measured<Total> { rat(851445, 10000) });
    CHECK(sunnier.calculate<NetDraw>().measurement() == formula::Measured<NetDraw> { rat(239) });
    CHECK(sunnier.recomputed() == 34);
    CHECK(sunnier.reused() == 8);
    // The original is unchanged, and calculates nothing.
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(979965, 10000) });
    CHECK(sheet.calculate<NetDraw>().measurement() == formula::Measured<NetDraw> { rat(279) });
    CHECK(sheet.recomputed() == 25);
    CHECK(sheet.reused() == 8);
}

TEST_CASE("set chains on a worksheet about to be discarded", "[calculation][worksheet]")
{
    using namespace household;
    formula::Outcome<Total> const cheaper = formula::worksheet(bill, bill_environment(billValues))
                                                .set(formula::Measured<Price> { rat(1, 4) })
                                                .calculate<Total>();
    CHECK(cheaper.measurement() == formula::Measured<Total> { rat(950215, 10000) });

    auto kept = formula::worksheet(bill, bill_environment(billValues))
                    .set(formula::Measured<Price> { rat(1, 4) })
                    .set(formula::Measured<BaseFee> { rat(15) });
    CHECK(kept.calculate<Total>().measurement() == formula::Measured<Total> { rat(979965, 10000) });
    CHECK(kept.recomputed() == 15);
}

TEST_CASE("clear_override chains on a worksheet about to be discarded", "[calculation][worksheet]")
{
    using namespace household;
    formula::Outcome<NetDraw> const calculatedAgain =
        formula::worksheet(bill, bill_environment(billValues))
            .set(formula::entered(formula::Measured<NetDraw> { rat(250) }))
            .clear_override<NetDraw>()
            .calculate<NetDraw>();
    CHECK(calculatedAgain.measurement() == formula::Measured<NetDraw> { rat(279) });
    CHECK(calculatedAgain.source() == formula::ValueSource::Derived);

    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    REQUIRE(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    REQUIRE(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
    REQUIRE(sheet.recomputed() == 20);
    STATIC_REQUIRE(std::is_same_v<decltype(std::move(sheet).clear_override<NetDraw>()), decltype(sheet)>);
    auto kept = std::move(sheet).clear_override<NetDraw>();
    CHECK_FALSE(kept.is_overridden<NetDraw>());
    // The net draw is calculated again, and the five values built on it.
    CHECK(kept.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(kept.recomputed() == 26);
}

TEST_CASE("a worksheet answers several values at once, in the order asked", "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));

    auto const [total, netDraw] = sheet.calculate<Total, NetDraw>();
    STATIC_REQUIRE(std::is_same_v<decltype(sheet.calculate<Total, NetDraw>()),
                                  std::tuple<formula::Outcome<Total>, formula::Outcome<NetDraw>>>);
    CHECK(total.measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(netDraw.measurement() == formula::Measured<NetDraw> { rat(279) });

    auto const checked = sheet.checked_calculate<NetDraw, Price, Total>();
    STATIC_REQUIRE(std::is_same_v<decltype(checked),
                                  std::tuple<std::expected<formula::Outcome<NetDraw>, formula::ArithmeticError>,
                                             std::expected<formula::Outcome<Price>, formula::ArithmeticError>,
                                             std::expected<formula::Outcome<Total>, formula::ArithmeticError>> const>);
    CHECK(std::get<0>(checked)->measurement() == formula::Measured<NetDraw> { rat(279) });
    CHECK(std::get<1>(checked)->measurement() == formula::Measured<Price> { rat(8, 25) });
    CHECK(std::get<2>(checked) == total);
    CHECK(sheet.recomputed() == 15);
}

TEST_CASE("a worksheet answers the quantities named by their variables", "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));

    auto const [total, netDraw] = sheet.calculate(var<Total>, var<NetDraw>);
    CHECK(total.measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(netDraw.measurement() == formula::Measured<NetDraw> { rat(279) });

    auto const one = sheet.calculate(var<Exported>);
    STATIC_REQUIRE(std::is_same_v<decltype(one), formula::Outcome<Exported> const>);
    CHECK(one.measurement() == formula::Measured<Exported> { rat(30) });

    auto const checkedOne = sheet.checked_calculate(var<Vat>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(checkedOne), std::expected<formula::Outcome<Vat>, formula::ArithmeticError> const>);
    CHECK(checkedOne->measurement() == formula::Measured<Vat> { rat(94411, 5000) });

    auto const [checkedTotal, checkedSolar] = sheet.checked_calculate(var<Total>, var<Solar>);
    CHECK(checkedTotal == total);
    CHECK(checkedSolar->measurement() == formula::Measured<Solar> { rat(150) });
}

TEST_CASE("a worksheet reads an input back as it was given, measured or typed in", "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    formula::Outcome<Price> const measured = sheet.calculate<Price>();
    CHECK(measured.measurement() == formula::Measured<Price> { rat(8, 25) });
    CHECK(measured.source() == formula::ValueSource::Measured);
    CHECK(sheet.checked_calculate<Price>()->source() == formula::ValueSource::Measured);
    CHECK(sheet.recomputed() == 0);
    REQUIRE(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });

    // The same number, typed in: a change of source, so the grid cost is
    // calculated again -- to the same answer, so what reads it is reused.
    sheet.set(formula::entered(formula::Measured<Price> { rat(8, 25) }));
    formula::Outcome<Price> const typedIn = sheet.calculate<Price>();
    CHECK(typedIn.measurement() == formula::Measured<Price> { rat(8, 25) });
    CHECK(typedIn.source() == formula::ValueSource::ManuallyEntered);
    CHECK(sheet.checked_calculate<Price>()->source() == formula::ValueSource::ManuallyEntered);
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(sheet.recomputed() == 16);
    CHECK(sheet.reused() == 4);

    // Measured again.
    sheet.set(formula::Measured<Price> { rat(8, 25) });
    CHECK(sheet.calculate<Price>().source() == formula::ValueSource::Measured);
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(sheet.recomputed() == 17);
    CHECK(sheet.reused() == 8);

    // Typed in in the environment the worksheet was made from.
    auto typedFirst = formula::worksheet(
        guarded,
        formula::environment(formula::entered(formula::Measured<Factor> { rat(3) }), formula::Measured<Other> { rat(2) }));
    CHECK(typedFirst.calculate<Factor>().source() == formula::ValueSource::ManuallyEntered);
    CHECK(typedFirst.calculate<Other>().source() == formula::ValueSource::Measured);
    CHECK(typedFirst.calculate<Share>().measurement() == formula::Measured<Share> { rat(3, 2) });
}

TEST_CASE("a worksheet calculates definitions given out of order in dependency order", "[calculation][worksheet]")
{
    // The apex is given before the high point it reads, which reads the low
    // point, which reads the start.
    constexpr auto againstTheGrain = formula::calculation(formula::define<Low>(var<Start> + rat(1)),
                                                          formula::define<Apex>(var<High> + rat(1)),
                                                          formula::define<High>(var<Low> + rat(1)));
    auto sheet = formula::worksheet(againstTheGrain, formula::environment(formula::Measured<Start> { rat(1) }));
    CHECK(sheet.calculate<Apex>().measurement() == formula::Measured<Apex> { rat(4) });
    CHECK(sheet.recomputed() == 3);
    sheet.set(formula::Measured<Start> { rat(10) });
    CHECK(sheet.calculate<Apex>().measurement() == formula::Measured<Apex> { rat(13) });
    CHECK(sheet.calculate<High>().measurement() == formula::Measured<High> { rat(12) });
    CHECK(sheet.recomputed() == 6);
}

TEST_CASE("an override stands in for a calculated value until it is cleared", "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    REQUIRE(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK_FALSE(sheet.is_overridden<NetDraw>());

    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    CHECK(sheet.is_overridden<NetDraw>());
    formula::Outcome<NetDraw> const overridden = sheet.calculate<NetDraw>();
    CHECK(overridden.measurement() == formula::Measured<NetDraw> { rat(250) });
    CHECK(overridden.source() == formula::ValueSource::ManuallyEntered);
    CHECK(overridden.is_overridden());
    // The grid cost, the energy cost, the subtotal, the tax and the total.
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
    CHECK(sheet.recomputed() == 20);

    // The same override again changes nothing.
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
    CHECK(sheet.recomputed() == 20);

    // A change upstream of the override does not get past it: the values
    // it reaches beyond the override read nothing changed, and are reused.
    sheet.set(formula::Measured<FridgeW> { rat(300) });
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
    CHECK(sheet.recomputed() == 20);
    CHECK(sheet.reused() == 5);

    // Cleared, the net draw is calculated again -- from the fridge's new
    // power, which reaches it now -- and so is everything after it.
    sheet.clear_override<NetDraw>();
    CHECK_FALSE(sheet.is_overridden<NetDraw>());
    formula::Outcome<NetDraw> const calculated = sheet.calculate<NetDraw>();
    CHECK(calculated.measurement() == formula::Measured<NetDraw> { rat(351) });
    CHECK(calculated.source() == formula::ValueSource::Derived);
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(1456798, 10000) });
    CHECK(sheet.recomputed() == 30);
    CHECK(sheet.reused() == 5);

    // Clearing an override that is not there changes nothing.
    sheet.clear_override<NetDraw>();
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(1456798, 10000) });
    CHECK(sheet.recomputed() == 30);
    CHECK(sheet.reused() == 5);

    // Given, cleared and given again before anything is asked, the override
    // stands again.
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    sheet.clear_override<NetDraw>();
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    CHECK(sheet.is_overridden<NetDraw>());
    CHECK(sheet.calculate<NetDraw>().source() == formula::ValueSource::ManuallyEntered);
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
}

TEST_CASE("an override can be given in a worksheet's environment", "[calculation][worksheet]")
{
    using namespace household;
    auto sheet = formula::worksheet(
        bill, bill_environment(billValues, formula::entered(formula::Measured<NetDraw> { rat(250) })));
    CHECK(sheet.is_overridden<NetDraw>());
    CHECK(sheet.calculate<NetDraw>().source() == formula::ValueSource::ManuallyEntered);
    // What the total needs past the override: the solar energy used at home
    // and exported, the grid cost, the credit, the energy cost, the subtotal,
    // the tax and the total. The loads the override stands in front of are
    // not calculated.
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(107219, 1000) });
    CHECK(sheet.recomputed() == 8);
    CHECK(sheet.calculate<MonthlyLoad>().measurement() == formula::Measured<MonthlyLoad> { rat(399) });
    CHECK(sheet.recomputed() == 14);
}

TEST_CASE("a failed calculation fails what reads it, and calculate throws it", "[calculation][worksheet]")
{
    auto sheet = guarded_sheet(rat(3), rat(0));
    std::expected<formula::Outcome<Share>, formula::ArithmeticError> const share = sheet.checked_calculate<Share>();
    REQUIRE_FALSE(share.has_value());
    CHECK(share.error() == formula::ArithmeticError::DivisionByZero);
    std::expected<formula::Outcome<Doubled>, formula::ArithmeticError> const doubled =
        sheet.checked_calculate<Doubled>();
    REQUIRE_FALSE(doubled.has_value());
    CHECK(doubled.error() == formula::ArithmeticError::DivisionByZero);

    CHECK_THROWS_AS(sheet.calculate<Doubled>(), formula::ArithmeticException);
    CHECK_THROWS_AS((sheet.calculate<Factor, Doubled>()), formula::ArithmeticException);
    CHECK_THROWS_AS(sheet.calculate(var<Doubled>), formula::ArithmeticException);
    // A failure is kept like any answer: asking again calculates nothing.
    CHECK(sheet.recomputed() == 2);

    // Once the other factor is set, the failure is gone.
    sheet.set(formula::Measured<Other> { rat(2) });
    CHECK(sheet.calculate<Doubled>().measurement() == formula::Measured<Doubled> { rat(3) });
}

TEST_CASE("a value that fails again with the same error counts as unchanged", "[calculation][worksheet]")
{
    auto sheet = guarded_sheet(rat(3), rat(0));
    REQUIRE_FALSE(sheet.checked_calculate<Doubled>().has_value());
    REQUIRE(sheet.calculate<Halved>().measurement() == formula::Measured<Halved> { rat(3) });
    CHECK(sheet.recomputed() == 3);
    CHECK(sheet.reused() == 0);

    // A new factor reaches the share, the doubled share and the halved value.
    // The share fails again, with the same error, so the doubled share, which
    // reads only the share, is reused; the halved value reads the factor too,
    // and is calculated again.
    sheet.set(formula::Measured<Factor> { rat(4) });
    std::expected<formula::Outcome<Share>, formula::ArithmeticError> const share = sheet.checked_calculate<Share>();
    REQUIRE_FALSE(share.has_value());
    CHECK(share.error() == formula::ArithmeticError::DivisionByZero);
    std::expected<formula::Outcome<Doubled>, formula::ArithmeticError> const doubled =
        sheet.checked_calculate<Doubled>();
    REQUIRE_FALSE(doubled.has_value());
    CHECK(doubled.error() == formula::ArithmeticError::DivisionByZero);
    CHECK(sheet.calculate<Halved>().measurement() == formula::Measured<Halved> { rat(4) });
    CHECK(sheet.recomputed() == 5);
    CHECK(sheet.reused() == 1);
}

TEST_CASE("a when() branch not taken never reads a failed value", "[calculation][worksheet]")
{
    // The other factor is zero: the share fails, and the halved value takes
    // the branch that reads the factor instead.
    auto sheet = guarded_sheet(rat(3), rat(0));
    std::expected<formula::Outcome<Halved>, formula::ArithmeticError> const halved = sheet.checked_calculate<Halved>();
    REQUIRE(halved.has_value());
    CHECK(halved->measurement() == formula::Measured<Halved> { rat(3) });
    CHECK_FALSE(sheet.checked_calculate<Share>().has_value());

    // Not zero: the branch reading the share is taken.
    sheet.set(formula::Measured<Other> { rat(2) });
    CHECK(sheet.calculate<Halved>().measurement() == formula::Measured<Halved> { rat(3, 2) });
}

TEST_CASE("an absent input leaves what reads it empty", "[calculation][worksheet]")
{
    using namespace household;
    BillValues dark = billValues;
    dark.solar = std::nullopt;
    auto sheet = formula::worksheet(bill, bill_environment(dark));
    CHECK(sheet.calculate<Solar>().is_empty());
    CHECK(sheet.calculate<SelfUsed>().is_empty());
    CHECK(sheet.calculate<Total>().is_empty());
    // What does not read it is calculated.
    CHECK(sheet.calculate<DailyLoad>().measurement() == formula::Measured<DailyLoad> { rat(133, 10) });

    // Once the yield is given, everything is.
    sheet.set(formula::Measured<Solar> { rat(150) });
    CHECK(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });
}

TEST_CASE("a worksheet answers what its formulas inlined into one another answer", "[calculation][worksheet]")
{
    using namespace household;
    // The bill's definitions inlined by hand, each into what reads it.
    constexpr auto fridgeKwh = var<FridgeW> * var<FridgeH>;
    constexpr auto dailyLoad = fridgeKwh + var<OvenKw> * var<OvenH> + var<HeaterKw> * var<HeaterH>;
    constexpr auto selfUsed = var<Solar> * rat(4, 5);
    constexpr auto netDraw = dailyLoad * rat(30) - selfUsed;
    constexpr auto feedInCredit = (var<Solar> - selfUsed) * var<FeedIn>;
    constexpr auto energyCost = netDraw * var<Price> - feedInCredit;
    constexpr auto subtotal = energyCost + var<BaseFee>;
    constexpr auto total = subtotal + subtotal * rat(19, 100);

    BillValues cheaper = billValues;
    cheaper.fridgeW = rat(400);
    cheaper.fridgeH = rat(12);
    cheaper.price = rat(1, 4);
    cheaper.baseFee = rat(15);
    BillValues exporting = billValues;
    exporting.solar = rat(1000);
    exporting.feedIn = rat(0);
    BillValues sunless = billValues;
    sunless.solar = rat(0);
    sunless.price = rat(0);
    BillValues const uneven { rat(1500), rat(20),     rat(9, 4),  rat(7, 3),  rat(3, 2),
                              rat(11, 2), rat(333, 7), rat(3, 10), rat(1, 20), rat(0) };
    BillValues dark = billValues;
    dark.solar = std::nullopt;
    BillValues unpriced = billValues;
    unpriced.price = std::nullopt;

    for (BillValues const& values : { billValues, cheaper, exporting, sunless, uneven, dark, unpriced })
    {
        auto const inputs = bill_environment(values);
        auto sheet = formula::worksheet(bill, inputs);
        CHECK(sheet.checked_calculate<FridgeKwh>() == formula::checked_evaluate<FridgeKwh>(fridgeKwh, inputs));
        CHECK(sheet.checked_calculate<DailyLoad>() == formula::checked_evaluate<DailyLoad>(dailyLoad, inputs));
        CHECK(sheet.checked_calculate<SelfUsed>() == formula::checked_evaluate<SelfUsed>(selfUsed, inputs));
        CHECK(sheet.checked_calculate<NetDraw>() == formula::checked_evaluate<NetDraw>(netDraw, inputs));
        CHECK(sheet.checked_calculate<FeedInCredit>()
              == formula::checked_evaluate<FeedInCredit>(feedInCredit, inputs));
        CHECK(sheet.checked_calculate<EnergyCost>() == formula::checked_evaluate<EnergyCost>(energyCost, inputs));
        CHECK(sheet.checked_calculate<Subtotal>() == formula::checked_evaluate<Subtotal>(subtotal, inputs));
        CHECK(sheet.checked_calculate<Total>() == formula::checked_evaluate<Total>(total, inputs));
    }
}

TEST_CASE("a worksheet changed step by step answers what one made from scratch answers", "[calculation][worksheet]")
{
    using namespace household;
    BillValues cheaper = billValues;
    cheaper.price = rat(1, 4);
    BillValues fridgeSwapped = billValues;
    fridgeSwapped.fridgeW = rat(400);
    fridgeSwapped.fridgeH = rat(12);
    BillValues sunnier = billValues;
    sunnier.solar = rat(200);
    BillValues dark = billValues;
    dark.solar = std::nullopt;
    BillValues const renewed { rat(100), rat(20), rat(2),     rat(3, 2),  rat(1),
                               rat(5),   rat(90), rat(3, 10), rat(1, 10), rat(10) };
    BillValues sunless = billValues;
    sunless.solar = rat(0);
    sunless.price = rat(0);

    std::vector<std::vector<BillStep>> const sequences {
        // Inputs only, asked for different values.
        { { .values = billValues },
          { .values = cheaper, .asked = Asked::NetDraw },
          { .values = cheaper },
          { .values = billValues, .asked = Asked::Nothing },
          { .values = fridgeSwapped },
          { .values = sunnier, .asked = Asked::FeedInCredit },
          { .values = dark },
          { .values = billValues } },
        // An override, changed around, then cleared.
        { { .values = billValues, .netDrawOverride = rat(250) },
          { .values = cheaper, .netDrawOverride = rat(250), .asked = Asked::NetDraw },
          { .values = cheaper },
          { .values = fridgeSwapped, .netDrawOverride = rat(0), .asked = Asked::FeedInCredit },
          { .values = renewed } },
        // The price typed in, and measured again.
        { { .values = billValues, .asked = Asked::Nothing },
          { .values = billValues, .priceTypedIn = true },
          { .values = billValues, .priceTypedIn = true },
          { .values = billValues },
          { .values = renewed, .priceTypedIn = true, .asked = Asked::NetDraw },
          { .values = sunless } },
        // An absent input, overridden past and then given.
        { { .values = dark },
          { .values = dark, .netDrawOverride = rat(100) },
          { .values = billValues, .netDrawOverride = rat(100), .asked = Asked::Nothing },
          { .values = billValues, .asked = Asked::Nothing },
          { .values = sunnier } },
    };

    std::size_t reusedAcrossSequences = 0;
    for (std::vector<BillStep> const& sequence : sequences)
    {
        auto sheet = formula::worksheet(bill, bill_environment(billValues));
        for (BillStep const& step : sequence)
        {
            take_step(sheet, step);
            // Compared on a copy, so that the worksheet goes on holding only
            // what its steps asked for.
            auto incremental = sheet;
            auto fresh = from_scratch(step);
            check_same_values(incremental, fresh, static_cast<BillGraph::slots const*>(nullptr));
        }
        reusedAcrossSequences += sheet.reused();
    }
    // The steps did reuse values: this compares an incremental calculation
    // with one from scratch, not two from scratch.
    CHECK(reusedAcrossSequences > 0);
}

TEST_CASE("a worksheet sets and recalculates at compile time", "[calculation][worksheet]")
{
    // The apex is 1 squared, plus 1, times 5. Then the start becomes -1,
    // whose square is the same, and the factor 6: the square and the apex are
    // calculated again, and the value between them is reused.
    STATIC_REQUIRE(run_squared()
                   == SquaredRun { formula::Measured<Apex> { rat(10) }, formula::Measured<Apex> { rat(12) }, 5, 1 });
}

TEST_CASE("a definition is evaluated against the values it reads, and the reads it makes are recorded",
          "[calculation][worksheet]")
{
    auto sheet = guarded_sheet(rat(3), rat(0));
    REQUIRE(sheet.checked_calculate<Halved>().has_value());

    using Sheet = decltype(sheet);
    using Graph = formula::detail::WorksheetGraphOf<Sheet>::type;
    using HalvedView = formula::detail::WorksheetView<Sheet, Graph::reads[Graph::slot_of<Halved>]>;
    STATIC_REQUIRE(HalvedView::provides<Share>);
    STATIC_REQUIRE(HalvedView::provides<Other>);
    STATIC_REQUIRE(HalvedView::provides<Factor>);
    STATIC_REQUIRE_FALSE(HalvedView::provides<Halved>);
    STATIC_REQUIRE_FALSE(HalvedView::provides<Doubled>);
    STATIC_REQUIRE_FALSE(HalvedView::provides<Width>);
    STATIC_REQUIRE_FALSE(HalvedView::is_entered<Share>);
    STATIC_REQUIRE(formula::detail::ReportsReadFailure<HalvedView, Share>);
    STATIC_REQUIRE(formula::detail::RunTimeSource<HalvedView, Share>);

    // Only the branch taken is read: the other factor, then the factor.
    constexpr std::uint64_t factorBit = std::uint64_t { 1 } << Graph::slot_of<Factor>;
    constexpr std::uint64_t otherBit = std::uint64_t { 1 } << Graph::slot_of<Other>;
    constexpr std::uint64_t shareBit = std::uint64_t { 1 } << Graph::slot_of<Share>;
    auto const& halvedDefinition = std::get<1>(guarded.definitions).expression;
    std::uint64_t made = 0;
    CHECK(formula::checked_evaluate<Halved>(halvedDefinition, HalvedView { &sheet, &made })
          == sheet.checked_calculate<Halved>());
    CHECK(made == (otherBit | factorBit));

    sheet.set(formula::Measured<Other> { rat(2) });
    REQUIRE(sheet.checked_calculate<Halved>().has_value());
    made = 0;
    CHECK(formula::checked_evaluate<Halved>(halvedDefinition, HalvedView { &sheet, &made })
          == sheet.checked_calculate<Halved>());
    CHECK(made == (otherBit | shareBit));

    // A failed calculation reads as its failure, and, through get, as absent.
    sheet.set(formula::Measured<Other> { rat(0) });
    REQUIRE_FALSE(sheet.checked_calculate<Doubled>().has_value());
    HalvedView const view { &sheet, nullptr };
    CHECK(view.checked_get<Share>() == std::unexpected { formula::ArithmeticError::DivisionByZero });
    CHECK(view.get<Share>().is_absent());
    CHECK(view.get<Factor>() == formula::Measured<Factor> { rat(3) });

    // Where each value came from, as the worksheet holds it.
    CHECK(view.source_of<Factor>() == formula::ValueSource::Measured);
    CHECK(view.source_of<Share>() == formula::ValueSource::Derived);
    sheet.set(formula::entered(formula::Measured<Factor> { rat(3) }),
              formula::entered(formula::Measured<Share> { rat(7) }));
    CHECK(view.source_of<Factor>() == formula::ValueSource::ManuallyEntered);
    CHECK(view.source_of<Share>() == formula::ValueSource::ManuallyEntered);
    CHECK(view.get<Share>() == formula::Measured<Share> { rat(7) });
}

TEST_CASE("a worksheet answers the questions its calculation answers", "[calculation][worksheet]")
{
    using namespace household;
    constexpr auto sheet = formula::worksheet(bill, bill_environment(billValues));
    STATIC_REQUIRE(formula::dependencies_of<FridgeKwh>(sheet) == Names<2> { "fridge_h", "fridge_kw" });
    STATIC_REQUIRE(formula::dependents_of<SelfUsed>(sheet) == Names<2> { "exported", "net_draw" });
    STATIC_REQUIRE(formula::upstream_of<Exported>(sheet) == Names<2> { "solar", "self_used" });
    STATIC_REQUIRE(formula::affected_by<BaseFee>(sheet) == Names<3> { "subtotal", "vat", "total" });
    STATIC_REQUIRE(formula::inputs_of(sheet) == formula::inputs_of(bill));
    STATIC_REQUIRE(formula::calculation_order(sheet) == formula::calculation_order(bill));
    STATIC_REQUIRE(formula::depends_on<Total, FridgeW>(sheet));
    STATIC_REQUIRE_FALSE(formula::depends_on<FeedInCredit, Price>(sheet));

    constexpr auto words =
        formula::vocabulary(formula::renames<NetDraw>("E_grid"), formula::renames<Price>("c_grid"));
    STATIC_REQUIRE(formula::dependents_of<MonthlyLoad>(sheet, words) == Names<1> { "E_grid" });
    STATIC_REQUIRE(formula::upstream_of<GridCost>(sheet, words)[7] == "c_grid");
    STATIC_REQUIRE(formula::inputs_of(sheet, words)[7] == "c_grid");
    STATIC_REQUIRE(formula::calculation_order(sheet, words)[8] == "E_grid");
}

TEST_CASE("a worksheet holds as many quantities as a calculation does", "[calculation][worksheet]")
{
    // The chain of 64 links reads no input: its first link is 1, and each
    // after it one more.
    auto sheet = formula::worksheet(chain, formula::environment());
    CHECK(sheet.calculate<Link<63>>().measurement() == formula::Measured<Link<63>> { rat(64) });
    CHECK(sheet.recomputed() == 64);

    // The first link, given last and so in the last slot, overridden: the
    // 63 links after it are calculated again. Cleared: all 64 are.
    sheet.set(formula::entered(formula::Measured<Link<0>> { rat(11) }));
    CHECK(sheet.calculate<Link<63>>().measurement() == formula::Measured<Link<63>> { rat(74) });
    CHECK(sheet.recomputed() == 127);
    sheet.clear_override<Link<0>>();
    CHECK(sheet.calculate<Link<63>>().measurement() == formula::Measured<Link<63>> { rat(64) });
    CHECK(sheet.recomputed() == 191);

    // The middle link overridden: only the 32 links after it.
    sheet.set(formula::entered(formula::Measured<Link<31>> { rat(0) }));
    CHECK(sheet.calculate<Link<63>>().measurement() == formula::Measured<Link<63>> { rat(32) });
    CHECK(sheet.calculate<Link<0>>().measurement() == formula::Measured<Link<0>> { rat(1) });
    CHECK(sheet.recomputed() == 223);
}
