// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <iterator>
#include <string_view>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::describe;
using formula::detail::or_throw;

static_assert(describe(ArithmeticError::DivisionByZero) == std::string_view { "division by zero" });
static_assert(!describe(ArithmeticError::Overflow).empty());
static_assert(!describe(ArithmeticError::NotFinite).empty());
static_assert(!describe(ArithmeticError::DomainError).empty());

static_assert(or_throw(std::expected<int, ArithmeticError> { 42 }) == 42);

TEST_CASE("every error code has a distinct, non-empty description", "[error]")
{
    ArithmeticError const codes[] = {
        ArithmeticError::DivisionByZero, ArithmeticError::Overflow, ArithmeticError::NotFinite, ArithmeticError::DomainError
    };

    for (std::size_t i = 0; i < std::size(codes); ++i)
    {
        CHECK_FALSE(describe(codes[i]).empty());
        for (std::size_t j = i + 1; j < std::size(codes); ++j)
            CHECK(describe(codes[i]) != describe(codes[j]));
    }
}

TEST_CASE("or_throw passes a value through and throws on an error", "[error]")
{
    CHECK(or_throw(std::expected<int, ArithmeticError> { 7 }) == 7);

    auto const failed = std::expected<int, ArithmeticError> { std::unexpect, ArithmeticError::DivisionByZero };
    CHECK_THROWS_AS(or_throw(failed), ArithmeticException);
}

TEST_CASE("the exception carries the code and a readable message", "[error]")
{
    try
    {
        auto const failed = std::expected<int, ArithmeticError> { std::unexpect, ArithmeticError::Overflow };
        (void) or_throw(failed);
        FAIL("or_throw did not throw");
    }
    catch (ArithmeticException const& exception)
    {
        CHECK(exception.code() == ArithmeticError::Overflow);
        CHECK(std::string_view { exception.what() } == describe(ArithmeticError::Overflow));
    }
}
