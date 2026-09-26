// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One series formula and one environment, `inline constexpr` in a header and
/// used from two translation units: `series_tests.cpp` and
/// `series_cross_tu_b.cpp`. `series<Q, N>` is an `inline constexpr` variable
/// template, as `var<Q>` is, and the guarantee that the program holds one
/// object per specialisation is what `expression_cross_tu` checks for `var`.
/// The other unit's function declarations below name the formula's own types,
/// so a declaration whose types differ between the two units declares a
/// different function in each and the test fails to link.
///
/// Everything shared lives in a named namespace, never an anonymous one: an
/// anonymous namespace in a header is a different namespace in every unit
/// that includes it.

#include <formula-cpp/series.hpp>

#include <cstddef>
#include <expected>

namespace series_cross_tu
{
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto retained = formula::series<Retained, 3>;

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 13 } },
                                                            formula::Measured<Retained> { formula::Rational { 21 } },
                                                            formula::Measured<Retained> { formula::Rational { 34 } }));

using Read = std::expected<formula::SeriesOutcome<Retained, 3>, formula::SeriesFailure>;
} // namespace series_cross_tu

/// Defined in `series_cross_tu_b.cpp`: the address of the `series` object
/// that translation unit sees, so the test can show both units share one.
[[nodiscard]] void const* series_variable_address_in_other_tu() noexcept;

/// Defined in `series_cross_tu_b.cpp`: the shared formula evaluated there.
[[nodiscard]] series_cross_tu::Read series_read_in_other_tu() noexcept;
