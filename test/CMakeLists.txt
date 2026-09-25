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
    constraint_tests.cpp
    conditional_tests.cpp
    escape_tests.cpp
    band_tests.cpp
    lookup_tests.cpp
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

formula_add_negative_test(escape_empty_justification
    "formula: numeric_value_of requires a justification saying why this rule is stated")

formula_add_negative_test(escape_blank_justification
    "formula: numeric_value_of requires a justification saying why this rule is stated")

formula_add_negative_test(escape_unit_dimension_mismatch
    "formula: this numeric_value_of names a unit that does not measure the dimension")

# Same NAME-not-message shape as the exponent and symbol-length sentinels
# above: there is no static_assert to quote here, only a deleted default
# constructor, and every compiler's own wording for that differs (cl says
# "attempting to reference a deleted function", clang says "call to deleted
# constructor", GCC says "use of deleted function") -- but all three name the
# type whose constructor is deleted, so that name is the one substring common
# to all of them.
formula_add_negative_test(render_trace_no_step_limit "StepLimit")

formula_add_negative_test(band_gap
    "formula: this band table has a gap or overlap between two adjacent bands")

formula_add_negative_test(band_overlap
    "formula: this band table has a gap or overlap between two adjacent bands")

formula_add_negative_test(band_inverted
    "formula: this band is not well-formed")

formula_add_negative_test(band_zero_width
    "formula: this band is not well-formed")

formula_add_negative_test(lookup_band_gap
    "formula: this band table has a gap or overlap between two adjacent bands")

# The corrections-count guard is shared by every lookup table in lookup.hpp,
# so its message names a table's "rows" rather than one kind's own word for
# them, and every case that trips it asserts this same string. A second
# wording per table kind is how two surfaces that must agree begin to
# disagree -- see `RequireCorrectionCountMatches`.
formula_add_negative_test(lookup_short_corrections
    "this lookup table was given a different number of corrections than it has rows")

formula_add_negative_test(exact_lookup_short_corrections
    "this lookup table was given a different number of corrections than it has rows")

formula_add_negative_test(interpolating_lookup_short_corrections
    "this lookup table was given a different number of corrections than it has rows")

# The same rule on the route the three cases above cannot reach: the node
# declared directly, with no factory call anywhere. Every lookup node is a
# public aggregate with public members, so a short braced list can be handed to
# one without the factory's parameter type ever being consulted -- and it was,
# silently, until `corrections` became a `Corrections<N>` rather than a bare
# `std::array<Rational, N>`. Measured before the fix on all three kinds: the
# rows nobody typed evaluated to `0`, indistinguishable from a deliberate zero,
# which is the "every answer is a lie" failure this node refuses on the miss
# side.
#
# One case per kind, for the reason the key-unit pair just below gives: each
# node declares its own `corrections` member, so a single case standing for all
# three would let a revert on one kind pass while the other two still fired.
formula_add_negative_test(lookup_short_corrections_no_factory
    "this lookup table was given a different number of corrections than it has rows")

formula_add_negative_test(exact_lookup_short_corrections_no_factory
    "this lookup table was given a different number of corrections than it has rows")

formula_add_negative_test(interpolating_lookup_short_corrections_no_factory
    "this lookup table was given a different number of corrections than it has rows")

# The key-unit guard is shared by every lookup kind that HAS a key unit -- the
# banded and the interpolating one -- for the same reason the corrections-count
# guard is shared: one rule, one mistake, one sentence. (The exact lookup has no
# key unit; its key is a discriminator, not a quantity.) Both cases assert the
# same string, and both exist, because deleting the guard from one node kind
# leaves the other kind's case failing exactly as before.
formula_add_negative_test(lookup_key_dimension_mismatch
    "this lookup table's key unit does not measure the dimension of the expression whose value selects a row")

formula_add_negative_test(interpolating_lookup_key_dimension_mismatch
    "this lookup table's key unit does not measure the dimension of the expression whose value selects a row")

# Three duplicate-key cases, asserting one string, because each pins a
# different way the check can be broken while the other two still pass:
#   - the base case has its duplicate in the MIDDLE and in NON-ADJACENT rows,
#     which kills a check confined to the first pair, to the last pair, or to
#     neighbours -- three mutations on one table;
#   - _two_rows is the smallest table that can carry a duplicate at all, and
#     is what fails when the underflow guard in `detail::key_table_is_valid`
#     is widened from `size() < 2` to `< 3`;
#   - _no_factory declares the node directly, which is the ONLY case that
#     tells a class-body `static_assert` from one in the factory -- the
#     factory returns the node by value, so every other case completes the
#     class either way.
formula_add_negative_test(exact_lookup_duplicate_key
    "formula: this exact lookup table declares the same key twice")

formula_add_negative_test(exact_lookup_duplicate_key_two_rows
    "formula: this exact lookup table declares the same key twice")

formula_add_negative_test(exact_lookup_duplicate_key_no_factory
    "formula: this exact lookup table declares the same key twice")

# The scoped-enumeration requirement, at each of the three public entry points
# that takes a key: the node, the runtime validator and the predicate both are
# built on. One file each, so that removing any one guard is caught rather
# than masked by another still firing.
formula_add_negative_test(exact_lookup_unscoped_key
    "formula: an exact lookup's keys must be enumerators of a scoped enumeration")

formula_add_negative_test(exact_lookup_int_key_table
    "formula: an exact lookup's keys must be enumerators of a scoped enumeration")

formula_add_negative_test(exact_lookup_keys_match_int_key
    "formula: an exact lookup's keys must be enumerators of a scoped enumeration")

# Both lookups' Rep guards. Their messages are author-facing text and are the
# only place either refusal's reason reaches a user, so they are tested API
# like every other static_assert here -- and pinning only one of the two would
# leave the halves of one rule in different states.
formula_add_negative_test(lookup_rep_not_rational
    "formula: a banded lookup node can only be evaluated with Rep = Rational")

formula_add_negative_test(exact_lookup_rep_not_rational
    "formula: an exact lookup node can only be evaluated with Rep = Rational")

formula_add_negative_test(interpolating_lookup_rep_not_rational
    "formula: an interpolating lookup node can only be evaluated with Rep = Rational")

# An interpolating table's own well-formedness, in its two independent rules.
#
# The ordering rule has three cases asserting one string, because each pins a
# different way it can be broken while the other two still pass:
#   - _duplicate_breakpoint states the same key twice, which is what survives a
#     comparison relaxed from `<` to `<=`;
#   - _descending_breakpoint types two rows out of order, which survives the
#     opposite mistake -- a check that only rejected equality;
#   - _no_factory declares the node directly, which is the ONLY case that tells
#     a class-body `static_assert` from one in the factory, since the factory
#     returns the node by value and so completes the class either way.
# Each puts its defect in the MIDDLE pair of four rows, which kills a sweep
# confined to the first pair or to the last.
formula_add_negative_test(interpolating_lookup_duplicate_breakpoint
    "formula: this interpolating lookup table's breakpoints do not strictly ascend")

formula_add_negative_test(interpolating_lookup_descending_breakpoint
    "formula: this interpolating lookup table's breakpoints do not strictly ascend")

formula_add_negative_test(interpolating_lookup_descending_breakpoint_no_factory
    "formula: this interpolating lookup table's breakpoints do not strictly ascend")

# The other rule: a row whose key is not a rational number at all. Separate from
# the ordering rule because a one-row table has no pair for the ordering sweep
# to look at, and because "not ascending" would be a true statement about the
# wrong mistake.
formula_add_negative_test(interpolating_lookup_malformed_breakpoint
    "formula: this interpolating lookup table has a row whose key is not a rational number")

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

# A compiler diagnostic quoted in docs/ names a header and a line number, and
# the number goes stale the moment anyone inserts a line above it -- which
# happened twice in this repository before anything checked. See the script's
# own comment for both instances and for what it does and does not verify.
add_test(NAME hygiene.documented-diagnostics
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckDocumentedDiagnostics.cmake")
