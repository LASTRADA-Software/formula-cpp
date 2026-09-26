// SPDX-License-Identifier: Apache-2.0
// EXPECT: MeasuredSeries<
//
// The triple-brace spelling.
//
// Five elements for a series of six, written so that std::array's own
// aggregate initialisation would make the sixth absent, silently. The
// std::array constructor is deduced, so no braced list reaches it, and this
// spelling has no constructor at all. The words are the compiler's, and
// differ: cl "cannot convert from 'initializer list'" / "no overloaded
// function could convert", clang "no matching constructor", g++ "no matching
// function for call" / "would use explicit constructor" / "could not
// convert" -- measured on cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3
// and g++ 14.2. The type's name is the text they share. Reverting the
// constructor to take std::array by value makes this case compile.
#include <formula-cpp/environment.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr formula::Measured<Retained> a { formula::Rational { 130 } };
inline constexpr formula::Measured<Retained> b { formula::Rational { 210 } };
inline constexpr formula::Measured<Retained> c { formula::Rational { 95 } };
inline constexpr formula::Measured<Retained> d { formula::Rational { 340 } };
inline constexpr formula::Measured<Retained> e { formula::Rational { 28 } };

inline constexpr formula::MeasuredSeries<Retained, 6> screens { { { a, b, c, d, e } } };

int main()
{
    return screens.element(5).has_value() ? 0 : 1;
}
