// SPDX-License-Identifier: Apache-2.0
// EXPECT: StepLimit
// REJECT: no matching
// REJECT: cannot convert argument
//
// render_derivation(explained, {}) must not compile: it takes the options
// render_trace takes, whose maxSteps is a StepLimit, a type with no default
// constructor, so there is no zero for `{}` to produce and a derivation is
// never rendered to nothing, silently. See StepLimit's own comment in
// trace_render.hpp.
//
// Spelled with the options named, as render_trace_no_step_limit spells it
// and for its reason: cl reports the inline `render_derivation(explained,
// {})` as a failed overload resolution naming TraceRenderOptions, not
// StepLimit, where every compiler reports the named form as the deleted
// StepLimit constructor. The call that follows is refused as nothing else
// only while render_derivation takes those options. Not counted: the one
// message names StepLimit more than once -- the type, its constructor and
// the line declaring it.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
} // namespace

int main()
{
    constexpr auto shares = formula::calculation(formula::define<Share>(var<Factor> * var<Other>));
    auto sheet = formula::worksheet(shares,
                                    formula::environment(formula::Measured<Factor> { formula::Rational { 3 } },
                                                         formula::Measured<Other> { formula::Rational { 2 } }));
    auto const explained = formula::explain_worksheet<Share>(sheet);
    formula::TraceRenderOptions const options {};
    return static_cast<int>(formula::render_derivation(explained, options).size());
}
