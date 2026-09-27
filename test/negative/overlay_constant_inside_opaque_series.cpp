// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes a quantity the method reads as a series; one constant cannot stand for a series
// REJECT: cannot see inside
// REJECT: overrides a quantity that no variant or constraint of the method uses
//
// `with_constant<Length>` on a method whose only use of the length is the
// series a fit reads. The rewrite sees through the opaque call to its inputs
// (`ConstantRewrite` for `OpaqueOutputNode`, `overlay.hpp`), so the refusal is
// phase 12's for a series -- once -- and not also "cannot see inside", nor
// "nobody reads it", which would both be false.
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <tuple>

namespace
{
struct Fitted
{
};

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto fit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>), { .reference = "Example Standard 12" });

inline constexpr auto m = formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(fit))),
                                          formula::rounding_rule<formula::unit::MillimetrePerMinute,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(formula::overlay(formula::with_constant<Length>(
                                                 formula::Rational { 127, 10 }, { .reference = "Example Standard 12" })),
                                             m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}