// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/enumerator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

// Every enumeration here is named for this file -- see `lookup.hpp`'s note on
// giving each translation unit's key enumeration a name of its own. None of
// these is a lookup key, but a later test that made one a key should not have
// to rename it first.

enum class NamingShape
{
    Cube,
    Cylinder = 7,
    Prism,
};

namespace naming_outer::naming_inner
{
enum class NamingNested
{
    Deep = 2,
};
} // namespace naming_outer::naming_inner

namespace
{
enum class NamingAnonymous
{
    Hidden,
    Shown = 3,
};

enum class NamingWide : unsigned long long
{
    Small = 1,
    Top = 18446744073709551615ULL,
};

enum class NamingNegative : std::int8_t
{
    Below = -3,
    Zero = 0,
};

enum NamingUnscoped
{
    NamingBare = 5,
};

enum class NamingAlias
{
    First = 1,
    Second = 1,
};

enum class NamingUnicode
{
    Größe,
};

template <typename T>
struct NamingHolder
{
    enum class Inner
    {
        Held,
    };
};

enum class NamingCustomized
{
    Cube,
    Cylinder,
    Prism,
};

enum class NamingPartial
{
    Spelled,
    Unspelled,
};
} // namespace

template <>
struct formula::EnumeratorName<NamingCustomized>
{
    static constexpr std::string_view of(NamingCustomized shape) noexcept
    {
        switch (shape)
        {
            case NamingCustomized::Cube:
                return "cube 150 mm";
            case NamingCustomized::Cylinder:
                return "cylinder 150/300 mm";
            case NamingCustomized::Prism:
                return "prism 100 mm";
        }
        return {};
    }
};

template <>
struct formula::EnumeratorName<NamingPartial>
{
    static constexpr std::string_view of(NamingPartial variant) noexcept
    {
        return variant == NamingPartial::Spelled ? "the spelled one" : "";
    }
};

using formula::enumerator_name;

TEST_CASE("an enumerator is named as written, without its enumeration's qualification", "[enumerator]")
{
    // A default-valued enumerator, one with an explicit value, and one that
    // follows it -- so a name read off the value's position rather than off
    // the enumerator itself would be caught at every step. The bare name,
    // never `NamingShape::Cube`: every compiler prints the qualified form,
    // and this kills a parse that keeps it.
    STATIC_REQUIRE(enumerator_name<NamingShape::Cube>() == "Cube");
    STATIC_REQUIRE(enumerator_name<NamingShape::Cylinder>() == "Cylinder");
    STATIC_REQUIRE(enumerator_name<NamingShape::Prism>() == "Prism");
}

TEST_CASE("an enumerator is named whatever namespace or class its enumeration sits in", "[enumerator]")
{
    // Each compiler qualifies these differently, and an anonymous namespace
    // most differently of all -- `(anonymous namespace)::`, `<unnamed>::`,
    // `` `anonymous-namespace'::`` -- which is why the name is read back from
    // the end of the signature rather than forward past the qualification.
    STATIC_REQUIRE(enumerator_name<naming_outer::naming_inner::NamingNested::Deep>() == "Deep");
    STATIC_REQUIRE(enumerator_name<NamingAnonymous::Hidden>() == "Hidden");
    STATIC_REQUIRE(enumerator_name<NamingAnonymous::Shown>() == "Shown");

    // A `]` inside the enumeration's own qualification. The reference
    // implementation's parse -- everything after `E = ` up to the first `]`
    // -- returns a fragment of `NamingHolder<int[3]>` here; reading from the
    // end does not.
    STATIC_REQUIRE(enumerator_name<NamingHolder<int[3]>::Inner::Held>() == "Held");
}

TEST_CASE("an enumerator is named whatever its underlying type", "[enumerator]")
{
    // The top of `unsigned long long` and a negative value of a signed type:
    // printed as a name, both are just a name, but the value each one stands
    // for is exactly what a cast of a non-enumerator prints as a number.
    STATIC_REQUIRE(enumerator_name<NamingWide::Top>() == "Top");
    STATIC_REQUIRE(enumerator_name<NamingWide::Small>() == "Small");
    STATIC_REQUIRE(enumerator_name<NamingNegative::Below>() == "Below");
}

TEST_CASE("a value that names no enumerator has no name", "[enumerator]")
{
    // Every compiler prints one of these as a cast ending in a number --
    // `(NamingShape)9`, `(enum NamingShape)0x9` -- and a name read off the
    // end of that would be `9` or `0x9`. Kills a parse that returns whatever
    // identifier-like run it finds.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingShape>(9)>().empty());
    // Negative: `(NamingNegative)-4` from clang and GCC, `0xfc` from cl.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingNegative>(-4)>().empty());
    // Past every signed type: cl prints it in hex, clang and GCC in decimal.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingWide>(18446744073709551614ULL)>().empty());
    // In an anonymous namespace, whose cast spelling differs per compiler.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingAnonymous>(2)>().empty());
}

TEST_CASE("an unscoped enumerator, an alias and a Unicode name are named as the compiler sees them", "[enumerator]")
{
    // An unscoped enumerator is printed unqualified by every compiler, so the
    // run ends at the space (or the `<` on cl) rather than at a `::`.
    STATIC_REQUIRE(enumerator_name<NamingBare>() == "NamingBare");

    // Two enumerators with one value are one template argument, and every
    // compiler prints the first one declared. Pinned so that a change here is
    // noticed rather than discovered in a rendered table.
    STATIC_REQUIRE(enumerator_name<NamingAlias::Second>() == "First");

    // A C++23 identifier may be Unicode, and each compiler prints it as UTF-8.
    // Kills a scan that stops at the first non-ASCII byte, which would name
    // this enumerator `e`.
    STATIC_REQUIRE(enumerator_name<NamingUnicode::Größe>()
                   == "Gr\xc3\xb6\xc3\x9f"
                      "e");
}

TEST_CASE("a customized enumerator is spelled the author's way, at compile time", "[enumerator]")
{
    // Every enumerator customized, including the middle one.
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Cube>() == "cube 150 mm");
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Cylinder>() == "cylinder 150/300 mm");
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Prism>() == "prism 100 mm");

    // And usable in a plain static_assert, not only through Catch's macro: the
    // requirement is that a customized name is a constant expression.
    static_assert(enumerator_name<NamingCustomized::Cube>() == "cube 150 mm");
}

TEST_CASE("a customization that leaves an enumerator empty falls back to its own name", "[enumerator]")
{
    // Kills an implementation that returns the customization's answer even
    // when it is empty, which would show a key as `key ` with nothing after.
    STATIC_REQUIRE(enumerator_name<NamingPartial::Spelled>() == "the spelled one");
    STATIC_REQUIRE(enumerator_name<NamingPartial::Unspelled>() == "Unspelled");
}
