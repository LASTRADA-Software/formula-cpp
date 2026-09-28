// SPDX-License-Identifier: Apache-2.0
//
// Defines the overflow census's hook, `formula::detail::census_record`, which
// the library declares only when FORMULA_OVERFLOW_CENSUS is defined, and the
// tally it keeps. See census_tally.hpp.
#include "census_tally.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace
{

/// The largest magnitude of each role seen since the last reset. One
/// program, one thread: the census programs evaluate on the main thread only.
std::array<std::uint64_t, 4> largestSeen {};

[[nodiscard]] std::size_t slot(formula::detail::CensusRole role) noexcept
{
    return static_cast<std::size_t>(role);
}

} // namespace

namespace formula::detail
{

void census_record(CensusRole role, std::uint64_t magnitudeSeen) noexcept
{
    largestSeen[slot(role)] = std::max(largestSeen[slot(role)], magnitudeSeen);
}

} // namespace formula::detail

namespace formula_census
{

int bits_used(formula::detail::CensusRole role) noexcept
{
    return static_cast<int>(std::bit_width(largestSeen[slot(role)]));
}

int signed_bits_used() noexcept
{
    using formula::detail::CensusRole;
    // IntMin's magnitude, 2^63, is representable as a numerator: it uses
    // every bit there is, and no more.
    return std::min(
        63,
        std::max(
            { bits_used(CensusRole::Numerator), bits_used(CensusRole::Denominator), bits_used(CensusRole::Intermediate) }));
}

void reset() noexcept
{
    largestSeen.fill(0);
}

} // namespace formula_census
