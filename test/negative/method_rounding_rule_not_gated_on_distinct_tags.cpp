// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report
//
// A method whose two variants both report a pressure and both declare Cube,
// given a rounding rule that rounds in millimetres. Two mistakes, independent
// of each other.
//
// The rounding rule is gated on the variants AGREEING -- see
// `canAskRoundingRule` -- and these agree, so there is a dimension to compare
// and the rule must still report. A gate on distinct tags as well would leave
// only the repeated tag's message, and the author would learn about the
// rounding rule one build later. The repeated tag's own message is not what
// this case pins; `method_duplicate_tag.cpp` does that.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Millimetre,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(m.variantSet.cases)>);
}
