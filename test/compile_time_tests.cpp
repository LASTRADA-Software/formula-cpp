// SPDX-License-Identifier: Apache-2.0
// These are tests of a `detail` header, so it is included directly rather
// than reached through the umbrella header that happens to include it.
#include <formula-cpp/detail/type_list.hpp>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <type_traits>

namespace
{
struct Alpha
{
    int value {};
};
struct Beta
{
    int value {};
};
struct Gamma
{
    int value {};
};
struct Delta
{
    int value {};
};

using Bag = std::tuple<Alpha, Beta, Gamma>;

// Plain structs, not quantities: the list utilities only compare, select and
// join the types they are given.
template <typename... Qs>
using List = formula::detail::QuantityList<Qs...>;
} // namespace

TEST_CASE("index_in_tuple locates each type", "[type_list]")
{
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Alpha, Bag> == 0);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Beta, Bag> == 1);
    STATIC_REQUIRE(formula::detail::index_in_tuple_v<Gamma, Bag> == 2);
}

TEST_CASE("JoinQuantities concatenates lists in the order given", "[type_list]")
{
    using formula::detail::JoinQuantities;

    STATIC_REQUIRE(std::is_same_v<JoinQuantities<>::type, List<>>);
    STATIC_REQUIRE(std::is_same_v<JoinQuantities<List<Gamma, Alpha>>::type, List<Gamma, Alpha>>);
    STATIC_REQUIRE(std::is_same_v<JoinQuantities<List<Gamma>, List<Alpha, Beta>>::type, List<Gamma, Alpha, Beta>>);
    // Empty lists vanish, and a quantity in two lists stays in both places.
    STATIC_REQUIRE(std::is_same_v<JoinQuantities<List<>, List<Beta>, List<>, List<Beta, Alpha>>::type,
                                  List<Beta, Beta, Alpha>>);
}

TEST_CASE("quantity_count_v counts every entry, repeats included", "[type_list]")
{
    using formula::detail::quantity_count_v;

    STATIC_REQUIRE(quantity_count_v<List<>> == 0);
    STATIC_REQUIRE(quantity_count_v<List<Alpha>> == 1);
    STATIC_REQUIRE(quantity_count_v<List<Alpha, Beta, Alpha>> == 3);
}

TEST_CASE("quantity_index_v is the first position, or the length when absent", "[type_list]")
{
    using formula::detail::quantity_count_v;
    using formula::detail::quantity_index_v;

    using Trio = List<Alpha, Beta, Gamma>;
    STATIC_REQUIRE(quantity_index_v<Alpha, Trio> == 0);
    STATIC_REQUIRE(quantity_index_v<Beta, Trio> == 1);
    STATIC_REQUIRE(quantity_index_v<Gamma, Trio> == 2);

    // A quantity listed twice is found where it first appears.
    using Repeated = List<Beta, Alpha, Beta, Gamma, Alpha>;
    STATIC_REQUIRE(quantity_index_v<Beta, Repeated> == 0);
    STATIC_REQUIRE(quantity_index_v<Alpha, Repeated> == 1);
    STATIC_REQUIRE(quantity_index_v<Gamma, Repeated> == 3);

    // A quantity that is not listed is not an error: it is the length, which no listed quantity has.
    STATIC_REQUIRE(quantity_index_v<Delta, Trio> == 3);
    STATIC_REQUIRE(quantity_index_v<Delta, Trio> == quantity_count_v<Trio>);
    STATIC_REQUIRE(quantity_index_v<Delta, Repeated> == quantity_count_v<Repeated>);
    STATIC_REQUIRE(quantity_index_v<Alpha, List<>> == 0);
    STATIC_REQUIRE(quantity_index_v<Alpha, List<>> == quantity_count_v<List<>>);
}

TEST_CASE("QuantityAt names the quantity at a position", "[type_list]")
{
    using formula::detail::QuantityAt;
    using formula::detail::quantity_index_v;

    using Trio = List<Alpha, Beta, Gamma>;
    STATIC_REQUIRE(std::is_same_v<QuantityAt<0, Trio>, Alpha>);
    STATIC_REQUIRE(std::is_same_v<QuantityAt<1, Trio>, Beta>);
    STATIC_REQUIRE(std::is_same_v<QuantityAt<2, Trio>, Gamma>);

    // Where a quantity is listed twice, each position names it.
    using Repeated = List<Beta, Alpha, Beta>;
    STATIC_REQUIRE(std::is_same_v<QuantityAt<0, Repeated>, Beta>);
    STATIC_REQUIRE(std::is_same_v<QuantityAt<1, Repeated>, Alpha>);
    STATIC_REQUIRE(std::is_same_v<QuantityAt<2, Repeated>, Beta>);

    // The position of a quantity names it again.
    STATIC_REQUIRE(std::is_same_v<QuantityAt<quantity_index_v<Gamma, Trio>, Trio>, Gamma>);
    STATIC_REQUIRE(std::is_same_v<QuantityAt<quantity_index_v<Alpha, Trio>, Trio>, Alpha>);
}

TEST_CASE("UniqueQuantities keeps each quantity once, in order of first appearance", "[type_list]")
{
    using formula::detail::UniqueQuantities;

    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<>>, List<>>);
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<Gamma>>, List<Gamma>>);
    // Nothing repeated: nothing changes, and the order is not sorted.
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<Gamma, Alpha, Beta>>, List<Gamma, Alpha, Beta>>);
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<Alpha, Alpha, Alpha>>, List<Alpha>>);
    // The first appearance is the one kept: last-wins would give <Alpha, Gamma> here.
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<Gamma, Alpha, Gamma>>, List<Gamma, Alpha>>);
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<List<Alpha, Beta, Alpha, Gamma, Beta, Alpha>>, List<Alpha, Beta, Gamma>>);
}

TEST_CASE("QuantitiesWithout drops what the other list holds and keeps the order", "[type_list]")
{
    using formula::detail::QuantitiesWithout;

    using Trio = List<Alpha, Beta, Gamma>;
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<>>, Trio>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Beta>>, List<Alpha, Gamma>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Alpha>>, List<Beta, Gamma>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Gamma>>, List<Alpha, Beta>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, Trio>, List<>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<List<>, Trio>, List<>>);
    // Members the list does not hold change nothing; the order of the excluded ones does not matter.
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Delta>>, Trio>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Gamma, Delta, Alpha>>, List<Beta>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<Trio, List<Beta, Beta>>, List<Alpha, Gamma>>);
    // What is kept keeps its repeats: dropping is not de-duplicating.
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<List<Alpha, Beta, Alpha>, List<Beta>>, List<Alpha, Alpha>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<List<Alpha, Beta, Alpha>, List<Alpha>>, List<Beta>>);
}

TEST_CASE("the list utilities compose: the quantities read but not defined", "[type_list]")
{
    using formula::detail::JoinQuantities;
    using formula::detail::QuantitiesWithout;
    using formula::detail::UniqueQuantities;

    // Three expressions read <Gamma, Beta>, <Beta, Alpha> and <Delta, Gamma>; Beta is defined.
    using Reads = JoinQuantities<List<Gamma, Beta>, List<Beta, Alpha>, List<Delta, Gamma>>::type;
    STATIC_REQUIRE(std::is_same_v<Reads, List<Gamma, Beta, Beta, Alpha, Delta, Gamma>>);
    STATIC_REQUIRE(std::is_same_v<UniqueQuantities<Reads>, List<Gamma, Beta, Alpha, Delta>>);
    STATIC_REQUIRE(std::is_same_v<QuantitiesWithout<UniqueQuantities<Reads>, List<Beta>>, List<Gamma, Alpha, Delta>>);
}
