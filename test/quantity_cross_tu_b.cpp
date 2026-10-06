// SPDX-License-Identifier: Apache-2.0
#include "quantity_cross_tu.hpp"

#include <type_traits>

namespace cross
{

std::string_view symbol_of_rise()
{
    return Rise::symbol;
}

void const* address_of_rise_dimension()
{
    return &Rise::dimension;
}

bool rise_and_run_are_distinct()
{
    return !std::is_same_v<Rise, Run>
           && !std::is_same_v<formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>,
                              formula::Quantity<Run, "h", "height gained", formula::unit::Millimetre>>;
}

} // namespace cross
