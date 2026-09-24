// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/fixed_string.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

using formula::detail::FixedString;

// ---- it carries exactly the text it was given ----

static_assert(FixedString { "V_w" }.view() == std::string_view { "V_w" });
static_assert(FixedString { "V_w" }.view().size() == 3);
static_assert(FixedString { "" }.view().empty());
static_assert(FixedString { "a sentence with spaces" }.view().size() == 22);

// The capacity includes the terminator; the view does not.
static_assert(std::is_same_v<decltype(FixedString { "abc" }), FixedString<4>>);

// ---- the property it exists for: usable as a template argument ----

template <FixedString S>
struct Tagged
{
    static constexpr std::string_view text = S.view();
};

static_assert(Tagged<"V_w">::text == std::string_view { "V_w" });

// Same text means the same type; different text means a different type. This is
// what makes a quantity's symbol part of its identity rather than a field
// somebody can change without the type noticing.
static_assert(std::is_same_v<Tagged<"V_w">, Tagged<"V_w">>);
static_assert(!std::is_same_v<Tagged<"V_w">, Tagged<"V_c">>);

// ---- equality across capacities ----
//
// Two literals of different lengths produce two different FixedString TYPES, so
// a defaulted member operator== would not compare them at all. Text equality has
// to work regardless of capacity, or every comparison silently depends on how
// long the literals happened to be.
static_assert(FixedString { "abc" } == FixedString { "abc" });
static_assert(!(FixedString { "abc" } == FixedString { "abcd" }));
static_assert(!(FixedString { "abc" } == FixedString { "abd" }));

TEST_CASE("a fixed string reports exactly the text it was built from", "[fixed-string]")
{
    constexpr FixedString symbol { "rho" };
    CHECK(symbol.view() == std::string_view { "rho" });
    CHECK(symbol.view().size() == 3);

    // Embedded UTF-8 survives byte for byte; the type counts bytes, not
    // characters, and says so.
    constexpr FixedString degrees { "\xc2\xb0" "C" };
    CHECK(degrees.view().size() == 3);
    CHECK(degrees.view() == std::string_view { "\xc2\xb0" "C" });
}
