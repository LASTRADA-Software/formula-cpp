// SPDX-License-Identifier: Apache-2.0
// Two quantities alike in symbol, description and unit are still different
// types, so one must not be usable where the other is expected. That is the
// whole reason for the CRTP tag. This must not compile.
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V", "a volume", formula::unit::Litre>
{
};

struct CementVolume: formula::Quantity<CementVolume, "V", "a volume", formula::unit::Litre>
{
};

void takes_water(WaterVolume);

int main()
{
    takes_water(CementVolume {});
    return 0;
}
