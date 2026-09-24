// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

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

} // namespace formula::detail
