// SPDX-License-Identifier: Apache-2.0
#include "opaque_cross_tu.hpp"

std::expected<formula::Outcome<opaque_cross_tu::Reading>, formula::ArithmeticError> highest_in_other_tu(
    decltype(opaque_cross_tu::highest) const& highestOutput) noexcept
{
    return formula::checked_evaluate<opaque_cross_tu::Reading>(highestOutput, opaque_cross_tu::inputs);
}
