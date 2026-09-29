// SPDX-License-Identifier: Apache-2.0
// `{:x}` is no form format.hpp's grammar allows: a Rational has no
// hexadecimal spelling. A literal format string is checked while compiling,
// with std::formatter's parse running in that check, and parse refuses the
// spec by calling a guard that is not constexpr -- so the compiler names it:
// formula_number_format_spec_not_understood.
//
// This must not compile.
#include <formula-cpp/format.hpp>

#include <format>
#include <string>

int main()
{
    std::string const written = std::format("{:x}", formula::Rational { 1, 3 });
    return static_cast<int>(written.size());
}
