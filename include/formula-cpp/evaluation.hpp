// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <formula-cpp/detail/type_list.hpp>

#include <bitset>
#include <cassert>
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace formula
{

// ---- Overloader ----
template <typename... Ts>
struct Overloader: Ts...
{
    using Ts::operator()...;
};
template <typename... T>
Overloader(T...) -> Overloader<T...>;

// ---- EvaluationFunctors ----
template <typename... Funcs>
struct EvaluationFunctors
{
    Overloader<Funcs...> _overload;

    // How many functors return T when called with Ctx?
    template <typename T, typename Ctx>
    static constexpr std::size_t count_producers()
    {
        return (std::size_t { 0 } + ... + std::size_t { std::is_same_v<T, std::invoke_result_t<Funcs const&, Ctx>> });
    }

    template <typename T, typename Ctx>
    static constexpr bool can_produce()
    {
        return count_producers<T, Ctx>() > 0;
    }

    // Every functor must return a different type
    template <typename Ctx>
    static constexpr bool producers_are_unique()
    {
        return ((count_producers<std::invoke_result_t<Funcs const&, Ctx>, Ctx>() == 1) && ...);
    }

    template <typename T>
    T get(auto const& ctx) const
    {
        using Ctx = decltype(ctx);
        static_assert(count_producers<T, Ctx>() == 1, "formula: exactly one functor must return the requested type");
        constexpr std::size_t I = detail::index_in_tuple_v<T, std::tuple<std::invoke_result_t<Funcs const&, Ctx>...>>;
        using F = std::tuple_element_t<I, std::tuple<Funcs...>>;
        return static_cast<F const&>(_overload)(ctx);
    }
};

template <typename... T>
EvaluationFunctors(T...) -> EvaluationFunctors<T...>;

// ---- EvaluationArguments ----
template <typename... Ts>
struct EvaluationArguments
{
    static constexpr std::size_t number_of_args = sizeof...(Ts);
    using tuple_type = std::tuple<Ts...>;
    tuple_type _vals;
};

// ---- Evaluation ----
template <typename Args, auto Funcs>
struct Evaluation
{
    struct Context
    {
        mutable Args _args;
        mutable std::bitset<Args::number_of_args> _setFlags;

        template <typename T>
        T& get() const
        {
            constexpr std::size_t index = detail::index_in_tuple_v<T, typename Args::tuple_type>;
            if constexpr (Funcs.template can_produce<T, Context const&>())
            {
                if (!_setFlags[index])
                {
                    std::get<T>(_args._vals) = Funcs.template get<T>(*this);
                    _setFlags[index] = true;
                }
            }
            else
            {
                // no functor computes T, so it must be an input set via set()
                assert(_setFlags[index] && "input value was never set");
            }
            return std::get<T>(_args._vals);
        }
    };

    static_assert(Funcs.template producers_are_unique<Context const&>(),
                  "formula: two or more functors return the same type");

    Context _ctx;

    template <typename T>
    Evaluation set(T t) &&
    {
        constexpr std::size_t index = detail::index_in_tuple_v<T, typename Args::tuple_type>;
        std::get<T>(_ctx._args._vals) = t;
        _ctx._setFlags[index] = true;
        return std::move(*this);
    }

    template <typename T>
    T calculate(T) &&
    {
        return _ctx.template get<T>();
    }
};

template <typename T, typename Ctx>
T get(Ctx const& ctx)
{
    return ctx.template get<T>();
}

} // namespace formula
