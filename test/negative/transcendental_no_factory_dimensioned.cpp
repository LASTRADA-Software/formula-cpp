// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the argument of this logarithm or exponential is not dimensionless
// The node built as an aggregate, without a factory, so only a check in the class body can see it.
// One mistake, one message.
// This must not compile.

#include <formula-cpp/formula.hpp>

namespace
{
struct Height: formula::Quantity<Height, "h", "an invented height", formula::unit::Metre>
{
};
} // namespace

inline constexpr auto height = formula::var<Height>;
inline constexpr formula::TranscendentalNode<formula::Transcendental::Exponential, decltype(height)> broken { {}, height };

int main()
{
    return decltype(broken)::dimension == formula::dim::Scalar ? 0 : 1;
}
