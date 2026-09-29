// SPDX-License-Identifier: Apache-2.0
// render_trace's options with a number style and no step limit must not
// compile. TraceRenderOptions::numbers has a default member initialiser,
// fractions, and TraceRenderOptions::maxSteps still has none: designating
// only `.numbers` leaves `maxSteps` to be initialised from `{}`, and
// StepLimit's deleted default constructor refuses exactly that, as it refuses
// `TraceRenderOptions {}` (render_trace_no_step_limit.cpp). A caller who asks
// for decimals has still to say how many steps to show.
//
// Spelled as a named `TraceRenderOptions const options { ... };`, as that
// case is, for its reason: every compiler then reports the deleted
// `StepLimit` default constructor, by name.
//
// This must not compile.
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

int main()
{
    formula::Trace<> const trace {};
    formula::TraceRenderOptions const options { .numbers = formula::NumberStyle::exact_decimal() };
    return static_cast<int>(formula::render_trace(trace, options).size());
}
