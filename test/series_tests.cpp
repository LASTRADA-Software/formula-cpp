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
        formula::environment(formula::measured_series<Retained>(m<Retained>(130), m<Retained>(210)), m<TotalMass>(1000));

    STATIC_REQUIRE(mixed.get<TotalMass>().value() == rat(1000));
    constexpr auto total = formula::checked_evaluate<TotalMass>(formula::var<TotalMass>, mixed);
    STATIC_REQUIRE(total->measurement().value() == rat(1000));
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
