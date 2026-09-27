// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record was given a series, raw observations or a curve
//
// The refused series-valued read of `record_scope_of_series`, rendered. The
// refused scope renders as a refused series does, so `render` adds no
// "no matching function for call to render_node" to the one refusal.
//
// This must not compile.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

namespace
{
struct Reference
{
};
struct Cube
{
};

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Factor: formula::Quantity<Factor, "k", "correction factor", formula::unit::One>
{
};

using formula::var;

inline constexpr auto read = var<Factor> * formula::sum(formula::series<Retained, 2>)
                             / formula::from_record<Reference>(formula::series<Retained, 2>);
} // namespace

int main()
{
    return formula::render(read).empty() ? 1 : 0;
}
