// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Outlier rejection: a sample transformer that removes determinations a
/// declared criterion finds too far from the rest, re-running the mean until
/// nothing more is rejected or a declared bound aborts it (T4, T6).
///
///     formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep,
///                               formula::AtMost<2>, formula::KeepAtLeast<4>>(
///         formula::series<Mass, 6>,
///         formula::deviation_from_mean(formula::rat(6, 100) * formula::pass_mean<Mass>),
///         formula::Verdict { "discard the determinations and repeat the test" },
///         formula::Citation { .title = "Example Standard", .section = "7.4" })
///
/// **It is a sample** (`SampleSource`), whose determinations are the
/// survivors: `sample_mean(without_outliers<…>(…))` is an ordinary `Node`.
/// It is neither a `Node` nor a series -- how many determinations survive is
/// known only once it has run.
///
/// **The fixed point.** Each pass takes the current determinations' mean
/// (and, for `deviation_in_stddevs`, their sample variance), binds
/// `pass_mean` and `pass_count` for the limit expression, evaluates the limit
/// **once**, and compares every determination's statistic with it. It then
/// rejects the most extreme candidate -- every candidate tied for most
/// extreme, so that the result never depends on the order of entry -- or
/// every exceeding one (`PerPass`), and runs again, until a pass rejects
/// nothing.
///
/// **Every parameter that shapes the result is required:** `PerPass`,
/// `OnLimit` (an element exactly on the limit kept or rejected), `AtMost<k>`
/// (the most elements rejected in total) and `KeepAtLeast<m>` (the fewest
/// that may remain). Each count is its own type, so the two cannot be
/// swapped without a message.
///
/// **Termination is guaranteed by the type.** Every pass but the last
/// removes at least one determination and at most k are ever removed, so at
/// most k + 1 passes run; the loop is bounded by that, and needs no cap of
/// its own.
///
/// **Abort.** When the next rejection would exceed k, or leave fewer than m,
/// nothing more is rejected: the trace records the author's `Verdict`,
/// naming the determinations that would have gone, and anything reduced from
/// the rejection fails with `DomainError` (the scalar channel carries the
/// enum; S8's precedent). `checked_evaluate_rejection` returns the whole
/// result -- the mean of the survivors, or the verdict, the rejected
/// positions and the passes -- in a `RejectionOutcome` only the library
/// builds.
///
/// **Exact only.** A rejection decides, so it evaluates in `Rational` only
/// (S15): a comparison a few ULPs off would move a determination across the
/// limit, not a result by a few ULPs.
///
/// **Absence is strict** (T2): one absent determination and no pass runs;
/// the outcome is empty.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/statistics.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>

namespace formula
{

/// How many candidates one pass rejects.
enum class PerPass : std::uint8_t
{
    /// The most extreme -- every candidate tied for most extreme.
    MostExtreme,
    /// Every candidate that exceeds the limit.
    EveryExceeding,
};

/// What becomes of a determination exactly on the limit.
enum class OnLimit : std::uint8_t
{
    /// Kept: only a determination beyond the limit is a candidate.
    Keep,
    /// Rejected: a determination on the limit is a candidate too.
    Reject,
};

/// The most determinations a rejection may reject in total, k >= 1.
template <std::size_t K>
struct AtMost
{
    /// k.
    static constexpr std::size_t value = K;
};

/// The fewest determinations a rejection may leave, m >= 1.
template <std::size_t M>
struct KeepAtLeast
{
    /// m.
    static constexpr std::size_t value = M;
};

/// Which statistic a criterion compares with its limit.
enum class CriterionKind : std::uint8_t
{
    /// abs(x - mean), against a limit in the determinations' dimension.
    DeviationFromMean,
    /// abs(x - mean) / s, against a bare-number limit, decided by squares.
    DeviationInStddevs,
};

// ------------------------------------------------------------ placeholders

/// The current pass's mean, read inside a rejection's limit expression: a
/// relative tolerance is `rat(6, 100) * pass_mean<Mass>`. `Q` names the
/// quantity the mean is a value of.
template <Described Q>
struct PassMeanNode: NodeBase
{
    /// The quantity the mean is a value of.
    using quantity = Q;
    /// The mean measures what `Q` measures.
    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The current pass's mean: `pass_mean<Mass>`.
template <Described Q>
inline constexpr PassMeanNode<Q> pass_mean {};

/// The current pass's number of determinations, read inside a rejection's
/// limit expression: a table keyed by it is
/// `critical_value<Sizes, unit::One>(pass_count, {...})`.
struct PassCountNode: NodeBase
{
    /// A count is a bare number.
    static constexpr Dimension dimension = dim::Scalar;
};

/// The current pass's number of determinations.
inline constexpr PassCountNode pass_count {};

// ---------------------------------------------------------------- criteria

/// The deviation from the pass's mean, abs(x - mean), against @p Limit, which
/// measures what the determinations measure.
template <Node Limit>
struct DeviationFromMean
{
    /// The limit expression, evaluated once per pass. No `{}` initialiser:
    /// see `Corrections` (`lookup.hpp`).
    Limit limit;

    /// Which statistic this is.
    static constexpr CriterionKind kind = CriterionKind::DeviationFromMean;
};

/// The deviation from the pass's mean in sample standard deviations,
/// abs(x - mean) / s, against the bare number @p Limit -- decided exactly as
/// (x - mean)^2 against limit^2 * s^2, with no square root.
template <Node Limit>
struct DeviationInStddevs
{
    /// The limit expression, evaluated once per pass. No `{}` initialiser:
    /// see `Corrections` (`lookup.hpp`).
    Limit limit;

    /// Which statistic this is.
    static constexpr CriterionKind kind = CriterionKind::DeviationInStddevs;
};

/// abs(x - pass mean) against @p limitExpression: `deviation_from_mean(rat(6, 100) * pass_mean<Mass>)`.
template <Node Limit>
[[nodiscard]] constexpr DeviationFromMean<Limit> deviation_from_mean(Limit limitExpression) noexcept
{
    return DeviationFromMean<Limit> { limitExpression };
}

/// abs(x - pass mean) / s against @p limitExpression: `deviation_in_stddevs(rat(7, 4))`.
template <Node Limit>
[[nodiscard]] constexpr DeviationInStddevs<Limit> deviation_in_stddevs(Limit limitExpression) noexcept
{
    return DeviationInStddevs<Limit> { limitExpression };
}

namespace detail
{
    /// Whether @p T is one of this header's criteria.
    template <typename T>
    inline constexpr bool is_criterion = false;

    template <Node Limit>
    inline constexpr bool is_criterion<DeviationFromMean<Limit>> = true;

    template <Node Limit>
    inline constexpr bool is_criterion<DeviationInStddevs<Limit>> = true;

    /// Whether @p T is `AtMost<k>`, and whether it is `KeepAtLeast<m>`.
    template <typename T>
    inline constexpr bool is_at_most = false;

    template <std::size_t K>
    inline constexpr bool is_at_most<AtMost<K>> = true;

    template <typename T>
    inline constexpr bool is_keep_at_least = false;

    template <std::size_t M>
    inline constexpr bool is_keep_at_least<KeepAtLeast<M>> = true;

    /// The count a bound declares, read only through here: 1 for anything
    /// that is not `AtMost<k>` or `KeepAtLeast<m>`, which the order check
    /// refuses in its own words -- so that `without_outliers<…, int, …>`
    /// draws that one message, not a cascade of "`int` has no `value`".
    template <typename Bound>
    inline constexpr std::size_t bound_value = 1;

    template <std::size_t K>
    inline constexpr std::size_t bound_value<AtMost<K>> = K;

    template <std::size_t M>
    inline constexpr std::size_t bound_value<KeepAtLeast<M>> = M;

    /// Fails to compile unless the two bounds are `AtMost<k>` and then
    /// `KeepAtLeast<m>`: swapped, `without_outliers<…, KeepAtLeast<4>,
    /// AtMost<2>>` would otherwise reject up to four and keep two.
    template <typename AtMostT, typename KeepAtLeastT>
    struct RequireBoundsInOrder
    {
        static_assert(is_at_most<AtMostT> && is_keep_at_least<KeepAtLeastT>,
                      "formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order; the two "
                      "bounds given appear in this diagnostic as the template arguments of RequireBoundsInOrder -- "
                      "swapped, or not these two types");

        static constexpr bool value = true;
    };

    /// Fails to compile when `AtMost<0>` is declared: a rejection that may
    /// reject nothing is no rejection. Asked only once the bounds are in
    /// order (@p InOrder).
    template <std::size_t K, bool InOrder>
    struct RequireAtMostPositive
    {
        static_assert(!InOrder || K >= 1,
                      "formula: without_outliers declares AtMost<0>; a rejection that may reject nothing is no "
                      "rejection -- declare AtMost<k> with k at least 1, or drop the rejection");

        static constexpr bool value = true;
    };

    /// Fails to compile when `KeepAtLeast<0>` is declared: a rejection may
    /// never leave an empty sample, whose mean does not exist.
    template <std::size_t M, bool InOrder>
    struct RequireKeepAtLeastPositive
    {
        static_assert(!InOrder || M >= 1,
                      "formula: without_outliers declares KeepAtLeast<0>; a rejection may never leave an empty "
                      "sample, which has no mean -- declare KeepAtLeast<m> with m at least 1");

        static constexpr bool value = true;
    };

    /// Whether @p Criterion's limit measures what it must: the
    /// determinations' dimension @p D for `deviation_from_mean`, a bare
    /// number for `deviation_in_stddevs`.
    template <typename Criterion, Dimension D>
    [[nodiscard]] consteval bool criterion_limit_dimension_matches() noexcept
    {
        using Limit = std::remove_cvref_t<decltype(std::declval<Criterion>().limit)>;
        if constexpr (Criterion::kind == CriterionKind::DeviationFromMean)
            return Limit::dimension == D;
        else
            return Limit::dimension == dim::Scalar;
    }

    /// Fails to compile when a criterion's limit measures the wrong thing:
    /// a mass compared with a length, or a count of standard deviations
    /// given in grams. Gated behind the bounds (@p Asked).
    template <typename Criterion, Dimension D, bool Asked>
    struct RequireCriterionLimitDimension
    {
        static_assert(!Asked || criterion_limit_dimension_matches<Criterion, D>(),
                      "formula: this rejection's limit does not measure what its criterion compares -- "
                      "deviation_from_mean needs a limit in the determinations' dimension, deviation_in_stddevs "
                      "a bare number of standard deviations; the criterion appears in this diagnostic as the "
                      "template argument of RequireCriterionLimitDimension");

        static constexpr bool value = true;
    };

    /// Whether @p N is a pass placeholder.
    template <typename N>
    inline constexpr bool is_pass_placeholder = false;

    template <Described Q>
    inline constexpr bool is_pass_placeholder<PassMeanNode<Q>> = true;

    template <>
    inline constexpr bool is_pass_placeholder<PassCountNode> = true;

    template <typename N>
    [[nodiscard]] consteval bool mentions_pass_placeholder() noexcept;

    template <typename... Children>
    [[nodiscard]] consteval bool any_mentions_pass_placeholder(std::tuple<Children...> const*) noexcept
    {
        return (mentions_pass_placeholder<Children>() || ...);
    }

    /// Whether a `pass_mean` or `pass_count` no rejection inside @p N binds
    /// appears in it -- walked through `LevelChildren`, which lists every
    /// library kind's children, a bound limit expression included; a nested
    /// rejection's own limit binds its own placeholders (`sampleChildren`).
    template <typename N>
    [[nodiscard]] consteval bool mentions_pass_placeholder() noexcept
    {
        using Bare = std::remove_cv_t<N>;
        using Children = LevelChildren<Bare>;
        if constexpr (is_pass_placeholder<Bare>)
            return true;
        else if constexpr (requires { typename Children::sampleChildren; })
            return any_mentions_pass_placeholder(static_cast<typename Children::sampleChildren const*>(nullptr));
        else if constexpr (requires { typename Children::bound; })
            return any_mentions_pass_placeholder(static_cast<typename Children::type const*>(nullptr))
                   || any_mentions_pass_placeholder(static_cast<typename Children::bound const*>(nullptr));
        else
            return any_mentions_pass_placeholder(static_cast<typename Children::type const*>(nullptr));
    }

    /// Fails to compile when the sample a rejection reads mentions
    /// `pass_mean` or `pass_count`: the sample is what the passes are taken
    /// over, and cannot depend on a pass.
    template <typename S, bool Asked>
    struct RequireSampleWithoutPassPlaceholder
    {
        static_assert(!Asked || !mentions_pass_placeholder<S>(),
                      "formula: the sample this rejection reads mentions pass_mean or pass_count; the sample is "
                      "what the passes are taken over and cannot depend on one -- read the pass only in the "
                      "limit expression; the sample appears in this diagnostic as the template argument of "
                      "RequireSampleWithoutPassPlaceholder");

        static constexpr bool value = true;
    };

    template <typename N, Dimension D>
    [[nodiscard]] consteval bool pass_means_measure() noexcept;

    template <Dimension D, typename... Children>
    [[nodiscard]] consteval bool all_pass_means_measure(std::tuple<Children...> const*) noexcept
    {
        return (pass_means_measure<Children, D>() && ...);
    }

    /// Whether every `pass_mean<Q>` free in @p N measures @p D.
    template <typename N, Dimension D>
    [[nodiscard]] consteval bool pass_means_measure() noexcept
    {
        using Bare = std::remove_cv_t<N>;
        using Children = LevelChildren<Bare>;
        if constexpr (is_pass_placeholder<Bare>)
            return Bare::dimension == D || std::is_same_v<Bare, PassCountNode>;
        else if constexpr (requires { typename Children::sampleChildren; })
            return all_pass_means_measure<D>(static_cast<typename Children::sampleChildren const*>(nullptr));
        else if constexpr (requires { typename Children::bound; })
            return all_pass_means_measure<D>(static_cast<typename Children::type const*>(nullptr))
                   && all_pass_means_measure<D>(static_cast<typename Children::bound const*>(nullptr));
        else
            return all_pass_means_measure<D>(static_cast<typename Children::type const*>(nullptr));
    }

    /// Fails to compile when a `pass_mean<Q>` in the limit names a quantity
    /// that does not measure the determinations' dimension.
    template <typename Limit, Dimension D, bool Asked>
    struct RequirePassMeanDimension
    {
        static_assert(!Asked || pass_means_measure<Limit, D>(),
                      "formula: this rejection's limit reads a pass_mean whose quantity does not measure the "
                      "determinations' dimension; name a quantity of that dimension; the limit appears in this "
                      "diagnostic as the template argument of RequirePassMeanDimension");

        static constexpr bool value = true;
    };
} // namespace detail

/// A rejection of outliers from @p S under @p Criterion: see the file comment.
/// Built by `without_outliers`. A public aggregate: its verdict and citation
/// are the author's declaration, as a `Constraint`'s are, and every check
/// sits in the class body, so a node declared without the factory is refused
/// as well.
template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, SampleSource S, typename Criterion>
struct RejectionNode
{
    static_assert(detail::RequireBoundsInOrder<AtMostT, KeepAtLeastT>::value);

    /// Whether the bounds are in order: every check after this one is asked
    /// only then, so a swap draws one message.
    static constexpr bool boundsInOrder = detail::is_at_most<AtMostT> && detail::is_keep_at_least<KeepAtLeastT>;

    static_assert(detail::RequireAtMostPositive<detail::bound_value<AtMostT>, boundsInOrder>::value);
    static_assert(detail::RequireKeepAtLeastPositive<detail::bound_value<KeepAtLeastT>, boundsInOrder>::value);
    static_assert(detail::RequireCriterionLimitDimension<Criterion, S::dimension, boundsInOrder>::value);
    static_assert(detail::RequireSampleWithoutPassPlaceholder<S, boundsInOrder>::value);
    static_assert(detail::RequirePassMeanDimension<
                  decltype(std::declval<Criterion>().limit),
                  S::dimension,
                  boundsInOrder && detail::criterion_limit_dimension_matches<Criterion, S::dimension>()>::value);

    /// The sample outliers are rejected from. No `{}` initialiser: see
    /// `Corrections` (`lookup.hpp`).
    S sample;
    /// What decides a candidate. No `{}` initialiser, for the same reason.
    Criterion criterion;
    /// What the author declares when the bound is reached.
    Verdict verdict {};
    /// Where the rule comes from.
    Citation citation {};

    /// How many candidates one pass rejects.
    static constexpr PerPass perPass = P;
    /// What becomes of a determination exactly on the limit.
    static constexpr OnLimit onLimit = L;
    /// k: the most rejected in total.
    static constexpr std::size_t atMost = detail::bound_value<AtMostT>;
    /// m: the fewest that may remain.
    static constexpr std::size_t keepAtLeast = detail::bound_value<KeepAtLeastT>;
    /// As many determinations as the sample can hold.
    static constexpr std::size_t capacity = detail::sample_capacity<S>;
    /// The survivors measure what the sample measures.
    static constexpr Dimension dimension = S::dimension;
};

namespace detail
{
    template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    inline constexpr bool is_sample_transformer<RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion>> = true;

    /// Whether @p T is a `RejectionNode`.
    template <typename T>
    inline constexpr bool is_rejection_node = false;

    template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    inline constexpr bool is_rejection_node<RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion>> = true;

    /// Fails to compile when `without_outliers` is given a single value.
    template <typename Operand>
    struct RequireRejectionOfSample
    {
        static_assert(SampleSource<Operand>,
                      "formula: without_outliers rejects determinations from a sample (a series or observations), "
                      "and this is a single value; the operand appears in this diagnostic as the template argument "
                      "of RequireRejectionOfSample -- read a quantity determined several times with series<Q, N>");

        static constexpr bool value = true;
    };

    // The placeholders and the rejection, seen by the level checks
    // (`precision.hpp`) and walked for pass placeholders above.
    template <Described Q>
    struct LevelChildren<PassMeanNode<Q>>: LevelLeaf
    {
    };

    template <>
    struct LevelChildren<PassCountNode>: LevelLeaf
    {
    };

    template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    struct LevelChildren<RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion>>:
        LevelParent<S, std::remove_cvref_t<decltype(std::declval<Criterion>().limit)>>
    {
        /// What a pass placeholder may not appear in: the sample alone, since
        /// the limit binds its own.
        using sampleChildren = std::tuple<S>;
    };
} // namespace detail

/// A rejection of outliers from @p sampleSource: see the file comment. Every
/// template argument is required.
template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, SampleSource S, typename Criterion>
    requires detail::is_criterion<Criterion>
[[nodiscard]] constexpr auto without_outliers(S sampleSource,
                                              Criterion criterion,
                                              Verdict declared,
                                              Citation cited = {}) noexcept
{
    return RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> { sampleSource, criterion, declared, cited };
}

/// A single value handed to `without_outliers`: refused in this library's
/// words. It returns a rejection over a series already refused
/// (`detail::RefusedSeries`), so that a statistic over it adds nothing.
template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, Node N, typename Criterion>
    requires detail::is_criterion<Criterion>
[[nodiscard]] constexpr auto without_outliers(N, Criterion criterion, Verdict declared, Citation cited = {}) noexcept
{
    static_assert(detail::RequireRejectionOfSample<N>::value);
    return RejectionNode<P, L, AtMostT, KeepAtLeastT, detail::RefusedSeries<N::dimension>, Criterion> {
        detail::RefusedSeries<N::dimension> {}, criterion, declared, cited
    };
}

// ---------------------------------------------------------------- outcome

/// A rejected determination: its zero-based position in the sample as
/// entered, and the pass, counted from one, that rejected it.
struct RejectedElement
{
    /// Where it stood in the sample as entered.
    std::size_t position;
    /// The pass that rejected it, from 1.
    std::size_t pass;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(RejectedElement const&) const noexcept = default;
};

namespace detail
{
    struct RejectionOutcomeAccess;
} // namespace detail

/// The whole result of a rejection: the mean of the survivors -- or the
/// author's verdict, when the bound was reached, or nothing, when a
/// determination is absent -- the rejected positions, the survivors and the
/// passes run. **Only the library builds one** (`checked_evaluate_rejection`),
/// so what it says is what the rejection did.
template <Described Q, std::size_t C>
class RejectionOutcome
{
  public:
    /// The mean of the survivors in `Q`'s unit (`Derived`) on a settled run;
    /// the verdict on an abort; empty when a determination is absent.
    [[nodiscard]] constexpr Outcome<Q> outcome() const noexcept
    {
        return _outcome;
    }

    /// The rejected determinations, in the order they were rejected.
    [[nodiscard]] constexpr std::span<RejectedElement const> rejected() const noexcept
    {
        return std::span<RejectedElement const> { _rejected.data(), _rejectedCount };
    }

    /// The positions of the determinations that remain, in order of entry.
    [[nodiscard]] constexpr std::span<std::size_t const> survivors() const noexcept
    {
        return std::span<std::size_t const> { _survivors.data(), _survivorCount };
    }

    /// How many passes ran: 0 when a determination is absent.
    [[nodiscard]] constexpr std::size_t passes() const noexcept
    {
        return _passes;
    }

  private:
    friend struct detail::RejectionOutcomeAccess;

    constexpr RejectionOutcome(Outcome<Q> produced) noexcept:
        _outcome { produced }
    {
        for (RejectedElement& each: _rejected)
            each = RejectedElement { 0, 0 };
        for (std::size_t& each: _survivors)
            each = 0;
    }

    Outcome<Q> _outcome;
    std::array<RejectedElement, C> _rejected;
    std::size_t _rejectedCount = 0;
    std::array<std::size_t, C> _survivors;
    std::size_t _survivorCount = 0;
    std::size_t _passes = 0;
};

namespace detail
{
    /// How a rejection ended.
    enum class RejectionEnd : std::uint8_t
    {
        /// A pass rejected nothing.
        Settled,
        /// The next rejection would have passed a bound.
        Aborted,
        /// A determination, or the limit, was absent: no decision.
        Absent,
        /// The sample, or a pass's arithmetic, failed.
        Failed,
    };

    /// What one pass told the sink: its number, the sample size and mean it
    /// ran at, and the limit it evaluated (absent, or an error, when it did
    /// not produce one).
    struct RejectionPassEvent
    {
        std::size_t pass;
        std::size_t sampleSize;
        std::optional<Rational> passMean;
        Evaluated<Rational> limit;
        std::optional<ArithmeticError> error;
    };

    /// One rejected determination, as the sink is told of it: every number
    /// the decision used, exactly -- for `deviation_in_stddevs`, the squared
    /// deviation and limit^2 * s^2.
    struct OutlierEvent
    {
        std::size_t pass;
        std::size_t position;
        std::size_t originalSize;
        Rational rejectedValue;
        Rational statistic;
        Rational limit;
        bool squared;
        CriterionKind criterion;
        OnLimit onLimit;
    };

    /// How the rejection ended, as the sink is told of it.
    struct RejectionEndEvent
    {
        RejectionEnd end;
        std::size_t originalSize;
        std::size_t rejectedCount;
        std::size_t remaining;
        /// For an abort: the positions that would have been rejected.
        std::span<std::size_t const> wouldReject;
        /// For an abort: whether it was `KeepAtLeast`, not `AtMost`, that the
        /// next rejection would have passed.
        bool belowKeepAtLeast;
        std::size_t atMost;
        std::size_t keepAtLeast;
        Verdict verdict;
        Citation citation;
    };

    /// Whether @p Sink wants to hear a rejection's passes: true when it
    /// defines **every** rejection hook, as `variant_entered` is asked for.
    template <typename Sink>
    concept HearsRejection =
        requires(Sink sink, RejectionPassEvent const& pass, OutlierEvent const& outlier, RejectionEndEvent const& end) {
            sink.rejection_entered();
            sink.rejection_pass_entered();
            sink.rejection_pass_produced(pass);
            sink.outlier_rejected(outlier);
            sink.rejection_finished(end);
        };

    /// The pass bindings: the current pass's mean, in the coherent SI unit of
    /// @p D, and its size -- what `pass_mean` and `pass_count` read.
    template <typename Rep, Dimension D>
    struct PassBinding
    {
        /// The pass's mean.
        Rep passMean;
        /// The pass's number of determinations.
        std::size_t passCount;
    };

    template <typename Rep, typename... Bindings>
    struct FirstPassIn
    {
        static constexpr bool found = false;
        static constexpr Dimension dimension {};
    };

    template <typename Rep, Dimension D, typename... Rest>
    struct FirstPassIn<Rep, PassBinding<Rep, D>, Rest...>
    {
        static constexpr bool found = true;
        static constexpr Dimension dimension = D;
    };

    template <typename Rep, typename Binding, typename... Rest>
    struct FirstPassIn<Rep, Binding, Rest...>: FirstPassIn<Rep, Rest...>
    {
    };

    /// Whether @p Env binds a pass, and the innermost one's dimension.
    template <typename Rep, typename Env>
    struct InnermostPass
    {
        static constexpr bool found = false;
        static constexpr Dimension dimension {};
    };

    template <typename Rep, typename Env, typename... Bindings>
    struct InnermostPass<Rep, BoundEnvironment<Env, Bindings...>>:
        std::conditional_t<FirstPassIn<Rep, Bindings...>::found, FirstPassIn<Rep, Bindings...>, InnermostPass<Rep, Env>>
    {
    };

    /// Fails to compile when a pass placeholder is evaluated where no
    /// rejection binds it.
    template <typename Env>
    struct RequirePassBound
    {
        static constexpr bool bound = false;
        static_assert(bound,
                      "formula: pass_mean and pass_count are meaningful only inside the limit expression of "
                      "without_outliers; the environment it was evaluated in appears in this diagnostic as the "
                      "template argument of RequirePassBound");

        static constexpr bool value = true;
    };

    /// Fails to compile when a rejection is evaluated in a representation
    /// other than `Rational`: it decides, and a decision is exact (S15).
    template <typename Rep>
    struct RequireRationalRejection
    {
        static_assert(std::is_same_v<Rep, Rational>,
                      "formula: a rejection of outliers can only be evaluated with Rep = Rational; it decides "
                      "which determinations remain, and a comparison a few ULPs off would move one across the "
                      "limit");

        static constexpr bool value = true;
    };
} // namespace detail

/// Reads the innermost rejection's pass mean. Refused, in one message,
/// anywhere no rejection binds one.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PassMeanNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    using Innermost = detail::InnermostPass<Rep, Env>;
    if constexpr (!Innermost::found)
    {
        static_assert(detail::RequirePassBound<Env>::value);
        return detail::nothing<Rep>();
    }
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated =
            detail::present<Rep>(environment.template bound<detail::PassBinding<Rep, Innermost::dimension>>().passMean);
        sink.produced(node, evaluated);
        return evaluated;
    }
}

/// Reads the innermost rejection's pass size. Refused, in one message,
/// anywhere no rejection binds one.
template <typename Rep = Rational, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PassCountNode const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    using Innermost = detail::InnermostPass<Rep, Env>;
    if constexpr (!Innermost::found)
    {
        static_assert(detail::RequirePassBound<Env>::value);
        return detail::nothing<Rep>();
    }
    else
    {
        sink.entered(node);
        std::expected<Rep, ArithmeticError> const counted = RepTraits<Rep>::from(Rational { static_cast<std::int64_t>(
            environment.template bound<detail::PassBinding<Rep, Innermost::dimension>>().passCount) });
        Evaluated<Rep> const evaluated =
            counted.has_value() ? detail::present<Rep>(*counted) : Evaluated<Rep> { std::unexpected { counted.error() } };
        sink.produced(node, evaluated);
        return evaluated;
    }
}

namespace detail
{
    /// A rejection, run: how it ended, the survivors, the rejected
    /// determinations in rejection order, and the passes.
    template <std::size_t C>
    struct RejectionRun
    {
        RejectionEnd end;
        std::optional<SeriesFailure> failure;
        SampleValue<Rational, C> survivors;
        std::array<RejectedElement, C> rejected;
        std::size_t rejectedCount;
        std::size_t passes;
    };

    /// Evaluates a rejection's sample -- unless it mentions a pass
    /// placeholder, which the node's own check has refused already
    /// (`RequireSampleWithoutPassPlaceholder`): evaluating it would refuse
    /// the placeholder a second time, unbound, and one mistake would draw two
    /// messages.
    template <typename S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSample<Rational, sample_capacity<S>> dispatch_rejected_sample(S const& node,
                                                                                                   Env const& environment,
                                                                                                   Sink sink) noexcept
    {
        if constexpr (mentions_pass_placeholder<S>())
            return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
        else
            return dispatch_sample<Rational>(node, environment, sink);
    }

    /// Runs T4's loop over a working copy of the sample. See the file comment.
    template <PerPass P,
              OnLimit L,
              typename AtMostT,
              typename KeepAtLeastT,
              typename S,
              typename Criterion,
              typename Env,
              typename Sink>
    [[nodiscard]] constexpr RejectionRun<sample_capacity<S>> run_rejection(
        RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node, Env const& environment, Sink sink) noexcept
    {
        constexpr std::size_t sampleCapacity = sample_capacity<S>;
        constexpr bool hears = HearsRejection<Sink>;
        constexpr Dimension sampleDimension = S::dimension;

        RejectionRun<sampleCapacity> run;
        run.end = RejectionEnd::Settled;
        run.rejectedCount = 0;
        run.passes = 0;
        run.survivors.count = 0;
        for (RejectedElement& each: run.rejected)
            each = RejectedElement { 0, 0 };
        for (std::size_t& each: run.survivors.positions)
            each = 0;

        if constexpr (hears)
            sink.rejection_entered();

        std::array<std::size_t, sampleCapacity> wouldReject;
        for (std::size_t& each: wouldReject)
            each = 0;
        std::size_t wouldRejectCount = 0;
        bool belowKeepAtLeast = false;

        auto const finish = [&](RejectionEnd ending) {
            run.end = ending;
            if constexpr (hears)
                sink.rejection_finished(
                    RejectionEndEvent { .end = ending,
                                        .originalSize = sampleCapacity,
                                        .rejectedCount = run.rejectedCount,
                                        .remaining = run.survivors.count,
                                        .wouldReject = std::span<std::size_t const> { wouldReject.data(), wouldRejectCount },
                                        .belowKeepAtLeast = belowKeepAtLeast,
                                        .atMost = bound_value<AtMostT>,
                                        .keepAtLeast = bound_value<KeepAtLeastT>,
                                        .verdict = node.verdict,
                                        .citation = node.citation });
            return run;
        };
        auto const fail = [&](ArithmeticError failed, std::optional<std::size_t> at) {
            run.failure = SeriesFailure { failed, at };
            return finish(RejectionEnd::Failed);
        };

        EvaluatedSample<Rational, sampleCapacity> const sampled = dispatch_rejected_sample(node.sample, environment, sink);
        if (!sampled.has_value())
        {
            run.failure = sampled.error();
            return finish(RejectionEnd::Failed);
        }
        if (!sampled->has_value())
            return finish(RejectionEnd::Absent);
        run.survivors = **sampled;

        // At most k + 1 passes: every pass but the last removes at least one
        // determination, and at most k are ever removed (AtMost). This bound
        // is the whole termination argument; no second cap is needed.
        for (std::size_t passNumber = 1; passNumber <= bound_value<AtMostT> + 1; ++passNumber)
        {
            run.passes = passNumber;
            SampleValue<Rational, sampleCapacity>& working = run.survivors;
            std::size_t const passSize = working.count;

            std::optional<std::size_t> failedAt;
            std::expected<Rational, ArithmeticError> const passMean = mean_of(working, failedAt);

            // The sample variance, for a criterion in standard deviations: over
            // n - 1, the candidate included. Fewer than three determinations
            // is refused: two are always equidistant from their mean, so every
            // such pass would be a tie that empties the sample.
            std::expected<Rational, ArithmeticError> variance = Rational { 0 };
            if constexpr (Criterion::kind == CriterionKind::DeviationInStddevs)
            {
                if (passSize < 3)
                    variance = std::unexpected { ArithmeticError::DomainError };
                else if (passMean.has_value())
                {
                    std::optional<Rational> squaresTotal;
                    for (std::size_t taken = 0; taken < passSize && variance.has_value(); ++taken)
                    {
                        std::expected<Rational, ArithmeticError> const deviation =
                            RepTraits<Rational>::subtract(working.values[taken], *passMean);
                        std::expected<Rational, ArithmeticError> const squared =
                            deviation.has_value() ? RepTraits<Rational>::multiply(*deviation, *deviation) : deviation;
                        std::expected<Rational, ArithmeticError> const added =
                            !squared.has_value() || !squaresTotal.has_value()
                                ? squared
                                : RepTraits<Rational>::add(*squaresTotal, *squared);
                        if (!added.has_value())
                        {
                            failedAt = working.positions[taken];
                            variance = std::unexpected { added.error() };
                        }
                        else
                            squaresTotal = *added;
                    }
                    if (variance.has_value())
                        variance =
                            RepTraits<Rational>::divide(*squaresTotal, Rational { static_cast<std::int64_t>(passSize - 1) });
                }
            }

            std::optional<ArithmeticError> const passError =
                !passMean.has_value()   ? std::optional<ArithmeticError> { passMean.error() }
                : !variance.has_value() ? std::optional<ArithmeticError> { variance.error() }
                                        : std::nullopt;
            if (passError.has_value())
            {
                if constexpr (hears)
                {
                    sink.rejection_pass_entered();
                    sink.rejection_pass_produced(RejectionPassEvent {
                        .pass = passNumber,
                        .sampleSize = passSize,
                        .passMean = passMean.has_value() ? std::optional<Rational> { *passMean } : std::nullopt,
                        .limit = Evaluated<Rational> { std::unexpected { *passError } },
                        .error = passError });
                }
                return fail(*passError, failedAt);
            }

            // The limit, once per pass, with the pass bound.
            if constexpr (hears)
                sink.rejection_pass_entered();
            using Binding = PassBinding<Rational, sampleDimension>;
            BoundEnvironment<Env, Binding> const inPass { environment, Binding { *passMean, passSize } };
            Evaluated<Rational> const evaluatedLimit = dispatch<Rational>(node.criterion.limit, inPass, sink);
            if constexpr (hears)
                sink.rejection_pass_produced(RejectionPassEvent { .pass = passNumber,
                                                                  .sampleSize = passSize,
                                                                  .passMean = *passMean,
                                                                  .limit = evaluatedLimit,
                                                                  .error = std::nullopt });
            if (!evaluatedLimit.has_value())
                return fail(evaluatedLimit.error(), std::nullopt);
            if (!evaluatedLimit->has_value())
                return finish(RejectionEnd::Absent);

            // Every determination equal: no outlier, whatever the criterion --
            // never a division by zero, never every element "on" a zero limit.
            bool allEqual = true;
            for (std::size_t taken = 1; taken < passSize; ++taken)
                if (!(working.values[taken] == working.values[0]))
                    allEqual = false;
            if (allEqual)
                return finish(RejectionEnd::Settled);

            // The threshold the statistic is compared with: the limit itself,
            // or limit^2 * s^2 for a criterion in standard deviations.
            Rational compareAgainst = **evaluatedLimit;
            if constexpr (Criterion::kind == CriterionKind::DeviationInStddevs)
            {
                std::expected<Rational, ArithmeticError> const limitSquared =
                    RepTraits<Rational>::multiply(**evaluatedLimit, **evaluatedLimit);
                std::expected<Rational, ArithmeticError> const scaledLimit =
                    limitSquared.has_value() ? RepTraits<Rational>::multiply(*limitSquared, *variance) : limitSquared;
                if (!scaledLimit.has_value())
                    return fail(scaledLimit.error(), std::nullopt);
                compareAgainst = *scaledLimit;
            }

            // Each determination's statistics, and whether it is a candidate.
            std::array<Rational, sampleCapacity> statistics;
            std::array<bool, sampleCapacity> candidate;
            for (bool& each: candidate)
                each = false;
            std::optional<Rational> mostExtreme;
            for (std::size_t taken = 0; taken < passSize; ++taken)
            {
                std::expected<Rational, ArithmeticError> const deviation =
                    RepTraits<Rational>::subtract(working.values[taken], *passMean);
                if (!deviation.has_value())
                    return fail(deviation.error(), working.positions[taken]);
                std::expected<Rational, ArithmeticError> measured =
                    *deviation < Rational { 0 } ? RepTraits<Rational>::negate(*deviation)
                                                : std::expected<Rational, ArithmeticError> { *deviation };
                if constexpr (Criterion::kind == CriterionKind::DeviationInStddevs)
                    measured = measured.has_value() ? RepTraits<Rational>::multiply(*deviation, *deviation) : measured;
                if (!measured.has_value())
                    return fail(measured.error(), working.positions[taken]);
                statistics[taken] = *measured;
                candidate[taken] = L == OnLimit::Keep ? compareAgainst < *measured : !(*measured < compareAgainst);
                if (candidate[taken] && (!mostExtreme.has_value() || *mostExtreme < *measured))
                    mostExtreme = *measured;
            }
            if (!mostExtreme.has_value())
                return finish(RejectionEnd::Settled);

            // The candidates this pass rejects: every one tied for most
            // extreme, or every one -- judged on the whole candidate set, so
            // the order of entry decides nothing.
            std::size_t chosen = 0;
            for (std::size_t taken = 0; taken < passSize; ++taken)
            {
                if (P == PerPass::MostExtreme && candidate[taken] && !(statistics[taken] == *mostExtreme))
                    candidate[taken] = false;
                if (candidate[taken])
                    ++chosen;
            }

            // The bounds, before anything is removed.
            bool const pastAtMost = run.rejectedCount + chosen > bound_value<AtMostT>;
            bool const pastKeepAtLeast = passSize - chosen < bound_value<KeepAtLeastT>;
            if (pastAtMost || pastKeepAtLeast)
            {
                belowKeepAtLeast = !pastAtMost;
                for (std::size_t taken = 0; taken < passSize; ++taken)
                    if (candidate[taken])
                        wouldReject[wouldRejectCount++] = working.positions[taken];
                return finish(RejectionEnd::Aborted);
            }

            // Remove and record, keeping the survivors in order of entry.
            std::size_t kept = 0;
            for (std::size_t taken = 0; taken < passSize; ++taken)
            {
                if (candidate[taken])
                {
                    run.rejected[run.rejectedCount++] = RejectedElement { working.positions[taken], passNumber };
                    if constexpr (hears)
                        sink.outlier_rejected(OutlierEvent { .pass = passNumber,
                                                             .position = working.positions[taken],
                                                             .originalSize = sampleCapacity,
                                                             .rejectedValue = working.values[taken],
                                                             .statistic = statistics[taken],
                                                             .limit = compareAgainst,
                                                             .squared = Criterion::kind == CriterionKind::DeviationInStddevs,
                                                             .criterion = Criterion::kind,
                                                             .onLimit = L });
                }
                else
                {
                    working.values[kept] = working.values[taken];
                    working.positions[kept] = working.positions[taken];
                    ++kept;
                }
            }
            working.count = kept;
        }
        // Unreachable: pass k + 1 finds no candidate or aborts, since k are
        // already rejected. Settled keeps the function total.
        return finish(RejectionEnd::Settled);
    }

    /// Builds the `RejectionOutcome` a rejection's run describes; the one way
    /// to build one.
    struct RejectionOutcomeAccess
    {
        template <Described Q, std::size_t C>
        [[nodiscard]] static constexpr RejectionOutcome<Q, C> build(Outcome<Q> result, RejectionRun<C> const& run) noexcept
        {
            RejectionOutcome<Q, C> built { result };
            built._rejectedCount = run.rejectedCount;
            for (std::size_t at = 0; at < run.rejectedCount; ++at)
                built._rejected[at] = run.rejected[at];
            if (run.end == RejectionEnd::Settled || run.end == RejectionEnd::Aborted)
            {
                built._survivorCount = run.survivors.count;
                for (std::size_t at = 0; at < run.survivors.count; ++at)
                    built._survivors[at] = run.survivors.positions[at];
            }
            built._passes = run.passes;
            return built;
        }
    };
} // namespace detail

/// A rejection's survivors, as a sample: `dispatch_sample`'s path for a
/// rejection. An abort is `DomainError`, with no element to name -- the
/// verdict is in the trace.
template <typename Rep = Rational,
          PerPass P,
          OnLimit L,
          typename AtMostT,
          typename KeepAtLeastT,
          typename S,
          typename Criterion,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSample<Rep, detail::sample_capacity<S>> checked_evaluate_sample_si(
    RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t sampleCapacity = detail::sample_capacity<S>;
    static_assert(detail::RequireRationalRejection<Rep>::value);
    if constexpr (!std::is_same_v<Rep, Rational>)
        return std::optional<detail::SampleValue<Rep, sampleCapacity>> {};
    else
    {
        detail::RejectionRun<sampleCapacity> const run = detail::run_rejection(node, environment, sink);
        switch (run.end)
        {
            case detail::RejectionEnd::Settled:
                return std::optional<detail::SampleValue<Rational, sampleCapacity>> { run.survivors };
            case detail::RejectionEnd::Aborted:
                return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
            case detail::RejectionEnd::Absent:
                return std::optional<detail::SampleValue<Rational, sampleCapacity>> {};
            case detail::RejectionEnd::Failed:
                break;
        }
        return std::unexpected { run.failure.value_or(SeriesFailure { ArithmeticError::DomainError, std::nullopt }) };
    }
}

/// Evaluates a rejection for quantity @p Result: the whole result, as
/// `RejectionOutcome`. A failed sample or pass is the failure; anything else
/// -- settled, aborted, absent -- is an outcome.
template <Described Result,
          PerPass P,
          OnLimit L,
          typename AtMostT,
          typename KeepAtLeastT,
          typename S,
          typename Criterion,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<RejectionOutcome<Result, detail::sample_capacity<S>>, SeriesFailure>
checked_evaluate_rejection(RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& rejection,
                           Env const& environment,
                           Sink sink = {}) noexcept
{
    constexpr std::size_t sampleCapacity = detail::sample_capacity<S>;
    static_assert(Describe<Result>::dimension == S::dimension,
                  "formula: this result quantity does not measure what the rejection's determinations measure; "
                  "the quantity appears in this diagnostic as the template argument Result of "
                  "checked_evaluate_rejection");
    detail::RejectionRun<sampleCapacity> const run = detail::run_rejection(rejection, environment, sink);
    switch (run.end)
    {
        case detail::RejectionEnd::Failed:
            return std::unexpected { run.failure.value_or(SeriesFailure { ArithmeticError::DomainError, std::nullopt }) };
        case detail::RejectionEnd::Absent:
            return detail::RejectionOutcomeAccess::build(Outcome<Result>::empty(), run);
        case detail::RejectionEnd::Aborted:
            return detail::RejectionOutcomeAccess::build(Outcome<Result>::verdict(rejection.verdict), run);
        case detail::RejectionEnd::Settled:
            break;
    }
    std::optional<std::size_t> failedAt;
    std::expected<Rational, ArithmeticError> const survivorsMean = detail::mean_of(run.survivors, failedAt);
    if (!survivorsMean.has_value())
        return std::unexpected { SeriesFailure { survivorsMean.error(), failedAt } };
    std::expected<Rational, ArithmeticError> const inDeclaredUnit =
        checked_convert(*survivorsMean, coherent(S::dimension), Describe<Result>::unit);
    if (!inDeclaredUnit.has_value())
        return std::unexpected { SeriesFailure { inDeclaredUnit.error(), std::nullopt } };
    return detail::RejectionOutcomeAccess::build(
        Outcome<Result>::value(Measured<Result> { *inDeclaredUnit }, ValueSource::Derived), run);
}

} // namespace formula
