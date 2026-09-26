// SPDX-License-Identifier: Apache-2.0
// EXPECT: Corrections<
// REJECT: this lookup table was given a different number of corrections than it has rows
//
// An EMPTY corrections list for a table with rows is still refused -- but by
// the compiler, not by the library's own sentence, and that is deliberate.
// `{}` is value-initialisation, which is exactly what a default-
// constructibility probe performs, so refusing it in a `static_assert` body
// is what stopped a `Method` holding a lookup from compiling on clang++ and
// g++ (see `Corrections` in `lookup.hpp`, and `method_lookup_tests.cpp`).
// The expected text is the one part of the diagnostic every compiler shares,
// the type's own name; the rejected text pins that the empty list is refused
// by the constraint and never reaches that body. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
// Named for this file: see `exact_lookup_short_corrections_no_factory.cpp`.
enum class EmptyCorrectionsShape
{
    Cube,
    Cylinder,
    Prism,
};

inline constexpr formula::KeyTable<EmptyCorrectionsShape, 3> ShapeKeys {
    EmptyCorrectionsShape::Cube,
    EmptyCorrectionsShape::Cylinder,
    EmptyCorrectionsShape::Prism,
};

inline constexpr formula::ExactLookupNode<ShapeKeys, formula::unit::One> broken { {}, {}, EmptyCorrectionsShape::Prism };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
