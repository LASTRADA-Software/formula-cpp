// SPDX-License-Identifier: Apache-2.0
//
// Fifty invented readings of a length against time, which
// opaque_and_retry.cpp fits a line through and test/least_squares_tests.cpp
// pins the figures of. One source for both, so that the example and the widths
// docs/opaque-and-retry.md quotes describe the same data. Not a public header
// and not installed.
#pragma once

#include <formula-cpp/rational.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace formula_examples
{

/// Fifty readings at four decimals: t = k + 1 + (7919 k mod 997) / 10^4 s and
/// L = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm, for k from 0. At
/// eight, each gains (1237 k mod 10^4) / 10^8 s and (4111 k mod 10^4) / 10^8 mm.
struct FiftyReadings
{
    /// The times t, in seconds.
    std::array<formula::Rational, 50> seconds;
    /// The lengths L, in millimetres.
    std::array<formula::Rational, 50> millimetres;
};

/// The fifty readings at four decimals when @p morePlaces is 1, and at eight
/// when it is 10'000.
[[nodiscard]] inline FiftyReadings fifty_readings(std::int64_t const morePlaces)
{
    FiftyReadings made {};
    for (std::size_t at = 0; at < 50; ++at)
    {
        auto const position = static_cast<std::int64_t>(at);
        made.seconds[at] = formula::Rational {
            (10'000 * (position + 1) + (7919 * position) % 997) * morePlaces + (1237 * position) % morePlaces,
            10'000 * morePlaces
        };
        made.millimetres[at] = formula::Rational {
            (24'100'000 + 31'700 * position + (3217 * position) % 1009 - 504) * morePlaces
                + (4111 * position) % morePlaces,
            10'000 * morePlaces
        };
    }
    return made;
}

} // namespace formula_examples
