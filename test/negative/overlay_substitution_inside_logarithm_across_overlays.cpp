// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it
// REJECT: cannot see inside
//
// The first overlay fixes the ratio, which the method reads only inside a
// logarithm. The second replaces the other variant with one that reads the
// ratio plainly. The whole-method rule must see the substitution inside the
// logarithm (`SubstitutedIn` for `TranscendentalNode`): without it, one
// method would read the ratio at 103/100 in one variant and from the
// environment in the other.
//
// This must not compile.
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <tuple>

namespace
{
struct Logged
{
};
struct Plain
{
};

struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};

constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };

inline constexpr auto m = formula::method(
    formula::variants(formula::variant<Logged>(formula::ln(formula::var<Ratio>)),
                      formula::variant<Plain>(formula::constant<formula::unit::One>(formula::Rational { 1 }))),
    formula::rounding_rule<formula::unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto first =
        formula::apply(formula::overlay(formula::with_constant<Ratio>(formula::Rational { 103, 100 }, annex)), m);
    constexpr auto second =
        formula::apply(formula::overlay(formula::replace_variant<Plain>(formula::var<Ratio>, annex)), first);
    return std::tuple_size_v<decltype(second.variantSet.cases)> == 0 ? 1 : 0;
}
