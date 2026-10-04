// SPDX-License-Identifier: Apache-2.0
//
// Defines the overflow census's hook, `formula::detail::census_record`, which
// the library declares only when FORMULA_OVERFLOW_CENSUS is defined, and the
// tally it keeps. See census_tally.hpp.
#include "census_tally.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace
{

/// The largest magnitude of each role seen since the last reset. One
/// program, one thread: the census programs evaluate on the main thread only.
std::array<formula::detail::UInt128, 4> largestSeen {};

/// The most bits a wide intermediate used since the last reset.
std::size_t widestWideBits = 0;

[[nodiscard]] std::size_t slot(formula::detail::CensusRole role) noexcept
{
    return static_cast<std::size_t>(role);
}

} // namespace

namespace formula::detail
{

void census_record(CensusRole role, UInt128 magnitudeSeen) noexcept
{
    if (largestSeen[slot(role)] < magnitudeSeen)
        largestSeen[slot(role)] = magnitudeSeen;
}

void census_record_width(CensusRole role, std::size_t bitsUsed) noexcept
{
    if (role == CensusRole::Wide && widestWideBits < bitsUsed)
        widestWideBits = bitsUsed;
}

} // namespace formula::detail

namespace formula_census
{

int bits_used(formula::detail::CensusRole role) noexcept
{
    if (role == formula::detail::CensusRole::Wide)
        return static_cast<int>(widestWideBits);
    return largestSeen[slot(role)].bit_width();
}

int signed_bits_used() noexcept
{
    using formula::detail::CensusRole;
    // `Rational::Int`'s minimum, -2^127, uses every bit there is, and no more.
    return std::min(
        127,
        std::max(
            { bits_used(CensusRole::Numerator), bits_used(CensusRole::Denominator), bits_used(CensusRole::Intermediate) }));
}

void reset() noexcept
{
    largestSeen.fill(formula::detail::UInt128 {});
    widestWideBits = 0;
}

} // namespace formula_census
