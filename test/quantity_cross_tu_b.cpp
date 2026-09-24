// SPDX-License-Identifier: Apache-2.0
#include "quantity_cross_tu.hpp"

#include <type_traits>

namespace cross
{

std::string_view symbol_of_water_volume()
{
    return WaterVolume::symbol;
}

void const* address_of_water_volume_dimension()
{
    return &WaterVolume::dimension;
}

bool water_and_cement_are_distinct()
{
    return !std::is_same_v<WaterVolume, CementVolume>
           && !std::is_same_v<formula::Quantity<WaterVolume, "V_w", "volume of water added", formula::unit::Litre>,
                              formula::Quantity<CementVolume, "V_w", "volume of water added", formula::unit::Litre>>;
}

} // namespace cross
