// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/tag.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <string_view>

// Every tag here is named for this file, for the reason `enumerator_tests.cpp`
// names its enumerations: a later test that made one a method's tag should
// not have to rename it first. The shapes are the four the library's own
// comments make claims about -- an anonymous namespace, a nested namespace, a
// class nested in a class, and a class template specialization -- measured on
// cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and g++-14 14.2.

struct TagGlobal;

namespace tag_outer::tag_inner
{
struct TagNested;
} // namespace tag_outer::tag_inner

struct TagHolder
{
    struct Member;
};

template <typename T>
struct TagBox;

template <int N>
struct TagSized;

template <typename A, typename B>
struct TagPair;

template <bool B>
struct TagFlag;

template <typename T>
struct TagTemplateHolder
{
    struct Member;
};

namespace
{
struct TagAnonymous;

template <typename T>
struct TagAnonymousBox;

struct TagCustomized;
struct TagLeftAlone;

/// A tag whose spelling comes from the codebase's own function, found by
/// argument-dependent lookup through the generic bridge below.
struct TagBridged
{
};

[[nodiscard]] constexpr std::string_view tag_to_string(TagBridged const*) noexcept
{
    return "bridged tag";
}
} // namespace

template <>
struct formula::TagName<TagCustomized>
{
    static constexpr std::string_view of() noexcept { return "cylinder 150 x 300 mm"; }
};

template <>
struct formula::TagName<TagLeftAlone>
{
    static constexpr std::string_view of() noexcept { return {}; }
};

/// A constrained partial specialization covering every class type that has a
/// `tag_to_string` -- the ordinary way to bridge a codebase's own naming into
/// this trait. `std::is_class_v` is true of a cv-qualified class too, so this
/// also matches `TagBridged const`, which must not be mistaken for a
/// specialization of the qualified type alone.
template <typename T>
    requires std::is_class_v<T> && requires(T const* tag) {
        { tag_to_string(tag) } -> std::convertible_to<std::string_view>;
    }
struct formula::TagName<T>
{
    static constexpr std::string_view of() noexcept { return tag_to_string(static_cast<T const*>(nullptr)); }
};

using formula::tag_name;

TEST_CASE("a tag is named as written, without its namespace", "[tag]")
{
    STATIC_REQUIRE(tag_name<TagGlobal>() == "TagGlobal");
    STATIC_REQUIRE(tag_name<tag_outer::tag_inner::TagNested>() == "TagNested");
}

TEST_CASE("a tag in an anonymous namespace is named without it, however the compiler spells it", "[tag]")
{
    // `(anonymous namespace)::` from clang and clang-cl, `{anonymous}::` from
    // GCC, `` `anonymous-namespace'::`` from cl at the top level and
    // `` `anonymous namespace'::`` inside a template argument list. None of
    // them may reach a trace.
    STATIC_REQUIRE(tag_name<TagAnonymous>() == "TagAnonymous");
    STATIC_REQUIRE(tag_name<TagAnonymousBox<TagAnonymous>>() == "TagAnonymousBox<TagAnonymous>");
    STATIC_REQUIRE(tag_name<TagBox<TagAnonymous>>() == "TagBox<TagAnonymous>");
}

TEST_CASE("a tag nested in a class is named without the class", "[tag]")
{
    STATIC_REQUIRE(tag_name<TagHolder::Member>() == "Member");
    // The enclosing class may itself be a specialization, whose arguments go
    // with it.
    STATIC_REQUIRE(tag_name<TagTemplateHolder<int>::Member>() == "Member");
}

TEST_CASE("a class template specialization keeps its arguments, spelt alike on every compiler", "[tag]")
{
    STATIC_REQUIRE(tag_name<TagSized<150>>() == "TagSized<150>");
    STATIC_REQUIRE(tag_name<TagSized<-3>>() == "TagSized<-3>");
    // An argument's qualification goes too, at every depth.
    STATIC_REQUIRE(tag_name<TagBox<tag_outer::tag_inner::TagNested>>() == "TagBox<TagNested>");
    STATIC_REQUIRE(tag_name<TagBox<TagBox<TagBox<TagAnonymous>>>>() == "TagBox<TagBox<TagBox<TagAnonymous>>>");
    // cl writes `struct` before every class argument, no space after a
    // comma, and a space between two closing brackets; GCC writes that last
    // space too. All three are evened out.
    STATIC_REQUIRE(tag_name<TagPair<int, double>>() == "TagPair<int, double>");
    STATIC_REQUIRE(tag_name<TagPair<TagBox<int>, TagBox<TagGlobal>>>() == "TagPair<TagBox<int>, TagBox<TagGlobal>>");
}

TEST_CASE("a bool template argument is where cl's spelling cannot be evened out", "[tag]")
{
    // Pins the limit `detail::normalized_type_name` documents, rather than
    // leaving it a comment nobody can break: cl prints the argument as a
    // number, and no parse of the text recovers a `true` it did not print.
    // A tag like this one is what `TagName` is for.
#if defined(_MSC_VER) && !defined(__clang__)
    STATIC_REQUIRE(tag_name<TagFlag<true>>() == "TagFlag<1>");
#else
    STATIC_REQUIRE(tag_name<TagFlag<true>>() == "TagFlag<true>");
#endif
}

TEST_CASE("a reflected tag name lives in static storage of its own", "[tag]")
{
    // Not a view into some temporary: the characters are those of
    // `TypeNameStorage<T>::chars`, which a trace may keep for as long as it
    // lives (`Step::variantTag`).
    constexpr std::string_view name = tag_name<TagBox<TagAnonymous>>();
    CHECK(name.data() == formula::detail::TypeNameStorage<TagBox<TagAnonymous>>::chars.data());
}

TEST_CASE("a customized tag is spelled the author's way, at compile time", "[tag]")
{
    STATIC_REQUIRE(tag_name<TagCustomized>() == "cylinder 150 x 300 mm");
}

TEST_CASE("a customization that leaves a tag empty falls back to its own name", "[tag]")
{
    STATIC_REQUIRE(tag_name<TagLeftAlone>() == "TagLeftAlone");
}

TEST_CASE("a generic partial specialization over every class is a customization, not a cv-qualified one", "[tag]")
{
    // The bridge matches `TagBridged const` as well as `TagBridged`, so a
    // cv-qualification check that looked only at the qualified form would
    // refuse this name with a message that is false. Kills that check; this
    // case is a compile error under it.
    STATIC_REQUIRE(tag_name<TagBridged>() == "bridged tag");
}
