// SPDX-License-Identifier: Apache-2.0
#include "method_cross_tu.hpp"

#include <string>
#include <type_traits>

std::string join_page_in_other_tu()
{
    return join_cross_tu::page(join_cross_tu::south);
}

std::string join_trace_in_other_tu()
{
    return join_cross_tu::trace(join_cross_tu::south);
}

std::remove_cvref_t<decltype(join_cross_tu::joined)> joined_in_other_tu()
{
    return join_cross_tu::joined;
}

formula::Evaluated<formula::Rational> evaluate_joined_in_other_tu(
    std::remove_cvref_t<decltype(join_cross_tu::joined)> const& handed)
{
    return formula::evaluate_method<join_cross_tu::Cylinder>(handed, join_cross_tu::inputs);
}
