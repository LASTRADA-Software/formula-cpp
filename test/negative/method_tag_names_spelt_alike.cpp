// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: two variants of this method are spelt the same in a trace
//
// `TagName<Cube>` spelling `Cylinder` beside a real `Cylinder` variant. The
// tags are distinct types, so the distinct-tag rule accepts them, but a trace
// line naming the variant that ran would read `Cylinder` for either. Refused
// where the names are shown, the first time the method is evaluated,
// whichever tag is asked for -- here `Cube`.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>

#include <string_view>

namespace
{
struct Cube
{
};
struct Cylinder
{
};
} // namespace

template <>
struct formula::TagName<Cube>
{
    static constexpr std::string_view of() noexcept
    {
        return "Cylinder";
    }
};

namespace
{
struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 139 } });
} // namespace

int main()
{
    return formula::evaluate_method<Cube>(m, inputs).has_value() ? 0 : 1;
}
