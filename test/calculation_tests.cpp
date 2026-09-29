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
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>

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
