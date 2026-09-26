// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A series: one quantity at each of the `N` points of a method's domain --
/// the masses retained on a screen analysis's screens, say -- evaluated
/// element by element.
///
/// A series is a family of expression nodes of its own (`SeriesNode`, declared
/// in `expression.hpp` beside `Node`), and deliberately not a kind of `Node`:
/// every `Node` promises one value, and a series in a scalar position is
/// therefore a compile error, which `checked_evaluate`, `evaluate` and
/// `variant<Tag>` put in this library's words.
///
/// **The length is static.** `N` is part of every series node's type, as it is
/// of the input (`MeasuredSeries<Q, N>`, `environment.hpp`), because a method's
/// domain is part of the method and not data. A point the laboratory did not
/// use is an absent element, and each element is absent or present on its own.
/// Evaluation is `noexcept` throughout and allocates nothing: a `std::vector`
/// allocating inside it would turn `bad_alloc` into `std::terminate`.
///
/// **A failure names its element.** A series evaluates to
/// `EvaluatedSeries<Rep, N>`: every element in the coherent SI unit, or a
/// `SeriesFailure` carrying the arithmetic error and, when the failure belongs
/// to one element, that element's zero-based position. There is no partial
/// series -- a series with one wrong element is not a series of right ones --
/// and there is no throwing spelling, which would have to drop the position.
///
/// This header holds the family's leaves, `series<Q, N>` and
/// `series_constant`; elementwise arithmetic; running totals, `cumulative`;
/// and `sum`, the bridge back to one value, which is a `Node`. And the two
/// entry points: `checked_evaluate_series_si<Rep>`, the representation-agnostic
/// core, and `checked_evaluate_series<Result>`, the auditable one, which
/// returns a `SeriesOutcome` in `Result`'s own unit.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <type_traits>

namespace formula
{

/// A named series input: the quantity is the key the environment is asked
/// with, and `N` the length it must have been supplied with.
///
/// Empty, like `VarNode`: its whole shape is in the type.
template <Described Q, std::size_t N>
struct SeriesVarNode: SeriesNodeBase
{
    static_assert(DescribesConsistentDimension<Q>,
                  "formula: this quantity describes a dimension its own unit does not measure, so "
                  "no formula containing it can be trusted; the quantity appears in this "
                  "diagnostic as the template argument of SeriesVarNode");
    static_assert(N > 0,
                  "formula: this series has no elements; a series is a value at each point of a method's domain, "
                  "and a domain of no points has nothing to sum, round or trace -- the quantity appears in this "
                  "diagnostic as the template argument of SeriesVarNode");

    /// The quantity this node names -- the key an `Environment` is asked with.
    using quantity = Q;

    /// The dimension of each element: the one `Q` describes.
    static constexpr Dimension dimension = Describe<Q>::dimension;

    /// How many elements the series has.
    static constexpr std::size_t length = N;
};

/// The spelling of a series in a formula: `series<Retained, 5>`.
///
/// `inline` for `var`'s reason: it is what guarantees the whole program one
/// object per specialisation. `series_cross_tu` uses it from two translation
/// units.
template <Described Q, std::size_t N>
inline constexpr SeriesVarNode<Q, N> series {};

namespace detail
{
    /// Fails to compile when a per-element constant is given a different
    /// number of values than its length. Named so both counts print.
    template <std::size_t Given, std::size_t Expected>
    struct RequireSeriesConstantCountMatches
    {
        static_assert(Given == Expected,
                      "formula: this series constant was given a different number of elements than its length; "
                      "the two counts appear in this diagnostic as the template arguments Given and Expected of "
                      "RequireSeriesConstantCountMatches -- give one value per point of the series");

        static constexpr bool value = true;
    };
} // namespace detail

/// The values of a per-element constant (`SeriesConstantNode`), one per point
/// of the series, in the series' own order.
///
/// `Corrections<N>`'s idiom (`lookup.hpp`), and for its reasons, in a type of
/// its own because "corrections" is the wrong word for a vector of factors or
/// limits. Two arity-disjoint constructors: the matching one builds `values`,
/// and every other non-zero count reaches a body whose `static_assert` names
/// both counts, rather than letting `std::array` pad the values nobody typed
/// with zeros. It is the node's own member type, so aggregate initialisation of
/// the node with no factory call is refused too.
///
/// **A count of zero is left to the compiler's words**, by the one rule
/// `Envelope` (`conformity.hpp`) follows too: a type an expression holds
/// leaves `{}` undeclared, and a type no expression holds may refuse `{}` in
/// this library's words. An `Elements` sits inside a method's variant, which
/// `std::tuple` holds, and `std::tuple` asks whether its members are
/// default-constructible: a constructor reachable from `{}` would answer
/// `true` for a node that cannot be built, as `Corrections`' would
/// (`lookup.hpp`). `series_constant` with no values is refused in words
/// already, so no author's spelling reaches the compiler's.
///
/// No `{}` default member initialiser, deliberately (defect class 4): a
/// constant must state its contents.
template <std::size_t N>
struct Elements
{
    /// The `N`-value case: the one path that actually builds `values`.
    template <typename... Rs>
        requires(sizeof...(Rs) == N) && (std::convertible_to<Rs, Rational> && ...)
    constexpr Elements(Rs... rs) noexcept:
        values { rs... }
    {
    }

    /// Every other non-zero count: fails to compile, naming both counts
    /// through `detail::RequireSeriesConstantCountMatches`.
    template <typename... Rs>
        requires(sizeof...(Rs) != N) && (sizeof...(Rs) != 0) && (std::convertible_to<Rs, Rational> && ...)
    constexpr Elements(Rs...) noexcept
    {
        static_assert(detail::RequireSeriesConstantCountMatches<sizeof...(Rs), N>::value);
    }

    /// One value per point, in the unit the constant is stated in.
    std::array<Rational, N> values;

    /// The value at zero-based position @p at, which must be below `N`.
    [[nodiscard]] constexpr Rational operator[](std::size_t at) const noexcept
    {
        return values[at];
    }

    /// How many values there are -- `N`.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }
};

/// A per-element constant: one value per point of the series, stated in `U` --
/// factors, offsets, or a limit per screen. Runtime state, as a scalar
/// `ConstantNode`'s number is, because the values may arrive from master data;
/// the length and the unit are the method's, and live in the type.
template <Unit U, std::size_t N>
struct SeriesConstantNode: SeriesNodeBase
{
    static_assert(N > 0,
                  "formula: this series constant has no elements; a series is a value at each point of a method's "
                  "domain, and a domain of no points has nothing to sum, round or trace -- give it at least one "
                  "value");

    /// The values. No `{}` initialiser, deliberately: see `Elements`.
    Elements<N> elements;

    /// The unit every value is stated in.
    static constexpr Unit unit = U;
    /// The dimension of each element: `U`'s.
    static constexpr Dimension dimension = U.dimension;
    /// How many elements the series has.
    static constexpr std::size_t length = N;
};

/// A per-element constant, its length counted from the values given:
/// `series_constant<unit::One>(rat(1), rat(2), rat(3))`.
template <Unit U, typename... Rs>
    requires(sizeof...(Rs) > 0) && (std::convertible_to<Rs, Rational> && ...)
[[nodiscard]] constexpr auto series_constant(Rs... values) noexcept
{
    return SeriesConstantNode<U, sizeof...(Rs)> { {}, Elements<sizeof...(Rs)> { values... } };
}

/// A per-element constant whose length is stated, `N`, so that it is tied to
/// the method's domain: `series_constant<unit::One, Screens>(...)`. A value
/// left out is refused naming both counts (`Elements`), never padded.
template <Unit U, std::size_t N, typename... Rs>
    requires(std::convertible_to<Rs, Rational> && ...)
[[nodiscard]] constexpr auto series_constant(Rs... values) noexcept
{
    return SeriesConstantNode<U, N> { {}, Elements<N> { values... } };
}

namespace detail
{
    /// The length of @p T when it is a series, and zero for a scalar operand
    /// broadcast to every element.
    template <typename T>
    [[nodiscard]] consteval std::size_t series_length_of() noexcept
    {
        if constexpr (SeriesNode<T>)
            return T::length;
        else
            return 0;
    }

    /// Whether two operands of an elementwise operation can be paired element
    /// by element: always, when one of them is a broadcast scalar.
    template <typename Left, typename Right>
    inline constexpr bool series_lengths_agree =
        !(SeriesNode<Left> && SeriesNode<Right>) || series_length_of<Left>() == series_length_of<Right>();

    /// Fails to compile when two series of different lengths are combined.
    /// Named so both series, each with its length, print.
    template <typename Left, typename Right>
    struct RequireSeriesLengthsAgree
    {
        static_assert(series_lengths_agree<Left, Right>,
                      "formula: the two series combined here have different lengths; the two series appear in "
                      "this diagnostic as the template arguments of RequireSeriesLengthsAgree, each with its "
                      "length -- elementwise arithmetic pairs element i with element i, so both sides must have "
                      "the method's one length");

        static constexpr bool value = true;
    };

    /// The two operands of an elementwise operation: each a `Node` or a
    /// `SeriesNode`, and at least one of them a series. Two `Node`s are the
    /// scalar operators' business (`expression.hpp`).
    template <typename Left, typename Right>
    concept ElementwiseOperands =
        (Node<Left> || SeriesNode<Left>) && (Node<Right> || SeriesNode<Right>) && (SeriesNode<Left> || SeriesNode<Right>);

    /// True for multiplication and division; addition and subtraction go
    /// through the scalar operators' own guard, `RequireAddendsAgree`, in its
    /// own words -- the same rule, applied at each element. That guard takes
    /// any two operands, not only two `Node`s, for this reason.
    template <BinaryOperator Op, typename Left, typename Right>
    struct ElementwiseDimensionsAgree: std::true_type
    {
    };

    template <typename Left, typename Right>
    struct ElementwiseDimensionsAgree<BinaryOperator::Add, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    template <typename Left, typename Right>
    struct ElementwiseDimensionsAgree<BinaryOperator::Subtract, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    /// `ElementwiseDimensionsAgree`'s rule as a plain answer, asserting
    /// nothing: whether the node's own dimension check would pass.
    template <BinaryOperator Op, typename Left, typename Right>
    inline constexpr bool elementwise_dimensions_agree =
        (Op != BinaryOperator::Add && Op != BinaryOperator::Subtract) || Left::dimension == Right::dimension;

    /// Whether @p T is a series node that has already been refused, or holds
    /// one: its `refused`, where it declares one, and false for every other
    /// operand (a scalar, or a leaf). A node over a refused operand asks no
    /// question of its own -- the operand's length and dimension are
    /// stand-ins taken after the refusal, and asking about them would report
    /// the one mistake a second time (defect class 2).
    template <typename T>
    [[nodiscard]] consteval bool refused_already() noexcept
    {
        if constexpr (requires { T::refused; })
            return T::refused;
        else
            return false;
    }
} // namespace detail

/// A `UnaryOperator` applied to every element of a series: `-m_r(i)`.
template <UnaryOperator Op, SeriesNode Operand>
struct ElementwiseUnaryNode: SeriesNodeBase
{
    /// The series the operator is applied to. No `{}` initialiser,
    /// deliberately: see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// Which operator this is.
    static constexpr UnaryOperator op = Op;
    /// Negation keeps each element's dimension.
    static constexpr Dimension dimension = Operand::dimension;
    /// As long as its operand.
    static constexpr std::size_t length = Operand::length;
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr bool refused = detail::refused_already<Operand>();
};

/// A `BinaryOperator` applied element by element: element i of the result is
/// element i of each series operand combined with element i of the other, or
/// with a scalar operand broadcast to every element (`m_r(i) / m_t`).
///
/// **A length mismatch is refused once, and gates the dimension check off**:
/// with the lengths known to disagree, whether the dimensions agree is a
/// second message about the same mistake, so it is not asked.
///
/// **A refused node gates every node above it off** (`refused`): its length
/// and dimension are then stand-ins -- the left operand's -- and a node
/// over it that compared them would report the same mistake again. So
/// `b + a + c` with a short `b` first, or a `b` of the wrong dimension first,
/// draws one message, as `a + b + c` does.
template <BinaryOperator Op, typename Left, typename Right>
    requires detail::ElementwiseOperands<Left, Right>
struct ElementwiseBinaryNode: SeriesNodeBase
{
    /// Whether either operand was already refused -- see
    /// `detail::refused_already`.
    static constexpr bool operandRefused = detail::refused_already<Left>() || detail::refused_already<Right>();

    static_assert(
        std::conditional_t<!operandRefused, detail::RequireSeriesLengthsAgree<Left, Right>, std::true_type>::value);
    static_assert(std::conditional_t<!operandRefused && detail::series_lengths_agree<Left, Right>,
                                     detail::ElementwiseDimensionsAgree<Op, Left, Right>,
                                     std::true_type>::value);

    /// Whether this node, or an operand of it, was refused: true once any
    /// check above failed, and read by every node built over this one.
    static constexpr bool refused = operandRefused || !detail::series_lengths_agree<Left, Right>
                                    || !detail::elementwise_dimensions_agree<Op, Left, Right>;

    /// The left-hand operand, a series or a scalar. No `{}` initialiser,
    /// deliberately: see `Corrections` (`lookup.hpp`).
    Left lhs;
    /// The right-hand operand, a series or a scalar.
    Right rhs;

    /// Which operator this is.
    static constexpr BinaryOperator op = Op;
    /// The series operands' length -- the left one's, when they disagree and
    /// have already been refused.
    static constexpr std::size_t length =
        SeriesNode<Left> ? detail::series_length_of<Left>() : detail::series_length_of<Right>();
    /// Each element's dimension, as the scalar operator computes it.
    static constexpr Dimension dimension = detail::combined_dimension<Op, Left::dimension, Right::dimension>();
};

/// Elementwise addition. At least one side is a series, the other a series of
/// the same length or a scalar broadcast to every element; both must measure
/// one dimension.
template <typename Left, typename Right>
    requires detail::ElementwiseOperands<Left, Right>
[[nodiscard]] constexpr auto operator+(Left lhs, Right rhs) noexcept
{
    return ElementwiseBinaryNode<BinaryOperator::Add, Left, Right> { {}, lhs, rhs };
}

/// Elementwise subtraction, as elementwise addition.
template <typename Left, typename Right>
    requires detail::ElementwiseOperands<Left, Right>
[[nodiscard]] constexpr auto operator-(Left lhs, Right rhs) noexcept
{
    return ElementwiseBinaryNode<BinaryOperator::Subtract, Left, Right> { {}, lhs, rhs };
}

/// Elementwise multiplication; each element's dimension is the product.
template <typename Left, typename Right>
    requires detail::ElementwiseOperands<Left, Right>
[[nodiscard]] constexpr auto operator*(Left lhs, Right rhs) noexcept
{
    return ElementwiseBinaryNode<BinaryOperator::Multiply, Left, Right> { {}, lhs, rhs };
}

/// Elementwise division; each element's dimension is the quotient.
template <typename Left, typename Right>
    requires detail::ElementwiseOperands<Left, Right>
[[nodiscard]] constexpr auto operator/(Left lhs, Right rhs) noexcept
{
    return ElementwiseBinaryNode<BinaryOperator::Divide, Left, Right> { {}, lhs, rhs };
}

/// Elementwise negation.
template <SeriesNode Operand>
[[nodiscard]] constexpr auto operator-(Operand operand) noexcept
{
    return ElementwiseUnaryNode<UnaryOperator::Negate, Operand> { {}, operand };
}

// A bare `Rational` beside a series is a dimensionless coefficient broadcast to
// every element, spelled out per operator exactly as `expression.hpp` spells
// it beside a `Node`.

/// `lhs + rhs`, with `rhs` a dimensionless coefficient.
template <SeriesNode Left>
[[nodiscard]] constexpr auto operator+(Left lhs, Rational rhs) noexcept
{
    return lhs + number(rhs);
}
/// `lhs + rhs`, with `lhs` a dimensionless coefficient.
template <SeriesNode Right>
[[nodiscard]] constexpr auto operator+(Rational lhs, Right rhs) noexcept
{
    return number(lhs) + rhs;
}
/// `lhs - rhs`, with `rhs` a dimensionless coefficient.
template <SeriesNode Left>
[[nodiscard]] constexpr auto operator-(Left lhs, Rational rhs) noexcept
{
    return lhs - number(rhs);
}
/// `lhs - rhs`, with `lhs` a dimensionless coefficient.
template <SeriesNode Right>
[[nodiscard]] constexpr auto operator-(Rational lhs, Right rhs) noexcept
{
    return number(lhs) - rhs;
}
/// `lhs * rhs`, with `rhs` a dimensionless coefficient.
template <SeriesNode Left>
[[nodiscard]] constexpr auto operator*(Left lhs, Rational rhs) noexcept
{
    return lhs * number(rhs);
}
/// `lhs * rhs`, with `lhs` a dimensionless coefficient.
template <SeriesNode Right>
[[nodiscard]] constexpr auto operator*(Rational lhs, Right rhs) noexcept
{
    return number(lhs) * rhs;
}
/// `lhs / rhs`, with `rhs` a dimensionless coefficient.
template <SeriesNode Left>
[[nodiscard]] constexpr auto operator/(Left lhs, Rational rhs) noexcept
{
    return lhs / number(rhs);
}
/// `lhs / rhs`, with `lhs` a dimensionless coefficient.
template <SeriesNode Right>
[[nodiscard]] constexpr auto operator/(Rational lhs, Right rhs) noexcept
{
    return number(lhs) / rhs;
}

/// One granularity per point of a series, in the series' own order: what
/// `rounded_elementwise` rounds each element to.
template <std::size_t N>
using PlacesTable = std::array<DecimalPlaces, N>;

namespace detail
{
    /// Whether @p Places is a `PlacesTable` of exactly @p N granularities.
    template <typename Places, std::size_t N>
    inline constexpr bool places_per_element = std::is_same_v<std::remove_cv_t<Places>, PlacesTable<N>>;

    /// Fails to compile when a per-element rounding's places are not a
    /// `PlacesTable` of one granularity per element of its series: a table of
    /// another length, or anything else (a lone `DecimalPlaces`, an array of
    /// `int`). Named so the places' type -- with its count, when it has one --
    /// and the length both print.
    template <typename Places, std::size_t N>
    struct RequirePlacesPerElement
    {
        static_assert(places_per_element<Places, N>,
                      "formula: this per-element rounding's places are not a PlacesTable<N> of one DecimalPlaces per "
                      "element of its series; the places' type and the series length appear in this diagnostic as "
                      "the template arguments of RequirePlacesPerElement -- give one granularity per point of the "
                      "series, as a PlacesTable<N>");

        static constexpr bool value = true;
    };
} // namespace detail

/// A series rounded element by element, each element to its own granularity:
/// element i to `Places[i]` decimal places of `U`, under `Mode` -- a method
/// that rounds a coarse screen to whole percent and a fine one to a tenth.
///
/// Rounding happens **in `U`**, exactly as `RoundNode`'s does, through the same
/// `RepRounding<Rep>::round_in`: each element is converted from the coherent
/// SI unit into `U`, rounded there, and converted back.
///
/// **A count mismatch gates the unit check off**, and an operand already
/// refused gates both: each would otherwise report one mistake twice.
template <Unit U, auto Places, RoundingMode Mode, SeriesNode S>
struct ElementwiseRoundNode: SeriesNodeBase
{
    /// Whether the operand was already refused -- see `detail::refused_already`.
    static constexpr bool operandRefused = detail::refused_already<S>();
    /// Whether the places are a table of one granularity per element. When
    /// they are not, nothing past the refusal reads them: the evaluator, the
    /// trace and the renderer each check this first, so that no index or loop
    /// over them adds a compiler-worded error to the one refusal.
    static constexpr bool countMatches = detail::places_per_element<decltype(Places), S::length>;

    static_assert(std::conditional_t<!operandRefused,
                                     detail::RequirePlacesPerElement<decltype(Places), S::length>,
                                     std::true_type>::value);
    static_assert(std::conditional_t<!operandRefused && countMatches,
                                     detail::RequireRoundingUnitMatches<U, S>,
                                     std::true_type>::value);

    /// The series rounded. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S operand;

    /// The unit every element is rounded in.
    static constexpr Unit unit = U;
    /// The granularity of each element, in order.
    static constexpr auto places = Places;
    /// Which way to break ties, and which way to go -- one mode for every
    /// element.
    static constexpr RoundingMode mode = Mode;
    /// Rounding changes a number, never its dimension.
    static constexpr Dimension dimension = S::dimension;
    /// As long as its operand.
    static constexpr std::size_t length = S::length;
    /// Whether this node, or its operand, was refused.
    static constexpr bool refused = operandRefused || !countMatches || !(U.dimension == S::dimension);
};

/// @p seriesOperand rounded element by element, element i to `Places[i]`
/// decimal places of `U`:
/// `rounded_elementwise<unit::Percent, places, RoundingMode::HalfEven>(series<Passing, 5>)`
/// with `places` a `PlacesTable<5>`.
template <Unit U, auto Places, RoundingMode Mode, SeriesNode S>
[[nodiscard]] constexpr auto rounded_elementwise(S seriesOperand) noexcept
{
    return ElementwiseRoundNode<U, Places, Mode, S> { {}, seriesOperand };
}

/// Which end of a series a running total starts from.
///
/// An `enum class` rather than a `bool`, and never defaulted: "a total running
/// from one end" is only half a formula, and the two ends give different
/// numbers at every element but one.
enum class CumulativeDirection : std::uint8_t
{
    /// From element 0 towards the last: element i is the total of elements 0
    /// to i.
    FromFirst,
    /// From the last element towards element 0: element i is the total of
    /// elements i to the last -- the mass retained on a screen and on every
    /// coarser one, when the screens are listed from fine to coarse.
    FromLast,
};

/// `direction` in the words a rendering and a trace use: `from first`,
/// `from last`.
[[nodiscard]] constexpr std::string_view describe(CumulativeDirection direction) noexcept
{
    switch (direction)
    {
        case CumulativeDirection::FromFirst:
            return "from first";
        case CumulativeDirection::FromLast:
            return "from last";
    }
    return "from an unknown end";
}

namespace detail
{
    /// Fails to compile when `sum` is given a single value. Named so the
    /// operand prints.
    template <typename Operand>
    struct RequireSumOfSeries
    {
        static_assert(SeriesNode<Operand>,
                      "formula: sum adds up the elements of a series, and this is a single value, not a series; "
                      "the operand appears in this diagnostic as the template argument of RequireSumOfSeries -- "
                      "read a quantity measured at every point with series<Q, N>");

        static constexpr bool value = true;
    };

    /// Fails to compile when `cumulative` is given a single value. Named so
    /// the operand prints.
    template <typename Operand>
    struct RequireCumulativeOfSeries
    {
        static_assert(SeriesNode<Operand>,
                      "formula: cumulative runs a total along a series, and this is a single value, not a series; "
                      "the operand appears in this diagnostic as the template argument of "
                      "RequireCumulativeOfSeries -- read a quantity measured at every point with series<Q, N>");

        static constexpr bool value = true;
    };
} // namespace detail

/// A running total along a series, from the end `D` names: element i of the
/// result is the total of the operand's elements from that end up to and
/// including element i.
///
/// **Absence stops the total.** Once an element is absent, the total there
/// and at every element after it, in the running direction, is absent: it
/// would include a value nobody measured. The totals before it stand.
template <CumulativeDirection D, SeriesNode S>
struct CumulativeNode: SeriesNodeBase
{
    /// The series the total runs along. No `{}` initialiser, deliberately:
    /// see `Corrections` (`lookup.hpp`).
    S operand;

    /// The end the total starts from.
    static constexpr CumulativeDirection direction = D;
    /// A total keeps each element's dimension.
    static constexpr Dimension dimension = S::dimension;
    /// As long as its operand.
    static constexpr std::size_t length = S::length;
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr bool refused = detail::refused_already<S>();
};

/// A running total along @p seriesOperand, from the end `D` names:
/// `cumulative<CumulativeDirection::FromLast>(series<Retained, 5>)`. The
/// direction has no default.
template <CumulativeDirection D, SeriesNode S>
[[nodiscard]] constexpr auto cumulative(S seriesOperand) noexcept
{
    return CumulativeNode<D, S> { {}, seriesOperand };
}

namespace detail
{
    /// What a refused `cumulative` of a single value stands for: a series,
    /// already refused (`refused`), so that nothing built over it -- a `sum`,
    /// another `cumulative`, an elementwise operator, `checked_evaluate` or
    /// `checked_evaluate_series` -- reports the one mistake a second time
    /// (defect class 2). It evaluates to a `DomainError` with no position and
    /// tells no sink: a program holding one never compiles, so neither is
    /// ever seen.
    template <Dimension D>
    struct RefusedSeries: SeriesNodeBase
    {
        /// The dimension of the single value it stands in for.
        static constexpr Dimension dimension = D;
        /// One element, as the single value was.
        static constexpr std::size_t length = 1;
        /// Always refused.
        static constexpr bool refused = true;
    };
} // namespace detail

/// A single value handed to `cumulative`: refused in this library's words,
/// returning a refused series (`detail::RefusedSeries`) that silences every
/// check downstream. The return type is deduced, so that the body -- the
/// refusal -- is instantiated wherever the call is, even inside an outer call
/// whose own overload resolution would otherwise fail first and hide it (cl
/// 19.51 did, for `checked_evaluate_series`).
template <CumulativeDirection D, Node N>
[[nodiscard]] constexpr auto cumulative(N) noexcept
{
    static_assert(detail::RequireCumulativeOfSeries<N>::value);
    return detail::RefusedSeries<N::dimension> {};
}

namespace detail
{
    /// Fails to compile when `rounded_elementwise` is given a single value.
    /// Named so the operand prints.
    template <typename Operand>
    struct RequireRoundElementwiseOfSeries
    {
        static_assert(SeriesNode<Operand>,
                      "formula: rounded_elementwise rounds each element of a series, and this is a single value, not "
                      "a series; the operand appears in this diagnostic as the template argument of "
                      "RequireRoundElementwiseOfSeries -- round a single value with rounded<U, Places, Mode>");

        static constexpr bool value = true;
    };
} // namespace detail

/// A single value handed to `rounded_elementwise`: refused in this library's
/// words, returning a refused series for `cumulative`'s reasons above.
template <Unit U, auto Places, RoundingMode Mode, Node N>
[[nodiscard]] constexpr auto rounded_elementwise(N) noexcept
{
    static_assert(detail::RequireRoundElementwiseOfSeries<N>::value);
    return detail::RefusedSeries<N::dimension> {};
}

/// The total of every element of a series: **one value**, and so a `Node`,
/// which stands wherever a number stands -- inside a method's variant, beside
/// a `var`, or broadcast back over the series it came from (`m_r(i) /
/// sum(m_r)`).
///
/// Absent when any element is absent, never the total of the ones that were
/// measured: a sum missing a screen is not the sum.
template <SeriesNode S>
struct SumNode: NodeBase
{
    /// The series summed. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S operand;

    /// A total has its elements' dimension.
    static constexpr Dimension dimension = S::dimension;
    /// Whether its operand was refused -- see `detail::refused_already`. A
    /// sum broadcast back over a series is then asked nothing either.
    static constexpr bool refused = detail::refused_already<S>();
};

/// The total of every element of @p seriesOperand: `sum(series<Retained, 5>)`.
template <SeriesNode S>
[[nodiscard]] constexpr auto sum(S seriesOperand) noexcept
{
    return SumNode<S> { {}, seriesOperand };
}

/// A single value handed to `sum`: refused in this library's words. It returns
/// the value itself, which is already the one value a sum would be, so nothing
/// downstream refuses again. The return type is deduced, for `cumulative`'s
/// reason above.
template <Node N>
[[nodiscard]] constexpr auto sum(N singleValue) noexcept
{
    static_assert(detail::RequireSumOfSeries<N>::value);
    return singleValue;
}

/// An evaluated series in the coherent SI unit of its dimension: one value per
/// element, each absent when the element was never measured.
template <typename Rep, std::size_t N>
struct SeriesValue
{
    /// The elements, in the series' own order.
    ///
    /// No `{}` initialiser, and the evaluators below default-initialise a
    /// `SeriesValue` rather than writing `{}`: an empty `std::optional` either
    /// way, but value-initialising an array of class type makes cl 19.51
    /// instantiate a compiler-internal `__builtin_array_init_helper` that
    /// declares an `i`, which hides a consumer's global of that name (C4459,
    /// an error under `/WX`). `consumer_globals_tests.cpp` found it here and on
    /// an array of `Measured` in `checked_evaluate_series`, which is
    /// default-initialised for the same reason. Not seen on clang or g++.
    std::array<std::optional<Rep>, N> elements;

    /// Element-wise equality.
    [[nodiscard]] constexpr bool operator==(SeriesValue const&) const noexcept = default;
};

/// Why a series could not be evaluated, and where.
struct SeriesFailure
{
    /// What went wrong.
    ArithmeticError error;

    /// The zero-based position, in the series the failing step produces, of
    /// the element it went wrong at -- or nothing, when the failure belongs to
    /// no single element (a reduction's overflow, say). Never a stand-in
    /// position for such a failure: element 0 would name a real element that
    /// did nothing wrong.
    ///
    /// Zero-based here, as every position in this library's API is. Every
    /// text the library writes -- a trace, a rendering, a message -- shows a
    /// position one-based.
    ///
    /// A failed scalar operand of an elementwise operation belongs to no
    /// element (`detail::operand_failure`), and produces the empty shape.
    std::optional<std::size_t> element;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SeriesFailure const&) const noexcept = default;
};

/// The result of evaluating a series: every element (each possibly absent), or
/// the one failure that stopped it. Absence stays per element; an arithmetic
/// error fails the whole series.
template <typename Rep, std::size_t N>
using EvaluatedSeries = std::expected<SeriesValue<Rep, N>, SeriesFailure>;

namespace detail
{
    /// Whether @p Sink wants to hear about the series node @p S: true when it
    /// defines **both** `series_entered(node)` and `series_produced(node,
    /// result)`, and false otherwise -- a sink defining only one is told
    /// nothing, rather than told half and left with an `entered` it will never
    /// see matched. The shape `evaluate_method` uses for `variant_entered` and
    /// `variant_produced` (`sink.hpp`), for the same reason. `NullSink`
    /// defines neither and pays nothing, and a sink written before series
    /// existed, whose `entered` and `produced` are constrained on `Node`,
    /// keeps compiling and is told nothing.
    template <typename Sink, typename S, typename Rep>
    concept HearsSeries = requires(Sink sink, S const& node, EvaluatedSeries<Rep, S::length> const& evaluated) {
        sink.series_entered(node);
        sink.series_produced(node, evaluated);
    };

    /// Tells @p sink that @p node is about to be evaluated, if it asks.
    template <typename Rep, SeriesNode S, typename Sink>
    constexpr void tell_series_entered(Sink& sink, S const& node)
    {
        if constexpr (HearsSeries<Sink, S, Rep>)
            sink.series_entered(node);
    }

    /// Tells @p sink what @p node produced, if it asks.
    template <typename Rep, SeriesNode S, typename Sink>
    constexpr void tell_series_produced(Sink& sink, S const& node, EvaluatedSeries<Rep, S::length> const& evaluated)
    {
        if constexpr (HearsSeries<Sink, S, Rep>)
            sink.series_produced(node, evaluated);
    }
} // namespace detail

/// Looks the series for `Q` up in `environment` and converts each present
/// element to the coherent SI unit of its dimension.
///
/// The environment refuses, at compile time and in this library's words, a
/// quantity it holds as a single value and a series of another length than
/// `N`.
///
/// A sink hears about the series through `series_entered` and
/// `series_produced` when it defines both (`detail::HearsSeries`), never
/// through `entered` and `produced`, which are constrained on `Node`.
template <typename Rep = Rational, Described Q, std::size_t N, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, N> checked_evaluate_series_si(SeriesVarNode<Q, N> const& node,
                                                                           Env const& environment,
                                                                           Sink sink = {}) noexcept
{
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, N> const evaluated = [&]() -> EvaluatedSeries<Rep, N> {
        MeasuredSeries<Q, N> const measured = environment.template get_series<Q, N>();
        SeriesValue<Rep, N> inCoherentUnit;
        for (std::size_t at = 0; at < N; ++at)
        {
            Measured<Q> const measuredElement = measured.element(at);
            if (measuredElement.is_absent())
                continue;
            Evaluated<Rep> const elementInSi = detail::in_si<Rep>(*measuredElement.stored(), Describe<Q>::unit);
            if (!elementInSi.has_value())
                return std::unexpected { SeriesFailure { elementInSi.error(), at } };
            inCoherentUnit.elements[at] = **elementInSi;
        }
        return inCoherentUnit;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

namespace detail
{
    /// Evaluates the series node @p node with @p sink: `detail::dispatch`'s
    /// counterpart for a `SeriesNode`.
    ///
    /// Every series node kind declares its own `checked_evaluate_series_si`
    /// in `namespace formula`, and ADL finds each at instantiation, including
    /// the ones declared after this point. There is no two-parameter fallback,
    /// unlike `dispatch`: no consumer overload of this name predates the
    /// three-parameter form, so there is nothing to keep reachable.
    template <typename Rep, typename S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSeries<Rep, std::remove_cvref_t<S>::length> dispatch_series(S const& node,
                                                                                                 Env const& environment,
                                                                                                 Sink sink) noexcept
    {
        return checked_evaluate_series_si<Rep>(node, environment, sink);
    }
} // namespace detail

namespace detail
{
    /// Evaluates one operand of an elementwise operation: a series through
    /// `dispatch_series`, a scalar through `dispatch` -- **once**, however long
    /// the series, so that a broadcast scalar is one step of the derivation.
    template <typename Rep, typename Operand, typename Env, typename Sink>
    [[nodiscard]] constexpr auto evaluate_operand(Operand const& operand, Env const& environment, Sink sink) noexcept
    {
        if constexpr (SeriesNode<Operand>)
            return dispatch_series<Rep>(operand, environment, sink);
        else
            return dispatch<Rep>(operand, environment, sink);
    }

    /// **The broadcast rule, in one place:** element @p at of an evaluated
    /// series operand, or the evaluated scalar itself, whatever @p at is.
    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr std::optional<Rep> element_operand(SeriesValue<Rep, N> const& evaluatedSeries,
                                                               std::size_t at) noexcept
    {
        return evaluatedSeries.elements[at];
    }

    /// The scalar half of the broadcast rule: the same value at every element.
    template <typename Rep>
    [[nodiscard]] constexpr std::optional<Rep> element_operand(std::optional<Rep> const& evaluatedScalar,
                                                               std::size_t) noexcept
    {
        return evaluatedScalar;
    }

    /// A failed series operand's failure, relayed as it is.
    template <typename Rep, std::size_t N>
    [[nodiscard]] constexpr SeriesFailure operand_failure(EvaluatedSeries<Rep, N> const& failed) noexcept
    {
        return failed.error();
    }

    /// A failed scalar operand's failure: it belongs to no element, since the
    /// scalar was evaluated once, before any element was computed.
    template <typename Rep>
    [[nodiscard]] constexpr SeriesFailure operand_failure(Evaluated<Rep> const& failed) noexcept
    {
        return SeriesFailure { failed.error(), std::nullopt };
    }

    /// `Op` applied to one pair of elements, exactly as the scalar
    /// `BinaryNode` applies it (`evaluate.hpp`).
    template <BinaryOperator Op, typename Rep>
    [[nodiscard]] constexpr std::expected<Rep, ArithmeticError> apply_binary(Rep leftValue, Rep rightValue) noexcept
    {
        if constexpr (Op == BinaryOperator::Add)
            return RepTraits<Rep>::add(leftValue, rightValue);
        else if constexpr (Op == BinaryOperator::Subtract)
            return RepTraits<Rep>::subtract(leftValue, rightValue);
        else if constexpr (Op == BinaryOperator::Multiply)
            return RepTraits<Rep>::multiply(leftValue, rightValue);
        else
            return RepTraits<Rep>::divide(leftValue, rightValue);
    }
} // namespace detail

/// Reads each value of a per-element constant into the coherent SI unit of its
/// dimension. Every element is present; a conversion that overflows fails the
/// series at that element.
template <typename Rep = Rational, Unit U, std::size_t N, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, N> checked_evaluate_series_si(SeriesConstantNode<U, N> const& node,
                                                                           Env const&,
                                                                           Sink sink = {}) noexcept
{
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, N> const evaluated = [&]() -> EvaluatedSeries<Rep, N> {
        SeriesValue<Rep, N> inCoherentUnit;
        for (std::size_t at = 0; at < N; ++at)
        {
            Evaluated<Rep> const elementInSi = detail::in_si<Rep>(node.elements[at], U);
            if (!elementInSi.has_value())
                return std::unexpected { SeriesFailure { elementInSi.error(), at } };
            inCoherentUnit.elements[at] = **elementInSi;
        }
        return inCoherentUnit;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

/// Negates each element. An absent element stays absent; a failure names its
/// element; a failed operand is relayed unchanged.
template <typename Rep = Rational, UnaryOperator Op, SeriesNode Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, Operand::length> checked_evaluate_series_si(
    ElementwiseUnaryNode<Op, Operand> const& node, Env const& environment, Sink sink = {}) noexcept
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    constexpr std::size_t seriesLength = Operand::length;
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, seriesLength> const evaluated = [&]() -> EvaluatedSeries<Rep, seriesLength> {
        EvaluatedSeries<Rep, seriesLength> const operandResult =
            detail::dispatch_series<Rep>(node.operand, environment, sink);
        if (!operandResult.has_value())
            return std::unexpected { operandResult.error() };
        SeriesValue<Rep, seriesLength> negated;
        for (std::size_t at = 0; at < seriesLength; ++at)
        {
            if (!operandResult->elements[at].has_value())
                continue;
            std::expected<Rep, ArithmeticError> const elementResult = RepTraits<Rep>::negate(*operandResult->elements[at]);
            if (!elementResult.has_value())
                return std::unexpected { SeriesFailure { elementResult.error(), at } };
            negated.elements[at] = *elementResult;
        }
        return negated;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

/// Combines the two operands element by element (`detail::element_operand`).
///
/// The left operand is evaluated, then the right, each once; a failure of
/// either fails the whole series at once -- a series operand's with its own
/// position, a scalar's with none. Only then is each element considered: an
/// element absent on either side is absent in the result and no other, an
/// absent scalar makes every element absent (S7), and an arithmetic error at
/// an element fails the whole series naming that element (S8). There is no
/// partial result.
template <typename Rep = Rational, BinaryOperator Op, typename Left, typename Right, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, ElementwiseBinaryNode<Op, Left, Right>::length> checked_evaluate_series_si(
    ElementwiseBinaryNode<Op, Left, Right> const& node, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t seriesLength = ElementwiseBinaryNode<Op, Left, Right>::length;
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, seriesLength> const evaluated = [&]() -> EvaluatedSeries<Rep, seriesLength> {
        auto const leftResult = detail::evaluate_operand<Rep>(node.lhs, environment, sink);
        if (!leftResult.has_value())
            return std::unexpected { detail::operand_failure(leftResult) };
        auto const rightResult = detail::evaluate_operand<Rep>(node.rhs, environment, sink);
        if (!rightResult.has_value())
            return std::unexpected { detail::operand_failure(rightResult) };

        SeriesValue<Rep, seriesLength> combined;
        for (std::size_t at = 0; at < seriesLength; ++at)
        {
            std::optional<Rep> const leftElement = detail::element_operand(*leftResult, at);
            std::optional<Rep> const rightElement = detail::element_operand(*rightResult, at);
            if (!leftElement.has_value() || !rightElement.has_value())
                continue;
            std::expected<Rep, ArithmeticError> const elementResult = detail::apply_binary<Op>(*leftElement, *rightElement);
            if (!elementResult.has_value())
                return std::unexpected { SeriesFailure { elementResult.error(), at } };
            combined.elements[at] = *elementResult;
        }
        return combined;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

/// A refused series (`detail::RefusedSeries`): a `DomainError` belonging to no
/// element, and nothing told to the sink. Never reached by a program that
/// compiles.
template <typename Rep = Rational, Dimension D, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, 1> checked_evaluate_series_si(detail::RefusedSeries<D> const&,
                                                                           Env const&,
                                                                           Sink = {}) noexcept
{
    return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
}

/// Rounds each element to its own granularity in `U` (`ElementwiseRoundNode`).
/// An absent element stays absent; a failed operand is relayed; a rounding
/// that fails -- an overflow converting into `U`, say -- fails the whole
/// series at that element.
template <typename Rep = Rational, Unit U, auto Places, RoundingMode Mode, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, S::length> checked_evaluate_series_si(
    ElementwiseRoundNode<U, Places, Mode, S> const& node, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t seriesLength = S::length;
    // Refused already (`countMatches`): the places are not a table to index.
    if constexpr (!ElementwiseRoundNode<U, Places, Mode, S>::countMatches)
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else
    {
        detail::tell_series_entered<Rep>(sink, node);
        EvaluatedSeries<Rep, seriesLength> const evaluated = [&]() -> EvaluatedSeries<Rep, seriesLength> {
            EvaluatedSeries<Rep, seriesLength> const operandResult =
                detail::dispatch_series<Rep>(node.operand, environment, sink);
            if (!operandResult.has_value())
                return std::unexpected { operandResult.error() };

            SeriesValue<Rep, seriesLength> roundedElements;
            for (std::size_t at = 0; at < seriesLength; ++at)
            {
                if (!operandResult->elements[at].has_value())
                    continue;
                std::expected<Rep, ArithmeticError> const elementResult =
                    RepRounding<Rep>::round_in(*operandResult->elements[at], U, Places[at], Mode);
                if (!elementResult.has_value())
                    return std::unexpected { SeriesFailure { elementResult.error(), at } };
                roundedElements.elements[at] = *elementResult;
            }
            return roundedElements;
        }();
        detail::tell_series_produced<Rep>(sink, node, evaluated);
        return evaluated;
    }
}

/// The running total along the operand, from the end `D` names.
///
/// A failed operand is relayed unchanged. Once an element is absent, the total
/// there and at every later element in the running direction stays absent
/// (`CumulativeNode`). A total that overflows fails the whole series at the
/// element whose addition overflowed.
template <typename Rep = Rational, CumulativeDirection D, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, S::length> checked_evaluate_series_si(CumulativeNode<D, S> const& node,
                                                                                   Env const& environment,
                                                                                   Sink sink = {}) noexcept
{
    constexpr std::size_t seriesLength = S::length;
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, seriesLength> const evaluated = [&]() -> EvaluatedSeries<Rep, seriesLength> {
        EvaluatedSeries<Rep, seriesLength> const operandResult =
            detail::dispatch_series<Rep>(node.operand, environment, sink);
        if (!operandResult.has_value())
            return std::unexpected { operandResult.error() };

        SeriesValue<Rep, seriesLength> totals;
        std::optional<Rep> runningTotal;
        for (std::size_t taken = 0; taken < seriesLength; ++taken)
        {
            std::size_t const at = D == CumulativeDirection::FromFirst ? taken : seriesLength - 1 - taken;
            std::optional<Rep> const addend = operandResult->elements[at];
            // This total and every later one would include a value nobody
            // measured; they stay absent.
            if (!addend.has_value())
                break;
            if (!runningTotal.has_value())
                runningTotal = *addend;
            else
            {
                std::expected<Rep, ArithmeticError> const added = RepTraits<Rep>::add(*runningTotal, *addend);
                if (!added.has_value())
                    return std::unexpected { SeriesFailure { added.error(), at } };
                runningTotal = *added;
            }
            totals.elements[at] = runningTotal;
        }
        return totals;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

/// The total of every element: one value. A failed operand's error is relayed
/// -- its position cannot be, since the result is one value, not a series. Any
/// absent element makes the total absent, judged over the whole series before
/// anything is added, as `interpolate_at` judges a curve: where the gap is
/// plays no part, and a gap after an addition that would overflow still makes
/// the total absent. An overflow of a total with every element present fails
/// it, with no element to name.
template <typename Rep = Rational, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SumNode<S> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr std::size_t seriesLength = S::length;
    sink.entered(node);
    Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
        EvaluatedSeries<Rep, seriesLength> const operandResult =
            detail::dispatch_series<Rep>(node.operand, environment, sink);
        if (!operandResult.has_value())
            return std::unexpected { operandResult.error().error };

        for (std::optional<Rep> const& candidate: operandResult->elements)
            if (!candidate.has_value())
                return detail::nothing<Rep>();

        std::optional<Rep> runningTotal;
        for (std::size_t at = 0; at < seriesLength; ++at)
        {
            std::optional<Rep> const addend = operandResult->elements[at];
            if (!runningTotal.has_value())
                runningTotal = *addend;
            else
            {
                std::expected<Rep, ArithmeticError> const added = RepTraits<Rep>::add(*runningTotal, *addend);
                if (!added.has_value())
                    return std::unexpected { added.error() };
                runningTotal = *added;
            }
        }
        return Evaluated<Rep> { runningTotal };
    }();
    sink.produced(node, evaluated);
    return evaluated;
}

/// The result of evaluating a series for quantity @p Q: `N` measurements in
/// `Q`'s declared unit, and where they came from.
///
/// One source for the whole series: a series is either computed by this
/// library or typed in by a person, never a mixture, because an override
/// replaces the whole result (`checked_evaluate_series`).
template <Described Q, std::size_t N>
class SeriesOutcome
{
  public:
    /// A series and its provenance -- the counterpart of `Outcome::value`.
    [[nodiscard]] static constexpr SeriesOutcome value(std::array<Measured<Q>, N> measurements, ValueSource source) noexcept
    {
        return SeriesOutcome { measurements, source };
    }

    /// Every element, in order.
    [[nodiscard]] constexpr std::array<Measured<Q>, N> elements() const noexcept
    {
        return _elements;
    }

    /// The element at zero-based position @p at. Past the end it is absent:
    /// never a neighbour's value, and never a zero -- but not reported
    /// either, so a caller indexes only inside `for (at = 0; at < N; ++at)`,
    /// never with a position it computed.
    [[nodiscard]] constexpr Measured<Q> element(std::size_t at) const noexcept
    {
        return at < N ? _elements[at] : Measured<Q>::absent();
    }

    /// How many elements there are -- `N`, present or not.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }

    /// Where the series came from.
    [[nodiscard]] constexpr ValueSource source() const noexcept
    {
        return _source;
    }

    /// True when a person typed this series in place of a computed one.
    [[nodiscard]] constexpr bool is_overridden() const noexcept
    {
        return _source == ValueSource::ManuallyEntered;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SeriesOutcome const&) const noexcept = default;

  private:
    constexpr SeriesOutcome(std::array<Measured<Q>, N> measurements, ValueSource source) noexcept:
        _elements { measurements },
        _source { source }
    {
    }

    std::array<Measured<Q>, N> _elements;
    ValueSource _source;
};

/// Evaluates the series @p expression for quantity @p Result, element by
/// element, and returns each element in `Result`'s declared unit.
///
/// `Result` is never deduced, for `checked_evaluate`'s reason. When the
/// environment holds an entered value for `Result`, that is returned with
/// `ValueSource::ManuallyEntered` and the expression is not evaluated; an
/// entered **single** value for `Result` is refused where it is read, because a
/// series cannot be answered with one number.
///
/// There is deliberately no throwing twin: an exception would have to drop the
/// position `SeriesFailure` carries.
template <Described Result, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<SeriesOutcome<Result, S::length>, SeriesFailure> checked_evaluate_series(
    S const& expression, Env const& environment, Sink sink = {}) noexcept
{
    static_assert(detail::RequireResultDimension<Result, S>::value);
    constexpr std::size_t seriesLength = S::length;

    // Gated, unlike `checked_evaluate`: without it g++ 13.3 and 14.2 follow
    // this one refusal with errors from the evaluation below, among them "no
    // matching function for call to ~expected()", while cl 19.51, clang-cl
    // and clang++ give one message either way. Pinned by the REJECT on
    // `evaluate_series_result_dimension_mismatch`.
    if constexpr (!(Describe<Result>::dimension == S::dimension))
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else if constexpr (Env::template is_entered<Result>)
    {
        MeasuredSeries<Result, seriesLength> const typedIn = environment.template get_series<Result, seriesLength>();
        std::array<Measured<Result>, seriesLength> measurements;
        for (std::size_t at = 0; at < seriesLength; ++at)
            measurements[at] = typedIn.element(at);
        return SeriesOutcome<Result, seriesLength>::value(measurements, ValueSource::ManuallyEntered);
    }
    else
    {
        EvaluatedSeries<Rational, seriesLength> const computed =
            detail::dispatch_series<Rational>(expression, environment, sink);
        if (!computed.has_value())
            return std::unexpected { computed.error() };

        std::array<Measured<Result>, seriesLength> inDeclaredUnit;
        for (std::size_t at = 0; at < seriesLength; ++at)
        {
            if (!computed->elements[at].has_value())
                continue;
            std::expected<Rational, ArithmeticError> const elementInResultUnit =
                checked_convert(*computed->elements[at], coherent(S::dimension), Describe<Result>::unit);
            if (!elementInResultUnit.has_value())
                return std::unexpected { SeriesFailure { elementInResultUnit.error(), at } };
            inDeclaredUnit[at] = Measured<Result> { *elementInResultUnit };
        }
        return SeriesOutcome<Result, seriesLength>::value(inDeclaredUnit, ValueSource::Derived);
    }
}

} // namespace formula
