// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 6: constraints. Each side of the specimen must measure
// 150 mm within 1 mm, and a side nobody measured is not checked.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <array>
#include <cstddef>
#include <print>
#include <string_view>

namespace
{
using formula::var;
namespace unit = formula::unit;
using namespace formula::literals;

using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;

// --8<-- [start:constraints]
constexpr auto sideANotTooShort = formula::constraint(var<SideA> >= formula::constant<unit::Millimetre>(149),
                                                      formula::Verdict { "reject the specimen: side a out of tolerance" });
constexpr auto sideANotTooLong = formula::constraint(var<SideA> <= formula::constant<unit::Millimetre>(151),
                                                     formula::Verdict { "reject the specimen: side a out of tolerance" });
constexpr auto sideBNotTooShort = formula::constraint(var<SideB> >= formula::constant<unit::Millimetre>(149),
                                                      formula::Verdict { "reject the specimen: side b out of tolerance" });
constexpr auto sideBNotTooLong = formula::constraint(var<SideB> <= formula::constant<unit::Millimetre>(151),
                                                     formula::Verdict { "reject the specimen: side b out of tolerance" });
// --8<-- [end:constraints]

// --8<-- [start:set]
constexpr auto sideTolerances = formula::constraints(sideANotTooShort, sideANotTooLong, sideBNotTooShort, sideBNotTooLong);
constexpr std::array<std::string_view, 4> ruleNames { "a >= 149 mm", "a <= 151 mm", "b >= 149 mm", "b <= 151 mm" };
// --8<-- [end:set]

// --8<-- [start:print]
// Prints one rule's outcome on a line of its own. Returns false if checking
// the rule failed, after printing why.
bool print_outcome(std::string_view rule, formula::ConstraintOutcome const& outcome)
{
    if (auto const verdict = outcome.verdict())
        std::println("  {}: {} ({})", rule, outcome.kind(), verdict->label);
    else if (auto const error = outcome.error())
    {
        std::println("  {}: {} ({})", rule, outcome.kind(), *error);
        return false;
    }
    else
        std::println("  {}: {}", rule, outcome.kind());
    return true;
}
// --8<-- [end:print]
} // namespace

int main()
{
    // --8<-- [start:specimens]
    auto const withinTolerance =
        formula::environment(formula::Measured<SideA> { 150.2_r }, formula::Measured<SideB> { 149.8_r });
    auto const sideBTooLong =
        formula::environment(formula::Measured<SideA> { 150.2_r }, formula::Measured<SideB> { 152.5_r });
    auto const sideBMissing = formula::environment(formula::Measured<SideA> { 150.2_r }, formula::Measured<SideB>::absent());
    // --8<-- [end:specimens]

    // --8<-- [start:check]
    std::println("specimen 2, one rule:");
    if (!print_outcome("b <= 151 mm", formula::check(sideBNotTooLong, sideBTooLong)))
        return 1;
    // --8<-- [end:check]

    // --8<-- [start:check-all]
    auto const checkAndPrint = [](std::string_view name, auto const& specimen) {
        std::array<formula::ConstraintOutcome, 4> const outcomes = formula::check_all(sideTolerances, specimen);
        std::println("{}:", name);
        bool checked = true;
        for (std::size_t index = 0; index < outcomes.size(); ++index)
            checked = print_outcome(ruleNames[index], outcomes[index]) && checked;
        return checked;
    };
    if (!checkAndPrint("specimen 1, 150.2 mm by 149.8 mm", withinTolerance)
        || !checkAndPrint("specimen 2, 150.2 mm by 152.5 mm", sideBTooLong)
        || !checkAndPrint("specimen 3, side b not measured", sideBMissing))
        return 1;
    // --8<-- [end:check-all]
    return 0;
}
