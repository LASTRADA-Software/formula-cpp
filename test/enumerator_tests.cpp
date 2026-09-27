// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/enumerator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>

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

namespace naming_library
{
inline namespace naming_v1
{
    enum class NamingInline
    {
        Versioned = 1,
    };
} // namespace naming_v1
} // namespace naming_library

struct NamingPlainHolder
{
    enum class Member
    {
        Inside = 2,
    };
};

namespace
{
enum class NamingCharacter : char
{
    Letter = 'A',
};

enum class NamingFlag : bool
{
    Off = false,
};

enum class NamingDigits
{
    C163 = 163,
    X2Y3,
};

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

/// An enumeration whose spelling comes from the codebase's own `to_string`,
/// found by argument-dependent lookup, through the generic bridge below.
enum class NamingBridged
{
    Alpha,
    Beta,
};

[[nodiscard]] constexpr std::string_view naming_to_string(NamingBridged value) noexcept
{
    return value == NamingBridged::Alpha ? "alpha, bridged" : "";
}
} // namespace

/// A constrained partial specialization covering every enumeration that has a
/// `naming_to_string` -- the ordinary way to bridge a codebase's own
/// `to_string` into this trait. `std::is_enum_v` is true of a cv-qualified
/// enumeration too, so this also matches `NamingBridged const`, which must not
/// be mistaken for a specialization of the qualified type alone.
template <typename E>
    requires std::is_enum_v<E> && requires(E value) {
        { naming_to_string(value) } -> std::convertible_to<std::string_view>;
    }
struct formula::EnumeratorName<E>
{
    static constexpr std::string_view of(E value) noexcept
    {
        return naming_to_string(value);
    }
};

template <>
struct formula::EnumeratorName<NamingCustomized>
{
    static constexpr std::string_view of(NamingCustomized shape) noexcept
    {
        switch (shape)
        {
            case NamingCustomized::Cube:
                return "cube 139 mm";
            case NamingCustomized::Cylinder:
                return "cylinder 139/277 mm";
            case NamingCustomized::Prism:
                return "prism 103 mm";
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
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Cube>() == "cube 139 mm");
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Cylinder>() == "cylinder 139/277 mm");
    STATIC_REQUIRE(enumerator_name<NamingCustomized::Prism>() == "prism 103 mm");

    // And usable in a plain static_assert, not only through Catch's macro: the
    // requirement is that a customized name is a constant expression.
    static_assert(enumerator_name<NamingCustomized::Cube>() == "cube 139 mm");
}

TEST_CASE("a customization that leaves an enumerator empty falls back to its own name", "[enumerator]")
{
    // Kills an implementation that returns the customization's answer even
    // when it is empty, which would show a key as `key ` with nothing after.
    STATIC_REQUIRE(enumerator_name<NamingPartial::Spelled>() == "the spelled one");
    STATIC_REQUIRE(enumerator_name<NamingPartial::Unspelled>() == "Unspelled");
}

TEST_CASE("an enumerator is named through an inline namespace and a non-template class", "[enumerator]")
{
    // GCC and cl print the inline namespace (`naming_library::naming_v1::`),
    // clang omits it; either way the name is the last run, after a `::`.
    STATIC_REQUIRE(enumerator_name<naming_library::NamingInline::Versioned>() == "Versioned");
    STATIC_REQUIRE(enumerator_name<NamingPlainHolder::Member::Inside>() == "Inside");
}

TEST_CASE("an enumerator with digits inside its name keeps them", "[enumerator]")
{
    // Digits are identifier bytes; only a LEADING digit marks a number. Kills
    // a scan that treats a digit as the end of a name, which would leave both empty.
    STATIC_REQUIRE(enumerator_name<NamingDigits::C163>() == "C163");
    STATIC_REQUIRE(enumerator_name<NamingDigits::X2Y3>() == "X2Y3");
}

TEST_CASE("a char- or bool-based enumeration's non-enumerator has no name either", "[enumerator]")
{
    STATIC_REQUIRE(enumerator_name<NamingCharacter::Letter>() == "Letter");
    STATIC_REQUIRE(enumerator_name<NamingFlag::Off>() == "Off");
    // clang, GCC and cl all print a char-based value as a number, never as a
    // character literal: `(NamingCharacter)66`, `(enum NamingCharacter)0x42`.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingCharacter>('B')>().empty());
    // cl prints this one as `(enum NamingFlag)true` -- a cast whose value is
    // an identifier, not a number -- so the digit rule alone would name it
    // `true`. Kills the rule that a run right after `)` is a cast's value, on
    // cl; clang and GCC print `(NamingFlag)1`, which the digit rule catches.
    STATIC_REQUIRE(enumerator_name<static_cast<NamingFlag>(true)>().empty());
}

TEST_CASE("a generic partial specialization over every enumeration is a customization, not a cv-qualified one",
          "[enumerator]")
{
    // The bridge matches `NamingBridged const` as well as `NamingBridged`, so
    // a cv-qualification check that looked only at the qualified form would
    // refuse every name here with a message that is false. Kills that check;
    // this case is a compile error under it.
    STATIC_REQUIRE(enumerator_name<NamingBridged::Alpha>() == "alpha, bridged");
    STATIC_REQUIRE(enumerator_name<NamingBridged::Beta>() == "Beta");
}
