// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Where a formula comes from.
///
/// `documented(expr, {...})` wraps an expression in a node that carries a
/// citation and is otherwise invisible: it forwards the dimension, it forwards
/// evaluation, and it composes like any other node. Wrapping therefore changes
/// neither the arithmetic nor the result, and a citation stays reachable from a
/// composed root however deeply the formula is nested.
///
/// This header deliberately holds no rendering and no string building. A
/// consumer who only evaluates numbers includes it through the umbrella and
/// pays nothing for the documentation layer.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>

#include <string_view>

namespace formula
{

/// Where a formula came from, in the words of whoever wrote it down.
///
/// The fields are `std::string_view`, not owning strings, because a citation is
/// written as string literals at the point the formula is declared and literals
/// have static storage. That keeps a `Citation` usable inside a `constexpr`
/// tree. A citation built from a runtime `std::string` is legal, but that
/// string must outlive every node holding the view.
///
/// Every field defaults, so a caller may name only the ones that apply -- a
/// designated initialiser cannot skip a field that has no default.
struct Citation
{
    /// What the formula is called, in prose: "Water/cement ratio".
    std::string_view title {};
    /// The document it comes from, however the citing organisation writes it.
    std::string_view reference {};
    /// The clause or section within that document.
    std::string_view section {};
    /// The equation number within that section.
    std::string_view equation {};
    /// The definition in full, for a reader who has not got the document.
    std::string_view text {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Citation const&) const noexcept = default;
};

/// An expression with a citation attached.
///
/// It forwards `dimension` rather than declaring one of its own, so a
/// dimensional error inside a documented formula is still caught where the
/// formula is written -- wrapping is not a way to smuggle one past the check.
template <Node Inner>
struct DocumentedNode: NodeBase
{
    /// The wrapped expression.
    Inner inner {};
    /// Where `inner` comes from.
    Citation citation {};

    /// Forwarded from `Inner` unchanged -- wrapping never alters the dimension.
    static constexpr Dimension dimension = Inner::dimension;
};

/// Attaches a citation: `documented(var<A> / var<B>, { .title = "…" })`.
///
/// The `Citation` parameter is deliberately **not** deduced. That is what lets
/// the call site write a braced designated initialiser, which was verified on
/// cl 19.51, clang-cl 22 and g++ 13.3 before this was written.
template <Node Inner>
[[nodiscard]] constexpr DocumentedNode<Inner> documented(Inner inner, Citation citation) noexcept
{
    return DocumentedNode<Inner> { {}, inner, citation };
}

/// Evaluating a documented expression evaluates what it documents. The wrapper
/// is invisible to arithmetic; only the documentation walk and, from phase 7,
/// the trace sink will notice it.
template <typename Rep = Rational, Node Inner, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(DocumentedNode<Inner> const& node,
                                                           Env const& environment) noexcept
{
    return checked_evaluate_si<Rep>(node.inner, environment);
}

} // namespace formula
