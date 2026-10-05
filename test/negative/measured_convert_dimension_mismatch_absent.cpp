// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two quantities measure different dimensions, so no conversion between them exists
//
// The same mistake with no value present. An absent measurement must not make
// a conversion nobody could perform look like it succeeded, so it is refused
// exactly as when a value is there. This must not compile, and draws one message.
#include <formula-cpp/measured.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", formula::unit::Kilogram>
{
};

int main()
{
    return formula::checked_convert_to<SpecimenMass>(formula::Measured<Rise> {}).has_value() ? 1 : 0;
}
