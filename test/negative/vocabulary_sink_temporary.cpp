// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a RecordingSink keeps a pointer to its vocabulary
#include <formula-cpp/trace.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A vocabulary that dies at the end of the statement constructing the sink.
int main()
{
    formula::Trace<> trace {};
    formula::RecordingSink sink { trace, formula::vocabulary(formula::renames<Strength>("R")) };
    (void) sink;
    return 0;
}
