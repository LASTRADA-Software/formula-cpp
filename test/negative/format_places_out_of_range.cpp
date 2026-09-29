// SPDX-License-Identifier: Apache-2.0
// `{:.19HalfEven}` asks for 19 decimal places; format.hpp rounds to 0 to 18,
// the places an exact decimal of a Rational reaches. A literal format string
// is checked while compiling, with std::formatter's parse running in that
// check, and parse refuses the spec by calling a guard that is not constexpr
// -- so the compiler names it: formula_number_format_places_out_of_range.
//
// This must not compile.
#include <formula-cpp/format.hpp>

#include <format>
#include <string>

int main()
{
    std::string const written = std::format("{:.19HalfEven}", formula::Rational { 1, 3 });
    return static_cast<int>(written.size());
}
