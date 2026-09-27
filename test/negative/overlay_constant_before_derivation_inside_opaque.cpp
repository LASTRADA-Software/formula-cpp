// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes a quantity that nothing read where the constant is listed
// REJECT: cannot see inside
//
// The constant for the correction is listed before the derivation that puts
// the correction inside the fit's input: where the constant stands, nothing
// reads the correction yet. Listed the other way round the overlay is
// accepted and fixes it (least_squares_tests.cpp): the two orders differ.
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <tuple>

namespace
{
struct Fitted
{
};
struct Nominal
{
};

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};
struct Scale: formula::Quantity<Scale, "s", "an invented scale", formula::unit::One>
{
};
struct Correction: formula::Quantity<Correction, "c", "an invented correction", formula::unit::One>
{
};

using TenthOfMillimetrePerMinute = formula::
    RoundingRule<formula::unit::MillimetrePerMinute, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };

// The fit over scaled lengths: the scale is read inside the call's input.
inline constexpr auto scaledFit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>* formula::var<Scale>),
    { .reference = "Example Standard 12", .section = "5.2" });

inline constexpr auto m = formula::method(
    formula::variants(
        formula::variant<Fitted>(formula::opaque_output<"slope">(scaledFit)),
        formula::variant<Nominal>(formula::constant<formula::unit::MillimetrePerMinute>(formula::Rational { 401, 10 }))),
    TenthOfMillimetrePerMinute {},
    formula::constraints());
} // namespace
int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<Correction>(formula::Rational { 515, 1000 }, annex),
                         formula::add_derived<Scale>(formula::var<Correction> * formula::Rational { 2 }, annex)),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
