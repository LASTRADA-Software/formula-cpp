// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <iostream>

// Named tag types, NOT `decltype([]{})`. A lambda in a default template argument
// gives the closure internal linkage, so the quantity type differs in every
// translation unit -- verified to fail at link time on cl, clang-cl and clang++.
template <typename T, typename Tag>
struct Quantity
{
    T value {};
};

struct FirstTag;
struct SecondTag;
struct ThirdTag;

using First = Quantity<int, FirstTag>;
using Second = Quantity<int, SecondTag>;
using Third = Quantity<int, ThirdTag>;

using Calculation =
    formula::Evaluation<formula::EvaluationArguments<First, Second, Third>,
                        formula::EvaluationFunctors {
                            [](auto const& ctx) -> Second { return Second { formula::get<First>(ctx).value + 1 }; },
                            [](auto const& ctx) -> Third {
                                return Third { formula::get<First>(ctx).value + formula::get<Second>(ctx).value };
                            } }>;

int main()
{
    auto const result = Calculation().set(First { 1 }).calculate(Third {});
    std::cout << "Third = " << result.value << '\n'; // prints 3
    return 0;
}
