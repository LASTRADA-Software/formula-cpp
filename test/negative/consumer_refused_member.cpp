// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this addition or subtraction measure different dimensions
// REJECT: no matching
//
// A consumer's node kind of length that declares `refused = true` for a
// meaning of its own, added to a mass: refused as any node kind would be.
// Only this library's node kinds can claim to be refused already, and so
// silence the checks around them; this one, once, gave 103 g + 127 mm =
// "230 g".
#include <formula-cpp/formula.hpp>

#include <optional>

namespace
{
namespace unit = formula::unit;

struct Mass: formula::Quantity<Mass, "m", "an invented mass", unit::Gram>
{
};
} // namespace

namespace consumer
{
// A fixed length of 127 mm, which the consumer's own quality check flagged.
struct FlaggedLength: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Length;
    static constexpr bool refused = true;
};

template <typename Rep, typename Env>
constexpr formula::Evaluated<Rep> checked_evaluate_si(FlaggedLength const&, Env const&) noexcept
{
    return formula::Evaluated<Rep> { std::optional<Rep> { Rep { formula::Rational { 127, 1000 } } } };
}
} // namespace consumer

int main()
{
    constexpr auto sum = formula::var<Mass> + consumer::FlaggedLength {};
    return sizeof(sum) > 0 ? 0 : 1;
}