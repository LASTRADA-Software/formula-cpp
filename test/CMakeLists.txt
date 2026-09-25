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
    enumerator_tests.cpp
    method_tests.cpp
    overlay_tests.cpp
    "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
target_link_libraries(formula-cpp-tests PRIVATE formula-cpp::formula-cpp Catch2::Catch2WithMain)
formula_apply_warnings(formula-cpp-tests)

catch_discover_tests(formula-cpp-tests)

# Each negative case is its own EXCLUDE_FROM_ALL target, so a normal build stays
# green while `ctest` drives the failing compile deliberately.
#
# An optional `REJECT "text" ...` names one or more literal substrings that must
# NOT appear in the output as well -- see RunNegativeCompileTest.cmake for what
# that buys. Every value is checked, and `REJECT "A" REJECT "B"` is the same as
# `REJECT "A" "B"`.
#
# A malformed call is refused at configure time rather than tolerated. Each of
# these used to go green while checking nothing: a third string with the
# keyword forgotten, a misspelt keyword, a `REJECT` with no value, and -- while
# REJECT was single-value -- a second `REJECT`. That last one is measured, not
# assumed: `cmake_parse_arguments(PARSE_ARGV 2 arg "" "REJECT" "")` over
# `REJECT "A" REJECT "B"` leaves `arg_REJECT` as `B` alone, with nothing
# unparsed, on cmake 3.28.3 and 4.3.1. An empty EXPECT or REJECT is refused
# too, because an empty needle is found in every output.
#
# EXPECT and the REJECT values reach the script through a generated file of
# bracket arguments (`EXPECT`, `REJECT_COUNT`, `REJECT_0`, `REJECT_1`, ...)
# rather than through `-D` on the test's command line. A variable number of
# REJECT values would have to be collected into a CMake list and expanded onto
# that command line, and a value holding a `;` would be split in two there;
# escaping the `;` is not enough either, because an unbalanced `[` in one value
# suppresses the list splitting of every argument after it. A static_assert
# message is free to contain both -- the tag rule's already has a `;`. Each
# REJECT value is taken from its own `ARGV<n>` for the same reason, not from
# the list `cmake_parse_arguments` builds.
#
# EXPECT is a single quoted argument, never a list, and a `;` in it did survive
# `-D` intact -- measured on cmake 4.3.1 with Ninja. It travels in the file all
# the same, so that the two kinds of text reach the script one way and a
# future change to how the command line is built cannot treat them apart.
#
# The file is rewritten only when its content changes, and then through a
# temporary file renamed over it, so that a reconfigure racing a running test
# under `ctest -j` cannot hand the script an empty or half-written file.
# Not `file(CONFIGURE)` or `configure_file`: both substitute `@VAR@` in the
# content, and the content here is text the library's messages choose.
function(formula_negative_test_quote out value)
    # A bracket argument holds any text literally, provided its closing bracket
    # does not occur in it; lengthen the `=` run until it does not.
    set(equals "")
    string(FIND "${value}]" "]${equals}]" clash)
    while(NOT clash EQUAL -1)
        string(APPEND equals "=")
        string(FIND "${value}]" "]${equals}]" clash)
    endwhile()
    set(${out} "[${equals}[${value}]${equals}]" PARENT_SCOPE)
endfunction()

function(formula_add_negative_test name expected)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "REJECT")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "formula_add_negative_test(${name}): unexpected arguments after the expected text: "
            "${arg_UNPARSED_ARGUMENTS}\n"
            "The only keyword accepted there is REJECT, followed by the text that must not appear.")
    endif()
    if(arg_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR
            "formula_add_negative_test(${name}): ${arg_KEYWORDS_MISSING_VALUES} was given no value.")
    endif()
    if("${expected}" STREQUAL "")
        message(FATAL_ERROR
            "formula_add_negative_test(${name}): the expected text is empty; an empty text is "
            "found in every output, so any failed build would pass.")
    endif()

    formula_negative_test_quote(quoted "${expected}")
    set(content "set(EXPECT ${quoted})\n")
    set(rejectCount 0)
    if(ARGC GREATER 2)
        math(EXPR lastArgument "${ARGC} - 1")
        foreach(index RANGE 2 ${lastArgument})
            set(value "${ARGV${index}}")
            if(value STREQUAL "REJECT")
                continue()
            endif()
            if(value STREQUAL "")
                message(FATAL_ERROR
                    "formula_add_negative_test(${name}): a REJECT value is empty; an empty text is "
                    "found in every output, so it would refuse every build.")
            endif()
            formula_negative_test_quote(quoted "${value}")
            string(APPEND content "set(REJECT_${rejectCount} ${quoted})\n")
            math(EXPR rejectCount "${rejectCount} + 1")
        endforeach()
    endif()
    string(APPEND content "set(REJECT_COUNT ${rejectCount})\n")

    set(expectationsFile "${CMAKE_CURRENT_BINARY_DIR}/negative/${name}.expect.cmake")
    set(previous "")
    if(EXISTS "${expectationsFile}")
        file(READ "${expectationsFile}" previous)
    endif()
    if(NOT previous STREQUAL content)
        file(WRITE "${expectationsFile}.tmp" "${content}")
        file(RENAME "${expectationsFile}.tmp" "${expectationsFile}")
    endif()

    add_executable(negative-${name} EXCLUDE_FROM_ALL "negative/${name}.cpp")
    target_link_libraries(negative-${name} PRIVATE formula-cpp::formula-cpp)
    add_test(NAME negative.${name}
        COMMAND "${CMAKE_COMMAND}"
                -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
                -D "CONFIG=$<CONFIG>"
                -D "TARGET=negative-${name}"
                -D "EXPECTATIONS_FILE=${expectationsFile}"
                -P "${PROJECT_SOURCE_DIR}/cmake/RunNegativeCompileTest.cmake")

    # Every negative case drives `cmake --build` in the one shared build tree,
    # and two of those at once collide on ninja's own state: "opening build
    # log: Permission denied", which the harness then reports as a failure for
    # the WRONG reason, and a truncated `.ninja_log`. It needs a stale tree, so
    # that every case's build regenerates at once -- measured on cl-debug with
    # test/CMakeLists.txt touched before `ctest -j 16 -R negative`: 4 trials of
    # 4 failed without this lock (8 to 29 of 70 cases each), 0 of 4 with it. A
    # fresh tree passed even without it, which is why this went unnoticed. The
    # lock serialises the negative cases against each other only; every other
    # test still runs in parallel beside them.
    set_tests_properties(negative.${name} PROPERTIES RESOURCE_LOCK formula-negative-build-tree)
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

# The REJECTs pin the two "report it once" guards in enumerator.hpp. The
# first REJECT of the misspelt case is the constant-expression message, which
# `!EnumeratorNameCustomization || ...` keeps from following the shape
# message. The rest are each compiler's own wording for the raw call that the
# `if constexpr (detail::ConstantEnumeratorName<E>)` guard keeps from
# following either message -- measured with that guard removed, on cl,
# clang-cl, clang and GCC, each of which emits exactly one of them.
formula_add_negative_test(enumerator_name_misspelt_of
    "formula: this EnumeratorName specialisation does not have the shape the library reads"
    REJECT "is not usable in a constant expression"
           "no matching function for call to"
           "no matching overloaded function found")

formula_add_negative_test(enumerator_name_not_constexpr
    "formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression"
    REJECT "must be initialized by a constant expression"
           "called in a constant expression"
           "expression did not evaluate to a constant")

formula_add_negative_test(enumerator_name_cv_qualified
    "formula: EnumeratorName is specialised for a const- or volatile-qualified enumeration")

formula_add_negative_test(enumerator_name_dangling_view
    "formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression")

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

# A method reports one quantity, so its variants must agree on the dimension
# they report -- they are free to disagree on everything else, and by design
# they do: the pack's whole point is that the variants are different TYPES.
#
# The disagreeing variant sits THIRD OF FOUR, not second of three. This guard
# compares the first variant against each later one rather than each row
# against its neighbour, so three variants yield only the comparisons (0,1)
# and (0,2) and there is no middle one to sit in -- second-of-three lands in
# the FIRST comparison, and a guard mutated to check only the first of the
# rest still refuses it. Measured on cl 19.51 against that exact fixture: the
# mutation survived and the test passed. Third of four lands in (0,2), and
# both the first-only and last-only mutations then make the case compile.
#
# The guard lives in `Variants`'s class body rather than in `variants()`, so
# this one case covers the no-factory route too -- declaring a `Variants<...>`
# directly completes the same class. That is measured, not assumed; the task
# report records the build that showed it.
formula_add_negative_test(method_variants_disagree
    "formula: two variants of this method measure different dimensions")

# The other two rules a variants pack obeys, one case each because each can be
# broken while the other still fires: an empty pack satisfies the
# every-argument-is-a-variant fold vacuously, and a pack of non-variants is
# not empty.
#
# Both were silent before these guards existed -- measured on cl 19.51 at
# /W4 /WX, exit 0 and no diagnostics for `variants(var<EdgeX>, var<EdgeX>)`
# and for `variants()`. The first slipped past the dimension rule because a
# VarNode publishes a `dimension` exactly as a variant does, which is the
# whole reason a separate rule is needed rather than a stricter one.
#
# The non-variant sits third of four, but only out of habit: this rule is an
# unordered fold with no first or last comparison to mutate, so position buys
# it nothing. Its strength is that IsVariantCase matches the VariantCase
# specialisation rather than duck-typing tag/expression/dimension.
formula_add_negative_test(method_variants_not_a_variant
    "formula: this argument of variants(...) is not a variant of a method")

# The same refusal, pinned on the position it reports rather than on its
# message: the offender is third of four, so the diagnostic must name index 2.
# `RequireVariant<2,` is the one spelling cl, clang-cl, clang++ and g++ all
# print -- measured, none adds a `ul` suffix -- so it is matched literally.
formula_add_negative_test(method_variants_not_a_variant_position "RequireVariant<2,")

# An empty pack passes the every-argument-is-a-variant gate vacuously, so
# the rules behind that gate are asked about it too, and must say nothing:
# the distinct-tag rule's primary template answers for the empty pack, and
# the REJECT pins it.
formula_add_negative_test(method_variants_empty
    "formula: this method declares no variants at all"
    REJECT "RequireDistinctVariantTags")

# A variant's tag must be a class type free of const and volatile. Of the
# degenerate tags this refuses -- void, int, Cube&, const Cube -- the case
# pins `const Cube`: it is the one an author could write by accident, and the
# one a duplicate-tag check comparing tags with is_same_v would otherwise
# miss, since `const Cube` and `Cube` are different types.
formula_add_negative_test(method_variant_tag_cv_qualified
    "formula: this tag is not a plain class type")

# Pins that the agreement rule is gated, not the order of the rules in
# general. The agreement rule is asked only once every argument is a variant,
# so that a non-variant is never asked for a `dimension` it has not got; this
# case hands over a bare `int`, which genuinely has none. Our refusal fires
# either way, so EXPECT alone cannot see the gate -- REJECT can:
# `RequireVariantsAgree` appears in the instantiation stack whenever the rule
# is asked, on each of the four toolchains measured locally (cl 19.51,
# clang-cl 22, clang++ 20, g++ 13; CI's g++-14 and Apple clang were not).
#
# A rule added behind the same gate adds its template name to this REJECT
# list, or its own cascade goes unnoticed -- see `RequireWellFormedVariants`.
formula_add_negative_test(method_variants_agreement_gated
    "formula: this argument of variants(...) is not a variant of a method"
    REJECT "RequireVariantsAgree" "RequireDistinctVariantTags")

# Two variants declaring the same tag. The repeat sits at positions 1 and 3
# of 5 -- not the first pair, not the last, not adjacent -- so that a rule
# narrowed to any one of those comparisons lets the pack compile; see the
# file. The second case pins the diagnostic's naming of the repeat, both
# positions and then the tag, rather than its message.
formula_add_negative_test(method_duplicate_tag
    "formula: this method declares two variants for the same tag")

formula_add_negative_test(method_duplicate_tag_position "RequireTagDeclaredOnce<1,")

# Two `variant<int>`: a repeated tag that is not a plain class type either.
# Fixing the tag clears both, so only the tag rule may report it -- the
# distinct-tag rule is gated on every tag being plain, and the REJECT pins
# that gate.
formula_add_negative_test(method_duplicate_tag_not_plain
    "formula: this tag is not a plain class type"
    REJECT "RequireTagDeclaredOnce")

# Selecting a tag no variant declares. There is no fallback variant and no
# "first match wins", so a method asked for a Prism it does not have is
# refused at compile time rather than answered with another variant's number.
formula_add_negative_test(method_no_matching_variant
    "formula: this method declares no variant for that tag")

# A selection tag obeys the rule a declared tag obeys, through the same guard,
# and that rule is asked FIRST: `evaluate_method<const Cube>` is refused as a
# malformed tag, not as an undeclared one. Both messages would be true, so
# EXPECT alone cannot see the order; REJECT pins it.
formula_add_negative_test(method_selection_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this method declares no variant for that tag")

# A method's rounding rule must round in a unit of the dimension its variants
# report. Refused where the method is declared, not where it is evaluated, so
# a method no test evaluates cannot ship broken. Two cases asserting one
# string: the second declares `Method<...>` directly, which is the only route
# that tells a check in the class body from one in `method()` -- the factory
# returns the class by value and so completes it either way.
formula_add_negative_test(method_rounding_rule_dimension_mismatch
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report")

formula_add_negative_test(method_rounding_rule_dimension_mismatch_no_factory
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report")

# The rounding rule is compared against the dimension the variants agree on,
# so when they disagree it stays quiet and the agreement rule alone reports.
# The rule's unit measures the SECOND variant's dimension, not the first's, so
# that an ungated rule -- which could only compare against the first -- fires
# here and REJECT sees it.
formula_add_negative_test(method_rounding_rule_gated
    "formula: two variants of this method measure different dimensions"
    REJECT "RequireRoundingRuleMeasuresVariants")

# The same gate on its other route: a pack holding a non-variant, which has no
# agreed dimension because one of its members has no dimension at all.
formula_add_negative_test(method_rounding_rule_gated_not_a_variant
    "formula: this argument of variants(...) is not a variant of a method"
    REJECT "RequireRoundingRuleMeasuresVariants" "RequireDistinctVariantTags")

# The rounding rule is gated on the variants agreeing, and on nothing else: a
# repeated tag leaves a dimension to compare, so a rule of the wrong dimension
# is still reported alongside it. Two independent mistakes, two messages --
# the same ruling `RequireWellFormedVariants` makes for agreement and
# distinct tags. EXPECT pins the rounding rule's message; a gate on distinct
# tags would silence it.
formula_add_negative_test(method_rounding_rule_not_gated_on_distinct_tags
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report")

# Each part of a method must be the kind of thing its factory builds. The
# first case passes the variants and the rounding rule in each other's place,
# which compiled before this rule and failed only at `evaluate_method`, in the
# compiler's own words. The second swaps the rounding rule and the
# constraints: its variants agree, so there is a dimension to compare, and its
# REJECT pins that the dimension rule is gated behind the shape rule rather
# than reading a `unit` from a `ConstraintSet`. The first case cannot pin that
# gate -- with no variants pack there is no dimension, so the rule is quiet
# either way.
formula_add_negative_test(method_arguments_out_of_order
    "formula: this method's variants are not a variants pack")

formula_add_negative_test(method_arguments_out_of_order_rule_last
    "formula: this method's rounding rule is not a rounding rule"
    REJECT "RequireRoundingRuleMeasuresVariants")

# The third part alone wrong. Both cases above have a wrong part ahead of the
# constraints and expect that part's message, so neither notices the
# constraint-set rule going missing -- measured: deleting it left both green.
formula_add_negative_test(method_constraints_not_a_constraint_set
    "formula: this method's constraints are not a constraint set")

# A malformed method declared AND evaluated. The declaration is refused, and
# `evaluate_method` must add nothing to that refusal: it gates its body on the
# method being well formed. Without that gate, measured on clang-cl 22,
# clang++ 20 and g++ 13, the first case also got the rounding node's own
# message for the same mistake, and the second the compiler's error for the
# undefined `SelectVariant` its body names. Each REJECT refuses that second
# report. cl 19.51 reported neither even without the gate, so these two pass
# on cl either way; the other three legs are what pin it.
formula_add_negative_test(method_evaluate_rounding_rule_dimension_mismatch
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report"
    REJECT "formula: this rounding node names a unit that does not measure")

formula_add_negative_test(method_evaluate_arguments_out_of_order
    "formula: this method's variants are not a variants pack"
    REJECT "SelectVariant")

# The overlay. Each case below is an override that would otherwise silently do
# nothing -- see the file comment of `overlay.hpp` -- or a gate between two
# refusals, pinned by a REJECT.
#
# Pinning or pruning a tag no variant declares. Two cases, because the pin and
# the prune each ask the rule in a place of their own.
formula_add_negative_test(overlay_pins_absent_variant
    "formula: this overlay pins or prunes a variant the method does not declare")

formula_add_negative_test(overlay_prunes_absent_variant
    "formula: this overlay pins or prunes a variant the method does not declare")

# Pruning every variant is refused by the overlay before the empty pack could
# be built, because the empty pack's own message says the author declared no
# variants -- false of the author's method. The REJECT pins that only the
# overlay's message fires.
formula_add_negative_test(overlay_prunes_every_variant
    "formula: this overlay prunes every variant of the method"
    REJECT "formula: this method declares no variants at all")

# A pinned or pruned tag obeys the tag rule, through the same guard a variant's
# tag does, and that rule is asked FIRST -- the REJECT pins the order, once for
# each operation, because each gates the declared-variant rule separately.
formula_add_negative_test(overlay_pin_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

formula_add_negative_test(overlay_prune_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

# Fixing a quantity the method never reads. The second case reaches the same
# refusal only because operations apply in order: it prunes the one variant
# that reads the quantity before fixing it, and the reverse order compiles.
formula_add_negative_test(overlay_constant_unused
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_after_its_variant_is_pruned
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# One overlay listing an operation twice. The REJECT pins `apply`'s gate on the
# overlay being well formed: ungated, the second prune is refused as well.
formula_add_negative_test(overlay_operation_twice
    "formula: this overlay lists the same operation twice"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

# An argument that is not an operation. Two REJECTs, two gates: the repeat rule
# waits for every argument to be an operation, and `apply` is not instantiated
# over a malformed overlay at all.
formula_add_negative_test(overlay_argument_not_an_operation
    "formula: this argument of overlay(...) is not an overlay operation"
    REJECT "RequireOperationListedOnce" "apply_operation")

# A constant fixed in a formula holding a node kind the rewrite cannot see
# inside. The REJECT pins that the unused-quantity rule waits until every node
# is known: the only use here is inside the unknown node, where it cannot be
# seen, and "unused" would be false.
formula_add_negative_test(overlay_constant_unknown_node
    "formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# An overlay applied to a malformed method. The method is refused where it is
# declared, and `apply` gates its body on the method being well formed, as
# `evaluate_method` does; the REJECT refuses the compiler's own error for the
# `apply_operation` an ungated body would call with the parts out of order.
formula_add_negative_test(overlay_apply_arguments_out_of_order
    "formula: this method's variants are not a variants pack"
    REJECT "apply_operation")

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
