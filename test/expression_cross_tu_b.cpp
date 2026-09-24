// SPDX-License-Identifier: Apache-2.0
#include "expression_cross_tu.hpp"

void const* sample_mass_variable_address_in_other_tu() noexcept
{
    return static_cast<void const*>(&formula::var<SampleMass>);
}

Density density_built_in_other_tu() noexcept
{
    return formula::var<SampleMass> / formula::var<SampleVolume>;
}
