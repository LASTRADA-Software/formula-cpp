// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it
// REJECT: cannot see inside
//
// overlay_substitution_inside_opaque_across_overlays, with the fitted slope
// rounded where it is used. The first overlay fixes the scale, which the
// method reads only inside the rounded output's call. The second replaces the
// other variant with a rounded output whose call reads the scale plainly. The
// whole-method rule must see the substitution inside the rounded output's
// call (`SubstitutedIn` for `RoundedOpaqueOutputNode`): without it, one method
// would read the scale at 1.03 in one variant and from the environment in the
// other.
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

using TenthOfMillimetrePerMinute = formula::
    RoundingRule<formula::unit::MillimetrePerMinute, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };

// The fit over scaled lengths: the scale is read inside the call's input.
inline constexpr auto scaledFit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>* formula::var<Scale>),
    { .reference = "Example Standard 12", .section = "5.2" });

// Its slope, rounded to 0.01 mm/min where it is used.
inline constexpr auto roundedSlope = formula::rounded_output<"slope",
                                                             formula::unit::MillimetrePerMinute,
                                                             formula::DecimalPlaces { 2 },
                                                             formula::RoundingMode::HalfEven>(scaledFit);

inline constexpr auto m = formula::method(
    formula::variants(
        formula::variant<Fitted>(roundedSlope),
        formula::variant<Nominal>(formula::constant<formula::unit::MillimetrePerMinute>(formula::Rational { 401, 10 }))),
    TenthOfMillimetrePerMinute {},
    formula::constraints());
} // namespace

int main()
{
    constexpr auto first =
        formula::apply(formula::overlay(formula::with_constant<Scale>(formula::Rational { 103, 100 }, annex)), m);
    constexpr auto second = formula::apply(formula::overlay(formula::replace_variant<Nominal>(roundedSlope, annex)), first);
    return std::tuple_size_v<decltype(second.variantSet.cases)> == 0 ? 1 : 0;
}
