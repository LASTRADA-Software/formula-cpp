// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it
// REJECT: cannot see inside
//
// One overlay fixes the scale, read inside the fit, and then replaces the
// other variant with a fit whose input reads the scale plainly. The rewrite
// must know the call (`known`), or none of the overlay's result checks run
// and the method reads one scale at two values.
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
        formula::overlay(formula::with_constant<Scale>(formula::Rational { 103, 100 }, annex),
                         formula::replace_variant<Nominal>(formula::opaque_output<"slope">(scaledFit), annex)),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
