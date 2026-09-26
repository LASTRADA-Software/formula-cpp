// SPDX-License-Identifier: Apache-2.0
#include "record_cross_tu.hpp"

#include <string>

std::string record_join_page_in_other_tu()
{
    return record_cross_tu::page();
}

std::string record_join_trace_in_other_tu()
{
    return record_cross_tu::trace();
}

formula::Evaluated<formula::Rational> record_join_value_in_other_tu()
{
    return formula::evaluate_method<record_cross_tu::Cube>(record_cross_tu::overlaid, record_cross_tu::context());
}
