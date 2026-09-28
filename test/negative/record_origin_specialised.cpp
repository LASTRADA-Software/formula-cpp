// SPDX-License-Identifier: Apache-2.0
// EXPECT: RecordOrigin
//
// A forgery once possible: an explicit specialisation of
// RecordOrigin's refusing constructor, written without naming anything in
// `detail::`, whose body -- a member's -- sets the private role and key. It
// compiled and rendered a forged origin while that constructor was a member
// template with a defaulted parameter. It is refused now because the
// constructor is not a template: there is nothing to specialise, and the
// compiler says so in its own words. The three wordings share no phrase but
// the class's name -- cl "cannot be explicitly specialized", g++ 13 "does not
// match any template declaration", clang++ 20.1.8 and clang-cl 22.1.3 "no
// function template matches function template specialization" -- so the
// EXPECT is that name, and the deletion check is what shows it is this
// error: with the old member template put back, the case compiles. No
// library static_assert is involved, which the registration holds it to.
//
// This must not compile.
#include <formula-cpp/record.hpp>

#include <optional>
#include <string_view>

template <>
constexpr formula::RecordOrigin::RecordOrigin(std::string_view roleName, formula::RecordKey recordKey, bool bound) noexcept:
    _role { roleName },
    _key { bound ? std::optional<formula::RecordKey> { recordKey } : std::nullopt }
{
}

int main()
{
    // Nothing is built here: building an origin by hand is refused on its own
    // (record_origin_by_hand.cpp), and would add that message to this one.
    return 0;
}
