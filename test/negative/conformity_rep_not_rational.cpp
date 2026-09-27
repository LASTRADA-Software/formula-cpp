// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a conformity check can only be evaluated with Rep = Rational
//
// `check_conformity<double>`: a value exactly on a closed limit, judged a few
// units in the last place off, is violated where it is satisfied. Refused in
// the library's words, as the snap, curve and binning guards are. This must
// not compile.
#include <formula-cpp/conformity.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr auto measured =
    formula::environment(formula::measured_series<Passing>(formula::Measured<Passing> { formula::Rational { 1574, 25 } }));

inline constexpr auto onItsLimit = formula::conformity<formula::unit::Percent>(
    formula::series<Passing, 1>,
    formula::Envelope<1> { formula::LimitRow { formula::limit(formula::Rational { 1574, 25 }), formula::unbounded } },
    formula::Verdict { "outside" });

int main()
{
    return formula::check_conformity<double>(onItsLimit, measured)[0].is_satisfied() ? 0 : 1;
}
