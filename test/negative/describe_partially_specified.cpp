// SPDX-License-Identifier: Apache-2.0
// A Describe specialisation naming only SOME of the four members Measured and
// checked_convert_to actually read must be refused in the library's own
// words -- not accepted as "described" and then failed three calls deeper
// with the compiler's raw "no member named 'dimension'". This must not
// compile.
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

struct HalfDescribed
{
};

template <>
struct formula::Describe<HalfDescribed>
{
    static constexpr std::string_view symbol = "h";
    static constexpr formula::Unit unit = formula::unit::Litre;
    // description and dimension deliberately omitted.
};

int main()
{
    // RequireDescribed, not Describe<HalfDescribed>::dimension directly: the
    // point is that the library's own gate catches this, not that naming the
    // missing member by hand also fails.
    return formula::RequireDescribed<HalfDescribed>::value ? 1 : 0;
}
