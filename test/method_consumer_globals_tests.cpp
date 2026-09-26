// SPDX-License-Identifier: Apache-2.0
//
// A consumer's translation unit with globals named the way this library's own
// locals once were. cl at /W4 reports a local in a header's template that
// hides a global as C4459 -- inside the header, in the consumer's build -- so
// under /WX a consumer's ordinary global broke the build of a library they
// only included. Measured on cl 19.51 with this file against the old
// names: `selected`, `expression`, `selectedTag` and `selection` in
// `evaluate_method`, and `outcomes` in `check_method`. This translation unit
// compiles with warnings as errors, like every test here, so on cl it fails
// to build if one of those locals comes back. g++ 13.3 and clang++ 20 at
// -Wshadow build this file cleanly against the old names too, so the check
// is cl's alone.
//
// Only these names: `result`, for one, is still a local in `evaluate.hpp`,
// `rounding_node.hpp`, `constraint.hpp` and `detail/type_name.hpp`, and a
// global of that name here would be reported there.
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

int selected = 0;
int expression = 0;
int selectedTag = 0;
int selection = 0;
int outcomes = 0;

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};

inline constexpr auto oneVariant = formula::method(
    formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 1 }),
                                             formula::Verdict { "no load" })));

inline constexpr auto specimen = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                      formula::Measured<EdgeX> { formula::Rational { 150 } });

/// A sink that asks for both pairs of method hooks, so that the branches of
/// `evaluate_method` and `check_method` that declare locals for a sink are
/// instantiated too.
struct MethodHooks: formula::NullSink
{
    std::string* events;

    void variant_entered(formula::VariantSelection const&) const
    {
        *events += "v";
    }
    void variant_produced(formula::VariantSelection const&, formula::Evaluated<formula::Rational> const&) const
    {
        *events += "V";
    }
    void acceptance_entered(formula::ConstraintOrigin const&) const
    {
        *events += "a";
    }
    void acceptance_produced(formula::ConstraintOrigin const&) const
    {
        *events += "A";
    }
};
} // namespace

TEST_CASE("a consumer's globals named like the method's locals do not break its build", "[method]")
{
    std::string events;
    auto const strength = formula::evaluate_method<Cube>(oneVariant, specimen, MethodHooks { {}, &events });
    auto const checked = formula::check_method(oneVariant, specimen, MethodHooks { {}, &events });

    // 90000 N over 150 mm squared is 4 MPa.
    CHECK(strength->value() == formula::Rational { 4'000'000 });
    CHECK(checked[0].is_satisfied());
    CHECK(events == "vVaA");
    CHECK(selected + expression + selectedTag + selection + outcomes == 0);
}
