// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <cstdio>

// Proves cxx_std_23 propagated from the exported target: `if consteval` is C++23.
constexpr int probe(int value) noexcept
{
    if consteval
    {
        return value + 1;
    }
    else
    {
        return value;
    }
}

static_assert(probe(1) == 2, "C++23 did not propagate from formula-cpp::formula-cpp");

int main()
{
    // Deliberately includes the umbrella header and uses a symbol from each
    // layer it pulls in (error.hpp, rational.hpp, rounding.hpp), so a header
    // missing from the installed FILE_SET fails this compile, not just a
    // hand review. version.hpp alone previously left that gap.
    formula::Rational const measured = *formula::Rational::from_decimal(1235, -2); // 12.35
    formula::Rational const rounded =
        formula::round(measured, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero);

    std::printf("formula-cpp %s consumed successfully: %lld/%lld\n",
                FORMULA_VERSION_STRING,
                static_cast<long long>(rounded.numerator()),
                static_cast<long long>(rounded.denominator()));
    return 0;
}
