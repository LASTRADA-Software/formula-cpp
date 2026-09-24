// SPDX-License-Identifier: Apache-2.0
// A Describe specialisation whose declared dimension disagrees with its own
// unit's must be refused wherever Measured consumes it -- the same
// contradiction Ruling A makes unwritable on the CRTP path (Quantity::dimension
// is derived from its unit, never stated beside it), closed here on the
// foreign path, where a specialisation states both independently. This must
// not compile.
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>

struct ForeignThing
{
    double value {};
};

template <>
struct formula::Describe<ForeignThing>
{
    static constexpr std::string_view symbol = "x";
    static constexpr std::string_view description = "declares litres, but claims to be a mass";
    static constexpr formula::Unit unit = formula::unit::Litre;      // a volume
    static constexpr formula::Dimension dimension = formula::dim::Mass; // ... but says Mass
};

int main()
{
    formula::Measured<ForeignThing> const value { *formula::Rational::make(1, 1) };
    (void) value;
    return 0;
}
