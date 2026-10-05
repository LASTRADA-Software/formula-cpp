// SPDX-License-Identifier: Apache-2.0
#include "quantity_alias_cross_tu.hpp"

#include <type_traits>

namespace cross_alias
{

std::string_view symbol_of(Rise)
{
    return Rise::symbol;
}

void const* address_of_rise_dimension()
{
    return &Rise::dimension;
}

bool rise_and_run_are_distinct()
{
    return !std::is_same_v<Rise, Run> && std::is_same_v<Rise::QuantityTag, RiseTag>
           && std::is_same_v<Run::QuantityTag, RunTag>;
}

} // namespace cross_alias
