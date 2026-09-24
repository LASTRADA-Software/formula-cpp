# SPDX-License-Identifier: Apache-2.0
include(CPM)
include(PedanticCompiler)

CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.6.0)

# Catch.cmake (which provides catch_discover_tests) reaches us by one of two
# routes: a CPM source build puts it in ${Catch2_SOURCE_DIR}/extras, a system
# find_package puts it next to Catch2Config.cmake in ${Catch2_DIR}.
# CPM_USE_LOCAL_PACKAGES makes the second the default wherever Catch2 is
# installed, so both must be handled.
if(Catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
elseif(Catch2_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
endif()
include(Catch)

add_executable(formula-cpp-tests
    runtime_tests.cpp
    compile_time_tests.cpp
    cross_tu_a.cpp
    expression_cross_tu_b.cpp
    checked_int_tests.cpp
    citation_tests.cpp
    document_tests.cpp
    error_tests.cpp
    expression_tests.cpp
    rational_tests.cpp
    render_tests.cpp
    rounding_tests.cpp
    dimension_tests.cpp
    dimension_cross_tu_b.cpp
    sink_tests.cpp
    unit_tests.cpp
    unit_cross_tu_b.cpp
    fixed_string_tests.cpp
    quantity_tests.cpp
    quantity_cross_tu_b.cpp
    measured_tests.cpp
    outcome_tests.cpp
    environment_tests.cpp
    evaluate_tests.cpp
    function_tests.cpp
    trace_tests.cpp
    trace_render_tests.cpp
    rounding_node_tests.cpp
    predicate_tests.cpp
    "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
target_link_libraries(formula-cpp-tests PRIVATE formula-cpp::formula-cpp Catch2::Catch2WithMain)
formula_apply_warnings(formula-cpp-tests)

catch_discover_tests(formula-cpp-tests)

# Each negative case is its own EXCLUDE_FROM_ALL target, so a normal build stays
# green while `ctest` drives the failing compile deliberately.
function(formula_add_negative_test name expected)
    add_executable(negative-${name} EXCLUDE_FROM_ALL "negative/${name}.cpp")
    target_link_libraries(negative-${name} PRIVATE formula-cpp::formula-cpp)
    add_test(NAME negative.${name}
        COMMAND "${CMAKE_COMMAND}"
                -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
                -D "CONFIG=$<CONFIG>"
                -D "TARGET=negative-${name}"
                -D "EXPECT=${expected}"
                -P "${PROJECT_SOURCE_DIR}/cmake/RunNegativeCompileTest.cmake")
endfunction()

# The expected text is the library's OWN static_assert message. All three
# compilers echo it verbatim (MSVC wraps it as `error C2338: static assertion
# failed: '<message>'`), so one literal substring matches on every leg.
formula_add_negative_test(rational_from_floating_point
    "formula: a floating-point value is not an exact rational")
formula_add_negative_test(rational_from_wide_unsigned
    "formula: this unsigned type can hold values above Rational's maximum")

# These two expect a NAME rather than a message. An invalid exponent is rejected
# by calling a deliberately non-`constexpr` sentinel whose name states the
# problem, so the compiler quotes that name. Measured on all three: clang and
# clang-cl say "non-constexpr function '<name>' cannot be used in a constant
# expression"; cl leads with the unhelpful "error C2131: expression did not
# evaluate to a constant" but follows it with "see usage of '<name>'". One
# literal substring therefore matches everywhere, as above.
formula_add_negative_test(exponent_zero_denominator
    "formula_exponent_denominator_must_not_be_zero")
formula_add_negative_test(exponent_out_of_range "formula_exponent_out_of_range")

formula_add_negative_test(dimension_mismatch
    "formula: these two dimensions are not the same")

formula_add_negative_test(unit_dimension_mismatch
    "formula: these two units measure different dimensions")

# Same NAME-not-message case as the exponent sentinels above, for the same
# reason: symbol() rejects an over-long symbol by calling a deliberately
# non-`constexpr` sentinel rather than truncating it.
formula_add_negative_test(unit_symbol_too_long "formula_unit_symbol_too_long")

formula_add_negative_test(fixed_string_not_terminated
    "formula_fixed_string_must_be_null_terminated")

formula_add_negative_test(describe_undeclared_type
    "formula: this type does not declare any quantity metadata")

# NOT "CementVolume": that is this test file's own type name, and every
# compiler echoes the offending type in ANY compile error, so a substring of
# just the type name matches even when the intended call is deleted entirely
# -- measured: deleting the wrong-type call and replacing it with an unrelated
# error that merely mentions CementVolume left this test green. The type-to-
# type wording below appears only for the mismatch this test exists to catch.
formula_add_negative_test(quantity_wrong_type "'CementVolume' to 'WaterVolume'")

formula_add_negative_test(describe_partially_specified
    "formula: this type does not declare any quantity metadata")

# Isolates the concept's widening from the dimension-consistency guard: this one
# satisfies the consistency guard and omits only `description`, so only the
# four-member requirement can catch it.
formula_add_negative_test(describe_missing_description
    "formula: this type does not declare any quantity metadata")
formula_add_negative_test(describe_dimension_mismatch
    "formula: this type's declared dimension does not match its unit's dimension")

formula_add_negative_test(expression_add_dimension_mismatch
    "the two sides of this addition or subtraction measure different")

formula_add_negative_test(expression_inconsistent_describe
    "describes a dimension its own unit does not measure")

formula_add_negative_test(expression_subtract_dimension_mismatch
    "the two sides of this addition or subtraction measure different")

formula_add_negative_test(environment_duplicate_quantity
    "supplies the same quantity more than once")

formula_add_negative_test(environment_missing_quantity
    "provides no value for this quantity")

formula_add_negative_test(evaluate_result_dimension_mismatch
    "does not measure the dimension this expression computes")

formula_add_negative_test(function_zero_root_degree
    "a root of degree zero describes no operation")

formula_add_negative_test(documented_dimension_mismatch
    "the two sides of this addition or subtraction measure different")

# Same NAME-not-message shape as the exponent and symbol-length sentinels
# above: there is no static_assert to quote here, only a deleted default
# constructor, and every compiler's own wording for that differs (cl says
# "attempting to reference a deleted function", clang says "call to deleted
# constructor", GCC says "use of deleted function") -- but all three name the
# type whose constructor is deleted, so that name is the one substring common
# to all of them.
formula_add_negative_test(render_trace_no_step_limit "StepLimit")

add_test(NAME hygiene.version
    COMMAND "${CMAKE_COMMAND}"
            -D "VERSION_HEADER=${PROJECT_SOURCE_DIR}/include/formula-cpp/version.hpp"
            -D "PROJECT_VERSION=${PROJECT_VERSION}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckVersionConsistency.cmake")

add_test(NAME hygiene.spdx
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckSpdxHeaders.cmake")

add_test(NAME hygiene.nolint
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckNolintUsage.cmake")

add_test(NAME hygiene.headers
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckPublicHeaderIncludes.cmake")

add_test(NAME hygiene.installed-headers
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckInstalledHeaders.cmake")

add_test(NAME hygiene.testnames
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckTestNames.cmake")
