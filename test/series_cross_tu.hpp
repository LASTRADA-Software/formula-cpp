// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One series formula and one environment, `inline constexpr` in a header and
/// used from two translation units: `series_tests.cpp` and
/// `series_cross_tu_b.cpp`. `series<Q, N>` is an `inline constexpr` variable
/// template, as `var<Q>` is, and the guarantee that the program holds one
/// object per specialisation is what `expression_cross_tu` checks for `var`.
/// The other unit's function declarations below name the formula's own types,
/// so a formula whose node structure differs between the two units -- a
/// different node, length or operand order -- declares a different function
/// in each and the test fails to link. Only the structure: a quantity whose
/// *definition* differs between the units while its name stays the same (its
/// symbol, say, or its unit) is the same type by name in both, and links
/// silently -- an ODR violation no linker is required to see.
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

/// A series expression whose type nests several node templates, taken as a
/// parameter by the other unit's function below.
inline constexpr auto totals = formula::cumulative<formula::CumulativeDirection::FromLast>(retained * formula::Rational { 2 });

using Read = std::expected<formula::SeriesOutcome<Retained, 3>, formula::SeriesFailure>;
} // namespace series_cross_tu

/// Defined in `series_cross_tu_b.cpp`: the address of the `series` object
/// that translation unit sees, so the test can show both units share one.
[[nodiscard]] void const* series_variable_address_in_other_tu() noexcept;

/// Defined in `series_cross_tu_b.cpp`: the shared formula evaluated there.
[[nodiscard]] series_cross_tu::Read series_read_in_other_tu() noexcept;

/// Defined in `series_cross_tu_b.cpp`: @p totalsExpression evaluated there.
///
/// The formula is a **parameter**, not only a return value: a parameter's
/// type is part of the function's mangled name on every compiler, so a
/// formula whose type differs between the two units names a different
/// function and fails the link -- where a return type alone would do so on
/// cl only.
[[nodiscard]] series_cross_tu::Read totals_read_in_other_tu(decltype(series_cross_tu::totals) const& totalsExpression) noexcept;
