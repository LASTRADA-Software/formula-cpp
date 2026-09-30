// SPDX-License-Identifier: Apache-2.0
//
// Exact numbers and specified rounding.
//
// The scenario is deliberately generic: a flow rate from a measured volume and a
// measured duration, reported to a precision the method fixes. It shows the two
// things binary floating point cannot do -- exact unit conversion that round
// trips, and rounding that is part of the calculation rather than of the output.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

int main()
{
    using formula::DecimalPlaces;
    using formula::Rational;
    using formula::RoundingMode;
    using namespace formula::literals;

    // 450 millilitres, written exactly. 450_r is the number 450, never a double.
    Rational const volumeInMillilitres = 450_r;

    // Convert to litres by an exact integer factor: multiply, then divide.
    // 450 ml -> 9/20 l, and back to 450 ml with nothing lost.
    Rational const volumeInLitres = volumeInMillilitres / 1000;
    Rational const roundTripped = volumeInLitres * 1000;

    // `{:/}` writes a Rational as its fraction; `{}` writes the exact decimal
    // where there is one, and the fraction otherwise.
    std::println("volume = {} ml = {:/} l", volumeInMillilitres, volumeInLitres);
    std::println("round trip exact: {}", roundTripped == volumeInMillilitres ? "yes" : "no");

    // 0.4 of a minute, exactly.
    Rational const durationInMinutes = 0.4_r;
    Rational const flowRate = volumeInLitres / durationInMinutes; // 9/8 l/min, i.e. 1.125

    // The method says: report to two decimal places, rounding half away from zero.
    // That rounding is part of the method, so it happens here, not at print time.
    Rational const reported = formula::round(flowRate, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero);

    std::println("flow rate = {:/} l/min", flowRate);
    std::println("reported  = {} l/min", reported);

    // Rounding up is a separate instruction from rounding to nearest, and the
    // two disagree on exactly the values where it matters.
    Rational const roundedUp = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::Ceiling);
    Rational const roundedNearest = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero);

    std::println("one decimal, ceiling = {}", roundedUp);
    std::println("one decimal, nearest = {}", roundedNearest);
    std::println("they differ: {}", roundedUp != roundedNearest ? "yes" : "no");

    // Ten tenths are exactly one. In double arithmetic they are not.
    Rational sum {};
    for (int step = 0; step < 10; ++step)
        sum += 0.1_r;

    std::println("ten tenths == one: {}", sum == 1 ? "yes" : "no");

    // Every number printed above is checked here; nothing is printed that this
    // bool does not also cover.
    bool const conversionRoundTrips =
        volumeInMillilitres == 450 && volumeInLitres == 0.45_r && roundTripped == volumeInMillilitres;
    bool const roundingIsPartOfTheMethod = flowRate == 1.125_r && reported == 1.13_r;
    bool const theModesDisagree = roundedUp == 1.2_r && roundedNearest == 1.1_r;
    bool const tenTenthsAreOne = sum == 1;

    bool const allChecksPassed = conversionRoundTrips && roundingIsPartOfTheMethod && theModesDisagree && tenTenthsAreOne;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
