// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque call is not a value, since an operation may have several outputs
// REJECT: no matching
//
// A bound whole opaque call handed to each verb that answers with one value:
// evaluate, checked_evaluate, explain, checked_explain, trace_of and define.
// Refused as checked_evaluate refuses the call itself, pointing at
// opaque_output -- rather than an overload nobody matched. A call over a
// series of its own length for each verb, so that each refusal is its own
// message: six.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", formula::unit::Gram>;

/// A consumer's operation: the first element of a series.
struct FirstElement
{
    static constexpr std::string_view name = "first element";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "first" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> elements) noexcept
    {
        return std::array<Rep, 1> { elements[0] };
    }
};

inline constexpr auto inputs = formula::environment();

template <std::size_t N>
[[nodiscard]] constexpr auto bound_call()
{
    return formula::yields<Retained>(
        formula::opaque<FirstElement>({ .reference = "Example Standard 3" }, formula::series<Retained, N>));
}

int main()
{
    auto const evaluated = formula::evaluate(bound_call<1>(), inputs);
    auto const checked = formula::checked_evaluate(bound_call<2>(), inputs);
    auto const explained = formula::explain(bound_call<3>(), inputs);
    auto const checkedExplained = formula::checked_explain(bound_call<4>(), inputs);
    auto const traced = formula::trace_of(bound_call<5>(), inputs);
    auto const defined = formula::define(bound_call<6>());
    return evaluated.is_value() && checked.has_value() && explained.outcome.is_value() && checkedExplained.has_value()
                   && !traced.empty() && decltype(defined)::valid
               ? 0
               : 1;
}
