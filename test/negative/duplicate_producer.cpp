// SPDX-License-Identifier: Apache-2.0
// MUST NOT COMPILE: two functors produce the same type, so the evaluator cannot
// decide which one derives it.
#include <formula-cpp/formula.hpp>

namespace
{
struct Alpha
{
    int value {};
};
struct Beta
{
    int value {};
};
} // namespace

using Broken =
    formula::Evaluation<formula::EvaluationArguments<Alpha, Beta>,
                        formula::EvaluationFunctors {
                            [](auto const& ctx) -> Beta { return Beta { formula::get<Alpha>(ctx).value }; },
                            [](auto const& ctx) -> Beta { return Beta { formula::get<Alpha>(ctx).value + 1 }; } }>;

int main()
{
    return Broken().set(Alpha { 1 }).calculate(Beta {}).value;
}
