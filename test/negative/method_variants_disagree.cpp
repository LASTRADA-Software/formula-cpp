// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: two variants of this method measure different dimensions
//
// Four variants, with the disagreeing one THIRD. That position is chosen so
// that the comparison this file exists to provoke is neither the first the
// guard makes nor the last.
//
// The rule inherited from phase 10 is "put the defect in the middle, because
// that defeats a first-only and a last-only sweep at once", and phase 10 read
// "middle" as a middle PAIR of four rows, because a band table is checked
// pairwise between neighbours. This guard is not pairwise between neighbours:
// it compares the first variant against each later one, so a pack of three
// yields only the two comparisons (0,1) and (0,2), and there is no middle one
// to sit in. A disagreeing variant placed second of three lands in (0,1) --
// the FIRST comparison -- and a guard mutated to check only the first of the
// rest still refuses it. Measured on cl 19.51 against exactly that fixture:
// the mutation survived, the test passed, and the defect would have shipped.
//
// Third of four lands in (0,2), between (0,1) and (0,3). Both mutations are
// measured in the task report: each makes this file compile, and each is
// caught.
//
// The three variants that DO agree are deliberately different types from one
// another, so this file does not quietly assume a pack's agreeing members
// look alike. What that cannot pin is that the rule is about dimensions
// rather than types -- under a guard mutated to compare types, this file
// still refuses to compile. The test that separates those is the positive one
// in `method_tests.cpp`, which requires a pack of DIFFERENT types to be
// accepted.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};
struct Core
{
};

inline constexpr formula::Unit Newton { .dimension = formula::dim::Force,
                                        .symbolText = formula::symbol("N"),
                                        .decimals = 1 };

struct Force: formula::Quantity<Force, "F", "applied force", Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

// The third variant reports a length where the other three report a pressure.
// A method reports one quantity, so this is not a method.
inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                 formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                                 formula::variant<Prism>(var<EdgeX>),
                                                 formula::variant<Core>(var<Force> / (var<EdgeY> * var<EdgeY>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
