// SPDX-License-Identifier: Apache-2.0
// Isolates the widened `Described` concept from the dimension-consistency
// guard that sits beside it.
//
// `describe_partially_specified.cpp` omits both `description` and `dimension`,
// so it trips either guard and cannot tell you which. This one names symbol,
// unit AND a dimension that agrees with the unit -- so the consistency guard is
// satisfied -- and omits only `description`. The one thing left to catch it is
// the concept requiring all four members rather than two, which is exactly what
// the final review found missing. This must not compile.
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

struct MissingDescription
{
};

template <>
struct formula::Describe<MissingDescription>
{
    static constexpr std::string_view symbol = "d";
    static constexpr formula::Unit unit = formula::unit::Litre;
    static constexpr formula::Dimension dimension = formula::unit::Litre.dimension; // consistent
    // description deliberately omitted, and nothing else is.
};

int main()
{
    return formula::RequireDescribed<MissingDescription>::value ? 1 : 0;
}
