// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: TagName<Tag>::of() is not usable in a constant expression
//
// A TagName specialisation whose `of` returns a `std::string` by value. It
// converts to `std::string_view`, so it has the right shape, and it is
// `constexpr` -- but the view the library would keep points into a string
// destroyed the moment `of` returns. A trace keeps that view for as long as
// the trace lives (`Step::variantTag`), so this is exactly the dangling name
// the static-storage rule on `TagName` exists to refuse. The library reads
// every character of the view in a constant expression, which a destroyed
// string's characters are not. This must not compile.
//
// Reached through recording a trace, which is where the view is kept.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

#include <string>

namespace
{
struct DanglingCylinder;

struct DanglingLoad: formula::Quantity<DanglingLoad, "F", "applied load", formula::unit::Megapascal>
{
};
} // namespace

template <>
struct formula::TagName<DanglingCylinder>
{
    static constexpr std::string of()
    {
        return "cylinder 139 x 277 mm";
    }
};

int main()
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<DanglingCylinder>(formula::var<DanglingLoad>)),
        formula::rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 },
                               formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::evaluate_method<DanglingCylinder>(
        m, formula::environment(formula::Measured<DanglingLoad> { formula::Rational { 1 } }), sink);
    return static_cast<int>(trace.steps.size());
}
