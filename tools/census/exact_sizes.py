# SPDX-License-Identifier: Apache-2.0
#
# The overflow census's exact companion (docs/numeric-headroom.md): for the
# same samples formula-cpp-census-tests draws, how many bits the exact,
# fully reduced RESULT needs -- as opposed to the intermediates the census
# measures. Each result is sized twice:
#
#  - in the coherent SI unit (kg2, Pa), the unit the evaluator works in; the
#    value it holds at the step before the last conversion;
#  - in the result's declared unit (g2, MPa), the unit checked_evaluate<Q>
#    hands the caller.
#
# A value that needs more than a signed 128-bit integer's 127 bits in SI but
# fits in the declared unit can be delivered by 128-bit storage only if the
# conversion happens inside wider arithmetic, or if the statistic is
# evaluated in a scaled unit.
#
# Mirrors test/overflow_census_tests.cpp's Draws (splitmix64, seed 20260926)
# and its fixtures exactly. Run: python tools/census/exact_sizes.py
# With --self-check it prints only the first sample's draws, at 6 and 4 dp,
# and the 4 dp sample's variance: the census test "the census draws the
# samples tools/census/exact_sizes.py draws" computes the same through the
# library, and CTest's census.exact-sizes-self-check pins this output to the
# same literals.
import sys
from fractions import Fraction

MASK = (1 << 64) - 1


class Draws:
    def __init__(self, seed):
        self.state = seed

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
        mixed = self.state
        mixed = ((mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9) & MASK
        mixed = ((mixed ^ (mixed >> 27)) * 0x94D049BB133111EB) & MASK
        return mixed ^ (mixed >> 31)

    def between(self, low, high, places):
        scale = 10 ** places
        span = (high - low) * scale + 1
        return Fraction(low * scale + self.next() % span, scale)


LIMIT = (1 << 127) - 1


def fits(value):
    """Whether an exact value is a Rational over formula::Int128: |numerator|
    and the denominator at most 2^127 - 1 (the numerator may also be -2^127)."""
    return abs(value.numerator) <= LIMIT and value.denominator <= LIMIT


def widest(value):
    """The bits of the wider of the numerator's magnitude and the denominator."""
    return max(abs(value.numerator).bit_length(), value.denominator.bit_length())


def sample_variance(values):
    """With n - 1, as sample_variance divides."""
    mean = sum(values) / len(values)
    return sum((value - mean) ** 2 for value in values) / (len(values) - 1)


def masses(draws, places):
    """One sample: six masses between 39 and 41 g at `places` decimal places."""
    return [draws.between(39, 41, places) for _ in range(6)]


def self_check():
    print("first draws at 6 dp, g: " + ", ".join(str(value) for value in masses(Draws(20260926), 6)))
    # At 4 dp the library's variance fits, so the census test can compare it.
    sample = masses(Draws(20260926), 4)
    print("first draws at 4 dp, g: " + ", ".join(str(value) for value in sample))
    print(f"their sample variance, g2: {sample_variance(sample)}")


def main():
    for places in (4, 5, 6):
        # A fresh seed per resolution, 1000 samples of six: the census's loop.
        draws = Draws(20260926)
        unrepresentable_si = 0
        unrepresentable_declared = 0
        widest_si = 0
        widest_declared = 0
        for _ in range(1000):
            grams = masses(draws, places)
            in_g2 = sample_variance(grams)
            in_kg2 = in_g2 / 1000 ** 2
            if not fits(in_kg2):
                unrepresentable_si += 1
            if not fits(in_g2):
                unrepresentable_declared += 1
            widest_si = max(widest_si, widest(in_kg2))
            widest_declared = max(widest_declared, widest(in_g2))
        print(f"six masses near 40 g at {places} dp: the exact variance does not fit 128 bits in "
              f"{unrepresentable_si} of 1000 in kg2 (SI; widest {widest_si} bits), in {unrepresentable_declared} "
              f"of 1000 in g2 (declared; widest {widest_declared} bits)")

    load = Fraction(89300)
    pi = Fraction(245850922, 78256779)
    unrepresentable_si = 0
    unrepresentable_declared = 0
    widest_si = 0
    widest_declared = 0
    for millimetres in range(101, 164):
        in_mpa = 4 * load / (pi * Fraction(millimetres) ** 2)
        in_pa = in_mpa * 10 ** 6
        if not fits(in_pa):
            unrepresentable_si += 1
        if not fits(in_mpa):
            unrepresentable_declared += 1
        widest_si = max(widest_si, widest(in_pa))
        widest_declared = max(widest_declared, widest(in_mpa))
    print(f"4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in Pa (SI) the exact strength does not fit 128 bits at "
          f"{unrepresentable_si} of 63 (widest {widest_si} bits)")
    print(f"4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in MPa (declared) it does not fit at "
          f"{unrepresentable_declared} of 63 (widest {widest_declared} bits)")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-check"]:
        self_check()
    else:
        main()
