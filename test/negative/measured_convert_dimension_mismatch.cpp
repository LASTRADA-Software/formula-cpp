// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two quantities measure different dimensions, so no conversion between them exists
//
// A measured height converted into a mass. The dimensions are known where the
// call is written, so this is refused when it is compiled, not returned as a
// DomainError at run time. This must not compile, and draws one message.
#include <formula-cpp/measured.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", formula::unit::Kilogram>
{
};

int main()
{
    return formula::checked_convert_to<SpecimenMass>(formula::Measured<Rise> { 450 }).has_value() ? 1 : 0;
}
