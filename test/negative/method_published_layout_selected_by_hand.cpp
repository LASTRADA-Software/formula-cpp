// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a variant's published position is stated only by the library
//
// A layout selected by hand from a throwaway pack of nine: a two-variant
// method no overlay touched would then report its variants as the 6th and
// 8th of 9, with no prune on record. No `detail::` is written: `decltype(nine.published)` names the layout,
// and `select` is its public member.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
struct T0
{
};
struct T1
{
};
struct T2
{
};
struct T3
{
};
struct T4
{
};
struct T5
{
};
struct T6
{
};
struct T7
{
};
struct T8
{
};

struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;
using formula::variant;

inline constexpr auto nine = formula::variants(variant<T0>(var<EdgeX>),
                                               variant<T1>(var<EdgeX>),
                                               variant<T2>(var<EdgeX>),
                                               variant<T3>(var<EdgeX>),
                                               variant<T4>(var<EdgeX>),
                                               variant<T5>(var<EdgeX>),
                                               variant<T6>(var<EdgeX>),
                                               variant<T7>(var<EdgeX>),
                                               variant<T8>(var<EdgeX>));
} // namespace

int main()
{
    auto pack = formula::variants(variant<T5>(var<EdgeX>), variant<T7>(var<EdgeX>));
    pack.published = decltype(nine.published) {}.select<5, 7>();
    return pack.published.count() == 9 ? 0 : 1;
}
