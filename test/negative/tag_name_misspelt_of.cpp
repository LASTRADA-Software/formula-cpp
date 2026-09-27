// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this TagName specialisation does not have the shape the library reads
//
// A TagName specialisation whose `of` is misspelt `Of`. It is plainly meant
// as a customization -- the author wrote the specialisation -- and the silent
// reading of it, "no `of`, so not customized", would record the variant under
// its reflected name `MisspeltCylinder` while the author believes the trace
// reads "cylinder 139 x 277 mm". Nothing would say the wording had been
// dropped. So it must be refused in the library's own words. This must not
// compile.
//
// Reached through evaluate_method, the way an author meets it: evaluating a
// method with this tag names the tag, traced or not, and the gate fires there.
#include <formula-cpp/formula.hpp>

#include <string_view>

namespace
{
struct MisspeltCylinder;

struct MisspeltLoad: formula::Quantity<MisspeltLoad, "F", "applied load", formula::unit::Megapascal>
{
};
} // namespace

template <>
struct formula::TagName<MisspeltCylinder>
{
    static constexpr std::string_view Of() noexcept
    {
        return "cylinder 139 x 277 mm";
    }
};

int main()
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<MisspeltCylinder>(formula::var<MisspeltLoad>)),
        formula::rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 },
                               formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    auto const result = formula::evaluate_method<MisspeltCylinder>(
        m, formula::environment(formula::Measured<MisspeltLoad> { formula::Rational { 1 } }));
    return result.has_value() ? 0 : 1;
}
