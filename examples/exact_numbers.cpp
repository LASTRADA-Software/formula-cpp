// SPDX-License-Identifier: Apache-2.0
//
// Exact numbers and specified rounding.
//
// The scenario is deliberately generic: a flow rate from a measured volume and a
// measured duration, reported to a precision the method fixes. It shows the two
// things binary floating point cannot do -- exact unit conversion that round
// trips, and rounding that is part of the calculation rather than of the output.

#include <formula-cpp/formula.hpp>

#include <iostream>

int main()
{
    using formula::DecimalPlaces;
    using formula::Rational;
    using formula::RoundingMode;

    // 450 millilitres, written exactly. from_decimal(45, 1) is 450, not a double.
    Rational const volumeInMillilitres = *Rational::from_decimal(45, 1);

    // Convert to litres by an exact integer factor: multiply, then divide.
    // 450 ml -> 9/20 l, and back to 450 ml with nothing lost.
    Rational const volumeInLitres = volumeInMillilitres / Rational { 1000 };
    Rational const roundTripped = volumeInLitres * Rational { 1000 };

    std::cout << "volume = " << volumeInMillilitres.numerator() << " ml"
              << " = " << volumeInLitres.numerator() << '/' << volumeInLitres.denominator() << " l\n";
    std::cout << "round trip exact: " << (roundTripped == volumeInMillilitres ? "yes" : "no") << '\n';

    // 0,4 of a minute, exactly.
    Rational const durationInMinutes { 2, 5 };
    Rational const flowRate = volumeInLitres / durationInMinutes; // 9/8 l/min, i.e. 1,125

    // The method says: report to two decimal places, rounding half away from zero.
    // That rounding is part of the method, so it happens here, not at print time.
    Rational const reported = formula::round(flowRate, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero);

    std::cout << "flow rate = " << flowRate.numerator() << '/' << flowRate.denominator() << " l/min\n";
    std::cout << "reported  = " << reported.to_double() << " l/min\n";

    // Rounding up is a separate instruction from rounding to nearest, and the
    // two disagree on exactly the values where it matters.
    Rational const roundedUp = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::Ceiling);
    Rational const roundedNearest = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero);

    std::cout << "one decimal, ceiling = " << roundedUp.to_double() << '\n';
    std::cout << "one decimal, nearest = " << roundedNearest.to_double() << '\n';
    std::cout << "they differ: " << (roundedUp != roundedNearest ? "yes" : "no") << '\n';

    // Ten tenths are exactly one. In double arithmetic they are not.
    Rational sum {};
    for (int step = 0; step < 10; ++step)
        sum += *Rational::from_decimal(1, -1);

    std::cout << "ten tenths == one: " << (sum == Rational { 1 } ? "yes" : "no") << '\n';
    return 0;
}
