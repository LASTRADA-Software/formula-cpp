// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// How a method's tag is spelled wherever the library shows one.
///
/// A method selects one of its variants by a tag, a class type the author
/// declares (`variant<Cylinder>(...)`, `method.hpp`), and a trace records
/// which one fired (`StepKind::VariantSelected`, `trace.hpp`). By default the
/// tag is shown by its name as written in the source -- `Cylinder` --
/// recovered at compile time from the compiler's own spelling of a function
/// signature (`detail/type_name.hpp`). An author whose published method words
/// a variant differently specializes `TagName` to say so.
///
/// **A header of its own**, for the reason `enumerator.hpp` is one: naming a
/// tag describes the author's type, a fact that belongs beside the type in a
/// header that costs nothing to include, and both `method.hpp` and
/// `trace.hpp` need it without either including the other. It reuses
/// `enumerator.hpp`'s compile-time readability check rather than restating
/// it, so that the two customization points cannot come to disagree about
/// what a safe spelling is.

#include <formula-cpp/detail/type_name.hpp>
#include <formula-cpp/enumerator.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// The base only `TagName`'s primary template has -- see
    /// `EnumeratorNameNotCustomized`, whose reason this repeats: a
    /// specialization with a misspelt `of`, or none, is still a
    /// specialization, and must be refused rather than read as none.
    struct TagNameNotCustomized
    {
    };
} // namespace detail

/// Customization point: how the tag `Tag` is spelled wherever the library
/// shows one (the variant a method selected, in a trace).
///
/// The primary template customizes nothing, and the library falls back to
/// the tag's own name as written in the source, unqualified. Specialize it to
/// spell a tag differently, for example as a published method words the
/// variant:
///
///     template <>
///     struct formula::TagName<Cylinder>
///     {
///         static constexpr std::string_view of() noexcept { return "cylinder 150 x 300 mm"; }
///     };
///
/// **The same shape as `EnumeratorName`, less the argument**, so that an
/// author who has customized one has already learned the other: a `static
/// constexpr` (or `consteval`) `of()` returning something convertible to
/// `std::string_view`, usable in a constant expression. A specialization of
/// any other shape is refused with this library's own message, never read as
/// "not customized".
///
/// **Declare it next to the tag, before anything uses it, and on the
/// unqualified type** -- the rules `EnumeratorName` gives, for the reasons it
/// gives. A specialization for `Cylinder const` alone is refused, since the
/// library only ever asks about `Cylinder` (a tag is never cv-qualified;
/// `method.hpp` refuses one).
///
/// **Return an empty view to leave the tag alone**; it then falls back to its
/// own name.
///
/// **What `of` returns must be readable at compile time, and so have static
/// storage duration** -- a string literal, or a view of a namespace-scope or
/// `static` `constexpr` array. A trace keeps the view (`Step::variantTag`,
/// `trace.hpp`) for as long as the trace lives. Enforced by the gates
/// `EnumeratorName` describes, the first two of them unchanged:
/// `RequireTagName` reads every character in a constant expression, and
/// `tag_name` is `consteval`, so its result must point into an object with
/// static storage duration.
///
/// **Use it for a template specialization used as a tag.** The reflected
/// name keeps a specialization's arguments, but cl prints some of them
/// differently from the other compilers -- `Flag<1>` for `Flag<true>` -- so
/// such a tag reads the same on every compiler only once it is spelled here.
/// See `detail::normalized_type_name` for what is and is not evened out.
template <typename Tag>
struct TagName: detail::TagNameNotCustomized
{
};

/// True when `TagName<Tag>` has the shape the library reads: a static `of()`
/// returning something convertible to `std::string_view`. Whether it is
/// usable in a constant expression is asked separately, by `RequireTagName`.
template <typename Tag>
concept TagNameCustomization = requires {
    { TagName<Tag>::of() } -> std::convertible_to<std::string_view>;
};

namespace detail
{
    /// True when the author specialized `TagName<Tag>` at all -- with any
    /// shape. See `TagNameNotCustomized`.
    template <typename Tag>
    inline constexpr bool customizesTagName = !std::is_base_of_v<TagNameNotCustomized, TagName<Tag>>;

    /// The customized spelling of @p Tag, as a view -- converted here, in a
    /// `return`, for the reason `customized_enumerator_name` gives.
    template <typename Tag>
        requires TagNameCustomization<Tag>
    [[nodiscard]] constexpr std::string_view customized_tag_name()
    {
        return TagName<Tag>::of();
    }

    /// True when `TagName`'s spelling of @p Tag can be computed at compile
    /// time and every character of it read there -- the first of the gates
    /// described on `TagName`.
    template <typename Tag>
    concept ConstantTagName = TagNameCustomization<Tag> && requires {
        typename std::bool_constant<every_character_readable(customized_tag_name<Tag>())>;
    };
} // namespace detail

/// Fails to compile when `TagName<Tag>` is specialized but the library cannot
/// read a spelling out of it: the specialization has the wrong shape, or its
/// `of()` is not usable in a constant expression, or the characters of what
/// it returns cannot be read at compile time.
///
/// Instantiated by `tag_name` whenever a specialization exists, so a broken
/// one is refused the first time a method is evaluated with that tag.
template <typename Tag>
struct RequireTagName
{
    static_assert(TagNameCustomization<Tag>,
                  "formula: this TagName specialisation does not have the shape the library reads -- it "
                  "needs a static constexpr member function of() returning something convertible to "
                  "std::string_view; the tag appears in this diagnostic as template argument Tag of "
                  "RequireTagName -- a specialisation the library cannot read is refused rather than "
                  "treated as no customization, so a misspelt of() cannot silently drop the author's wording");

    // `!TagNameCustomization || ...` so a wrongly shaped specialization gets
    // the message above and not this one as well -- the reason
    // `RequireEnumeratorName` gives, and pinned the same way, by
    // `tag_name_misspelt_of.cpp`.
    static_assert(!TagNameCustomization<Tag> || detail::ConstantTagName<Tag>,
                  "formula: TagName<Tag>::of() is not usable in a constant expression, or returns a view "
                  "whose characters cannot be read at compile time -- a string literal or a namespace-scope "
                  "or static constexpr array can be, a local buffer (constexpr or not), a std::string "
                  "returned by value or a non-constexpr array cannot, and a trace keeps the view; the tag "
                  "appears in this diagnostic as template argument Tag of RequireTagName -- make of() "
                  "constexpr and return a string literal");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when `TagName` is specialized for a const-qualified
/// @p Tag -- `TagName<Cylinder const>` -- and not for @p Tag itself. The
/// library only ever asks `TagName<Cylinder>`, so that specialization could
/// never take effect.
///
/// Only when the unqualified form is not customized, and `volatile` is not
/// checked: both for the reasons `RequireUnqualifiedEnumeratorName` gives. A
/// constrained partial specialization covering every class type also matches
/// `Cylinder const`, and customizes `Cylinder` too.
template <typename Tag>
struct RequireUnqualifiedTagName
{
    static_assert(detail::customizesTagName<Tag> || !detail::customizesTagName<Tag const>,
                  "formula: TagName is specialised for a const-qualified tag, which the library never asks "
                  "about, so it would silently never be used; the tag appears in this diagnostic as template "
                  "argument Tag of RequireUnqualifiedTagName -- specialise TagName for the unqualified tag "
                  "instead");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// The spelling of tag @p Tag: `TagName<Tag>`'s, if it customizes it;
/// otherwise the tag's own name as written in its declaration, unqualified
/// (`Cylinder`, never `specimen::Cylinder`, and never with an anonymous
/// namespace in front of it) and with a template specialization's arguments
/// kept (`Sized<150>`) -- see `detail::normalized_type_name` for how those
/// are spelt. Empty only if the compiler's signature is not in the shape
/// the library reads, in which case the caller falls back.
///
/// Every view this returns has static storage duration: a reflected name
/// points into `detail::TypeNameStorage`, and a customized one has passed the
/// gates described on `TagName`. It is safe to keep for as long as the image
/// that computed it stays loaded -- the limit `enumerator_name` states for a
/// shared library or plugin applies here unchanged.
template <typename Tag>
[[nodiscard]] consteval std::string_view tag_name() noexcept
{
    static_assert(RequireUnqualifiedTagName<Tag>::value);
    if constexpr (detail::customizesTagName<Tag>)
    {
        static_assert(RequireTagName<Tag>::value);
        // Guarded again so a refused specialization is reported once, in the
        // library's words, and not followed by the raw error from calling it.
        if constexpr (detail::ConstantTagName<Tag>)
        {
            std::string_view const customized = detail::customized_tag_name<Tag>();
            if (!customized.empty())
                return customized;
        }
    }
    return detail::reflected_type_name<Tag>();
}

} // namespace formula
