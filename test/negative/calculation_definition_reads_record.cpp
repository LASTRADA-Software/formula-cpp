// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation's definitions read the worksheet's own values, and this one reads from another record with from_record
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: has no detail::LevelChildren specialisation
// REJECT: no matching
//
// A share of the reference record's load. A calculation reads its own
// values; a formula that reads another record's is evaluated against a
// record context. The scope has no entry of its own in the node kinds a
// precision limit's checks see, and is refused before those are asked, so
// their message for a kind without an entry does not follow.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Reference
{
};

struct Load: formula::Quantity<Load, "F", "an invented load", unit::Newton>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto share =
        formula::define<Share>(formula::var<Load> / formula::from_record<Reference>(formula::var<Load>));
    return 0;
}
