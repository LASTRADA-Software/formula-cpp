// SPDX-License-Identifier: Apache-2.0
#include "quantity_alias_cross_tu.hpp"

#include <type_traits>

namespace cross_alias
{

std::string_view symbol_of(WaterVolume)
{
    return WaterVolume::symbol;
}

void const* address_of_water_volume_dimension()
{
    return &WaterVolume::dimension;
}

bool water_and_cement_are_distinct()
{
    return !std::is_same_v<WaterVolume, CementVolume> && std::is_same_v<WaterVolume::QuantityTag, WaterVolumeTag>
           && std::is_same_v<CementVolume::QuantityTag, CementVolumeTag>;
}

} // namespace cross_alias
