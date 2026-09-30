// SPDX-License-Identifier: Apache-2.0
//
// Prints the overflow census's tally when a census build of an example, or of
// the gallery generator, exits: one line, after everything the program
// printed itself. See docs/numeric-headroom.md.
#include "census_tally.hpp"

#include <cstdio>
#include <print>

namespace
{

/// Prints on destruction, at exit, once every formula the program evaluated
/// has been told to the tally.
struct ReportAtExit
{
    ReportAtExit() = default;
    ReportAtExit(ReportAtExit const&) = delete;
    ReportAtExit& operator=(ReportAtExit const&) = delete;

    ~ReportAtExit()
    {
        using formula::detail::CensusRole;
        std::println("overflow census: numerator {} bits, denominator {} bits, intermediate {} bits, unsigned {} "
                     "bits; headroom {} of 63",
                     formula_census::bits_used(CensusRole::Numerator),
                     formula_census::bits_used(CensusRole::Denominator),
                     formula_census::bits_used(CensusRole::Intermediate),
                     formula_census::bits_used(CensusRole::Unsigned),
                     63 - formula_census::signed_bits_used());
        std::fflush(stdout);
    }
};

ReportAtExit const reportAtExit;

} // namespace
