// SPDX-License-Identifier: Apache-2.0
// An Int128 is written as its decimal digits; any spec but the empty one is
// refused where the format string is written.
#include <formula-cpp/format.hpp>

#include <format>
#include <string>

std::string probe()
{
    return std::format("{:x}", formula::Int128 { 255 });
}
