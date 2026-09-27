// SPDX-License-Identifier: Apache-2.0
//
// The overflow census's tally: the largest magnitude of each role
// (`formula::detail::CensusRole`) seen since the last reset, as bits used.
//
// Linked only into the census programs, which are compiled with
// FORMULA_OVERFLOW_CENSUS defined. Not part of the library and not installed.
// See docs/numeric-headroom.md for what the census measures and why.
#pragma once

#include <formula-cpp/detail/checked_int.hpp>

namespace formula_census
{

/// The bits the largest magnitude of @p role used since the last `reset` --
/// 0 when none was seen. INT64_MAX uses 63; `rounded_sqrt`'s unsigned
/// intermediates may use 64.
[[nodiscard]] int bits_used(formula::detail::CensusRole role) noexcept;

/// The largest of the three signed roles' bits: what is left of 63 is the
/// headroom.
[[nodiscard]] int signed_bits_used() noexcept;

/// Forgets everything seen.
void reset() noexcept;

} // namespace formula_census
