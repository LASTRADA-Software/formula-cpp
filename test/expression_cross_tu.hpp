// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <formula-cpp/expression.hpp>

struct SampleVolume: formula::Quantity<SampleVolume, "V", "sample volume", formula::unit::Litre>
{
};
struct SampleMass: formula::Quantity<SampleMass, "m", "sample mass", formula::unit::Kilogram>
{
};

using Density = decltype(formula::var<SampleMass> / formula::var<SampleVolume>);

/// Defined in `expression_cross_tu_b.cpp`: the address of the `var` object that
/// translation unit sees, so the test can prove both units share one object
/// rather than each getting a private copy.
[[nodiscard]] void const* sample_mass_variable_address_in_other_tu() noexcept;

/// Defined in `expression_cross_tu_b.cpp`: a density formula built there.
[[nodiscard]] Density density_built_in_other_tu() noexcept;
