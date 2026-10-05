// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Opaque operations: a named computation no expression tree can state -- a
/// least-squares fit, say -- whose outputs are used as ordinary nodes.
///
/// An operation is a **type**, declared by this library or by a consumer, and
/// recognised by `OpaqueOperation`. It states its name, the shape of each
/// input, the name of each output, which input dimensions it accepts and what
/// dimension each output then has, and a `noexcept` `compute`. All of that is
/// static, so nothing about an operation can be set or changed per call. A
/// name is the author's label, not an identity: the trace names an operation
/// as its type names itself, and two operations may declare one name -- a
/// consumer's operation named "linear least squares" reads as the library's
/// fit. The citation says which computation a method means.
///
/// `opaque<Op>(citation, inputs...)` builds an `OpaqueCall`. A call is **not**
/// a `Node`, because it has several outputs; `opaque_output<"slope">(call)` is
/// the `Node` that stands for one of them, and it is how a coefficient enters
/// ordinary arithmetic. Each output used evaluates its whole call, so a formula
/// using two outputs of one call runs the operation twice.
///
/// **compute receives evaluated values only, never the environment; that is
/// what lets the trace show every input and an overlay reach every use of a
/// quantity.** Every input is evaluated by the library, in order, into the
/// coherent unit: a single value arrives as `Rep`, a series as
/// `std::span<Rep const>`, raw observations as one `std::span<Rep const>` over
/// those made, and a curve as two spans, its points first and then its values.
/// Nothing is converted on the way out either: each output is in
/// the coherent unit of the dimension the operation declares for it.
///
/// **Absence is strict**, as it is for a series: an absent single value, or an absent
/// element anywhere in a series or a curve, makes the whole call absent, and
/// `compute` is not called at all -- an operation cannot choose to fit "the
/// points someone happened to enter". Raw observations are never absent: none
/// made is an empty span. Absence is decided after every input
/// has been asked, as `BinaryNode` decides it, so an input's failure is never
/// hidden behind another's absence. The first input that fails stops the call
/// there, and its error is relayed as the call's; `compute` returning
/// `std::unexpected` is the operation's own failure. `OpaqueFailure` says
/// which, since both arrive as one `ArithmeticError`.
///
/// Every refusal here is a `Require...` struct with this library's words, and
/// each is gated on the ones before it, so that one mistake draws one message.
///
/// **An output the exact layer cannot hold is reported at a declared
/// precision.** `rounded_output<"slope", U, Places, Mode>(call)` is the
/// decimal the operation's true output rounds to in `U`, exact. An operation
/// may state its outputs exactly in wider integers (`compute_exact`, detected
/// by `detail::declares_compute_exact`; internal, not a customisation point
/// yet), and then it answers even where computing the output exactly leaves
/// `Rational`'s range, as a fit through many readings can. One that does not
/// is evaluated through `compute<Rational>`, rounded exactly, and fails with
/// `Overflow` where `opaque_output` would. The call is then told to a sink with
/// `OpaqueValues::RoundedWhereUsed`, and its trace names its outputs without
/// values. `opaque_output` is unchanged: the exact output, or `Overflow`.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/detail/fixed_string.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/observations.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

/// The shape of one input an opaque operation declares.
enum class InputShape : std::uint8_t
{
    /// One value: any `Node`. It arrives at `compute` as `Rep`.
    Single,
    /// A series: any `SeriesNode`. It arrives as `std::span<Rep const>`.
    Series,
    /// A curve: any `CurveExpression`. It arrives as two `std::span<Rep const>`,
    /// its points first, and contributes **two** dimensions to
    /// `output_dimensions`, points then values. A curve is evaluated only with
    /// `Rep = Rational` (`curve.hpp`), so a call with a curve input is too.
    Curve,
    /// Raw observations: any `ObservationsNode` (`observations.hpp`). They
    /// arrive as one `std::span<Rep const>` over the observations **made** --
    /// as many as were made, not the capacity, and empty when none were --
    /// pointing into the evaluated value; nothing is copied. They contribute
    /// one dimension. How many there are is data, so the library compares no
    /// counts: an operation over two independent samples takes two counts,
    /// and one that pairs its inputs row by row checks its counts itself.
    /// Evaluated in any `Rep`.
    Observations,
};

/// Whose failure an opaque call is carrying.
///
/// **Not a detail.** An operation's own failure -- a fit whose domain values
/// are all equal -- and an input's failure relayed through it both arrive as
/// one `ArithmeticError`, so a step carrying `DomainError` would otherwise be
/// ambiguous between "the fit was degenerate" and "an input failed two levels
/// down". `LookupFailure` (`trace.hpp`) exists for the same ambiguity.
enum class OpaqueFailure : std::uint8_t
{
    /// Nothing failed. The zero value, so every step that is not an opaque
    /// call is already correct.
    None,
    /// The operation's own `compute` returned an error.
    Own,
    /// An input failed, and the call is relaying its error unchanged.
    /// `compute` was never called.
    Propagated,
    /// The call failed and whose failure it was cannot be told: set only by a
    /// recorder that finds no input step to inspect.
    Undetermined,
};

/// Whether an opaque call holds its outputs' values, as a sink is told of it
/// (`OpaqueCallInfo::values`) and a trace records it (`OpaqueStepData::values`).
enum class OpaqueValues : std::uint8_t
{
    /// Every output is a `Rational`, exactly: `opaque_output`'s route. The zero
    /// value, so a call described before this existed reads as it did.
    Exact,
    /// The call was evaluated for an output rounded where it is used
    /// (`rounded_output`): no output exists as a `Rational` until it is
    /// rounded, so the call holds no values, and each output used states its
    /// own rounding.
    RoundedWhereUsed,
};

/// Why an opaque call could not be evaluated.
struct OpaqueCallFailure
{
    /// What went wrong.
    ArithmeticError error;
    /// Whose failure it is: `Own` or `Propagated`.
    OpaqueFailure origin;
    /// For an input series or curve that failed at one of its own elements,
    /// or raw observations that failed at one observation, that position,
    /// ZERO-BASED, relayed as `detail::relayed_failure` relays one
    /// (`series.hpp`); `site` says which of the two it counts. Empty
    /// otherwise.
    std::optional<std::size_t> element;
    /// For a relayed failure, how many of the call's inputs after the one
    /// that failed were never evaluated -- the call stops at the first input
    /// that fails; 0 otherwise.
    std::size_t notEvaluated = 0;
    /// What `element` counts: an element of the input that failed, or --
    /// for raw observations whose conversion into the coherent unit failed --
    /// an observation (`FailureSite::InputObservation`), which the trace
    /// names `at observation k`. Last, and defaulted, so that an aggregate
    /// initialisation naming the members before it stays valid.
    FailureSite site = FailureSite::ResultElement;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(OpaqueCallFailure const&) const noexcept = default;
};

/// What a sink is told of an opaque call it hears about (`opaque_entered`,
/// `opaque_produced`): plain data, so that a sink needs no operation type to
/// record the call. The views point into the operation's static members and
/// into the call, which outlives the evaluation that tells the sink; a sink
/// that keeps the citation copies it.
struct OpaqueCallInfo
{
    /// The operation's name, `Op::name`.
    std::string_view name {};
    /// Why the method uses the operation here -- empty when it cited nothing.
    Citation citation {};
    /// The operation's output names, in its declared order.
    std::span<std::string_view const> outputs {};
    /// Each output's dimension, in the same order.
    std::span<Dimension const> dimensions {};
    /// Whether the evaluation the sink is told of holds the outputs' values. A
    /// call evaluated for a `rounded_output` says `RoundedWhereUsed`, and its
    /// `opaque_produced` is handed an `OpaqueEvaluated<Rational, 0>`: whether
    /// the call answered, was absent or failed, and no value.
    OpaqueValues values = OpaqueValues::Exact;
};

/// What evaluating an opaque call produces: every output in the coherent
/// unit of its dimension, nothing (an input was absent), or the failure.
template <typename Rep, std::size_t M>
using OpaqueEvaluated = std::expected<std::optional<std::array<Rep, M>>, OpaqueCallFailure>;

namespace detail
{
    /// How many dimensions @p shapes contribute to `output_dimensions`: one
    /// per single value, series or observations, two per curve.
    template <std::size_t K>
    [[nodiscard]] consteval std::size_t input_dimension_count(std::array<InputShape, K> const& shapes) noexcept
    {
        std::size_t slotCount = 0;
        for (InputShape const declared: shapes)
            slotCount += declared == InputShape::Curve ? 2 : 1;
        return slotCount;
    }

    /// Whether @p candidate is a name every dialect shows as written: not empty,
    /// only ASCII letters, digits and spaces, no space at either end and no
    /// two spaces together. Anything else would need escaping somewhere, and
    /// the site's MathJax shows a LaTeX text-mode escape backslash and all
    /// (measured under MathJax 3.2.2 with the site's configuration).
    [[nodiscard]] consteval bool readable_name(std::string_view candidate) noexcept
    {
        if (candidate.empty() || candidate.front() == ' ' || candidate.back() == ' ')
            return false;
        for (std::size_t at = 0; at < candidate.size(); ++at)
        {
            char const glyph = candidate[at];
            bool const letterOrDigit =
                (glyph >= 'a' && glyph <= 'z') || (glyph >= 'A' && glyph <= 'Z') || (glyph >= '0' && glyph <= '9');
            if (!letterOrDigit && glyph != ' ')
                return false;
            if (glyph == ' ' && candidate[at + 1] == ' ')
                return false;
        }
        return true;
    }

    /// Whether every one of @p names is readable (`readable_name`).
    template <std::size_t M>
    [[nodiscard]] consteval bool all_readable(std::array<std::string_view, M> const& names) noexcept
    {
        for (std::string_view const named: names)
            if (!readable_name(named))
                return false;
        return true;
    }

    /// Whether no two of @p names are the same.
    template <std::size_t M>
    [[nodiscard]] consteval bool all_distinct(std::array<std::string_view, M> const& names) noexcept
    {
        for (std::size_t at = 0; at < M; ++at)
            for (std::size_t later = at + 1; later < M; ++later)
                if (names[at] == names[later])
                    return false;
        return true;
    }
} // namespace detail

/// A type declaring an opaque operation. It states:
///
///  - `static constexpr std::string_view name`: what the trace calls it;
///  - `static constexpr std::array<InputShape, K> shapes`: each input's shape;
///  - `static constexpr std::array<std::string_view, M> outputs`: each
///    output's name, which `opaque_output<"name">` selects by;
///  - `static consteval std::optional<std::array<Dimension, M>>
///    output_dimensions(std::array<Dimension, D> inputs) noexcept`: the
///    dimension of each output for inputs of these dimensions, or
///    `std::nullopt` when the operation does not accept them. `D` counts
///    dimensions, not inputs: a curve contributes two, its points then its
///    values;
///  - `template <typename Rep> static constexpr std::expected<std::array<Rep,
///    M>, ArithmeticError> compute(...) noexcept`, taking a single value as
///    `Rep`, a series as `std::span<Rep const>`, raw observations as one
///    `std::span<Rep const>` over those made and a curve as two spans,
///    points first -- every value in the coherent unit.
///
/// **`compute` must be a function of its arguments alone.** Nothing in its
/// signature lets it read the environment, but nothing in C++ stops it
/// reading a global, a static member or a clock. A value it read that way
/// would be an input the trace does not show and an overlay cannot reach --
/// exactly what an opaque operation exists to rule out -- so an operation
/// that needs a value takes it as an input.
///
/// **`compute` does its arithmetic through `RepTraits<Rep>`'s checked
/// operations, returns their error as its own, and never throws.**
/// `Rational`'s own `+`, `-`, `*` and `/` throw `ArithmeticException` on
/// overflow, and a throw out of `compute`, which is `noexcept`, calls
/// `std::terminate` -- ending the program, in a debug build with an abort
/// dialog -- where the library's contract is an `Overflow` on the trace.
/// Comparing and copying values never throw. The library checks the
/// `noexcept`; it cannot check the body, so this rule is the operation
/// author's part of the contract, as it is `LinearLeastSquares`'.
///
/// This concept checks only that the three data members exist in their
/// declared types. Everything else -- `output_dimensions` declared with the
/// right parameter and return, a name that says something, readable and
/// distinct output names, a `compute` that is `noexcept` and callable with
/// the declared inputs -- is checked where a call is built, in this
/// library's words, one message per mistake.
///
/// **This is the traced way for a consumer to add a computation.** A
/// consumer's own `Node` kind cannot appear in a recorded trace
/// (`detail::dispatch`, `sink.hpp`); an opaque operation's step is recorded by
/// the library, which is also why nothing about the operation beyond these
/// members is part of the contract. It is one of the library's customisation
/// points, beside `TagName`, `EnumeratorName`, `Describe`, `RepTraits` and
/// the vocabulary; specialising a `detail::` template instead, such as
/// `ConstantRewrite` or `StepKindOf`, is outside the contract.
template <typename Op>
concept OpaqueOperation = requires {
    { Op::name } -> std::convertible_to<std::string_view>;
    requires std::same_as<std::remove_cvref_t<decltype(Op::shapes)>, std::array<InputShape, Op::shapes.size()>>;
    requires std::same_as<std::remove_cvref_t<decltype(Op::outputs)>, std::array<std::string_view, Op::outputs.size()>>;
};

namespace detail
{
    /// Whether @p Op declares `output_dimensions` as `OpaqueOperation`
    /// describes it: taking one `Dimension` per single value, series or
    /// observations and two per curve, and answering one per output.
    template <typename Op>
    inline constexpr bool opaque_dimensions_declared = requires {
        {
            Op::output_dimensions(std::array<Dimension, input_dimension_count(Op::shapes)> {})
        } -> std::same_as<std::optional<std::array<Dimension, Op::outputs.size()>>>;
    };

    /// Fails to compile when `output_dimensions` is missing, or takes or
    /// answers the wrong number of dimensions.
    template <typename Op>
    struct RequireOpaqueOutputDimensionsDeclared
    {
        static_assert(
            opaque_dimensions_declared<Op>,
            "formula: this opaque operation's output_dimensions must be a static function taking "
            "std::array<Dimension, D> -- one Dimension per single value, series or observations input and "
            "two per curve, its points then its values -- and returning std::optional<std::array<Dimension, M>>, with M "
            "the number of outputs; the operation appears in this diagnostic as the template argument of "
            "RequireOpaqueOutputDimensionsDeclared");

        static constexpr bool value = true;
    };

    /// Fails to compile when an opaque operation's name is empty or blank.
    template <typename Op>
    struct RequireOpaqueNameSaysSomething
    {
        static_assert(saysSomething(std::string_view { Op::name }),
                      "formula: an opaque operation's name must say something; one that is empty or blank leaves the "
                      "trace unable to say what ran");

        static constexpr bool value = true;
    };

    /// Fails to compile when an opaque operation's name, or any of its output
    /// names, holds anything but ASCII letters, digits and single spaces.
    template <typename Op>
    struct RequireOpaqueNameReadable
    {
        static_assert(readable_name(std::string_view { Op::name }) && all_readable(Op::outputs),
                      "formula: an opaque operation's name and output names may hold only ASCII letters, digits and "
                      "single spaces, so that every dialect shows them as written");

        static constexpr bool value = true;
    };

    /// Fails to compile when an opaque operation declares no output at all.
    template <typename Op>
    struct RequireOpaqueHasOutputs
    {
        static_assert(Op::outputs.size() > 0,
                      "formula: an opaque operation declares no outputs; a call with nothing to use has nothing to "
                      "trace");

        static constexpr bool value = true;
    };

    /// Fails to compile when two of an opaque operation's outputs share a name.
    template <typename Op>
    struct RequireOpaqueOutputsDistinct
    {
        static_assert(all_distinct(Op::outputs),
                      "formula: an opaque operation declares two outputs with the same name; opaque_output could not "
                      "tell them apart");

        static constexpr bool value = true;
    };

    /// Whether @p Op's own declaration passes every check above, asserting
    /// nothing.
    template <typename Op>
    inline constexpr bool opaque_operation_well_formed = opaque_dimensions_declared<Op>
                                                         && saysSomething(std::string_view { Op::name })
                                                         && readable_name(std::string_view { Op::name })
                                                         && all_readable(Op::outputs) && Op::outputs.size() > 0
                                                         && all_distinct(Op::outputs);

    /// Checks @p Op's own declaration, each check gated on the ones before it:
    /// a blank name is not also unreadable, and unreadable output names are
    /// not also compared.
    template <typename Op>
    struct RequireValidOpaqueOperation
    {
        static constexpr bool declaredOk = opaque_dimensions_declared<Op>;
        static_assert(RequireOpaqueOutputDimensionsDeclared<Op>::value);
        static constexpr bool saysSomethingOk = declaredOk && saysSomething(std::string_view { Op::name });
        static_assert(std::conditional_t<declaredOk, RequireOpaqueNameSaysSomething<Op>, std::true_type>::value);
        static_assert(std::conditional_t<saysSomethingOk, RequireOpaqueNameReadable<Op>, std::true_type>::value);
        static constexpr bool readableOk =
            saysSomethingOk && readable_name(std::string_view { Op::name }) && all_readable(Op::outputs);
        static_assert(std::conditional_t<readableOk, RequireOpaqueHasOutputs<Op>, std::true_type>::value);
        static_assert(std::conditional_t<readableOk && (Op::outputs.size() > 0),
                                         RequireOpaqueOutputsDistinct<Op>,
                                         std::true_type>::value);

        static constexpr bool value = opaque_operation_well_formed<Op>;
    };

    /// What an input of type @p T is to an opaque call: its shape, the
    /// dimensions it contributes, and its length when it is a series or a
    /// curve. `known` is false for a type of none of the three shapes, which
    /// the shape check then refuses.
    template <typename T>
    struct OpaqueInput
    {
        static constexpr bool known = false;
        static constexpr InputShape shape = InputShape::Single;
        static constexpr std::size_t length = 0;
        static constexpr std::array<Dimension, 1> dimensions { dim::Scalar };
    };

    template <Node T>
    struct OpaqueInput<T>
    {
        static constexpr bool known = true;
        static constexpr InputShape shape = InputShape::Single;
        static constexpr std::size_t length = 0;
        static constexpr std::array<Dimension, 1> dimensions { T::dimension };
    };

    template <SeriesNode T>
    struct OpaqueInput<T>
    {
        static constexpr bool known = true;
        static constexpr InputShape shape = InputShape::Series;
        static constexpr std::size_t length = T::length;
        static constexpr std::array<Dimension, 1> dimensions { T::dimension };
    };

    template <CurveExpression T>
    struct OpaqueInput<T>
    {
        static constexpr bool known = true;
        static constexpr InputShape shape = InputShape::Curve;
        static constexpr std::size_t length = T::length;
        static constexpr std::array<Dimension, 2> dimensions { T::domainDimension, T::dimension };
    };

    template <ObservationsNode T>
    struct OpaqueInput<T>
    {
        static constexpr bool known = true;
        static constexpr InputShape shape = InputShape::Observations;
        /// Known only at run time: 0, which `opaque_lengths_agree` skips.
        static constexpr std::size_t length = 0;
        static constexpr std::array<Dimension, 1> dimensions { T::dimension };
    };

    /// Whether each of @p Inputs has the shape @p Op declares at its position.
    /// Only asked once the counts agree.
    template <typename Op, typename... Inputs>
    [[nodiscard]] consteval bool opaque_shapes_match() noexcept
    {
        constexpr std::array<InputShape, sizeof...(Inputs)> given { OpaqueInput<Inputs>::shape... };
        constexpr std::array<bool, sizeof...(Inputs)> recognised { OpaqueInput<Inputs>::known... };
        for (std::size_t at = 0; at < sizeof...(Inputs); ++at)
            if (!recognised[at] || given[at] != Op::shapes[at])
                return false;
        return true;
    }

    /// Whether every series and curve among @p Inputs has one length.
    template <typename... Inputs>
    [[nodiscard]] consteval bool opaque_lengths_agree() noexcept
    {
        constexpr std::array<std::size_t, sizeof...(Inputs) + 1> lengths { OpaqueInput<Inputs>::length..., 0 };
        std::size_t common = 0;
        for (std::size_t const given: lengths)
        {
            if (given == 0)
                continue;
            if (common != 0 && given != common)
                return false;
            common = given;
        }
        return true;
    }

    /// Every dimension @p Inputs contribute, in order: a curve's points, then
    /// its values.
    template <std::size_t D, typename... Inputs>
    [[nodiscard]] consteval std::array<Dimension, D> opaque_input_dimensions() noexcept
    {
        std::array<Dimension, D> gathered {};
        std::size_t at = 0;
        (
            [&] {
                for (Dimension const& contributed: OpaqueInput<Inputs>::dimensions)
                    gathered[at++] = contributed;
            }(),
            ...);
        return gathered;
    }

    /// The argument an evaluated input of type @p T passes to `compute`, as a
    /// tuple: one `Rep`, one span, or two spans for a curve.
    template <typename Rep, typename T>
    struct OpaqueArguments
    {
        using type = std::tuple<Rep>;
    };

    template <typename Rep, SeriesNode T>
    struct OpaqueArguments<Rep, T>
    {
        using type = std::tuple<std::span<Rep const>>;
    };

    template <typename Rep, CurveExpression T>
    struct OpaqueArguments<Rep, T>
    {
        using type = std::tuple<std::span<Rep const>, std::span<Rep const>>;
    };

    template <typename Rep, ObservationsNode T>
    struct OpaqueArguments<Rep, T>
    {
        using type = std::tuple<std::span<Rep const>>;
    };

    /// Every argument `compute` is called with, for inputs @p Inputs, as one
    /// tuple type.
    template <typename Rep, typename... Inputs>
    using OpaqueArgumentTuple = decltype(std::tuple_cat(std::declval<typename OpaqueArguments<Rep, Inputs>::type>()...));

    /// Whether `Op::compute<Rep>` can be called with @p Tuple's types and
    /// returns one value per output, in `std::expected`.
    template <typename Op, typename Rep, typename Tuple>
    struct OpaqueComputeCallable;

    template <typename Op, typename Rep, typename... Args>
    struct OpaqueComputeCallable<Op, Rep, std::tuple<Args...>>
    {
        static constexpr bool value = requires(Args... arguments) {
            {
                Op::template compute<Rep>(arguments...)
            } -> std::same_as<std::expected<std::array<Rep, Op::outputs.size()>, ArithmeticError>>;
        };
    };

    /// Whether that call is `noexcept`. Only asked once it is callable.
    template <typename Op, typename Rep, typename Tuple>
    struct OpaqueComputeNoexcept;

    template <typename Op, typename Rep, typename... Args>
    struct OpaqueComputeNoexcept<Op, Rep, std::tuple<Args...>>
    {
        static constexpr bool value = noexcept(Op::template compute<Rep>(std::declval<Args>()...));
    };

    /// Whether @p Op declares the exact hook a `rounded_output` prefers, for
    /// arguments @p Tuple: `static constexpr std::size_t exact_limbs`, at least
    /// 4, and a `noexcept` `compute_exact` taking what `compute<Rational>`
    /// takes and answering `std::expected<std::array<WideRatio<exact_limbs>,
    /// M>, ArithmeticError>`, each output exactly. An internal hook, not a
    /// customisation point: an operation that declares it any other way is
    /// evaluated through `compute<Rational>`, as if it had none.
    template <typename Op, typename Tuple>
    struct OpaqueExactCallable;

    template <typename Op, typename... Args>
    struct OpaqueExactCallable<Op, std::tuple<Args...>>
    {
        static constexpr bool value = requires(Args... arguments) {
            { Op::exact_limbs } -> std::convertible_to<std::size_t>;
            requires Op::exact_limbs >= 4;
            {
                Op::compute_exact(arguments...)
            } -> std::same_as<std::expected<std::array<WideRatio<Op::exact_limbs>, Op::outputs.size()>, ArithmeticError>>;
            requires noexcept(Op::compute_exact(arguments...));
        };
    };

    /// Whether a call of @p Op on @p Inputs is evaluated for a `rounded_output`
    /// through `compute_exact`.
    template <typename Op, typename... Inputs>
    inline constexpr bool declares_compute_exact = OpaqueExactCallable<Op, OpaqueArgumentTuple<Rational, Inputs...>>::value;

    /// Fails to compile when an opaque call passes a different number of
    /// inputs than its operation declares.
    template <typename Op, typename... Inputs>
    struct RequireOpaqueArity
    {
        static_assert(sizeof...(Inputs) == Op::shapes.size(),
                      "formula: this opaque call passes a different number of inputs than the operation declares");

        static constexpr bool value = true;
    };

    /// Fails to compile when an input's shape is not the one the operation
    /// declares at its position.
    template <typename Op, typename... Inputs>
    struct RequireOpaqueShapes
    {
        static_assert(opaque_shapes_match<Op, Inputs...>(),
                      "formula: this opaque call passes an input of another shape than the operation declares: a "
                      "series where it declares a single value, a single value where it declares a curve, or the "
                      "like");

        static constexpr bool value = true;
    };

    /// Fails to compile when an opaque call's series and curves differ in
    /// length.
    template <typename Op, typename... Inputs>
    struct RequireOpaqueLengthsAgree
    {
        static_assert(opaque_lengths_agree<Inputs...>(),
                      "formula: this opaque operation's series and curve inputs differ in length");

        static constexpr bool value = true;
    };

    /// Fails to compile when the operation's `output_dimensions` refuses the
    /// inputs' dimensions.
    template <typename Op, typename... Inputs>
    struct RequireOpaqueAcceptsDimensions
    {
        static_assert(
            Op::output_dimensions(opaque_input_dimensions<input_dimension_count(Op::shapes), Inputs...>()).has_value(),
            "formula: this opaque operation does not accept inputs of these dimensions");

        static constexpr bool value = true;
    };

    /// Fails to compile when `compute` cannot be called with what the inputs
    /// give, or does not answer with one value per output.
    template <typename Op, typename Rep, typename... Inputs>
    struct RequireOpaqueComputeCallable
    {
        static_assert(OpaqueComputeCallable<Op, Rep, OpaqueArgumentTuple<Rep, Inputs...>>::value,
                      "formula: this opaque operation's compute cannot be called with the values its declared inputs "
                      "give, or does not return one value per output; a single value arrives as Rep, a series or raw "
                      "observations as std::span<Rep const>, a curve as two spans, points first, and compute returns "
                      "std::expected<std::array<Rep, M>, ArithmeticError>");

        static constexpr bool value = true;
    };

    /// Fails to compile when `compute` is not `noexcept`.
    template <typename Op, typename Rep, typename... Inputs>
    struct RequireOpaqueComputeNoexcept
    {
        static_assert(OpaqueComputeNoexcept<Op, Rep, OpaqueArgumentTuple<Rep, Inputs...>>::value,
                      "formula: an opaque operation's compute must be noexcept; evaluation cannot throw, and an "
                      "exception escaping it would end the program");

        static constexpr bool value = true;
    };

    /// Checks an opaque call's inputs against @p Op for `Rep`, each check gated
    /// on the ones before it -- one mistake, one message. An input refused
    /// already (`refused_already`) gates them all off: its dimension and
    /// length are stand-ins.
    template <typename Op, typename Rep, typename... Inputs>
    struct RequireOpaqueCallValid
    {
        static constexpr bool inputsRefused = (refused_already<Inputs>() || ...);
        static constexpr bool operationOk = opaque_operation_well_formed<Op> && !inputsRefused;

        static constexpr bool arityOk = sizeof...(Inputs) == Op::shapes.size();
        static_assert(std::conditional_t<operationOk, RequireOpaqueArity<Op, Inputs...>, std::true_type>::value);

        // Each stage is asked in an `if constexpr` of its own, never behind a
        // `&&`: g++ 13.3 evaluates a `consteval` call in the right operand of
        // a `&&` whose left operand is already false, and so ran the shape
        // check of a call with too many inputs past the end of the
        // operation's shapes (6 errors). clang++ 20.1.8 did not (1 error).
        [[nodiscard]] static consteval bool shapes_match() noexcept
        {
            if constexpr (operationOk && arityOk)
                return opaque_shapes_match<Op, Inputs...>();
            else
                return false;
        }

        static constexpr bool shapesOk = shapes_match();
        static_assert(std::conditional_t<operationOk && arityOk, RequireOpaqueShapes<Op, Inputs...>, std::true_type>::value);

        [[nodiscard]] static consteval bool lengths_agree() noexcept
        {
            if constexpr (shapesOk)
                return opaque_lengths_agree<Inputs...>();
            else
                return false;
        }

        static constexpr bool lengthsOk = lengths_agree();
        static_assert(std::conditional_t<shapesOk, RequireOpaqueLengthsAgree<Op, Inputs...>, std::true_type>::value);

        /// Whether `output_dimensions` accepted the inputs. Asked only after
        /// the checks above passed.
        [[nodiscard]] static consteval bool dimensions_accepted() noexcept
        {
            if constexpr (lengthsOk)
                return Op::output_dimensions(opaque_input_dimensions<input_dimension_count(Op::shapes), Inputs...>())
                    .has_value();
            else
                return false;
        }

        static constexpr bool dimensionsOk = dimensions_accepted();
        static_assert(std::conditional_t<lengthsOk, RequireOpaqueAcceptsDimensions<Op, Inputs...>, std::true_type>::value);

        [[nodiscard]] static consteval bool compute_callable() noexcept
        {
            if constexpr (dimensionsOk)
                return OpaqueComputeCallable<Op, Rep, OpaqueArgumentTuple<Rep, Inputs...>>::value;
            else
                return false;
        }

        static constexpr bool callableOk = compute_callable();
        static_assert(
            std::conditional_t<dimensionsOk, RequireOpaqueComputeCallable<Op, Rep, Inputs...>, std::true_type>::value);

        [[nodiscard]] static consteval bool compute_noexcept() noexcept
        {
            if constexpr (callableOk)
                return OpaqueComputeNoexcept<Op, Rep, OpaqueArgumentTuple<Rep, Inputs...>>::value;
            else
                return false;
        }

        static constexpr bool noexceptOk = compute_noexcept();
        static_assert(
            std::conditional_t<callableOk, RequireOpaqueComputeNoexcept<Op, Rep, Inputs...>, std::true_type>::value);

        /// Whether the call is sound: every check above passed.
        static constexpr bool value = noexceptOk;
    };

    /// The dimension of each of @p Op's outputs for @p Inputs, or `Scalar`
    /// throughout for a call whose dimensions were never accepted -- a
    /// stand-in (`OpaqueCall::refused`). A call refused only for its
    /// `compute` keeps its true dimensions, so that nothing downstream reports
    /// the one mistake again.
    template <typename Op, bool Sound, typename... Inputs>
    [[nodiscard]] consteval std::array<Dimension, Op::outputs.size()> opaque_output_dimensions() noexcept
    {
        if constexpr (Sound)
            return *Op::output_dimensions(opaque_input_dimensions<input_dimension_count(Op::shapes), Inputs...>());
        else
        {
            std::array<Dimension, Op::outputs.size()> standIn {};
            for (Dimension& each: standIn)
                each = dim::Scalar;
            return standIn;
        }
    }
} // namespace detail

/// A call of the opaque operation @p Op on @p Inputs, with the citation that
/// says why the method uses it here. Not a `Node`: it has several outputs,
/// and `opaque_output<"name">(call)` is the `Node` for one of them.
///
/// Every check of the operation and of the call is made in this class's body,
/// so a call aggregate-initialised without `opaque()` is checked too.
template <OpaqueOperation Op, typename... Inputs>
struct OpaqueCall
{
    // `sizeof` of each check makes it a complete type, which is what fires
    // its assertions -- in the class body, where they are made whether or
    // not anything reads a member. A static data member's initialiser is
    // instantiated only when it is used, so with the checks named from one,
    // the operation's own were never made at all: measured on g++ 13.3 and
    // clang++ 20.1.8, where four negatives then compiled.
    static_assert(sizeof(detail::RequireValidOpaqueOperation<Op>) > 0);
    static_assert(sizeof(detail::RequireOpaqueCallValid<Op, Rational, Inputs...>) > 0);

    /// The operation called.
    using operation = Op;

    /// The inputs, in the order the operation declares them. No `{}`
    /// initialiser, deliberately: see `Corrections` (`lookup.hpp`).
    std::tuple<Inputs...> inputs;

    /// Why the method uses the operation here: required by `opaque()`, and
    /// shown as `(no citation given)` when it names nothing.
    ///
    /// **Author text, as `DocumentedNode::citation` is.** A call built as an
    /// aggregate may leave it out, and one call's citation may be copied onto
    /// another's; neither can be prevented, and neither changes what the
    /// trace states about the operation, whose name is its type's. An empty
    /// citation is never hidden: every surface says `(no citation given)`.
    Citation citation;

    /// How many inputs the call passes.
    static constexpr std::size_t inputCount = sizeof...(Inputs);

    /// Whether this call, its operation or one of its inputs was refused: then
    /// nothing downstream asks about its stand-in dimensions again, and its
    /// outputs are never evaluated.
    static constexpr detail::RefusedFlag refused = !detail::RequireOpaqueCallValid<Op, Rational, Inputs...>::value;

    /// The dimension of each output, in the order the operation declares them.
    static constexpr std::array<Dimension, Op::outputs.size()> output_dimensions = detail::
        opaque_output_dimensions<Op, detail::RequireOpaqueCallValid<Op, Rational, Inputs...>::dimensionsOk, Inputs...>();
};

/// Calls @p Op on @p inputs, for the reason @p citation gives:
/// `opaque<SeriesSpan>({ .title = "...", .reference = "Example Standard 12" },
/// series<Reading, 4>)`.
///
/// The citation is first and not deduced, so that a designated initialiser
/// can be written in place, and it has no default: the operation's name is
/// static, but which standard tells the method to use it is the call site's
/// to say.
template <OpaqueOperation Op, typename... Inputs>
[[nodiscard]] constexpr OpaqueCall<Op, Inputs...> opaque(Citation citation, Inputs... inputs) noexcept
{
    return OpaqueCall<Op, Inputs...> { std::tuple<Inputs...> { inputs... }, citation };
}

namespace detail
{
    /// The position of the output named @p Name among @p Op's, or
    /// `Op::outputs.size()` when it declares none of that name.
    template <typename Op, FixedString Name>
    [[nodiscard]] consteval std::size_t output_index() noexcept
    {
        for (std::size_t at = 0; at < Op::outputs.size(); ++at)
            if (Op::outputs[at] == Name.view())
                return at;
        return Op::outputs.size();
    }

    /// Fails to compile when @p Op declares no output named @p Name. Named so
    /// that both print.
    template <typename Op, FixedString Name>
    struct RequireOpaqueOutputNamed
    {
        static_assert(output_index<Op, Name>() < Op::outputs.size(),
                      "formula: this opaque operation has no output of that name; the operation and the name asked "
                      "for appear in this diagnostic as the template arguments of RequireOpaqueOutputNamed");

        static constexpr bool value = true;
    };

    /// The position an `OpaqueOutputNode` has when `opaque_output` found no
    /// output of the name asked for: past every real position, so that the
    /// node is refused and asks nothing more.
    inline constexpr std::size_t unknownOutput = static_cast<std::size_t>(-1);

    /// Where an `OpaqueOutputNode` came from: `opaque_output` with a name the
    /// operation declares (and by default, so also a node built by hand) ...
    struct NamedOpaqueOutput
    {
    };

    /// ... or `opaque_output` with a name it does not, which has been refused
    /// in `RequireOpaqueOutputNamed`'s words already. Only this origin may
    /// hold a position past every output: the same position built by hand,
    /// over a sound call, is refused by `RequireOpaqueOutputPosition`, and
    /// never silences the checks over it without a message.
    struct UnnamedOpaqueOutput
    {
    };

    /// Fails to compile when an opaque output's position names none of its
    /// operation's outputs -- reachable only by building the node by hand,
    /// at any position past the last, the sentinel included.
    template <std::size_t I, typename Call, typename Origin>
    struct RequireOpaqueOutputPosition
    {
        static_assert(std::is_same_v<Origin, UnnamedOpaqueOutput> || I < Call::operation::outputs.size(),
                      "formula: this opaque output's position names no output of its operation; build an output with "
                      "opaque_output<\"name\">(call), which finds its position by name");

        static constexpr bool value = true;
    };

    /// Fails to compile when a whole opaque call is used where one value is
    /// expected.
    template <typename Call>
    struct RequireOpaqueOutputChosen
    {
        static_assert(!std::is_same_v<Call, Call>,
                      "formula: an opaque call is not a value, since an operation may have several outputs; choose "
                      "one with opaque_output<\"name\">(call)");

        static constexpr bool value = true;
    };

    /// A bound whole opaque call handed to a verb that answers with one value
    /// (`yields.hpp`): refused as `checked_evaluate` refuses the call itself.
    template <OpaqueOperation Op, typename... Inputs>
    struct RequireSingleValueBound<OpaqueCall<Op, Inputs...>>: RequireOpaqueOutputChosen<OpaqueCall<Op, Inputs...>>
    {
    };

    /// Fails to compile when the unit a `rounded_output` is stated in does not
    /// measure the dimension of the output it rounds. Silent over a refused
    /// call or an output of no declared name: @p Output is refused already.
    template <typename Output, Unit U>
    struct RequireRoundedOutputUnitMatches
    {
        static_assert(refused_already<Output>() || U.dimension == Output::dimension,
                      "formula: this rounded_output names a unit that does not measure the dimension of the output it "
                      "rounds; the output and the unit appear in this diagnostic as the template arguments of "
                      "RequireRoundedOutputUnitMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when that unit has an offset, as degrees Celsius has:
    /// an operation's output -- a fitted coefficient, a spread -- is a
    /// difference or a ratio, and every reader converts the node's value
    /// through the unit, offset included. Asked only when @p DimensionMatches,
    /// so that a unit wrong in both ways draws the dimension's one message, as
    /// `RequireRootUnitWithoutOffset` is gated (`rounded_root.hpp`).
    template <Unit U, bool DimensionMatches>
    struct RequireRoundedOutputUnitWithoutOffset
    {
        static_assert(!DimensionMatches || U.offsetNumerator == 0,
                      "formula: this rounded_output names a unit with an offset, such as degrees Celsius; a fitted "
                      "coefficient is a difference or a ratio, which an offset unit would misstate; name the unit "
                      "without its offset");

        static constexpr bool value = true;
    };
} // namespace detail

/// Output @p I of the opaque call @p Call: one value, and so a `Node`.
/// @p Origin is `detail::`, and says whether `opaque_output` found the name
/// (see `detail::UnnamedOpaqueOutput`); leave it to its default.
///
/// No `{}` initialiser on `call`, deliberately: see `Corrections`
/// (`lookup.hpp`).
template <std::size_t I, typename Call, typename Origin = detail::NamedOpaqueOutput>
struct OpaqueOutputNode: NodeBase
{
    static_assert(detail::RequireOpaqueOutputPosition<I, Call, Origin>::value);

    /// The call whose output this is. Evaluating this node evaluates it whole.
    Call call;

    /// The dimension the operation declares for this output.
    static constexpr Dimension dimension = I < Call::output_dimensions.size() ? Call::output_dimensions[I] : dim::Scalar;
    /// The output's position among the operation's outputs, zero-based.
    static constexpr std::size_t index = I;
    /// The output's name, as the operation declares it.
    static constexpr std::string_view output =
        I < Call::operation::outputs.size() ? Call::operation::outputs[I] : std::string_view {};
    /// Whether the call was refused (`OpaqueCall::refused`), or this position
    /// names no output: then every check over this node is silent
    /// (`detail::refused_already`), and it is never evaluated.
    static constexpr detail::RefusedFlag refused = Call::refused || !(I < Call::operation::outputs.size());
};

/// The output named @p Name of @p call: `opaque_output<"slope">(fit)`. An
/// output the operation does not declare is refused once, in this library's
/// words; the node is then refused (`OpaqueOutputNode::refused`), and nothing
/// over it asks about its dimension again.
template <detail::FixedString Name, OpaqueOperation Op, typename... Inputs>
[[nodiscard]] constexpr auto opaque_output(OpaqueCall<Op, Inputs...> call) noexcept
{
    using Call = OpaqueCall<Op, Inputs...>;
    constexpr std::size_t namedAt = detail::output_index<Op, Name>();
    static_assert(std::conditional_t<detail::opaque_operation_well_formed<Op>,
                                     detail::RequireOpaqueOutputNamed<Op, Name>,
                                     std::true_type>::value);
    if constexpr (namedAt < Op::outputs.size())
        return OpaqueOutputNode<namedAt, Call> { {}, call };
    else
        return OpaqueOutputNode<detail::unknownOutput, Call, detail::UnnamedOpaqueOutput> { {}, call };
}

/// Output @p I of the opaque call @p Call, rounded to @p Places decimal places
/// of @p U under @p Mode -- the decimal the operation's true output rounds to,
/// exact: the fused counterpart of `opaque_output`, as `rounded_sqrt` is of
/// `rounded<>` around a root. Where that output is a fraction too wide for
/// `Rational`, it is still answered for an operation that states its outputs
/// in wider integers (`detail::declares_compute_exact`); any other operation's
/// output is computed in `Rational`, rounded exactly, and fails with
/// `Overflow` where `opaque_output` would. `rounded<>(opaque_output<>(...))`
/// keeps meaning what it says, an exact output rounded afterwards, which fails
/// where the exact output does.
///
/// Its dimension is the output's, and it rounds in `U`, never in the coherent
/// unit. @p Origin says whether `rounded_output` found the name, as
/// `OpaqueOutputNode`'s does (see `detail::UnnamedOpaqueOutput`); leave it to
/// its default.
///
/// No `{}` initialiser on `call`, deliberately, as on every member that holds
/// an expression or a call: a `{}` default member initialiser is instantiated
/// outside the immediate context of a default-constructibility probe, such as
/// the one `std::tuple`'s default constructor makes, and turns the probe into
/// a hard error, as measured on clang++, clang-cl and g++ for the members
/// `Corrections` (`lookup.hpp`) describes.
template <std::size_t I,
          typename Call,
          Unit U,
          DecimalPlaces Places,
          RoundingMode Mode,
          typename Origin = detail::NamedOpaqueOutput>
struct RoundedOpaqueOutputNode: NodeBase
{
    static_assert(detail::RequireOpaqueOutputPosition<I, Call, Origin>::value);
    static_assert(detail::RequireRoundedOutputUnitMatches<OpaqueOutputNode<I, Call, Origin>, U>::value);
    static_assert(detail::RequireRoundedOutputUnitWithoutOffset<
                  U,
                  !OpaqueOutputNode<I, Call, Origin>::refused
                      && U.dimension == OpaqueOutputNode<I, Call, Origin>::dimension>::value);
    static_assert(detail::RequireNamedScaledScalar<U>::value);
    static_assert(detail::RequireAsciiKey<U>::value);

    /// The call whose output this is. Evaluating this node evaluates it whole.
    Call call;

    /// The unit the rounding happens in, and the step's value is stated in.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to go; the three half modes differ only on a tie: an output
    /// exactly half a unit past the kept places.
    static constexpr RoundingMode mode = Mode;
    /// The dimension the operation declares for this output.
    static constexpr Dimension dimension = OpaqueOutputNode<I, Call, Origin>::dimension;
    /// The output's position among the operation's outputs, zero-based.
    static constexpr std::size_t index = I;
    /// The output's name, as the operation declares it.
    static constexpr std::string_view output = OpaqueOutputNode<I, Call, Origin>::output;
    /// Whether the call was refused, or this position names no output
    /// (`OpaqueOutputNode::refused`): then every check over this node is
    /// silent, and it is never evaluated.
    static constexpr detail::RefusedFlag refused = OpaqueOutputNode<I, Call, Origin>::refused;
};

/// The output named @p Name of @p call, rounded to @p Places decimal places of
/// @p U under @p Mode, exactly:
/// `rounded_output<"slope", millimetrePerSecond, DecimalPlaces { 4 }, RoundingMode::HalfEven>(fit)`.
/// An output the operation does not declare is refused once, in
/// `opaque_output`'s words.
template <detail::FixedString Name, Unit U, DecimalPlaces Places, RoundingMode Mode, OpaqueOperation Op, typename... Inputs>
[[nodiscard]] constexpr auto rounded_output(OpaqueCall<Op, Inputs...> call) noexcept
{
    using Call = OpaqueCall<Op, Inputs...>;
    constexpr std::size_t namedAt = detail::output_index<Op, Name>();
    static_assert(std::conditional_t<detail::opaque_operation_well_formed<Op>,
                                     detail::RequireOpaqueOutputNamed<Op, Name>,
                                     std::true_type>::value);
    if constexpr (namedAt < Op::outputs.size())
        return RoundedOpaqueOutputNode<namedAt, Call, U, Places, Mode> { {}, call };
    else
        return RoundedOpaqueOutputNode<detail::unknownOutput, Call, U, Places, Mode, detail::UnnamedOpaqueOutput> { {},
                                                                                                                    call };
}

/// The output named @p Name of @p call, rounded as @p R names:
/// `rounded_output<"slope", fourPlaces>(fit)`.
template <detail::FixedString Name, DecimalRounding R, OpaqueOperation Op, typename... Inputs>
[[nodiscard]] constexpr auto rounded_output(OpaqueCall<Op, Inputs...> call) noexcept
{
    return rounded_output<Name, R.unit, R.places, R.mode>(call);
}

namespace detail
{
    /// The output @p node rounds, as `opaque_output` builds it: for the walks
    /// that treat the two alike -- `document`, an overlay's rewrite, `render`.
    template <std::size_t I, typename Call, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin>
    [[nodiscard]] constexpr OpaqueOutputNode<I, Call, Origin> unrounded(
        RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin> const& node) noexcept
    {
        return OpaqueOutputNode<I, Call, Origin> { {}, node.call };
    }
} // namespace detail

namespace detail
{
    /// Evaluates one input of an opaque call: raw observations through
    /// `evaluate_observations`, a series through
    /// `dispatch_series`, a curve through `dispatch_curve`, a single value
    /// through `dispatch`.
    template <typename Rep, typename Input, typename Env, typename Sink>
    [[nodiscard]] constexpr auto evaluate_opaque_input(Input const& input, Env const& environment, Sink sink) noexcept
    {
        if constexpr (ObservationsNode<Input>)
            return evaluate_observations<Rep>(input, environment, sink);
        else if constexpr (SeriesNode<Input>)
            return dispatch_series<Rep>(input, environment, sink);
        else if constexpr (CurveExpression<Input>)
            return dispatch_curve<Rep>(input, environment, sink);
        else
            return dispatch<Rep>(input, environment, sink);
    }

    /// A failed input's failure, relayed: a single value's error alone; a
    /// series' or a curve's with its position when that is one of its own
    /// elements (`FailureSite::ResultElement`), as `relayed_failure` relays
    /// one.
    template <typename Rep>
    [[nodiscard]] constexpr OpaqueCallFailure opaque_input_failure(Evaluated<Rep> const& failed) noexcept
    {
        return OpaqueCallFailure { failed.error(), OpaqueFailure::Propagated, std::nullopt };
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr OpaqueCallFailure opaque_input_failure(EvaluatedSeries<Rep, N> const& failed) noexcept
    {
        SeriesFailure const relayed = relayed_failure(failed);
        return OpaqueCallFailure { relayed.error, OpaqueFailure::Propagated, relayed.element };
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr OpaqueCallFailure opaque_input_failure(EvaluatedCurve<Rep, N> const& failed) noexcept
    {
        std::optional<std::size_t> const failedAt =
            failed.error().site == FailureSite::ResultElement ? failed.error().element : std::nullopt;
        return OpaqueCallFailure { failed.error().error, OpaqueFailure::Propagated, failedAt };
    }

    /// A failed observations input's failure: its error, and the observation
    /// it arose at, counted as an observation.
    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr OpaqueCallFailure opaque_input_failure(
        EvaluatedObservations<Rep, Capacity> const& failed) noexcept
    {
        return OpaqueCallFailure {
            failed.error().error, OpaqueFailure::Propagated, failed.error().element, 0, failed.error().site
        };
    }

    /// Whether an evaluated input is wholly present: the value, every element
    /// of a series, every point and value of a curve.
    template <typename Rep>
    [[nodiscard]] constexpr bool opaque_input_present(std::optional<Rep> const& evaluatedValue) noexcept
    {
        return evaluatedValue.has_value();
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr bool opaque_input_present(SeriesValue<Rep, N> const& evaluatedSeries) noexcept
    {
        for (std::optional<Rep> const& candidate: evaluatedSeries.elements)
            if (!candidate.has_value())
                return false;
        return true;
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr bool opaque_input_present(CurveValue<Rep, N> const& evaluatedCurve) noexcept
    {
        for (std::size_t at = 0; at < N; ++at)
            if (!evaluatedCurve.domain[at].has_value() || !evaluatedCurve.values[at].has_value())
                return false;
        return true;
    }

    /// Raw observations are never absent: none made is an empty span.
    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr bool opaque_input_present(ObservationsValue<Rep, Capacity> const&) noexcept
    {
        return true;
    }

    /// The values `compute` reads for one evaluated input, held for as long as
    /// its spans point into them. Only built once the input is wholly present.
    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr std::array<Rep, N> opaque_values_of(std::array<std::optional<Rep>, N> const& elements) noexcept
    {
        std::array<Rep, N> held;
        for (std::size_t at = 0; at < N; ++at)
            held[at] = *elements[at];
        return held;
    }

    template <typename Rep>
    [[nodiscard]] constexpr Rep opaque_held(std::optional<Rep> const& evaluatedValue) noexcept
    {
        return *evaluatedValue;
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr std::array<Rep, N> opaque_held(SeriesValue<Rep, N> const& evaluatedSeries) noexcept
    {
        return opaque_values_of(evaluatedSeries.elements);
    }

    /// A curve's points and values, held for `compute`'s two spans.
    template <typename Rep, std::size_t N>
    struct OpaqueHeldCurve
    {
        std::array<Rep, N> points;
        std::array<Rep, N> pointValues;
    };

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr OpaqueHeldCurve<Rep, N> opaque_held(CurveValue<Rep, N> const& evaluatedCurve) noexcept
    {
        return OpaqueHeldCurve<Rep, N> { opaque_values_of(evaluatedCurve.domain), opaque_values_of(evaluatedCurve.values) };
    }

    /// The observations made, as `compute` reads them: a view into the
    /// evaluated value, which the input walk (`evaluate_opaque_inputs`) holds
    /// for as long as `compute` runs. Nothing is copied.
    template <typename Rep>
    struct OpaqueHeldObservations
    {
        /// The observations made, in the order made.
        std::span<Rep const> made;
    };

    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr OpaqueHeldObservations<Rep> opaque_held(
        ObservationsValue<Rep, Capacity> const& evaluatedObservations) noexcept
    {
        return OpaqueHeldObservations<Rep> { std::span<Rep const> { evaluatedObservations.elements.data(),
                                                                    evaluatedObservations.count } };
    }

    /// What one held input passes to `compute`, as a tuple.
    template <typename Rep>
    [[nodiscard]] constexpr std::tuple<Rep> opaque_arguments(Rep const& held) noexcept
    {
        return std::tuple<Rep> { held };
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr std::tuple<std::span<Rep const>> opaque_arguments(std::array<Rep, N> const& held) noexcept
    {
        return std::tuple<std::span<Rep const>> { std::span<Rep const> { held } };
    }

    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr std::tuple<std::span<Rep const>, std::span<Rep const>> opaque_arguments(
        OpaqueHeldCurve<Rep, N> const& held) noexcept
    {
        return std::tuple<std::span<Rep const>, std::span<Rep const>> { std::span<Rep const> { held.points },
                                                                        std::span<Rep const> { held.pointValues } };
    }

    template <typename Rep>
    [[nodiscard]] constexpr std::tuple<std::span<Rep const>> opaque_arguments(
        OpaqueHeldObservations<Rep> const& held) noexcept
    {
        return std::tuple<std::span<Rep const>> { held.made };
    }

    /// Calls @p onArguments with what every input, each wholly present,
    /// passes to `compute` in @p Rep: the values held for it, and the spans
    /// into them, which live until the call returns.
    template <typename Rep, typename OnArguments, typename... Evaluated>
    [[nodiscard]] constexpr auto opaque_apply(OnArguments const& onArguments, Evaluated const&... evaluatedInputs) noexcept
    {
        auto const held = std::tuple { opaque_held(evaluatedInputs)... };
        auto const arguments =
            std::apply([](auto const&... each) noexcept { return std::tuple_cat(opaque_arguments<Rep>(each)...); }, held);
        return std::apply(onArguments, arguments);
    }

    /// Calls `compute` on every input, each wholly present: the outputs, or
    /// the operation's own error.
    template <typename Rep, typename Op, typename... Evaluated>
    [[nodiscard]] constexpr std::expected<std::array<Rep, Op::outputs.size()>, ArithmeticError> opaque_compute(
        Evaluated const&... evaluatedInputs) noexcept
    {
        return opaque_apply<Rep>([](auto const&... each) noexcept { return Op::template compute<Rep>(each...); },
                                 evaluatedInputs...);
    }

    /// Evaluates the call's inputs from position @p At on, each once and in
    /// order, carrying the ones already evaluated; stops at the first that
    /// fails, and hands the inputs to @p finish only when every one is wholly
    /// present. @p finish answers `std::expected<Answer, ArithmeticError>`,
    /// and its error is the operation's own.
    template <typename Answer,
              typename Rep,
              std::size_t At,
              typename Op,
              typename... Inputs,
              typename Env,
              typename Sink,
              typename Finish,
              typename... Done>
    [[nodiscard]] constexpr std::expected<std::optional<Answer>, OpaqueCallFailure> evaluate_opaque_inputs(
        OpaqueCall<Op, Inputs...> const& call,
        Env const& environment,
        Sink sink,
        Finish const& finish,
        Done const&... evaluatedSoFar) noexcept
    {
        if constexpr (At == sizeof...(Inputs))
        {
            // Absence is decided here, after every input has been asked.
            if (!(opaque_input_present(evaluatedSoFar) && ...))
                return std::optional<Answer> {};
            std::expected<Answer, ArithmeticError> const answered = finish(evaluatedSoFar...);
            if (!answered.has_value())
                return std::unexpected { OpaqueCallFailure { answered.error(), OpaqueFailure::Own, std::nullopt } };
            return std::optional<Answer> { *answered };
        }
        else
        {
            auto const evaluatedInput = evaluate_opaque_input<Rep>(std::get<At>(call.inputs), environment, sink);
            if (!evaluatedInput.has_value())
            {
                OpaqueCallFailure stopped = opaque_input_failure(evaluatedInput);
                stopped.notEvaluated = sizeof...(Inputs) - At - 1;
                return std::unexpected { stopped };
            }
            return evaluate_opaque_inputs<Answer, Rep, At + 1>(
                call, environment, sink, finish, evaluatedSoFar..., *evaluatedInput);
        }
    }

    /// What a sink is told of @p call: its operation's name and outputs, the
    /// outputs' dimensions and the call's citation, as plain data.
    template <typename Op, typename... Inputs>
    [[nodiscard]] constexpr OpaqueCallInfo opaque_call_info(OpaqueCall<Op, Inputs...> const& call) noexcept
    {
        return OpaqueCallInfo { .name = Op::name,
                                .citation = call.citation,
                                .outputs = std::span<std::string_view const> { Op::outputs },
                                .dimensions = std::span<Dimension const> { OpaqueCall<Op, Inputs...>::output_dimensions } };
    }

    /// Whether @p Sink wants to hear about an opaque call: true when it
    /// defines **both** `opaque_entered(info)` and `opaque_produced(info,
    /// result)` -- `HearsSeries`' rule, for its reason (`series.hpp`).
    template <typename Sink, typename Rep, std::size_t M>
    concept HearsOpaque = requires(Sink sink, OpaqueCallInfo const& callInfo, OpaqueEvaluated<Rep, M> const& evaluated) {
        sink.opaque_entered(callInfo);
        sink.opaque_produced(callInfo, evaluated);
    };

    /// Evaluates the opaque call @p call: every input, then `compute`. A sink
    /// that asks is told before the first input and after the last
    /// (`HearsOpaque`), so that it can record the call as one step over its
    /// inputs' steps.
    template <typename Rep, typename Op, typename... Inputs, typename Env, typename Sink>
    [[nodiscard]] constexpr OpaqueEvaluated<Rep, Op::outputs.size()> evaluate_call(OpaqueCall<Op, Inputs...> const& call,
                                                                                   Env const& environment,
                                                                                   Sink sink) noexcept
    {
        constexpr std::size_t outputCount = Op::outputs.size();
        if constexpr (HearsOpaque<Sink, Rep, outputCount>)
            sink.opaque_entered(opaque_call_info(call));
        OpaqueEvaluated<Rep, outputCount> const evaluated = evaluate_opaque_inputs<std::array<Rep, outputCount>, Rep, 0>(
            call, environment, sink, [](auto const&... held) noexcept { return opaque_compute<Rep, Op>(held...); });
        if constexpr (HearsOpaque<Sink, Rep, outputCount>)
            sink.opaque_produced(opaque_call_info(call), evaluated);
        return evaluated;
    }

    /// Whether `compute` is sound for @p Rep: proven by the call's own checks
    /// for `Rational`, and checked here, in this library's words, for any
    /// other representation.
    template <typename Rep, typename Op, typename... Inputs>
    [[nodiscard]] consteval bool opaque_sound_for() noexcept
    {
        if constexpr (std::is_same_v<Rep, Rational>)
            return true;
        else
            return RequireOpaqueCallValid<Op, Rep, Inputs...>::value;
    }

    /// What a call evaluated for a `rounded_output` answers before it is
    /// rounded: output @p I exactly, wide when the operation declares the hook
    /// (`declares_compute_exact`), a `Rational` when it does not.
    template <typename Op, bool Exact>
    struct RoundedOpaqueAnswerOf
    {
        using type = Rational;
    };

    template <typename Op>
    struct RoundedOpaqueAnswerOf<Op, true>
    {
        using type = WideRatio<Op::exact_limbs>;
    };

    /// Output @p I of @p Op on every input, each wholly present: through
    /// `compute_exact` when @p Exact, through `compute<Rational>` otherwise.
    template <typename Op, std::size_t I, bool Exact, typename... Evaluated>
    [[nodiscard]] constexpr std::expected<typename RoundedOpaqueAnswerOf<Op, Exact>::type, ArithmeticError>
    opaque_rounding_answer(Evaluated const&... evaluatedInputs) noexcept
    {
        auto const computed = opaque_apply<Rational>(
            [](auto const&... each) noexcept {
                if constexpr (Exact)
                    return Op::compute_exact(each...);
                else
                    return Op::template compute<Rational>(each...);
            },
            evaluatedInputs...);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        return (*computed)[I];
    }

    /// The values a call evaluated for a rounded output holds: none. A
    /// constant at namespace scope, copied, rather than `{}` written inside a
    /// function, for `noSymbols`'s reason (`calculation.hpp`): cl 19.51
    /// value-initialises an array of `Rational` through a helper of its own
    /// that declares a local named `i`, which hides a consumer's global of that
    /// name (C4459, found by `consumer_globals_tests.cpp`).
    inline constexpr std::array<Rational, 0> noOpaqueValues {};

    /// What a sink is told of a call evaluated for a rounded output: whether it
    /// answered, was absent or failed -- and no value, since none exists yet.
    template <typename Answer>
    [[nodiscard]] constexpr OpaqueEvaluated<Rational, 0> without_values(
        std::expected<std::optional<Answer>, OpaqueCallFailure> const& answered) noexcept
    {
        if (!answered.has_value())
            return std::unexpected { answered.error() };
        if (!answered->has_value())
            return std::optional<std::array<Rational, 0>> {};
        return std::optional<std::array<Rational, 0>> { noOpaqueValues };
    }

    /// Evaluates the call @p call for its output @p I, rounded where it is
    /// used: every input, in `Rational`, as `evaluate_call` does, then
    /// `compute_exact` or `compute<Rational>`, keeping output @p I alone. A sink
    /// that asks is told of the call as `evaluate_call` tells it, with
    /// `OpaqueValues::RoundedWhereUsed` and an evaluation of no values.
    template <std::size_t I, typename Op, typename... Inputs, typename Env, typename Sink>
    [[nodiscard]] constexpr auto evaluate_rounded_call(OpaqueCall<Op, Inputs...> const& call,
                                                       Env const& environment,
                                                       Sink sink) noexcept
    {
        constexpr bool exact = declares_compute_exact<Op, Inputs...>;
        using Answer = typename RoundedOpaqueAnswerOf<Op, exact>::type;
        OpaqueCallInfo callInfo = opaque_call_info(call);
        callInfo.values = OpaqueValues::RoundedWhereUsed;
        if constexpr (HearsOpaque<Sink, Rational, 0>)
            sink.opaque_entered(callInfo);
        std::expected<std::optional<Answer>, OpaqueCallFailure> const answered =
            evaluate_opaque_inputs<Answer, Rational, 0>(call, environment, sink, [](auto const&... held) noexcept {
                return opaque_rounding_answer<Op, I, exact>(held...);
            });
        if constexpr (HearsOpaque<Sink, Rational, 0>)
            sink.opaque_produced(callInfo, without_values(answered));
        return answered;
    }

    /// @p computedExactly rounded in @p U: `rounded_in_unit` for a wide value,
    /// `RepRounding<Rational>::round_in` -- a rounding node's own -- for a
    /// `Rational`.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, std::size_t L>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_opaque_value(
        WideRatio<L> const& computedExactly) noexcept
    {
        return rounded_in_unit(computedExactly, U, Places, Mode);
    }

    template <Unit U, DecimalPlaces Places, RoundingMode Mode>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_opaque_value(Rational computedExactly) noexcept
    {
        return RepRounding<Rational>::round_in(computedExactly, U, Places, Mode);
    }
} // namespace detail

/// Evaluates output @p I of an opaque call: the whole call, then the one
/// output. Absent when the call is; a failure of the call, the operation's
/// own or an input's, is this output's.
///
/// The body is gated on the call being sound (`if constexpr`): without the
/// gate, g++ 14.2 follows a refused `compute`'s one message with errors of
/// its own -- the behaviour `checked_evaluate_series` records for its
/// refusal, and what `opaque_throwing_compute`'s REJECTs pin.
template <typename Rep = Rational, std::size_t I, typename Op, typename... Inputs, typename Origin, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    if constexpr (OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>::refused)
        return std::unexpected { ArithmeticError::DomainError };
    else if constexpr (!detail::opaque_sound_for<Rep, Op, Inputs...>())
        return std::unexpected { ArithmeticError::DomainError };
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
            OpaqueEvaluated<Rep, Op::outputs.size()> const called = detail::evaluate_call<Rep>(node.call, environment, sink);
            if (!called.has_value())
                return std::unexpected { called.error().error };
            if (!called->has_value())
                return detail::nothing<Rep>();
            return detail::present<Rep>((**called)[I]);
        }();
        sink.produced(node, evaluated);
        return evaluated;
    }
}

/// Evaluates a rounded opaque output: the whole call, then output @p I rounded
/// in `U`. Under `Rational`, exactly, through the operation's exact hook when it
/// has one (`detail::evaluate_rounded_call`); a failure of the call, the
/// operation's own or an input's, is this output's, and so is one of the
/// rounding itself. Under any other representation, that representation's own
/// output and its own rounding (`RepRounding<Rep>::round_in`) -- which for
/// `double` refuses to compile, as it does for every rounding node.
template <typename Rep = Rational,
          std::size_t I,
          typename Op,
          typename... Inputs,
          Unit U,
          DecimalPlaces Places,
          RoundingMode Mode,
          typename Origin,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(
    RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node,
    Env const& environment,
    Sink sink = {}) noexcept
{
    if constexpr (RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>::refused)
        return std::unexpected { ArithmeticError::DomainError };
    else if constexpr (!detail::opaque_sound_for<Rep, Op, Inputs...>())
        return std::unexpected { ArithmeticError::DomainError };
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
            if constexpr (std::is_same_v<Rep, Rational>)
            {
                auto const called = detail::evaluate_rounded_call<I>(node.call, environment, sink);
                if (!called.has_value())
                    return std::unexpected { called.error().error };
                if (!called->has_value())
                    return detail::nothing<Rep>();
                std::expected<Rational, ArithmeticError> const rounded =
                    detail::rounded_opaque_value<U, Places, Mode>(**called);
                if (!rounded.has_value())
                    return std::unexpected { rounded.error() };
                return detail::present<Rep>(*rounded);
            }
            else
            {
                OpaqueEvaluated<Rep, Op::outputs.size()> const called =
                    detail::evaluate_call<Rep>(node.call, environment, sink);
                if (!called.has_value())
                    return std::unexpected { called.error().error };
                if (!called->has_value())
                    return detail::nothing<Rep>();
                std::expected<Rep, ArithmeticError> const rounded =
                    RepRounding<Rep>::round_in((**called)[I], U, Places, Mode);
                if (!rounded.has_value())
                    return std::unexpected { rounded.error() };
                return detail::present<Rep>(*rounded);
            }
        }();
        sink.produced(node, evaluated);
        return evaluated;
    }
}

/// A whole opaque call handed to `checked_evaluate`: refused in this library's
/// words, pointing at `opaque_output`. The body is the refusal and nothing
/// else; what it returns is never seen.
template <Described Result, OpaqueOperation Op, typename... Inputs, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(OpaqueCall<Op, Inputs...> const&,
                                                                                         Env const&,
                                                                                         Sink = {}) noexcept
{
    static_assert(detail::RequireOpaqueOutputChosen<OpaqueCall<Op, Inputs...>>::value);
    return Outcome<Result>::empty();
}

/// A whole opaque call handed to `evaluate`: refused as `checked_evaluate`
/// refuses it.
template <Described Result, OpaqueOperation Op, typename... Inputs, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Result> evaluate(OpaqueCall<Op, Inputs...> const&, Env const&, Sink = {}) noexcept
{
    static_assert(detail::RequireOpaqueOutputChosen<OpaqueCall<Op, Inputs...>>::value);
    return Outcome<Result>::empty();
}

} // namespace formula
