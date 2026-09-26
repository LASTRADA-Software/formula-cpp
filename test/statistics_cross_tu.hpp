// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One statistic formula and one environment, `inline constexpr` in a header
/// and used from two translation units: `statistics_tests.cpp` and
/// `statistics_cross_tu_b.cpp`. The other unit's function declaration below
/// names the formula's own result type, so a declaration whose types differ
/// between the two units declares a different function in each and the test
/// fails to link. Everything shared lives in a named namespace, never an
/// anonymous one -- see `series_cross_tu.hpp`.

#include <formula-cpp/statistics.hpp>

namespace statistics_cross_tu
{
struct Specimen: formula::Quantity<Specimen, "m_s", "mass of a specimen", formula::unit::Gram>
{
};

/// The mean of three invented specimens, 13, 21 and 35 g: 23 g.
inline constexpr auto meanOfSpecimens = formula::sample_mean(formula::series<Specimen, 3>);

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Specimen>(formula::Measured<Specimen> { formula::Rational { 13 } },
                                                            formula::Measured<Specimen> { formula::Rational { 21 } },
                                                            formula::Measured<Specimen> { formula::Rational { 35 } }));

using Read = decltype(formula::checked_evaluate<Specimen>(meanOfSpecimens, inputs));
} // namespace statistics_cross_tu

/// Defined in `statistics_cross_tu_b.cpp`: the mean as that translation unit
/// evaluates it.
statistics_cross_tu::Read statistics_mean_in_other_tu() noexcept;
