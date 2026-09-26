// SPDX-License-Identifier: Apache-2.0
#include "series_cross_tu.hpp"

void const* series_variable_address_in_other_tu() noexcept
{
    return static_cast<void const*>(&formula::series<series_cross_tu::Retained, 3>);
}

series_cross_tu::Read series_read_in_other_tu() noexcept
{
    return formula::checked_evaluate_series<series_cross_tu::Retained>(series_cross_tu::retained, series_cross_tu::inputs);
}
