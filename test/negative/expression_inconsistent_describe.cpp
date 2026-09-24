// SPDX-License-Identifier: Apache-2.0
// EXPECT: describes a dimension its own unit does not measure
#include <formula-cpp/expression.hpp>

struct Muddle
{
};

// A hand-written Describe whose declared dimension contradicts its unit: the
// unit measures a length, the dimension claims a mass.
template <>
struct formula::Describe<Muddle>
{
    static constexpr std::string_view symbol = "x";
    static constexpr std::string_view description = "a muddled quantity";
    static constexpr formula::Unit unit = formula::unit::Metre;
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};

inline constexpr auto broken = formula::var<Muddle>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.mass.numerator);
}
