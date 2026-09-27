// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/binning.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::band;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented, and plainly so: particle sizes in metres, counted into the
// classes 0 to under 11, 11 to under 29 and 29 to under 83 m -- primes, and
// not a sieve size in any unit.
struct Size: formula::Quantity<Size, "d", "particle size", unit::Metre>
{
};
struct SizeInCm: formula::Quantity<SizeInCm, "d_c", "particle size, in centimetres", unit::Centimetre>
{
};
struct Share: formula::Quantity<Share, "s", "share of the particles in a class", unit::One>
{
};
struct Count: formula::Quantity<Count, "n", "particles in a class", unit::One>
{
};

constexpr formula::BandTable<3> sizeClasses { band(0, 1, 11, 1), band(11, 1, 29, 1), band(29, 1, 83, 1) };

// 11 and 29 m sit exactly on class boundaries: half-open, each is counted in
// the upper class, giving 2, 2, 3. Closed at the top instead, the counts
// would be 3, 2, 2.
constexpr auto sevenSizes =
    formula::MeasuredObservations<Size, 7>(rat(4), rat(11), rat(17), rat(79), rat(10), rat(29), rat(47));
constexpr auto sized = formula::environment(sevenSizes);

constexpr auto counted = formula::binned<unit::Metre, sizeClasses>(formula::observations<Size, 7>);

template <std::size_t N>
constexpr bool counts_are(auto const& outcome, std::array<formula::Rational, N> const& expected)
{
    if (!outcome.has_value())
        return false;
    for (std::size_t at = 0; at < N; ++at)
        if (outcome->elements()[at].stored() != std::optional<formula::Rational> { expected[at] })
            return false;
    return true;
}

template <typename Q, std::size_t Capacity, typename... Rs>
constexpr auto sizes_env(Rs... observed)
{
    return formula::environment(formula::MeasuredObservations<Q, Capacity>(observed...));
}
} // namespace

TEST_CASE("observations are counted into half-open classes, a boundary in the upper class", "[binning]")
{
    constexpr auto outcome = formula::checked_evaluate_series<Count>(counted, sized);
    STATIC_REQUIRE(counts_are(outcome, std::array { rat(2), rat(2), rat(3) }));
    STATIC_REQUIRE(decltype(counted)::length == 3);
}

TEST_CASE("an observation in no class is a miss at its own position, never dropped", "[binning]")
{
    // 83 m, the last class's high bound, is in no class: the fourth of seven.
    constexpr auto onTheTop = sizes_env<Size, 7>(rat(4), rat(11), rat(17), rat(83), rat(10), rat(29), rat(47));
    STATIC_REQUIRE(formula::checked_evaluate_series<Count>(counted, onTheTop).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 3 });
    // Below the first class, as the last observation.
    constexpr auto belowAll = sizes_env<Size, 7>(rat(4), rat(11), rat(17), rat(79), rat(10), rat(29), rat(-1));
    STATIC_REQUIRE(formula::checked_evaluate_series<Count>(counted, belowAll).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 6 });
}

TEST_CASE("observations in another unit are converted into the classes' unit before counting", "[binning]")
{
    constexpr auto inCm = sizes_env<SizeInCm, 7>(rat(400), rat(1100), rat(1700), rat(7900), rat(1000), rat(2900), rat(4700));
    constexpr auto countedInCm = formula::binned<unit::Metre, sizeClasses>(formula::observations<SizeInCm, 7>);
    STATIC_REQUIRE(
        counts_are(formula::checked_evaluate_series<Count>(countedInCm, inCm), std::array { rat(2), rat(2), rat(3) }));
}

TEST_CASE("classes stated in another unit than the coherent one are compared in their own", "[binning]")
{
    // The same classes in centimetres, the sizes in metres.
    constexpr formula::BandTable<3> classesInCm { band(0, 1, 1100, 1), band(1100, 1, 2900, 1), band(2900, 1, 8300, 1) };
    constexpr auto countedInCm = formula::binned<unit::Centimetre, classesInCm>(formula::observations<Size, 7>);
    STATIC_REQUIRE(
        counts_are(formula::checked_evaluate_series<Count>(countedInCm, sized), std::array { rat(2), rat(2), rat(3) }));
}

TEST_CASE("a count divided by the sum of the counts is each class's share", "[binning]")
{
    constexpr auto shares = counted / formula::sum(counted);
    STATIC_REQUIRE(
        counts_are(formula::checked_evaluate_series<Share>(shares, sized), std::array { rat(2, 7), rat(2, 7), rat(3, 7) }));
}

TEST_CASE("fewer observations than the capacity are counted as they are", "[binning][capacity]")
{
    // Seven of ten: a count that ran to the capacity would add three zeros to
    // the first class.
    constexpr auto roomy = sizes_env<Size, 10>(rat(4), rat(11), rat(17), rat(79), rat(10), rat(29), rat(47));
    constexpr auto countedRoomy = formula::binned<unit::Metre, sizeClasses>(formula::observations<Size, 10>);
    STATIC_REQUIRE(
        counts_are(formula::checked_evaluate_series<Count>(countedRoomy, roomy), std::array { rat(2), rat(2), rat(3) }));
    STATIC_REQUIRE(sevenSizes.size() == 7);
    STATIC_REQUIRE(sevenSizes.capacity() == 7);
    STATIC_REQUIRE(sevenSizes.observation(6).value() == rat(47));
    STATIC_REQUIRE(sevenSizes.observation(7).is_absent());

    // None at all: every count is zero.
    constexpr auto none = formula::environment(formula::MeasuredObservations<Size, 7> {});
    STATIC_REQUIRE(
        counts_are(formula::checked_evaluate_series<Count>(counted, none), std::array { rat(0), rat(0), rat(0) }));
}

TEST_CASE("a set known only at run time fills to its capacity, and one more is refused", "[binning][capacity]")
{
    std::vector<formula::Rational> const seven { rat(4), rat(11), rat(17), rat(79), rat(10), rat(29), rat(47) };
    auto const filled = formula::MeasuredObservations<Size, 7>::from(std::span<formula::Rational const> { seven });
    REQUIRE(filled.has_value());
    CHECK(*filled == sevenSizes);
    CHECK(counts_are(formula::checked_evaluate_series<Count>(counted, formula::environment(*filled)),
                     std::array { rat(2), rat(2), rat(3) }));

    std::vector<formula::Rational> eight = seven;
    eight.push_back(rat(5));
    auto const overfull = formula::MeasuredObservations<Size, 7>::from(std::span<formula::Rational const> { eight });
    REQUIRE(!overfull.has_value());
    // Refused with both counts, as `envelope_from` refuses a wrong number of
    // rows (final review, L6).
    CHECK(overfull.error() == formula::ObservationsOverCapacity { .given = 8, .capacity = 7 });

    std::vector<formula::Rational> const three { rat(4), rat(11), rat(79) };
    auto const few = formula::MeasuredObservations<Size, 7>::from(std::span<formula::Rational const> { three });
    REQUIRE(few.has_value());
    CHECK(few->size() == 3);
    CHECK(counts_are(formula::checked_evaluate_series<Count>(counted, formula::environment(*few)),
                     std::array { rat(1), rat(1), rat(1) }));
}

namespace
{
constexpr auto missed = sizes_env<Size, 7>(rat(4), rat(11), rat(17), rat(83), rat(10), rat(29), rat(47));
constexpr formula::SeriesFailure noElement { formula::ArithmeticError::DomainError, std::nullopt };

constexpr formula::BreakpointTable<3> classMidpoints { formula::breakpoint(5),
                                                       formula::breakpoint(20),
                                                       formula::breakpoint(56) };
} // namespace

TEST_CASE("an operation over the counts relays a binning's failure without its observation", "[binning]")
{
    // Observation 3 is no count's position: element 3 of three counts does
    // not exist.
    STATIC_REQUIRE(formula::checked_evaluate_series<Share>(counted / formula::sum(counted), missed).error() == noElement);
    STATIC_REQUIRE(formula::checked_evaluate_series<Share>(counted / rat(7), missed).error() == noElement);
    STATIC_REQUIRE(formula::checked_evaluate_series<Count>(-counted, missed).error() == noElement);
    STATIC_REQUIRE(formula::checked_evaluate_series<Count>(
                       formula::cumulative<formula::CumulativeDirection::FromFirst>(counted), missed)
                       .error()
                   == noElement);
    STATIC_REQUIRE(formula::checked_evaluate_series<Count>(
                       formula::rounded_elementwise<unit::One,
                                                    formula::PlacesTable<3> { formula::DecimalPlaces { 0 },
                                                                              formula::DecimalPlaces { 0 },
                                                                              formula::DecimalPlaces { 0 } },
                                                    formula::RoundingMode::HalfEven>(counted),
                       missed)
                       .error()
                   == noElement);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Size, Count>(
                       formula::curve(formula::domain<unit::Metre, classMidpoints>(), counted), missed)
                       .error()
                   == noElement);
}

// ---- Renderings ----

TEST_CASE("a binning renders its observations marked and each class as a band", "[binning][render]")
{
    CHECK(formula::render(counted) == "bin(d(i), 0 to under 11 m, 11 to under 29 m, 29 to under 83 m)");
    CHECK(formula::render<formula::Dialect::Markdown>(counted)
          == "bin(`d(i)`, 0 to under 11 m, 11 to under 29 m, 29 to under 83 m)");
    CHECK(formula::render<formula::Dialect::LaTeX>(counted)
          == "\\operatorname{bin}({d}_{i},\\allowbreak \\mathrm{0\\ to\\ under\\ 11\\ m},\\allowbreak "
             "\\mathrm{11\\ to\\ under\\ 29\\ m},\\allowbreak \\mathrm{29\\ to\\ under\\ 83\\ m})");
    CHECK(formula::render(counted / formula::sum(counted))
          == "bin(d(i), 0 to under 11 m, 11 to under 29 m, 29 to under 83 m) / sum(bin(d(i), 0 to under 11 m, "
             "11 to under 29 m, 29 to under 83 m))");
}

TEST_CASE("a binning's observations document as one row of their capacity", "[binning][document]")
{
    formula::Documentation const page = formula::document(counted);
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "d");
    CHECK(page.symbols[0].description == "particle size");
    CHECK(page.symbols[0].shape == formula::ValueShape::Observations);
    CHECK(page.symbols[0].length == 7);
    // Read twice, one row.
    CHECK(formula::document(counted / formula::sum(counted)).symbols.size() == 1);
}

// ---- Trace ----

TEST_CASE("a binning's trace lists the observations and then the counts", "[binning][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_series<Count>(counted, sized, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. d = 4 m; 11 m; 17 m; 79 m; 10 m; 29 m; 47 m\n"
             "2. bin(#1) = 2; 2; 3\n");
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[0].kind == formula::StepKind::ObservationsVariable);
    CHECK(trace.steps[1].kind == formula::StepKind::Binning);
    CHECK(trace.steps[1].operands == std::vector<std::size_t> { 0 });

    // Seven made of ten: the step lists the seven.
    formula::Trace<> roomy {};
    (void) formula::checked_evaluate_series<Count>(
        formula::binned<unit::Metre, sizeClasses>(formula::observations<Size, 10>),
        sizes_env<Size, 10>(rat(4), rat(11), rat(17), rat(79), rat(10), rat(29), rat(47)),
        formula::RecordingSink<> { roomy });
    CHECK(formula::render_trace(roomy, { .maxSteps = 30 }).starts_with("1. d = 4 m; 11 m; 17 m; 79 m; 10 m; 29 m; 47 m\n"));
}

TEST_CASE("a binning's observations spend the element budget, and say how many were left out", "[binning][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_series<Count>(counted, sized, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 5 }).starts_with("1. d = 4 m; 11 m; 17 m; 79 m; ... 3 more\n"));
}

TEST_CASE("a binning's miss names the observation, one-based, its value and the classes' extent", "[binning][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_series<Count>(counted, missed, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
              .ends_with("2. bin(#1) = argument outside the domain of the operation at observation 4 "
                         "[83 m in no class; the classes cover 0 to under 83 m]\n"));
    CHECK(trace.steps.back().failedElement == std::optional<std::size_t> { 3 });

    // Divided by 7, the failure is relayed with no position; the 7 was never
    // evaluated, so the step names only the counts (`binary_expression`).
    formula::Trace<> relayed {};
    (void) formula::checked_evaluate_series<Share>(counted / rat(7), missed, formula::RecordingSink<> { relayed });
    CHECK(formula::render_trace(relayed, { .maxSteps = 30 })
              .ends_with("3. / #2 = argument outside the domain of the operation\n"));
    CHECK(!relayed.steps.back().failedElement.has_value());
}
