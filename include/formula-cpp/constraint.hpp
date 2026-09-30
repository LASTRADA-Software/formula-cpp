// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Constraints: relationships stated purely to validate, not to compute.
///
/// A standard routinely states a relationship in the same symbols as its
/// computing formulas but existing only to check a result -- "the two
/// replicates shall agree within 4.7%", "reject the specimen below 27.3 MPa".
/// `PredicateNode` (`predicate.hpp`) already compares two expressions;
/// `Constraint` is the small addition that pairs one with what to do when it
/// does not hold.
///
/// A constraint is deliberately **not** a `Node`, for the same reason a
/// predicate is not one: it produces a verdict, which has no dimension, and
/// giving it one would mean inventing a dimension to lie about. A constraint
/// cannot ride the `Node`-constrained machinery that renders, traces and
/// documents expressions -- see `citation.hpp`'s `DocumentedNode`, which
/// requires `Node Inner` and so cannot wrap a constraint either. That is why
/// `Constraint` carries its own `Citation` directly instead of being wrapped
/// by `documented()`.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/sink.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace formula
{

/// Which of the four states checking a constraint reached.
///
/// A predicate has three outcomes, not two: it holds, it does not hold, or it
/// never resolved -- because an input was never measured, or because
/// evaluating it raised an arithmetic error. `NotChecked` and `Invalid` stay
/// distinct for the same reason `OutcomeKind::Empty` and `OutcomeKind::Invalid`
/// are distinct on `Outcome`: "nobody measured the input" and "we tried to
/// check and the arithmetic broke" call for different action from whoever
/// reads the report.
enum class ConstraintOutcomeKind : std::uint8_t
{
    /// The predicate held.
    Satisfied,
    /// The predicate did not hold; the constraint's `Verdict` applies.
    Violated,
    /// The predicate never resolved, because an input was never measured.
    NotChecked,
    /// Evaluating the predicate raised an arithmetic error.
    Invalid,
};

/// A lowercase phrase with no trailing punctuation, so callers can embed it in a longer sentence.
[[nodiscard]] constexpr std::string_view describe(ConstraintOutcomeKind judged) noexcept
{
    switch (judged)
    {
        case ConstraintOutcomeKind::Satisfied:
            return "satisfied";
        case ConstraintOutcomeKind::Violated:
            return "violated";
        case ConstraintOutcomeKind::NotChecked:
            return "not checked";
        case ConstraintOutcomeKind::Invalid:
            return "invalid";
    }
    return "unknown constraint outcome";
}

namespace detail
{
template <>
inline constexpr bool formats_by_describe<ConstraintOutcomeKind> = true;
} // namespace detail

/// What checking a `Constraint` produced.
///
/// **A constraint whose predicate could not be evaluated must never report
/// satisfied.** A check that silently passes when its input is missing is
/// worse than no check at all: it produces a record saying the specimen was
/// verified when nothing verified it. That is the entire reason this type
/// has four states instead of a `bool`.
class ConstraintOutcome
{
  public:
    /// The predicate held.
    [[nodiscard]] static constexpr ConstraintOutcome satisfied() noexcept
    {
        ConstraintOutcome made {};
        made._kind = ConstraintOutcomeKind::Satisfied;
        return made;
    }

    /// The predicate did not hold; carries the verdict the constraint was
    /// declared with.
    [[nodiscard]] static constexpr ConstraintOutcome violated(Verdict verdict) noexcept
    {
        ConstraintOutcome made {};
        made._kind = ConstraintOutcomeKind::Violated;
        made._verdict = verdict;
        return made;
    }

    /// The predicate never resolved -- an input was never measured.
    [[nodiscard]] static constexpr ConstraintOutcome not_checked() noexcept
    {
        ConstraintOutcome made {};
        made._kind = ConstraintOutcomeKind::NotChecked;
        return made;
    }

    /// Evaluating the predicate raised an arithmetic error; carries it.
    [[nodiscard]] static constexpr ConstraintOutcome invalid(ArithmeticError cause) noexcept
    {
        ConstraintOutcome made {};
        made._kind = ConstraintOutcomeKind::Invalid;
        made._error = cause;
        return made;
    }

    /// Which alternative this outcome holds.
    [[nodiscard]] constexpr ConstraintOutcomeKind kind() const noexcept
    {
        return _kind;
    }

    /// True when `kind()` is `ConstraintOutcomeKind::Satisfied`.
    [[nodiscard]] constexpr bool is_satisfied() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Satisfied;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::Violated`.
    [[nodiscard]] constexpr bool is_violated() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Violated;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::NotChecked`.
    [[nodiscard]] constexpr bool is_not_checked() const noexcept
    {
        return _kind == ConstraintOutcomeKind::NotChecked;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::Invalid`.
    [[nodiscard]] constexpr bool is_invalid() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Invalid;
    }

    // Deliberately no `bool satisfied()` and no `operator bool()`. Both would
    // have to answer *something* for `NotChecked` and `Invalid`, and every
    // answer is a lie: `true` collapses "held" together with "never checked"
    // or "broke while checking", which is exactly the safety property this
    // type exists to prevent someone from erasing by convenience-accessor.
    // `false` is not honest either -- it says the specimen failed the check,
    // when in fact no check ran at all. Callers who "just want to know if it
    // passed" are the exact callers who must instead look at `kind()`, or at
    // `is_satisfied()` *and* `is_violated()` *and* `is_not_checked()` *and*
    // `is_invalid()`, and decide what an unresolved check means for them. If
    // you are about to add one back: don't -- read this comment's title
    // first.

    /// The verdict, present only when `is_violated()`.
    [[nodiscard]] constexpr std::optional<Verdict> verdict() const noexcept
    {
        if (_kind == ConstraintOutcomeKind::Violated)
            return _verdict;
        return std::nullopt;
    }

    /// The arithmetic error, present only when `is_invalid()`.
    [[nodiscard]] constexpr std::optional<ArithmeticError> error() const noexcept
    {
        if (_kind == ConstraintOutcomeKind::Invalid)
            return _error;
        return std::nullopt;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(ConstraintOutcome const&) const noexcept = default;

  private:
    // Defaults to `NotChecked`, not `Satisfied` -- a default-constructed
    // outcome must never look like a passed check, the same reasoning that
    // makes `Outcome` default to `Empty` rather than to `Value`.
    ConstraintOutcomeKind _kind = ConstraintOutcomeKind::NotChecked;
    Verdict _verdict {};
    ArithmeticError _error { ArithmeticError::DivisionByZero };
};

/// A predicate paired with what to do when it does not hold.
///
/// The predicate states the condition that must **hold**, not the failure --
/// a standard says "the two replicates shall agree within 5%", so the
/// declaration reads as the standard reads, and `verdict` is what happens
/// when it does not. There is deliberately no separate verdict for success:
/// a satisfied constraint is silent by nature, and a slot for a pass verdict
/// only invites noise an auditor then has to read past.
///
/// Not a `Node` -- see the file comment -- so it carries its own `Citation`
/// rather than being wrapped by `documented()`.
template <Predicate P>
struct Constraint
{
    /// The condition that must hold for this constraint to be satisfied.
    ///
    /// Deliberately no `{}` default member initialiser: with one, a method
    /// whose constraint holds a lookup fails to compile on clang++ and
    /// clang-cl, and cl answers the trait wrongly -- see `Corrections`
    /// (`lookup.hpp`).
    P predicate;
    /// What the outcome is when `predicate` does not hold.
    Verdict verdict {};
    /// Where this constraint comes from.
    Citation citation {};
};

/// `constraint(predicate, verdict, citation = {})`: a predicate paired with
/// the verdict that applies when it does not hold.
template <Predicate P>
[[nodiscard]] constexpr Constraint<P> constraint(P predicate, Verdict verdict, Citation citation = {}) noexcept
{
    return Constraint<P> { predicate, verdict, citation };
}

/// Checks @p subject against @p environment, mapping `checked_evaluate_
/// predicate`'s three-state result onto the four `ConstraintOutcomeKind`
/// states: an arithmetic error becomes `Invalid`, an unresolved predicate
/// (an input was never measured) becomes `NotChecked`, a held predicate
/// becomes `Satisfied`, and a predicate that does not hold becomes `Violated`
/// carrying the constraint's own verdict. That mapping is the whole
/// implementation -- see the file comment and `ConstraintOutcome` for why
/// `NotChecked` must never collapse into `Satisfied`.
///
/// The parameter is named `subject`, not `constraint` -- the obvious name --
/// because `formula::constraint(...)` is a free function at namespace scope
/// and a parameter of the same name would shadow it. `-Wshadow` does not
/// catch a parameter shadowing a function, so nothing would fail to build,
/// but it is the same kind of name collision that shipped a `StepKind::Pi`
/// enumerator shadowing `formula::Pi` in phase 7 and broke GCC alone.
///
/// **Recorded in the trace as its own step**, the way spec sections 9 and
/// 9.1 require. A constraint is not a `Node`, so it cannot go through
/// `sink.entered`/`sink.produced` -- both constrained on `Node` -- the same
/// problem phase 8 solved for a `WhenNode`'s branch with an optional
/// `sink.branch_taken(...)` hook. The two calls below follow that
/// established shape: a sink that defines `constraint_entered`/
/// `constraint_produced` -- `RecordingSink` (`trace.hpp`) is the one that
/// does -- gets a step; `NullSink`, which defines neither, pays nothing.
template <typename Rep = Rational, typename P, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr ConstraintOutcome check(Constraint<P> const& subject, Env const& environment,
                                                Sink sink = {}) noexcept
{
    if constexpr (requires { sink.constraint_entered(subject); })
        sink.constraint_entered(subject);

    std::expected<std::optional<bool>, ArithmeticError> const checkedPredicate =
        checked_evaluate_predicate<Rep>(subject.predicate, environment, sink);

    ConstraintOutcome reached {};
    if (!checkedPredicate.has_value())
        reached = ConstraintOutcome::invalid(checkedPredicate.error());
    else if (!checkedPredicate->has_value())
        reached = ConstraintOutcome::not_checked();
    else if (**checkedPredicate)
        reached = ConstraintOutcome::satisfied();
    else
        reached = ConstraintOutcome::violated(subject.verdict);

    if constexpr (requires { sink.constraint_produced(subject, reached); })
        sink.constraint_produced(subject, reached);

    return reached;
}

/// A set of constraints checked together, in declaration order:
/// `constraints(dimensional_tolerance, replicate_agreement)`.
///
/// Bundles `Constraint`s the same way `environment()` bundles entries in
/// `environment.hpp` -- a factory taking a pack by value, returning a class
/// template over that pack -- so a reader who already knows `environment(...)`
/// recognises this immediately. `Method` (`method.hpp`) holds one of these as
/// an ordinary member (spec section 9.1: `formula::constraints(dimensional_
/// tolerance)` inside `formula::method(...)`), and `check_method` passes it
/// straight to `check_all()` below without unpacking it first -- the reason this is a
/// bundle type rather than only a variadic `check_all(environment, sink,
/// constraints...)` over a raw pack, which `Method` would then have to
/// re-expand on every check.
///
/// Deliberately a plain aggregate, unlike `Environment`: `Environment` earns
/// its private state and `get<Q>()` accessor because it enforces "each
/// quantity supplied at most once" and does type-keyed lookup. A constraint
/// set has neither invariant to protect -- it is only an ordered bundle to be
/// walked front to back -- so there is nothing an encapsulated class would
/// buy here that a public tuple does not.
template <Predicate... Ps>
struct ConstraintSet
{
    /// The constraints, in the order `constraints(...)` was called with them.
    /// `check_all()` reports one `ConstraintOutcome` per element of this
    /// tuple, at the same index -- see `check_all()` for why that order is
    /// part of the contract.
    ///
    /// Deliberately no `{}` default member initialiser. With one, no method
    /// fails to compile, but asking whether this type -- or a tuple holding
    /// it -- is default-constructible hard-errors or answers `true` when a
    /// lookup is inside; see `Corrections` (`lookup.hpp`).
    std::tuple<Constraint<Ps>...> items;
};

/// Builds a constraint set: `constraints(a, b, c)`. See `ConstraintSet`.
template <Predicate... Ps>
[[nodiscard]] constexpr ConstraintSet<Ps...> constraints(Constraint<Ps>... items) noexcept
{
    return ConstraintSet<Ps...> { std::tuple<Constraint<Ps>...> { items... } };
}

namespace detail
{
    /// Checks every element of @p items against @p environment and collects
    /// the results at the matching index of the returned array. The pack
    /// expansion sits inside a braced-init-list, whose elements C++ requires
    /// to be evaluated in the order written -- so this also evaluates the
    /// constraints in declaration order, though `check_all()`'s safety
    /// property (every constraint evaluated) holds regardless of order.
    template <typename Rep, typename Env, typename Sink, typename... Ps, std::size_t... Is>
    [[nodiscard]] constexpr std::array<ConstraintOutcome, sizeof...(Ps)> check_all_impl(
        std::tuple<Constraint<Ps>...> const& items, Env const& environment, Sink sink,
        std::index_sequence<Is...>) noexcept
    {
        // An empty set -- a method declared with `constraints()`, or an
        // overlay's `with_constraints(constraints(), source)` -- expands the pack
        // below to nothing, and g++ 13 then reports `sink` as set but not
        // used, an error under `-Werror`.
        static_cast<void>(sink);
        return { check<Rep>(std::get<Is>(items), environment, sink)... };
    }
} // namespace detail

/// Checks every constraint in @p set against @p environment and returns one
/// `ConstraintOutcome` per constraint, at the same index it was declared at
/// in `constraints(...)` -- so the nth result answers for the nth constraint,
/// and a reader can match them up without re-deriving which is which.
///
/// **Every constraint is evaluated. There is no short-circuit.** A specimen
/// can fail two checks at once, and a report naming only the first sends
/// someone back for a second round of testing they should not have needed.
/// This is deliberately the opposite of `when()` in `conditional.hpp`, which
/// evaluates only the branch it takes -- and the reason differs rather than
/// the rule being inconsistent: `when()` skips a branch because evaluating it
/// could raise an arithmetic error that has nothing to do with the answer,
/// whereas every constraint *is* about the answer, so skipping one to save
/// work would be discarding an answer the caller asked for, not avoiding a
/// meaningless one.
template <typename Rep = Rational, typename... Ps, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::array<ConstraintOutcome, sizeof...(Ps)> check_all(ConstraintSet<Ps...> const& set,
                                                                               Env const& environment,
                                                                               Sink sink = {}) noexcept
{
    return detail::check_all_impl<Rep>(set.items, environment, sink, std::index_sequence_for<Ps...> {});
}

} // namespace formula
