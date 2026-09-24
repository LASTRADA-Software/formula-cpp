// SPDX-License-Identifier: Apache-2.0
// render_trace(trace, {}) must not compile. TraceRenderOptions::maxSteps is a
// StepLimit, not a plain std::size_t, precisely so that this fails: a
// std::size_t member with no default initialiser would still let `{}`
// value-initialise TraceRenderOptions to a zero-step bound and render nothing
// at all, silently -- StepLimit has no default constructor, so there is no
// zero for `{}` to produce. See StepLimit's own comment in trace_render.hpp.
//
// Spelled as a named `TraceRenderOptions const options {};` rather than the
// inline `render_trace(trace, {})` the docs show, not the direct call: both
// are ill-formed the same way, but cl and clang-cl report the inline form as
// a failed overload-resolution conversion, in wording that names neither
// StepLimit nor TraceRenderOptions in a way common to every compiler this
// negative test must match across. Naming the variable makes every compiler
// -- cl, clang-cl and g++ alike -- report the same underlying cause: a
// deleted `StepLimit` default constructor.
//
// This must not compile.
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

int main()
{
    formula::Trace<> const trace {};
    formula::TraceRenderOptions const options {};
    return static_cast<int>(formula::render_trace(trace, options).size());
}
