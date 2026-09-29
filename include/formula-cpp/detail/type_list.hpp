// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula::detail
{

/// Index of @p T within @p Tuple, as a compile-time constant.
template <typename T, typename Tuple>
struct index_in_tuple;

template <typename T, typename... Ts>
struct index_in_tuple<T, std::tuple<Ts...>>
{
  private:
    static constexpr std::size_t compute()
    {
        constexpr bool matches[] = { std::is_same_v<T, Ts>..., false };
        for (std::size_t i = 0; i < sizeof...(Ts); ++i)
            if (matches[i])
                return i;
        return sizeof...(Ts);
    }

  public:
    static constexpr std::size_t value = compute();
    static_assert(value < sizeof...(Ts), "formula: type not found in the argument list");
};

template <typename T, typename Tuple>
inline constexpr std::size_t index_in_tuple_v = index_in_tuple<T, Tuple>::value;

/// A list of quantity types.
///
/// Nothing requires the members to be quantities: the utilities below only
/// compare, select and join the types they are given.
template <typename... Qs>
struct QuantityList
{
};

/// The concatenation of quantity lists.
template <typename... Lists>
struct JoinQuantities
{
    /// The empty join.
    using type = QuantityList<>;
};

template <typename... Qs>
struct JoinQuantities<QuantityList<Qs...>>
{
    /// One list, unchanged.
    using type = QuantityList<Qs...>;
};

template <typename... As, typename... Bs, typename... Rest>
struct JoinQuantities<QuantityList<As...>, QuantityList<Bs...>, Rest...>:
    JoinQuantities<QuantityList<As..., Bs...>, Rest...>
{
};

/// The number of quantities in a quantity list.
template <typename List>
struct QuantityCount;

template <typename... Qs>
struct QuantityCount<QuantityList<Qs...>>
{
    /// The length of the list, repeats and all.
    static constexpr std::size_t value = sizeof...(Qs);
};

/// The number of quantities in @p List, repeats and all.
template <typename List>
inline constexpr std::size_t quantity_count_v = QuantityCount<List>::value;

/// Index of @p Q within a quantity list -- its first, when it is listed more
/// than once -- or the length of the list when it is not listed.
///
/// Being absent is an answer, not a refusal, as it is for `Environment`'s
/// search for an entry: a caller that must tell a listed quantity from one
/// that is not compares the index with `quantity_count_v<List>` and refuses in
/// its own words, instead of drawing a second, generic message on top of them.
template <typename Q, typename List>
struct QuantityIndex;

template <typename Q, typename... Qs>
struct QuantityIndex<Q, QuantityList<Qs...>>
{
  private:
    static constexpr std::size_t compute() noexcept
    {
        constexpr std::size_t notFound = sizeof...(Qs);
        std::size_t foundAt = notFound;
        std::size_t scanned = 0;
        // Only the first match is kept: once one is found, `foundAt` is left alone.
        (((foundAt == notFound && std::is_same_v<Q, Qs> ? (foundAt = scanned) : foundAt), ++scanned), ...);
        return foundAt;
    }

  public:
    /// The index of @p Q, or the length of the list.
    static constexpr std::size_t value = compute();
};

/// Index of @p Q within @p List, or `quantity_count_v<List>` when it is not
/// listed -- see `QuantityIndex`.
template <typename Q, typename List>
inline constexpr std::size_t quantity_index_v = QuantityIndex<Q, List>::value;

/// The quantity at position @p I of a quantity list, counting from 0.
template <std::size_t I, typename List>
struct QuantityAtIndex;

template <std::size_t I, typename... Qs>
struct QuantityAtIndex<I, QuantityList<Qs...>>
{
    /// The @p I-th quantity.
    using type = std::tuple_element_t<I, std::tuple<Qs...>>;
};

/// The quantity at position @p I of @p List, counting from 0.
template <std::size_t I, typename List>
using QuantityAt = typename QuantityAtIndex<I, List>::type;

/// A list with each quantity kept at its first position only.
template <typename List, typename Positions>
struct KeepFirstOfEachQuantity;

template <typename... Qs, std::size_t... Is>
struct KeepFirstOfEachQuantity<QuantityList<Qs...>, std::index_sequence<Is...>>
{
  private:
    using Whole = QuantityList<Qs...>;

  public:
    /// The list without its repeats, in the order each quantity first appears.
    using type = typename JoinQuantities<
        std::conditional_t<quantity_index_v<Qs, Whole> == Is, QuantityList<Qs>, QuantityList<>>...>::type;
};

/// @p List without its repeats: each quantity once, in the order it first
/// appears.
template <typename List>
using UniqueQuantities =
    typename KeepFirstOfEachQuantity<List, std::make_index_sequence<quantity_count_v<List>>>::type;

/// A list without the quantities another list holds.
template <typename List, typename Exclude>
struct DropQuantities;

template <typename... Qs, typename Exclude>
struct DropQuantities<QuantityList<Qs...>, Exclude>
{
    /// What is left, in the order it had, repeats kept.
    using type = typename JoinQuantities<std::conditional_t<quantity_index_v<Qs, Exclude> == quantity_count_v<Exclude>,
                                                            QuantityList<Qs>,
                                                            QuantityList<>>...>::type;
};

/// The quantities of @p List that @p Exclude does not hold, in the order they
/// have in @p List; a quantity @p List holds twice is still held twice.
template <typename List, typename Exclude>
using QuantitiesWithout = typename DropQuantities<List, Exclude>::type;

} // namespace formula::detail
