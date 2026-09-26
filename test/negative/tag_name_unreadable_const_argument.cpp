// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// A template specialization over a const-qualified type used as a method's
// tag: `Box<const specimen::Cube>`. clang and GCC print `Box<const
// specimen::Cube>`, and stripping the qualifier cuts the `const ` with it,
// leaving `Box<Cube>` -- a plainly readable name, and the name of a DIFFERENT
// type. cl prints `Box<struct specimen::Cube const >`, so the compilers would
// disagree as well. The one defect here that no character rule sees, which is
// why the normalizer reports whether a `const` or `volatile` qualified a part
// of the type the name shows. This must not compile.
//
// Reached through evaluate_method, the way an author meets it.
#include <formula-cpp/formula.hpp>

namespace
{
namespace specimen
{
    struct Cube;
} // namespace specimen

template <typename T>
struct Box;

struct ConstLoad: formula::Quantity<ConstLoad, "F", "applied load", formula::unit::Megapascal>
{
};
} // namespace

int main()
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Box<specimen::Cube const>>(formula::var<ConstLoad>)),
        formula::rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 },
                               formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    auto const result = formula::evaluate_method<Box<specimen::Cube const>>(
        m, formula::environment(formula::Measured<ConstLoad> { formula::Rational { 1 } }));
    return result.has_value() ? 0 : 1;
}
