// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two quantities measure different dimensions, so no conversion between them exists
//
// A measured volume converted into a mass. The dimensions are known where the
// call is written, so this is refused when it is compiled, not returned as a
// DomainError at run time. This must not compile, and draws one message.
#include <formula-cpp/measured.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", formula::unit::Litre>
{
};
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "mass of the specimen", formula::unit::Kilogram>
{
};

int main()
{
    return formula::checked_convert_to<SpecimenMass>(formula::Measured<WaterVolume> { 450 }).has_value() ? 1 : 0;
}
