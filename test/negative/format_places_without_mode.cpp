// SPDX-License-Identifier: Apache-2.0
// `{:.2}` names two decimal places and no rounding mode, and format.hpp
// assumes none: the same number rounds differently under different methods.
// A literal format string is checked while compiling, with std::formatter's
// parse running in that check, and parse refuses the spec by calling a guard
// that is not constexpr -- so the compiler names it:
// formula_number_format_needs_a_rounding_mode.
//
// This must not compile.
#include <formula-cpp/format.hpp>

#include <format>
#include <string>

int main()
{
    std::string const written = std::format("{:.2}", formula::Rational { 1, 3 });
    return static_cast<int>(written.size());
}
