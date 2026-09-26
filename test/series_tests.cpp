// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/series.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace
{

// The shared fixture's quantities (see the phase 12 plan): an invented screen
// analysis, retained masses in grams.
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};
// A second series of the same dimension, so that an expression reading it can
// be told apart from a value entered for Retained.
struct Sieved: formula::Quantity<Sieved, "m_s", "mass passing a screen", formula::unit::Gram>
{
};
// Stated in tonnes, so that reading an element into kilograms multiplies and
// can overflow.
struct Stockpile: formula::Quantity<Stockpile, "m_p", "stockpile mass", formula::unit::Tonne>
{
};
struct Aperture: formula::Quantity<Aperture, "d", "screen aperture", formula::unit::Millimetre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

template <typename Q>
constexpr formula::Measured<Q> m(std::int64_t grams)
{
    return formula::Measured<Q> { rat(grams) };
}

// The shared fixture's retained masses, 130, 210, 95, 340 and 28 g, with the
// 95 g screen left unmeasured: every element differs from every other.
constexpr auto inputs = formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { rat(130) },
                                                                                formula::Measured<Retained> { rat(210) },
                                                                                formula::Measured<Retained>::absent(),
                                                                                formula::Measured<Retained> { rat(340) },
                                                                                formula::Measured<Retained> { rat(28) }));

} // namespace

TEST_CASE("a series reads each element from the environment, in order, in coherent SI", "[series]")
{
    constexpr auto si =
        formula::detail::dispatch_series<formula::Rational>(formula::series<Retained, 5>, inputs, formula::NullSink {});
    STATIC_REQUIRE(si.has_value());
    // Grams arrive as kilograms: 130 g is 13/100 kg. Every element differs,
    // so a reversed or rotated read fails; the absent one stays absent, not zero.
    STATIC_REQUIRE(si->elements[0] == formula::Rational { 13, 100 });
    STATIC_REQUIRE(si->elements[1] == formula::Rational { 21, 100 });
    STATIC_REQUIRE(!si->elements[2].has_value());
    STATIC_REQUIRE(si->elements[3] == formula::Rational { 17, 50 });
    STATIC_REQUIRE(si->elements[4] == formula::Rational { 7, 250 });

    constexpr auto outcome = formula::checked_evaluate_series<Retained>(formula::series<Retained, 5>, inputs);
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->element(0).value() == rat(130)); // back in the declared unit, not 13/100
    STATIC_REQUIRE(outcome->element(3).value() == rat(340));
    STATIC_REQUIRE(outcome->element(2).is_absent()); // absent, not 0 g
    STATIC_REQUIRE(outcome->element(4).value() == rat(28));
    STATIC_REQUIRE(outcome->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(!outcome->is_overridden());
    STATIC_REQUIRE(decltype(outcome)::value_type::size() == 5);
}

TEST_CASE("a series typed in by a person is returned as entered, not as derived", "[series]")
{
    // §16.1: any computed result may be overridden by direct entry, and the
    // result must say so. The expression reads Sieved, whose elements differ
    // from the entered ones at every position, so a result computed from it
    // anyway shows in the numbers and not only in the source.
    constexpr auto overridden =
        formula::environment(formula::entered(formula::measured_series<Retained>(
                                 m<Retained>(131), formula::Measured<Retained>::absent(), m<Retained>(96))),
                             formula::measured_series<Sieved>(m<Sieved>(500), m<Sieved>(600), m<Sieved>(700)));

    STATIC_REQUIRE(decltype(overridden)::is_entered_series<Retained>);
    STATIC_REQUIRE(decltype(overridden)::is_entered<Retained>);
    STATIC_REQUIRE_FALSE(decltype(overridden)::is_entered_series<Sieved>);

    constexpr auto outcome = formula::checked_evaluate_series<Retained>(formula::series<Sieved, 3>, overridden);
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->source() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(outcome->is_overridden());
    // As the person typed them, in the declared unit: not 131/1000 kg, and
    // not the 500, 600, 700 g the expression would have produced.
    STATIC_REQUIRE(outcome->element(0).value() == rat(131));
    STATIC_REQUIRE(outcome->element(1).is_absent()); // an element left blank stays blank
    STATIC_REQUIRE(outcome->element(2).value() == rat(96));

    // Without the entry, the same expression is derived from Sieved.
    constexpr auto measuredOnly =
        formula::environment(formula::measured_series<Sieved>(m<Sieved>(500), m<Sieved>(600), m<Sieved>(700)));
    constexpr auto derived = formula::checked_evaluate_series<Retained>(formula::series<Sieved, 3>, measuredOnly);
    STATIC_REQUIRE(derived->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(derived->element(0).value() == rat(500));
    STATIC_REQUIRE(derived->element(2).value() == rat(700));
}

TEST_CASE("a series entry answers the environment's questions about its quantity", "[series]")
{
    // The environment does hold Retained: a series is a value for it, so
    // `provides` must not say otherwise.
    STATIC_REQUIRE(decltype(inputs)::provides<Retained>);
    STATIC_REQUIRE_FALSE(decltype(inputs)::provides<TotalMass>);
    STATIC_REQUIRE_FALSE(decltype(inputs)::is_entered<Retained>);
    STATIC_REQUIRE_FALSE(decltype(inputs)::is_entered_series<Retained>);
    STATIC_REQUIRE(inputs.source_of<Retained>() == formula::ValueSource::Measured);

    constexpr auto typedIn =
        formula::environment(formula::entered(formula::measured_series<Retained>(m<Retained>(1), m<Retained>(2))));
    STATIC_REQUIRE(typedIn.source_of<Retained>() == formula::ValueSource::ManuallyEntered);

    // A single entered value is entered, but it is not an entered series.
    constexpr auto single = formula::environment(formula::entered(m<Retained>(1)));
    STATIC_REQUIRE(decltype(single)::is_entered<Retained>);
    STATIC_REQUIRE_FALSE(decltype(single)::is_entered_series<Retained>);
}

TEST_CASE("get_series returns the elements exactly as they were supplied", "[series]")
{
    constexpr formula::MeasuredSeries<Retained, 5> read = inputs.get_series<Retained, 5>();
    STATIC_REQUIRE(read.size() == 5);
    STATIC_REQUIRE(read.element(0).value() == rat(130)); // in grams, as entered: the environment does not convert
    STATIC_REQUIRE(read.element(1).value() == rat(210));
    STATIC_REQUIRE(read.element(2).is_absent());
    STATIC_REQUIRE(read.element(4).value() == rat(28));
    // Past the end is absent, never a neighbour's value and never zero.
    STATIC_REQUIRE(read.element(5).is_absent());
    // A braced list of exactly the series' length is taken as it is; only a
    // wrong count is refused (negative: measured_series_short).
    STATIC_REQUIRE(read
                   == formula::MeasuredSeries<Retained, 5>({ m<Retained>(130),
                                                             m<Retained>(210),
                                                             formula::Measured<Retained>::absent(),
                                                             m<Retained>(340),
                                                             m<Retained>(28) }));
    STATIC_REQUIRE(read
                   == formula::MeasuredSeries<Retained, 5> { std::array { m<Retained>(130),
                                                                          m<Retained>(210),
                                                                          formula::Measured<Retained>::absent(),
                                                                          m<Retained>(340),
                                                                          m<Retained>(28) } });
}

TEST_CASE("a series and single values share one environment, each read its own way", "[series]")
{
    constexpr auto mixed =
        formula::environment(formula::measured_series<Retained>(m<Retained>(130), m<Retained>(210)), m<TotalMass>(1250));

    STATIC_REQUIRE(mixed.get<TotalMass>().value() == rat(1250));
    constexpr auto total = formula::checked_evaluate<TotalMass>(formula::var<TotalMass>, mixed);
    STATIC_REQUIRE(total->measurement().value() == rat(1250));
    constexpr auto retained = formula::checked_evaluate_series<Retained>(formula::series<Retained, 2>, mixed);
    STATIC_REQUIRE(retained->element(1).value() == rat(210));
}

TEST_CASE("a series with every element absent reads as absent at every position, never as zero", "[series]")
{
    constexpr auto nobody = formula::environment(formula::measured_series<Retained>(formula::Measured<Retained>::absent(),
                                                                                    formula::Measured<Retained>::absent(),
                                                                                    formula::Measured<Retained>::absent()));
    constexpr auto si =
        formula::detail::dispatch_series<formula::Rational>(formula::series<Retained, 3>, nobody, formula::NullSink {});
    STATIC_REQUIRE(si.has_value());
    STATIC_REQUIRE(!si->elements[0].has_value());
    STATIC_REQUIRE(!si->elements[1].has_value());
    STATIC_REQUIRE(!si->elements[2].has_value());

    constexpr auto outcome = formula::checked_evaluate_series<Retained>(formula::series<Retained, 3>, nobody);
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->element(0).is_absent());
    STATIC_REQUIRE(outcome->element(1).is_absent());
    STATIC_REQUIRE(outcome->element(2).is_absent());
}

TEST_CASE("an element that cannot be read into SI fails the whole series and names that element", "[series]")
{
    // Tonnes become kilograms by multiplying by 1000, and the middle element
    // is too large for that. Its neighbours are fine, so a failure reported at
    // 0 or at the last element, or a partial series, is the wrong answer (S8).
    constexpr std::int64_t tooLarge = std::numeric_limits<std::int64_t>::max() / 100;
    constexpr auto overflowing =
        formula::environment(formula::measured_series<Stockpile>(formula::Measured<Stockpile> { rat(1) },
                                                                 formula::Measured<Stockpile> { rat(2) },
                                                                 formula::Measured<Stockpile> { rat(tooLarge) },
                                                                 formula::Measured<Stockpile> { rat(3) }));

    constexpr auto si = formula::detail::dispatch_series<formula::Rational>(
        formula::series<Stockpile, 4>, overflowing, formula::NullSink {});
    STATIC_REQUIRE(!si.has_value());
    STATIC_REQUIRE(si.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(si.error().element.has_value()); // a failure at an element says so
    STATIC_REQUIRE(*si.error().element == 2);

    constexpr auto outcome = formula::checked_evaluate_series<Stockpile>(formula::series<Stockpile, 4>, overflowing);
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 2 });
}

TEST_CASE("an element that cannot be written back in the declared unit names that element", "[series]")
{
    // Read as Stockpile (tonnes) and reported as Retained (grams): reading
    // into kilograms multiplies by 1000 and still fits, and the way back into
    // grams multiplies by 1000 again, which only element 1 is too large for.
    constexpr std::int64_t tooLarge = std::numeric_limits<std::int64_t>::max() / 10'000;
    constexpr auto large =
        formula::environment(formula::measured_series<Stockpile>(formula::Measured<Stockpile> { rat(1) },
                                                                 formula::Measured<Stockpile> { rat(tooLarge) },
                                                                 formula::Measured<Stockpile> { rat(2) }));

    constexpr auto si =
        formula::detail::dispatch_series<formula::Rational>(formula::series<Stockpile, 3>, large, formula::NullSink {});
    STATIC_REQUIRE(si.has_value()); // reading into kilograms still fits
    constexpr auto outcome = formula::checked_evaluate_series<Retained>(formula::series<Stockpile, 3>, large);
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 1 });
}

TEST_CASE("a failure's position is optional, so a failure of no element can say so", "[series]")
{
    // A shape pin, not behavioural coverage: nothing in this task produces a
    // failure of no element (a series variable fails only at an element).
    // Later reductions do (a sum's overflow), and their tests supply the
    // behaviour. This line fails if the position reverts to a plain size_t,
    // which would force such a failure to name element 0.
    STATIC_REQUIRE(std::is_same_v<decltype(formula::SeriesFailure::element), std::optional<std::size_t>>);
}

TEST_CASE("a series reads into double as well as into Rational", "[series]")
{
    auto const si = formula::detail::dispatch_series<double>(formula::series<Retained, 5>, inputs, formula::NullSink {});
    REQUIRE(si.has_value());
    CHECK(si->elements[0] == 0.13);
    CHECK(!si->elements[2].has_value());
    CHECK(si->elements[4] == 0.028);
}

TEST_CASE("a series of length one is a series", "[series]")
{
    constexpr auto one =
        formula::environment(formula::measured_series<Aperture>(formula::Measured<Aperture> { rat(7, 10) }));
    constexpr auto outcome = formula::checked_evaluate_series<Aperture>(formula::series<Aperture, 1>, one);
    STATIC_REQUIRE(decltype(outcome)::value_type::size() == 1);
    STATIC_REQUIRE(outcome->element(0).value() == rat(7, 10));
    STATIC_REQUIRE(outcome->element(1).is_absent());
}

TEST_CASE("a series variable is an empty node, and not a node that stands where a number stands", "[series]")
{
    STATIC_REQUIRE(formula::SeriesNode<decltype(formula::series<Retained, 5>)>);
    STATIC_REQUIRE_FALSE(formula::Node<decltype(formula::series<Retained, 5>)>);
    STATIC_REQUIRE_FALSE(formula::SeriesNode<decltype(formula::var<Retained>)>);
    STATIC_REQUIRE(std::is_empty_v<formula::SeriesVarNode<Retained, 5>>);
    STATIC_REQUIRE(formula::SeriesVarNode<Retained, 5>::length == 5);
    STATIC_REQUIRE(formula::SeriesVarNode<Retained, 5>::dimension == formula::unit::Gram.dimension);
    STATIC_REQUIRE(std::is_same_v<formula::SeriesVarNode<Retained, 5>::quantity, Retained>);
}

#include "series_cross_tu.hpp"

TEST_CASE("a series formula declared in a header is one formula in every translation unit", "[series]")
{
    // The other unit's formula has this unit's type -- the declaration in the
    // header forces that -- and names the same object.
    CHECK(series_variable_address_in_other_tu() == static_cast<void const*>(&formula::series<series_cross_tu::Retained, 3>));
    auto const there = series_read_in_other_tu();
    auto const here =
        formula::checked_evaluate_series<series_cross_tu::Retained>(series_cross_tu::retained, series_cross_tu::inputs);
    REQUIRE(there.has_value());
    REQUIRE(here.has_value());
    CHECK(*there == *here);
    CHECK(there->element(1).value() == formula::Rational { 21 });
}

TEST_CASE("a series expression passed to another translation unit is the same type there", "[series]")
{
    // Passed as a parameter, so the two units must agree on its whole type
    // -- the cumulative, the product and the series inside it -- or the call
    // does not link on any compiler. 13, 21, 34 g doubled and totalled from
    // the last: 136, 110, 68 g.
    auto const there = totals_read_in_other_tu(series_cross_tu::totals);
    REQUIRE(there.has_value());
    CHECK(there->element(0).value() == formula::Rational { 136 });
    CHECK(there->element(1).value() == formula::Rational { 110 });
    CHECK(there->element(2).value() == formula::Rational { 68 });
}

TEST_CASE("a curve expression passed to another translation unit is the same type there", "[series][curve]")
{
    // The splice, both curves, the declared domains and the running total
    // inside: one type in both units, or the call does not link. 13, 34 and
    // 68 g at 103, 127 and 163 m, then 100 g at 197 m.
    auto const there = spliced_read_in_other_tu(series_cross_tu::spliced);
    REQUIRE(there.has_value());
    CHECK(there->domain()[3].value() == formula::Rational { 197 });
    CHECK(there->values()[1].value() == formula::Rational { 34 });
    CHECK(there->values()[3].value() == formula::Rational { 100 });
}

// ---- Elementwise arithmetic and per-element constants (task 4) ----

namespace
{
namespace elementwise
{
    // Invented quantities; the shared fixture's retained masses and a total
    // of 1250 g -- 1.25 in SI, so dividing by it cannot pass for dividing by
    // one.
    struct FractionRetained: formula::Quantity<FractionRetained, "p_r", "fraction retained", formula::unit::Percent>
    {
    };
    struct Ratio: formula::Quantity<Ratio, "q", "ratio of two masses", formula::unit::One>
    {
    };
    struct PartA: formula::Quantity<PartA, "a", "first part", formula::unit::Gram>
    {
    };
    struct PartB: formula::Quantity<PartB, "b", "second part", formula::unit::Gram>
    {
    };

    constexpr auto screenInputs =
        formula::environment(formula::measured_series<Retained>(
                                 m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)),
                             m<TotalMass>(1250));

    constexpr auto absentMiddle = formula::environment(
        formula::measured_series<Retained>(
            m<Retained>(130), m<Retained>(210), formula::Measured<Retained>::absent(), m<Retained>(340), m<Retained>(28)),
        m<TotalMass>(1250));

    constexpr auto absentTotal =
        formula::environment(formula::measured_series<Retained>(
                                 m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)),
                             formula::Measured<TotalMass>::absent());

    constexpr auto twoSeries =
        formula::environment(formula::measured_series<PartA>(m<PartA>(1), m<PartA>(2), m<PartA>(3)),
                             formula::measured_series<PartB>(m<PartB>(10), m<PartB>(20), m<PartB>(40)));

    constexpr auto zeroInTheMiddle =
        formula::environment(formula::measured_series<PartA>(m<PartA>(1), m<PartA>(2), m<PartA>(3)),
                             formula::measured_series<PartB>(m<PartB>(2), m<PartB>(0), m<PartB>(5)));
} // namespace elementwise
} // namespace

TEST_CASE("a scalar divides every element, and each element keeps its own place", "[series]")
{
    using elementwise::FractionRetained;
    constexpr auto fraction = formula::series<Retained, 5> / formula::var<TotalMass>;
    // (fixture: m_t = 1250 g) -> 10.4 %, 16.8 %, 7.6 %, 27.2 %, 2.24 %. A
    // broadcast applied to element 0 only, a reversed operand order, or a
    // scalar skipped (dividing by one) gives different numbers at every
    // position; all five are asserted.
    constexpr auto out = formula::checked_evaluate_series<FractionRetained>(fraction, elementwise::screenInputs);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->element(0).value() == formula::Rational { 52, 5 });
    STATIC_REQUIRE(out->element(1).value() == formula::Rational { 84, 5 });
    STATIC_REQUIRE(out->element(2).value() == formula::Rational { 38, 5 });
    STATIC_REQUIRE(out->element(3).value() == formula::Rational { 136, 5 });
    STATIC_REQUIRE(out->element(4).value() == formula::Rational { 56, 25 });
    STATIC_REQUIRE(decltype(fraction)::length == 5);
    STATIC_REQUIRE(decltype(fraction)::dimension == formula::unit::One.dimension);
}

TEST_CASE("two series combine element by element, never across positions", "[series]")
{
    // A = 1, 2, 3 g; B = 10, 20, 40 g -> 11, 22, 43 g. A reversed pairing
    // gives 41, 22, 13: only the middle matches, so a first-only or a
    // last-only check would not catch it -- all three are asserted.
    using elementwise::PartA;
    using elementwise::PartB;
    constexpr auto combined = formula::checked_evaluate_series<PartA>(formula::series<PartA, 3> + formula::series<PartB, 3>,
                                                                      elementwise::twoSeries);
    STATIC_REQUIRE(combined->element(0).value() == rat(11));
    STATIC_REQUIRE(combined->element(1).value() == rat(22));
    STATIC_REQUIRE(combined->element(2).value() == rat(43));
    // Subtraction keeps its order: B - A, never A - B.
    constexpr auto difference = formula::checked_evaluate_series<PartA>(
        formula::series<PartB, 3> - formula::series<PartA, 3>, elementwise::twoSeries);
    STATIC_REQUIRE(difference->element(0).value() == rat(9));
    STATIC_REQUIRE(difference->element(2).value() == rat(37));
}

TEST_CASE("a zero divisor in the middle fails the whole series and names that element", "[series]")
{
    // A / B with B = 2, 0, 5 g -> SeriesFailure { DivisionByZero, 1 }; no
    // partial vector (S8). Review Focus 5.
    using elementwise::PartA;
    using elementwise::PartB;
    constexpr auto quotient = formula::detail::dispatch_series<formula::Rational>(
        formula::series<PartA, 3> / formula::series<PartB, 3>, elementwise::zeroInTheMiddle, formula::NullSink {});
    STATIC_REQUIRE(!quotient.has_value());
    STATIC_REQUIRE(quotient.error() == formula::SeriesFailure { formula::ArithmeticError::DivisionByZero, 1 });
}

TEST_CASE("a failing scalar operand fails the whole series, and belongs to no element", "[series]")
{
    // The scalar is evaluated once, before any element: its failure is not
    // the failure of element 0, or of any element, so the position is empty.
    constexpr auto broken =
        formula::series<Retained, 5> * (formula::var<TotalMass> / (formula::var<TotalMass> - formula::var<TotalMass>) );
    constexpr auto out =
        formula::detail::dispatch_series<formula::Rational>(broken, elementwise::screenInputs, formula::NullSink {});
    STATIC_REQUIRE(!out.has_value());
    STATIC_REQUIRE(out.error().error == formula::ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(!out.error().element.has_value());
}

TEST_CASE("an absent scalar makes every element absent; an absent element only itself", "[series]")
{
    // S7, both rows of the table.
    using elementwise::FractionRetained;
    constexpr auto fraction = formula::series<Retained, 5> / formula::var<TotalMass>;

    constexpr auto oneAbsent = formula::checked_evaluate_series<FractionRetained>(fraction, elementwise::absentMiddle);
    STATIC_REQUIRE(oneAbsent->element(1).value() == formula::Rational { 84, 5 });
    STATIC_REQUIRE(oneAbsent->element(2).is_absent());
    STATIC_REQUIRE(oneAbsent->element(3).value() == formula::Rational { 136, 5 });

    constexpr auto allAbsent = formula::checked_evaluate_series<FractionRetained>(fraction, elementwise::absentTotal);
    STATIC_REQUIRE(allAbsent.has_value());
    STATIC_REQUIRE(allAbsent->element(0).is_absent());
    STATIC_REQUIRE(allAbsent->element(2).is_absent());
    STATIC_REQUIRE(allAbsent->element(4).is_absent());
}

TEST_CASE("a scalar or a bare number broadcasts from either side, and negation is per element", "[series]")
{
    constexpr auto s = formula::series<Retained, 5>;
    // m_t - m_r: 1120, 1040, 1155, 910, 1222 g. The reversed order gives the
    // negatives, and a broadcast to element 0 only leaves the rest unchanged.
    constexpr auto passing =
        formula::checked_evaluate_series<Retained>(formula::var<TotalMass> - s, elementwise::screenInputs);
    STATIC_REQUIRE(passing->element(0).value() == rat(1120));
    STATIC_REQUIRE(passing->element(2).value() == rat(1155));
    STATIC_REQUIRE(passing->element(4).value() == rat(1222));

    constexpr auto doubled = formula::checked_evaluate_series<Retained>(s * rat(2), elementwise::screenInputs);
    constexpr auto doubledLeft = formula::checked_evaluate_series<Retained>(rat(2) * s, elementwise::screenInputs);
    constexpr auto halved = formula::checked_evaluate_series<Retained>(s / rat(2), elementwise::screenInputs);
    constexpr auto negated = formula::checked_evaluate_series<Retained>(-s, elementwise::screenInputs);
    STATIC_REQUIRE(doubled->element(3).value() == rat(680));
    STATIC_REQUIRE(doubledLeft->element(3).value() == rat(680));
    STATIC_REQUIRE(halved->element(4).value() == rat(14));
    STATIC_REQUIRE(negated->element(1).value() == rat(-210));
    STATIC_REQUIRE(negated->element(4).value() == rat(-28));
}

TEST_CASE("a bare number combines with a series from either side, each in its own order", "[series]")
{
    // A dimensionless series q = 1/2, 2, 5 -- no element its own reciprocal,
    // none equal to 3 -- so an operand swap in a subtraction negates every
    // element and one in a division inverts it.
    using elementwise::Ratio;
    constexpr auto ratios = formula::environment(formula::measured_series<Ratio>(
        formula::Measured<Ratio> { rat(1, 2) }, formula::Measured<Ratio> { rat(2) }, formula::Measured<Ratio> { rat(5) }));
    constexpr auto q = formula::series<Ratio, 3>;

    // q(i) + 3 and 3 + q(i): 7/2, 5, 8.
    constexpr auto plusRight = formula::checked_evaluate_series<Ratio>(q + rat(3), ratios);
    constexpr auto plusLeft = formula::checked_evaluate_series<Ratio>(rat(3) + q, ratios);
    STATIC_REQUIRE(plusRight->element(0).value() == rat(7, 2));
    STATIC_REQUIRE(plusRight->element(2).value() == rat(8));
    STATIC_REQUIRE(plusLeft->element(0).value() == rat(7, 2));
    STATIC_REQUIRE(plusLeft->element(2).value() == rat(8));

    // q(i) - 3: -5/2, -1, 2. And 3 - q(i): 5/2, 1, -2.
    constexpr auto minusRight = formula::checked_evaluate_series<Ratio>(q - rat(3), ratios);
    constexpr auto minusLeft = formula::checked_evaluate_series<Ratio>(rat(3) - q, ratios);
    STATIC_REQUIRE(minusRight->element(0).value() == rat(-5, 2));
    STATIC_REQUIRE(minusRight->element(1).value() == rat(-1));
    STATIC_REQUIRE(minusRight->element(2).value() == rat(2));
    STATIC_REQUIRE(minusLeft->element(0).value() == rat(5, 2));
    STATIC_REQUIRE(minusLeft->element(1).value() == rat(1));
    STATIC_REQUIRE(minusLeft->element(2).value() == rat(-2));

    // 3 / q(i): 6, 3/2, 3/5. And q(i) / 3: 1/6, 2/3, 5/3.
    constexpr auto overLeft = formula::checked_evaluate_series<Ratio>(rat(3) / q, ratios);
    constexpr auto overRight = formula::checked_evaluate_series<Ratio>(q / rat(3), ratios);
    STATIC_REQUIRE(overLeft->element(0).value() == rat(6));
    STATIC_REQUIRE(overLeft->element(1).value() == rat(3, 2));
    STATIC_REQUIRE(overLeft->element(2).value() == rat(3, 5));
    STATIC_REQUIRE(overRight->element(0).value() == rat(1, 6));
    STATIC_REQUIRE(overRight->element(2).value() == rat(5, 3));

    // A bare number is dimensionless: dividing it by a mass series gives an
    // inverse mass, and the other way round a mass.
    constexpr auto mass = formula::series<Retained, 5>;
    STATIC_REQUIRE(decltype(rat(3) / mass)::dimension == formula::unit::One.dimension / formula::unit::Gram.dimension);
    STATIC_REQUIRE(decltype(mass / rat(3))::dimension == formula::unit::Gram.dimension);
    STATIC_REQUIRE(decltype(rat(3) - q)::dimension == formula::unit::One.dimension);
}

TEST_CASE("a per-element constant pairs with the series position by position", "[series]")
{
    // Factors 2..6 against 130, 210, 95, 340, 28 g: 260, 630, 380, 1700, 168 g.
    // No factor is 1, so a value the constant dropped, or one defaulted to the
    // identity, shows at every position. A reversed pairing gives 780, 1050,
    // 380, 1020, 56: only the middle agrees.
    constexpr auto weights = formula::series_constant<formula::unit::One>(rat(2), rat(3), rat(4), rat(5), rat(6));
    constexpr auto weighted =
        formula::checked_evaluate_series<Retained>(formula::series<Retained, 5> * weights, elementwise::screenInputs);
    STATIC_REQUIRE(weighted->element(0).value() == rat(260));
    STATIC_REQUIRE(weighted->element(1).value() == rat(630));
    STATIC_REQUIRE(weighted->element(2).value() == rat(380));
    STATIC_REQUIRE(weighted->element(3).value() == rat(1700));
    STATIC_REQUIRE(weighted->element(4).value() == rat(168));
    STATIC_REQUIRE(decltype(weights)::length == 5);

    // A constant with a unit is read into coherent SI like any value, so its
    // unit must not be the coherent one for the fixture to tell: 5 g added to
    // 130 g is 135 g, where 5 read as kilograms would give 5130 g.
    constexpr auto offsets = formula::series_constant<formula::unit::Gram, 5>(rat(5), rat(6), rat(7), rat(8), rat(9));
    constexpr auto shifted =
        formula::checked_evaluate_series<Retained>(formula::series<Retained, 5> + offsets, elementwise::screenInputs);
    STATIC_REQUIRE(shifted->element(0).value() == rat(135));
    STATIC_REQUIRE(shifted->element(2).value() == rat(102));
    STATIC_REQUIRE(shifted->element(4).value() == rat(37));
}

TEST_CASE("elementwise nodes are series, empty of state but their operands, and not scalar nodes", "[series]")
{
    constexpr auto fraction = formula::series<Retained, 5> / formula::var<TotalMass>;
    STATIC_REQUIRE(formula::SeriesNode<decltype(fraction)>);
    STATIC_REQUIRE_FALSE(formula::Node<decltype(fraction)>);
    STATIC_REQUIRE(formula::SeriesNode<decltype(-formula::series<Retained, 5>)>);
    STATIC_REQUIRE(formula::SeriesNode<decltype(formula::series_constant<formula::unit::One>(rat(1)))>);
    // A well-formed node is not refused, so the nodes built over it still
    // check their own lengths and dimensions.
    STATIC_REQUIRE_FALSE(decltype(fraction)::refused);
    STATIC_REQUIRE_FALSE(decltype(-formula::series<Retained, 5>)::refused);
    // A series constant must state its contents: no default, as a lookup's
    // corrections have none.
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<formula::SeriesConstantNode<formula::unit::One, 3>>);
}

TEST_CASE("elementwise arithmetic works in double as well as in Rational", "[series]")
{
    auto const out = formula::detail::dispatch_series<double>(
        formula::series<Retained, 5> / formula::var<TotalMass>, elementwise::screenInputs, formula::NullSink {});
    REQUIRE(out.has_value());
    // Each side is read into SI and converted to double, then divided in
    // double: 0.13 kg / 1.25 kg, exactly the division written here.
    CHECK(out->elements[0] == 0.13 / 1.25);
    CHECK(out->elements[4] == 0.028 / 1.25);
}

// ---- Cumulative sums and sum (task 5) ----

namespace
{
namespace running
{
    using elementwise::FractionRetained;
    using elementwise::Ratio;

    // Stated in the coherent unit, so that the elements reach the running
    // total unchanged and two of them can sit next to Rational's limit.
    struct Load: formula::Quantity<Load, "L", "load on a screen", formula::unit::Kilogram>
    {
    };

    constexpr std::int64_t halfLimit = std::numeric_limits<std::int64_t>::max() / 2 + 1;

    // Two elements just over half of Rational's limit, at zero-based 1 and 2
    // of five -- off the centre, so that neither end is where it fails and a
    // position is never the number of additions made. From the first, the
    // total overflows at element 2, the second addition; from the last, at
    // element 1, the third addition.
    constexpr auto nearTheLimit =
        formula::environment(formula::measured_series<Load>(formula::Measured<Load> { rat(1) },
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(2) },
                                                            formula::Measured<Load> { rat(3) }));

    constexpr auto oneScreen = formula::environment(formula::measured_series<Retained>(m<Retained>(130)));

    // The shared fixture with its first, and then its last, screen unmeasured.
    constexpr auto absentFirst = formula::environment(formula::measured_series<Retained>(
        formula::Measured<Retained>::absent(), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)));
    constexpr auto absentLast = formula::environment(formula::measured_series<Retained>(
        m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), formula::Measured<Retained>::absent()));
} // namespace running
} // namespace

TEST_CASE("cumulative runs from the end it is told to, and the two ends differ", "[series]")
{
    constexpr auto s = formula::series<Retained, 5>;
    // From the last screen: 803, 673, 463, 368, 28 g. From the first: 130,
    // 340, 435, 775, 803 g. The two agree nowhere but where one's last total
    // is the other's first (803 g), so a swapped direction fails four of the
    // five assertions in each block.
    constexpr auto fromLast = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(s), elementwise::screenInputs);
    STATIC_REQUIRE(fromLast.has_value());
    STATIC_REQUIRE(fromLast->element(0).value() == rat(803));
    STATIC_REQUIRE(fromLast->element(1).value() == rat(673));
    STATIC_REQUIRE(fromLast->element(2).value() == rat(463));
    STATIC_REQUIRE(fromLast->element(3).value() == rat(368));
    STATIC_REQUIRE(fromLast->element(4).value() == rat(28));

    constexpr auto fromFirst = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(s), elementwise::screenInputs);
    STATIC_REQUIRE(fromFirst.has_value());
    STATIC_REQUIRE(fromFirst->element(0).value() == rat(130));
    STATIC_REQUIRE(fromFirst->element(1).value() == rat(340));
    STATIC_REQUIRE(fromFirst->element(2).value() == rat(435));
    STATIC_REQUIRE(fromFirst->element(3).value() == rat(775));
    STATIC_REQUIRE(fromFirst->element(4).value() == rat(803));

    using FromLastNode = decltype(formula::cumulative<formula::CumulativeDirection::FromLast>(s));
    STATIC_REQUIRE(formula::SeriesNode<FromLastNode>);
    STATIC_REQUIRE(FromLastNode::length == 5);
    STATIC_REQUIRE(FromLastNode::dimension == formula::unit::Gram.dimension);
    STATIC_REQUIRE(FromLastNode::direction == formula::CumulativeDirection::FromLast);
}

TEST_CASE("sum reduces a series to one value, a Node that stands where a number stands", "[series]")
{
    constexpr auto total = formula::sum(formula::series<Retained, 5>);
    STATIC_REQUIRE(formula::Node<decltype(total)>);
    STATIC_REQUIRE_FALSE(formula::SeriesNode<decltype(total)>);
    STATIC_REQUIRE(decltype(total)::dimension == formula::unit::Gram.dimension);

    constexpr auto out = formula::checked_evaluate<TotalMass>(total, elementwise::screenInputs);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->measurement().value() == rat(803));
}

TEST_CASE("passing percentages from the cumulative retained, run from the last screen", "[series]")
{
    using running::FractionRetained;
    // 100 % - cumulative<FromLast>(m_r) / m_t with m_t = 1250 g: 35.76,
    // 46.16, 62.96, 70.56 and 97.76 %. Run from the first screen it would give
    // 89.6, 72.8, 65.2, 38 and 35.76 %.
    constexpr auto passing =
        formula::constant<formula::unit::Percent>(rat(100))
        - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>) / formula::var<TotalMass>;
    constexpr auto out = formula::checked_evaluate_series<FractionRetained>(passing, elementwise::screenInputs);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->element(0).value() == rat(894, 25));
    STATIC_REQUIRE(out->element(1).value() == rat(1154, 25));
    STATIC_REQUIRE(out->element(2).value() == rat(1574, 25));
    STATIC_REQUIRE(out->element(3).value() == rat(1764, 25));
    STATIC_REQUIRE(out->element(4).value() == rat(2444, 25));
}

TEST_CASE("one series read per element and as a whole in one formula, in either order", "[series]")
{
    using running::Ratio;
    constexpr auto s = formula::series<Retained, 5>;
    // m_r(i) / sum(m_r): element 1 is 210/803. Both are read from the same
    // data, and neither may stand in for the other.
    constexpr auto share = formula::checked_evaluate_series<Ratio>(s / formula::sum(s), elementwise::screenInputs);
    STATIC_REQUIRE(share.has_value());
    STATIC_REQUIRE(share->element(1).value() == rat(210, 803));
    STATIC_REQUIRE(share->element(3).value() == rat(340, 803));

    // The other order, sum(m_r) / m_r(i): element 1 is 803/210.
    constexpr auto inverse = formula::checked_evaluate_series<Ratio>(formula::sum(s) / s, elementwise::screenInputs);
    STATIC_REQUIRE(inverse.has_value());
    STATIC_REQUIRE(inverse->element(1).value() == rat(803, 210));
    STATIC_REQUIRE(inverse->element(3).value() == rat(803, 340));
}

TEST_CASE("an absent element stops every running total past it, and makes the sum absent", "[series]")
{
    constexpr auto s = formula::series<Retained, 5>;
    // Element 2 (95 g) unmeasured. From the last: 28, 368, then absent three
    // times -- the total at element 2 would include what was never measured.
    constexpr auto fromLast = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(s), elementwise::absentMiddle);
    STATIC_REQUIRE(fromLast.has_value());
    STATIC_REQUIRE(fromLast->element(4).value() == rat(28));
    STATIC_REQUIRE(fromLast->element(3).value() == rat(368));
    STATIC_REQUIRE(fromLast->element(2).is_absent());
    STATIC_REQUIRE(fromLast->element(1).is_absent());
    STATIC_REQUIRE(fromLast->element(0).is_absent());

    // From the first: 130, 340, then absent three times.
    constexpr auto fromFirst = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(s), elementwise::absentMiddle);
    STATIC_REQUIRE(fromFirst.has_value());
    STATIC_REQUIRE(fromFirst->element(0).value() == rat(130));
    STATIC_REQUIRE(fromFirst->element(1).value() == rat(340));
    STATIC_REQUIRE(fromFirst->element(2).is_absent());
    STATIC_REQUIRE(fromFirst->element(3).is_absent());
    STATIC_REQUIRE(fromFirst->element(4).is_absent());

    // The sum is absent, never 708 g -- the total of the four that were
    // measured.
    constexpr auto total = formula::checked_evaluate<TotalMass>(formula::sum(s), elementwise::absentMiddle);
    STATIC_REQUIRE(total.has_value());
    STATIC_REQUIRE(total->measurement().is_absent());
}

TEST_CASE("the cumulative of a one-element series is that series, from either end", "[series]")
{
    constexpr auto s = formula::series<Retained, 1>;
    constexpr auto fromLast = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(s), running::oneScreen);
    constexpr auto fromFirst = formula::checked_evaluate_series<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(s), running::oneScreen);
    STATIC_REQUIRE(fromLast->element(0).value() == rat(130));
    STATIC_REQUIRE(fromFirst->element(0).value() == rat(130));
    constexpr auto total = formula::checked_evaluate<TotalMass>(formula::sum(s), running::oneScreen);
    STATIC_REQUIRE(total->measurement().value() == rat(130));
}

TEST_CASE("a running total that overflows fails at the element where it overflowed", "[series]")
{
    using running::Load;
    constexpr auto s = formula::series<Load, 5>;
    constexpr auto fromFirst = formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(s), running::nearTheLimit, formula::NullSink {});
    STATIC_REQUIRE(!fromFirst.has_value());
    STATIC_REQUIRE(fromFirst.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 2 });

    constexpr auto fromLast = formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(s), running::nearTheLimit, formula::NullSink {});
    STATIC_REQUIRE(!fromLast.has_value());
    STATIC_REQUIRE(fromLast.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 1 });

    // A sum overflows as a whole: one value has no element to name.
    constexpr auto total = formula::checked_evaluate_si<formula::Rational>(formula::sum(s), running::nearTheLimit);
    STATIC_REQUIRE(!total.has_value());
    STATIC_REQUIRE(total.error() == formula::ArithmeticError::Overflow);

    // Absence is judged over the whole series first, so where the gap is
    // plays no part (final review, L1): absent with the gap after the two
    // elements whose addition overflows, and absent with it before them.
    using running::halfLimit;
    constexpr auto gapAfter =
        formula::environment(formula::measured_series<Load>(formula::Measured<Load> { rat(1) },
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(2) },
                                                            formula::Measured<Load>::absent()));
    constexpr auto gapBefore =
        formula::environment(formula::measured_series<Load>(formula::Measured<Load>::absent(),
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(halfLimit) },
                                                            formula::Measured<Load> { rat(2) },
                                                            formula::Measured<Load> { rat(3) }));
    constexpr auto totalGapAfter = formula::checked_evaluate_si<formula::Rational>(formula::sum(s), gapAfter);
    STATIC_REQUIRE(totalGapAfter.has_value());
    STATIC_REQUIRE(!totalGapAfter->has_value());
    constexpr auto totalGapBefore = formula::checked_evaluate_si<formula::Rational>(formula::sum(s), gapBefore);
    STATIC_REQUIRE(totalGapBefore.has_value());
    STATIC_REQUIRE(!totalGapBefore->has_value());
}

TEST_CASE("a failed series fails its sum and its running totals, with the failure relayed", "[series]")
{
    // m_a / (m_a - m_a) divides by zero at its first element.
    using elementwise::PartA;
    constexpr auto broken = formula::series<PartA, 3> / (formula::series<PartA, 3> - formula::series<PartA, 3>);
    constexpr auto totals = formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(broken), elementwise::twoSeries, formula::NullSink {});
    STATIC_REQUIRE(totals.error() == formula::SeriesFailure { formula::ArithmeticError::DivisionByZero, 0 });
    constexpr auto total = formula::checked_evaluate_si<formula::Rational>(formula::sum(broken), elementwise::twoSeries);
    STATIC_REQUIRE(total.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("an absent element at either end stops the totals past it, and the sum", "[series]")
{
    // Replaces a test with nothing measured, which a sum or a running total
    // that skipped absent elements passed as well: everything stays absent
    // either way. Here an absent element comes BEFORE the first present one
    // in the running direction, where a total that skipped it would start
    // from the next.
    constexpr auto s = formula::series<Retained, 5>;
    using formula::CumulativeDirection;

    // First screen unmeasured. From the first: absent at all five. From the
    // last: 28, 368, 463, 673 g, with element 0 absent.
    constexpr auto firstFromFirst =
        formula::checked_evaluate_series<Retained>(formula::cumulative<CumulativeDirection::FromFirst>(s), running::absentFirst);
    STATIC_REQUIRE(firstFromFirst->element(0).is_absent());
    STATIC_REQUIRE(firstFromFirst->element(1).is_absent());
    STATIC_REQUIRE(firstFromFirst->element(4).is_absent());
    constexpr auto firstFromLast =
        formula::checked_evaluate_series<Retained>(formula::cumulative<CumulativeDirection::FromLast>(s), running::absentFirst);
    STATIC_REQUIRE(firstFromLast->element(0).is_absent());
    STATIC_REQUIRE(firstFromLast->element(1).value() == rat(673));
    STATIC_REQUIRE(firstFromLast->element(4).value() == rat(28));

    // Last screen unmeasured. From the first: 130, 340, 435, 775 g, then
    // absent. From the last: absent at all five.
    constexpr auto lastFromFirst =
        formula::checked_evaluate_series<Retained>(formula::cumulative<CumulativeDirection::FromFirst>(s), running::absentLast);
    STATIC_REQUIRE(lastFromFirst->element(0).value() == rat(130));
    STATIC_REQUIRE(lastFromFirst->element(3).value() == rat(775));
    STATIC_REQUIRE(lastFromFirst->element(4).is_absent());
    constexpr auto lastFromLast =
        formula::checked_evaluate_series<Retained>(formula::cumulative<CumulativeDirection::FromLast>(s), running::absentLast);
    STATIC_REQUIRE(lastFromLast->element(4).is_absent());
    STATIC_REQUIRE(lastFromLast->element(3).is_absent());
    STATIC_REQUIRE(lastFromLast->element(0).is_absent());

    // The sum is absent both times: never 673 g or 775 g, the totals of what
    // was measured.
    constexpr auto sumFirst = formula::checked_evaluate<TotalMass>(formula::sum(s), running::absentFirst);
    constexpr auto sumLast = formula::checked_evaluate<TotalMass>(formula::sum(s), running::absentLast);
    STATIC_REQUIRE(sumFirst->measurement().is_absent());
    STATIC_REQUIRE(sumLast->measurement().is_absent());
}

// ---- Per-element rounding (task 6) ----

namespace
{
namespace perElement
{
    // Invented, and signed so that the directed modes can be told apart: a
    // deviation from a target grading at each of five screens, in percent.
    struct Deviation: formula::Quantity<Deviation, "e", "deviation from the target grading", formula::unit::Percent>
    {
    };

    // 62.5, 63.5, 97.7, -41.25 and -8.03 %, at 0, 0, 0, 1 and 1 places: every
    // pair of the seven modes differs at some element (checked with exact
    // fractions), and a table used the wrong way round (1, 1, 0, 0, 0) or
    // flattened (all 0) differs too. The elements arrive in SI, as fractions
    // (62.5 % is 5/8), and are rounded in percent: rounded in SI, 5/8 at 0
    // places would be 1, which is 100 %.
    constexpr auto deviations =
        formula::environment(formula::measured_series<Deviation>(formula::Measured<Deviation> { rat(125, 2) },
                                                                 formula::Measured<Deviation> { rat(127, 2) },
                                                                 formula::Measured<Deviation> { rat(977, 10) },
                                                                 formula::Measured<Deviation> { rat(-165, 4) },
                                                                 formula::Measured<Deviation> { rat(-803, 100) }));

    constexpr formula::PlacesTable<5> places { formula::DecimalPlaces { 0 },
                                               formula::DecimalPlaces { 0 },
                                               formula::DecimalPlaces { 0 },
                                               formula::DecimalPlaces { 1 },
                                               formula::DecimalPlaces { 1 } };

    template <formula::RoundingMode Mode>
    constexpr auto roundedWith(auto const& inputs)
    {
        return formula::checked_evaluate_series<Deviation>(
            formula::rounded_elementwise<formula::unit::Percent, places, Mode>(formula::series<Deviation, 5>), inputs);
    }

    /// Whether @p out holds exactly the five values given, each in percent.
    template <typename Out>
    constexpr bool holds(Out const& out,
                         formula::Rational a,
                         formula::Rational b,
                         formula::Rational c,
                         formula::Rational d,
                         formula::Rational f)
    {
        return out.has_value() && out->element(0).value() == a && out->element(1).value() == b
               && out->element(2).value() == c && out->element(3).value() == d && out->element(4).value() == f;
    }
} // namespace perElement
} // namespace

TEST_CASE("each element is rounded to its own granularity, in the stated unit, under each of the seven modes", "[series]")
{
    using formula::RoundingMode;
    using perElement::deviations;
    using perElement::holds;
    using perElement::roundedWith;
    // Element 0 (62.5) splits ties up from ties down; element 1 (63.5) splits
    // ties to even from ties toward zero and floor; element 2 (97.7) splits
    // ties toward zero from the directed-down modes; element 3 (-41.25, a
    // tie below zero) splits away-from-zero from toward-zero, and floor from
    // ceiling; element 4 (-8.03) splits floor and away-from-zero from toward
    // zero and ceiling.
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::HalfAwayFromZero>(deviations), rat(63), rat(64), rat(98), rat(-413, 10), rat(-8)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::HalfTowardZero>(deviations), rat(62), rat(63), rat(98), rat(-206, 5), rat(-8)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::HalfEven>(deviations), rat(62), rat(64), rat(98), rat(-206, 5), rat(-8)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::Ceiling>(deviations), rat(63), rat(64), rat(98), rat(-206, 5), rat(-8)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::Floor>(deviations), rat(62), rat(63), rat(97), rat(-413, 10), rat(-81, 10)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::TowardZero>(deviations), rat(62), rat(63), rat(97), rat(-206, 5), rat(-8)));
    STATIC_REQUIRE(holds(roundedWith<RoundingMode::AwayFromZero>(deviations), rat(63), rat(64), rat(98), rat(-413, 10), rat(-81, 10)));
}

TEST_CASE("a per-element rounding keeps absence, and names the element a failure arose at", "[series]")
{
    using perElement::Deviation;
    constexpr auto oneAbsent = formula::environment(formula::measured_series<Deviation>(
        formula::Measured<Deviation> { rat(125, 2) }, formula::Measured<Deviation> { rat(127, 2) },
        formula::Measured<Deviation>::absent(), formula::Measured<Deviation> { rat(-165, 4) },
        formula::Measured<Deviation> { rat(-803, 100) }));
    constexpr auto out = perElement::roundedWith<formula::RoundingMode::HalfAwayFromZero>(oneAbsent);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->element(1).value() == rat(64));
    STATIC_REQUIRE(out->element(2).is_absent());
    STATIC_REQUIRE(out->element(3).value() == rat(-413, 10));

    // Rounding a load stated in kilograms in grams multiplies by 1000, which
    // overflows for an element near Rational's limit -- the middle one of
    // three, so neither end is where it fails.
    using running::Load;
    constexpr auto heavy = formula::environment(formula::measured_series<Load>(
        formula::Measured<Load> { rat(1) }, formula::Measured<Load> { rat(running::halfLimit) }, formula::Measured<Load> { rat(2) }));
    constexpr formula::PlacesTable<3> wholeGrams { formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 } };
    constexpr auto overflowed = formula::detail::dispatch_series<formula::Rational>(
        formula::rounded_elementwise<formula::unit::Gram, wholeGrams, formula::RoundingMode::HalfAwayFromZero>(
            formula::series<Load, 3>),
        heavy,
        formula::NullSink {});
    STATIC_REQUIRE(!overflowed.has_value());
    STATIC_REQUIRE(overflowed.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 1 });
}

TEST_CASE("a per-element rounding is a series node carrying its unit, table and mode", "[series]")
{
    using Rounding = decltype(formula::rounded_elementwise<formula::unit::Percent, perElement::places, formula::RoundingMode::Floor>(
        formula::series<perElement::Deviation, 5>));
    STATIC_REQUIRE(formula::SeriesNode<Rounding>);
    STATIC_REQUIRE_FALSE(formula::Node<Rounding>);
    STATIC_REQUIRE(Rounding::length == 5);
    STATIC_REQUIRE(Rounding::dimension == formula::unit::Percent.dimension);
    STATIC_REQUIRE(Rounding::unit == formula::unit::Percent);
    STATIC_REQUIRE(Rounding::places == perElement::places);
    STATIC_REQUIRE(Rounding::mode == formula::RoundingMode::Floor);
    STATIC_REQUIRE_FALSE(Rounding::refused);
}
