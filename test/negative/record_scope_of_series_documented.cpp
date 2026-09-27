// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record was given a series, raw observations or a curve
//
// The refused series-valued read of `record_scope_of_series`, documented.
// The refused scope renders, and names nothing on the page, so `document`
// adds neither a render_node nor a collect error to the one refusal.
//
// This must not compile.
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>

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
    return formula::document(read).formula.empty() ? 1 : 0;
}
