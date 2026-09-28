// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Lineage: which batch, which method, which regime a record belongs to, and
/// a read from another record that requires two records to agree on them.
///
/// A **lineage attribute** is a plain class tag the author declares --
/// `struct MaterialBatch {};` -- and names through `TagName`. The library
/// knows no attribute: it compares keys and nothing else. Which batch a
/// specimen belongs to is the lab system's data; declaring that a
/// computation requires two records to agree on it, and tracing the check,
/// is this library's part.
///
/// A record declares its keys where it is built:
/// `record<Reference>(key, environment(...), lineage<MaterialBatch>(4411))`,
/// or `unknown_lineage<MaterialBatch>()` when the batch is not known. A read
/// then requires agreement:
/// `from_record<Reference>(expression, same_lineage<MaterialBatch, TestMethod>())`
/// compares each attribute with this record's, and
/// `same_lineage<MaterialBatch>(against<PriorTest>)` with another record's.
///
/// **A gate, not a verdict beside the value.** Every attribute is compared
/// before the expression is read, and the trace records one step per
/// attribute, in the order the requirement names them:
/// - both keys known and equal: satisfied;
/// - both known and different: violated -- the read gives `DomainError`, and
///   the expression is not evaluated;
/// - either unknown: not checked -- the read is absent, since an unknown
///   batch is a missing input, not evidence that two batches differ.
/// Any violated attribute refuses the read, whatever else the others say.
///
/// **Refused at compile time, each with one message:** a requirement that
/// names no attribute, one that names an attribute twice, a record that
/// declares an attribute twice, a read that compares an attribute either
/// record does not declare, and `against<Role>` naming the read's own role.

#include <formula-cpp/tag.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace formula
{

struct ThisRecord;

template <typename Attr>
class LineageEntry;

namespace detail
{
    /// The one way to build a `LineageEntry`: `lineage<Attr>()` and
    /// `unknown_lineage<Attr>()` are its only callers.
    struct LineageEntryAccess
    {
        /// An entry for @p Attr holding @p lineageKey, or unknown.
        template <typename Attr>
        [[nodiscard]] static constexpr LineageEntry<Attr> make(std::optional<std::uint64_t> lineageKey) noexcept
        {
            return LineageEntry<Attr> { lineageKey };
        }
    };
} // namespace detail

/// One lineage key a record declares: the key of attribute @p Attr, or
/// unknown. Built by `lineage<Attr>(key)` or `unknown_lineage<Attr>()`.
///
/// The key is the caller's statement about its own data, recorded as stated,
/// as a record key is.
template <typename Attr>
class LineageEntry
{
  public:
    /// The attribute this entry is a key of.
    using attribute = Attr;

    /// The key; empty when it is not known.
    [[nodiscard]] constexpr std::optional<std::uint64_t> key() const noexcept { return _key; }

  private:
    friend struct detail::LineageEntryAccess;

    constexpr explicit LineageEntry(std::optional<std::uint64_t> lineageKey) noexcept:
        _key { lineageKey }
    {
    }

    std::optional<std::uint64_t> _key;
};

/// A record's key for attribute @p Attr: `lineage<MaterialBatch>(4411)`.
template <typename Attr>
[[nodiscard]] constexpr LineageEntry<Attr> lineage(std::uint64_t lineageKey) noexcept
{
    return detail::LineageEntryAccess::make<Attr>(lineageKey);
}

/// A record whose key for attribute @p Attr is not known. A requirement on
/// @p Attr is then not checked, and the read it gates is absent.
template <typename Attr>
[[nodiscard]] constexpr LineageEntry<Attr> unknown_lineage() noexcept
{
    return detail::LineageEntryAccess::make<Attr>(std::nullopt);
}

/// Names the record a lineage requirement compares with: `against<PriorTest>`.
template <typename Role>
struct Against
{
};

/// The record a lineage requirement compares with, when it is not this one:
/// `same_lineage<MaterialBatch>(against<PriorTest>)`.
template <typename Role>
inline constexpr Against<Role> against {};

namespace detail
{
    /// An unknown entry of the entry type named: what an unbound record
    /// holds for each attribute it declares.
    template <typename Attr>
    [[nodiscard]] constexpr LineageEntry<Attr> unknown_entry(std::type_identity<LineageEntry<Attr>>) noexcept
    {
        return unknown_lineage<Attr>();
    }

    /// Whether @p T is a `LineageEntry`.
    template <typename T>
    inline constexpr bool isLineageEntry = false;

    template <typename Attr>
    inline constexpr bool isLineageEntry<LineageEntry<Attr>> = true;

    /// The attribute of a lineage entry; `void` for anything else, so that a
    /// record whose pack holds something else is refused once, by
    /// `RequireLineageEntries`, and not again by every use of its attributes.
    template <typename T>
    struct AttributeOf
    {
        using type = void;
    };

    template <typename Attr>
    struct AttributeOf<LineageEntry<Attr>>
    {
        using type = Attr;
    };

    /// Fails to compile when a record is given something other than lineage
    /// entries after its environment.
    template <typename... Lineage>
    struct RequireLineageEntries
    {
        static_assert((... && isLineageEntry<Lineage>),
                      "formula: a record takes only lineage entries after its environment, lineage<Attr>(key) or "
                      "unknown_lineage<Attr>() -- what it was given appears in this diagnostic as the template "
                      "arguments of RequireLineageEntries");

        static constexpr bool value = true;
    };

    /// Fails to compile when a record declares one attribute twice: with two
    /// keys for one attribute, a requirement on it would compare one of them,
    /// and which one would be a guess.
    template <typename... Attrs>
    struct RequireAttributesDeclaredOnce
    {
        template <typename Attr>
        static constexpr std::size_t occurrences = (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Attr, Attrs> });

        static_assert((... && (occurrences<Attrs> == 1)),
                      "formula: this record declares the same lineage attribute more than once; a requirement on "
                      "it would compare one of the keys, and which one would be a guess -- the attributes appear "
                      "in this diagnostic as the template arguments of RequireAttributesDeclaredOnce");

        static constexpr bool value = true;
    };

    /// Fails to compile when a record is asked for, or a requirement compares,
    /// an attribute the record does not declare. Names the attribute and the
    /// role.
    template <typename Attr, typename Role, bool Declared>
    struct RequireDeclaredAttribute
    {
        static_assert(Declared,
                      "formula: this lineage requirement compares an attribute the record does not declare; "
                      "declare it where the record is built, lineage<Attr>(key) or unknown_lineage<Attr>(), or, for "
                      "a record not yet made, in its type, Record<Role, Env, LineageEntry<Attr>...>::unbound() -- "
                      "the attribute and the record's role appear in this diagnostic as the template arguments of "
                      "RequireDeclaredAttribute");

        static constexpr bool value = true;
    };

    /// Fails to compile when a lineage requirement names no attribute: it
    /// would check nothing, and would read in the source as a check.
    template <typename... Attrs>
    struct RequireSomeAttribute
    {
        static_assert(sizeof...(Attrs) > 0,
                      "formula: this lineage requirement names no attribute, so it checks nothing while reading as "
                      "a check; name the attributes the two records must agree on, same_lineage<MaterialBatch>()");

        static constexpr bool value = true;
    };

    /// Fails to compile when a lineage requirement names one attribute twice.
    template <typename... Attrs>
    struct RequireAttributesNamedOnce
    {
        template <typename Attr>
        static constexpr std::size_t occurrences = (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Attr, Attrs> });

        static_assert((... && (occurrences<Attrs> == 1)),
                      "formula: this lineage requirement names the same attribute more than once; name each "
                      "attribute the two records must agree on once -- the attributes appear in this diagnostic as "
                      "the template arguments of RequireAttributesNamedOnce");

        static constexpr bool value = true;
    };
} // namespace detail

/// A requirement that the record a scope reads agrees with the @p Comparand
/// record on every attribute in @p Attrs. Built by `same_lineage<...>()`.
template <typename Comparand, typename... Attrs>
struct LineageRequirement
{
    static_assert(detail::RequireSomeAttribute<Attrs...>::value);
    static_assert(detail::RequireAttributesNamedOnce<Attrs...>::value);

    /// The role of the record compared with: `ThisRecord` unless `against<Role>` said otherwise.
    using comparand = Comparand;
};

/// Requires the record a scope reads to agree with this record on every
/// attribute named: `same_lineage<MaterialBatch, TestMethod>()`.
template <typename... Attrs>
[[nodiscard]] constexpr LineageRequirement<ThisRecord, Attrs...> same_lineage() noexcept
{
    return {};
}

/// Requires the record a scope reads to agree with the record playing
/// @p Role on every attribute named: `same_lineage<MaterialBatch>(against<PriorTest>)`.
template <typename... Attrs, typename Role>
[[nodiscard]] constexpr LineageRequirement<Role, Attrs...> same_lineage(Against<Role>) noexcept
{
    return {};
}

namespace detail
{
    /// Whether @p T is a `LineageRequirement`.
    template <typename T>
    inline constexpr bool isLineageRequirement = false;

    template <typename Comparand, typename... Attrs>
    inline constexpr bool isLineageRequirement<LineageRequirement<Comparand, Attrs...>> = true;
} // namespace detail

} // namespace formula
