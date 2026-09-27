// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression
//
// An EnumeratorName specialisation whose `of` returns a `std::string` by
// value. It converts to `std::string_view`, so it has the right shape, and
// it is `constexpr` -- but the view the library would keep points into a
// string destroyed the moment `of` returns. A trace keeps that view for as
// long as the trace lives (`Step::lookupKeyName`), so this is exactly the
// dangling name the static-storage rule on `EnumeratorName` exists to
// refuse. The library reads every character of the view in a constant
// expression, which a destroyed string's characters are not. This must not
// compile.
//
// Reached through recording a trace, which is where the view is kept.
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/trace.hpp>

#include <string>

namespace
{
enum class DanglingShape
{
    Cube,
};

inline constexpr formula::KeyTable<DanglingShape, 1> DanglingKeys { DanglingShape::Cube };
} // namespace

template <>
struct formula::EnumeratorName<DanglingShape>
{
    static constexpr std::string of(DanglingShape)
    {
        return "cube 139 mm";
    }
};

int main()
{
    constexpr auto node =
        formula::exact_lookup<DanglingKeys, formula::unit::One>(DanglingShape::Cube, { formula::Rational { 1127, 1000 } });
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);
    return static_cast<int>(trace.steps.size());
}
