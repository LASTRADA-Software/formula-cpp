// SPDX-License-Identifier: Apache-2.0
//
// Raw observations are read through their own header, without a binning's
// classes, lookups and bands: this translation unit includes nothing else of
// the library's.
#include <formula-cpp/observations.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace
{
struct Size: formula::Quantity<Size, "d_o", "an invented particle size", formula::unit::Millimetre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Three made in room for eight: 103, 127 and 163 mm.
constexpr auto threeMade =
    formula::environment(formula::MeasuredObservations<Size, 8>(rat(103), rat(127), rat(163)));

constexpr auto read_three()
{
    formula::NullSink quiet {};
    return formula::detail::evaluate_observations<formula::Rational>(formula::observations<Size, 8>, threeMade, quiet);
}
} // namespace

TEST_CASE("raw observations are read through their own header, as many as were made", "[observations]")
{
    // In the coherent unit: 103 mm is 103/1000 m. The count is the three made,
    // not the capacity of eight, and every place past it holds zero.
    constexpr auto made = read_three();
    STATIC_REQUIRE(made.has_value());
    STATIC_REQUIRE(made->count == 3);
    STATIC_REQUIRE(made->elements[0] == rat(103, 1000));
    STATIC_REQUIRE(made->elements[2] == rat(163, 1000));
    STATIC_REQUIRE(made->elements[3] == rat(0));
    STATIC_REQUIRE(formula::ObservationsNode<decltype(formula::observations<Size, 8>)>);
    STATIC_REQUIRE(decltype(formula::observations<Size, 8>)::capacity == 8);
}
