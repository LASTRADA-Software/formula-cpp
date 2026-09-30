// SPDX-License-Identifier: Apache-2.0
#include "regression_cross_tu.hpp"

formula::Evaluated<formula::Rational> second_coefficient_in_other_tu(decltype(regression_cross_tu::secondCoefficient)
                                                                         const& output) noexcept
{
    return formula::checked_evaluate_si(output, regression_cross_tu::sixRows);
}

std::string_view output_name_in_other_tu(std::size_t at) noexcept
{
    return formula::MultipleLeastSquares<2>::outputs[at];
}
