// SPDX-License-Identifier: Apache-2.0
#include "record_cross_tu.hpp"

#include <type_traits>

formula::Evaluated<formula::Rational>
    record_join_evaluate_in_other_tu(std::remove_cvref_t<decltype(record_cross_tu::overlaid)> const& method)
{
    return formula::evaluate_method<record_cross_tu::Cube>(method, record_cross_tu::context());
}
