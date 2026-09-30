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
    quantity_alias_tests.cpp
    quantity_alias_cross_tu_b.cpp
    measured_tests.cpp
    outcome_tests.cpp
    environment_tests.cpp
    evaluate_tests.cpp
    function_tests.cpp
    trace_tests.cpp
    trace_render_tests.cpp
    rounding_node_tests.cpp
    rounded_root_tests.cpp
    critical_value_tests.cpp
    precision_tests.cpp
    predicate_tests.cpp
    constraint_tests.cpp
    conditional_tests.cpp
    escape_tests.cpp
    band_tests.cpp
    lookup_tests.cpp
    enumerator_tests.cpp
    tag_tests.cpp
    method_tests.cpp
    consumer_globals_tests.cpp
    consumer_globals_run_tests.cpp
    method_lookup_tests.cpp
    overlay_tests.cpp
    vocabulary_tests.cpp
    join_tests.cpp
    method_cross_tu_b.cpp
    series_tests.cpp
    series_cross_tu_b.cpp
    conformity_tests.cpp
    snap_tests.cpp
    binning_tests.cpp
    curve_tests.cpp
    statistics_tests.cpp
    statistics_cross_tu_b.cpp
    rejection_tests.cpp
    record_tests.cpp
    record_scope_tests.cpp
    record_trace_tests.cpp
    lineage_tests.cpp
    record_render_tests.cpp
    record_join_tests.cpp
    record_statistics_tests.cpp
    record_cross_tu_b.cpp
    opaque_tests.cpp
    opaque_cross_tu_b.cpp
    least_squares_tests.cpp
    retry_tests.cpp
    calculation_tests.cpp
    calculation_trace_tests.cpp
    number_text_tests.cpp
    format_tests.cpp
    wide_int_tests.cpp
    wide_rounding_tests.cpp
    "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
target_link_libraries(formula-cpp-tests PRIVATE formula-cpp::formula-cpp Catch2::Catch2WithMain)
formula_apply_warnings(formula-cpp-tests)

catch_discover_tests(formula-cpp-tests)

# The overflow census (docs/numeric-headroom.md) is its own program: it is
# built with FORMULA_OVERFLOW_CENSUS, which changes what the library's inline
# arithmetic does, so no translation unit of formula-cpp-tests may share it.
add_executable(formula-cpp-census-tests
    overflow_census_tests.cpp
    "${PROJECT_SOURCE_DIR}/support/census_tally.cpp"
    "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
target_link_libraries(formula-cpp-census-tests PRIVATE formula-cpp::formula-cpp Catch2::Catch2WithMain)
target_compile_definitions(formula-cpp-census-tests PRIVATE FORMULA_OVERFLOW_CENSUS)
target_include_directories(formula-cpp-census-tests PRIVATE "${PROJECT_SOURCE_DIR}/support")
formula_apply_warnings(formula-cpp-census-tests)

catch_discover_tests(formula-cpp-census-tests TEST_PREFIX "census.")

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
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "EXPECT_COUNT" "REJECT")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "formula_add_negative_test(${name}): unexpected arguments after the expected text: "
            "${arg_UNPARSED_ARGUMENTS}\n"
            "The keywords accepted there are EXPECT_COUNT, followed by how many times the expected "
            "text must appear, and REJECT, followed by the text that must not appear.")
    endif()
    if(DEFINED arg_EXPECT_COUNT AND NOT arg_EXPECT_COUNT MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "formula_add_negative_test(${name}): EXPECT_COUNT must be a positive whole number, "
            "not '${arg_EXPECT_COUNT}'.")
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
    # How many times EXPECT must appear: one message per mistake, made a check
    # that can fail. Not under cl, which stops at the first failed
    # static_assert of an instantiation chain, so a count there cannot tell one
    # message from several (measured on cl 19.51 with snap_rep_not_rational:
    # 1 either way). clang-cl counts as clang++ does.
    if(DEFINED arg_EXPECT_COUNT AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        string(APPEND content "set(EXPECT_COUNT ${arg_EXPECT_COUNT})\n")
    endif()
    set(rejectCount 0)
    if(ARGC GREATER 2)
        math(EXPR lastArgument "${ARGC} - 1")
        set(skipCountValue FALSE)
        foreach(index RANGE 2 ${lastArgument})
            set(value "${ARGV${index}}")
            if(skipCountValue)
                set(skipCountValue FALSE)
                continue()
            endif()
            if(value STREQUAL "EXPECT_COUNT")
                set(skipCountValue TRUE)
                continue()
            endif()
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
formula_add_negative_test(dimension_named_base_mismatch
    "formula: these two dimensions are not the same")

# Named base dimensions. A product or quotient needing more than four is the
# same NAME-not-message case as the exponent sentinels above, and follows
# them: no EXPECT_COUNT.
formula_add_negative_test(dimension_too_many_named_bases "formula_dimension_has_too_many_named_bases")
formula_add_negative_test(dimension_too_many_named_bases_divided "formula_dimension_has_too_many_named_bases")
formula_add_negative_test(expression_too_many_named_bases "formula_dimension_has_too_many_named_bases")

# A base name is a function argument, which no `static_assert` can see, so each
# rule `base_dimension` enforces is a non-`constexpr` sentinel named after it.
# No EXPECT_COUNT: clang and clang-cl report a failed `consteval` call twice,
# once for the constant it initialises and once for the call, each naming the
# sentinel, while cl and g++ report it once.
formula_add_negative_test(base_dimension_name_empty "formula_base_dimension_name_must_not_be_empty")
# A 16-byte name would also overflow symbol(); the base's own rule must be the one that fires.
formula_add_negative_test(base_dimension_name_too_long "formula_base_dimension_name_too_long"
    REJECT "formula_unit_symbol_too_long")
formula_add_negative_test(base_dimension_name_not_identifier
    "formula_base_dimension_name_must_be_a_letter_then_letters_or_digits")
formula_add_negative_test(base_dimension_name_si_symbol
    "formula_base_dimension_name_is_an_si_base_unit_symbol")

formula_add_negative_test(unit_dimension_mismatch
    "formula: these two units measure different dimensions")

# Money: two units whose SI exponents are the same, told apart only by their
# named base dimensions.
formula_add_negative_test(unit_currency_mismatch
    "formula: these two units measure different dimensions" EXPECT_COUNT 1)

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

# Money: sides whose SI exponents are the same, told apart only by their named
# base dimensions -- euros against a pure number, and against yen.
formula_add_negative_test(money_plus_number
    "formula: the two sides of this addition or subtraction measure different dimensions" EXPECT_COUNT 1)
formula_add_negative_test(money_plus_other_currency
    "formula: the two sides of this addition or subtraction measure different dimensions" EXPECT_COUNT 1)

formula_add_negative_test(expression_inconsistent_describe
    "describes a dimension its own unit does not measure")

formula_add_negative_test(expression_subtract_dimension_mismatch
    "the two sides of this addition or subtraction measure different")

formula_add_negative_test(environment_duplicate_quantity
    "supplies the same quantity more than once")

formula_add_negative_test(environment_missing_quantity
    "provides no value for this quantity")

# The two refusals above, for quantities declared by alias
# (`using Q = formula::Quantity<struct QTag, ...>`). The message is the
# library's either way; what the compiler prints around it differs -- cl
# usually keeps the alias's name, g++ and clang print the specialisation, tag
# first. See `quantity.hpp`.
formula_add_negative_test(quantity_alias_add_dimension_mismatch
    "the two sides of this addition or subtraction measure different")
formula_add_negative_test(quantity_alias_environment_missing
    "provides no value for this quantity")

formula_add_negative_test(evaluate_result_dimension_mismatch
    "does not measure the dimension this expression computes")

# Money: a result quantity in yen for an expression in euros, the SI exponents
# of both being the same.
formula_add_negative_test(evaluate_result_other_currency
    "formula: this result quantity does not measure the dimension this expression computes" EXPECT_COUNT 1)

# Series (`series.hpp`, `environment.hpp`). Each refusal is gated so that one
# mistake draws one message, and each REJECT names the message that would be
# the second, or the false, one.
#
# A series read as a single value: the environment does hold the quantity, so
# "provides no value" would be false.
formula_add_negative_test(environment_series_read_as_single
    "this environment holds a series for this quantity, not a single value"
    REJECT "provides no value for this quantity" "cannot convert" "no viable conversion" "could not convert")
# Pins get()'s `if constexpr` gate, as `method_variants_agreement_gated`
# pins its own: without it the series entry reaches get()'s scalar return
# and the compiler adds a conversion error to the library's message -- cl
# "cannot convert", clang "no viable conversion", g++ "could not convert",
# measured on cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and
# g++ 14.2. None of the three occurs with the gate.

# A single value read as a series: neither a missing quantity nor a length.
formula_add_negative_test(environment_single_read_as_series
    "this environment holds a single value for this quantity, not a series"
    REJECT "provides no value for this quantity" "holds a series of a different length")

# Four supplied, five read. The lengths print as RequireSeriesLength's
# template arguments Supplied and Read; their exact spelling differs between
# compilers, so the case matches the sentence.
formula_add_negative_test(environment_series_length_mismatch
    "this environment holds a series of a different length for this quantity than the one read"
    REJECT "holds a single value for this quantity")

# A braced list of three for a series of five, which std::array would pad
# with absent elements.
formula_add_negative_test(measured_series_short
    "this series was given a different number of elements than its length")

# A series where one value is asked for: checked_evaluate, evaluate and
# variant<Tag>. The REJECTs pin that nothing past the refusal reads the
# environment, and that variants(...) and method(...) around a refused
# variant find nothing further to refuse.
formula_add_negative_test(evaluate_series_as_single
    "this expression is a series, not a single value; evaluate it with checked_evaluate_series"
    REJECT "holds a series for this quantity")
formula_add_negative_test(evaluate_throwing_series_as_single
    "this expression is a series, not a single value; evaluate it with checked_evaluate_series"
    REJECT "holds a series for this quantity")
formula_add_negative_test(method_variant_holds_series
    "this expression is a series, not a single value; evaluate it with checked_evaluate_series"
    REJECT "is not a variant of a method" "RequireVariantsAgree" "RequireRoundingRuleMeasuresVariants")

# A series result quantity is held to the expression's dimension, with
# checked_evaluate's own guard.
formula_add_negative_test(evaluate_series_result_dimension_mismatch
    "does not measure the dimension this expression computes"
    REJECT "~expected")
# The REJECT pins the `if constexpr` gate in checked_evaluate_series:
# without it g++ 13.3 and 14.2 follow the refusal with errors from the
# evaluation below, among them "no matching function for call to
# ...~expected()" (3 more errors at 793dda5, 6 more once the series hooks
# arrived). cl, clang-cl and clang++ give one message either way, so only
# the gcc legs can see this REJECT fire.

# A braced list std::array would pad with absent elements: every spelling
# that could reach a std::array parameter, refused because the constructor
# taking one is deduced. In the compiler's words, which differ by compiler
# and share only the type's name -- see each case's comment. The one route
# left open, a std::array the caller built short, is documented on the
# constructor.
formula_add_negative_test(measured_series_padded_triple_brace "MeasuredSeries<")
formula_add_negative_test(measured_series_padded_double_brace "MeasuredSeries<")
formula_add_negative_test(measured_series_padded_empty "MeasuredSeries<")
formula_add_negative_test(entered_series_padded "MeasuredSeries<")
formula_add_negative_test(entered_series_aggregate_padded "MeasuredSeries<")

# An entered value is read in the shape the result asks for,
# and refused in the other shape or another length, never computed over.
formula_add_negative_test(series_evaluated_over_entered_single
    "this environment holds a single value for this quantity, not a series"
    REJECT "provides no value for this quantity" "holds a series of a different length")
formula_add_negative_test(single_evaluated_over_entered_series
    "this environment holds a series for this quantity, not a single value"
    REJECT "provides no value for this quantity")
formula_add_negative_test(entered_series_length_mismatch
    "this environment holds a series of a different length for this quantity than the one read"
    REJECT "holds a single value for this quantity")

# get_series for a quantity the environment lacks: RequireProvided, once.
formula_add_negative_test(environment_series_missing_quantity
    "provides no value for this quantity"
    REJECT "holds a single value for this quantity" "holds a series of a different length")

# Elementwise arithmetic. A length mismatch refuses once and gates
# the dimension check off, so the REJECT pins that the additive message stays
# silent; a dimension mismatch between series of one length is the scalar
# operators' own refusal, and the REJECT pins that no length is blamed.
formula_add_negative_test(series_elementwise_length_mismatch
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different")
formula_add_negative_test(series_elementwise_dimension_mismatch
    "the two sides of this addition or subtraction measure different"
    REJECT "the two series combined here have different lengths")
# Both wrong at once: the length is refused, and the dimension check gated
# behind it stays silent. Removing the gate makes this REJECT fire.
formula_add_negative_test(series_elementwise_length_and_dimension_mismatch
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different")
# The mistake FIRST in a chain: the refused node marks itself refused, and
# the node over it asks nothing. REJECT cannot refuse a second copy of the
# EXPECTed text, so the last REJECT names the second message's own operand
# instead: on g++ and clang++ it names the refused node as the guard's left
# template argument, which the first message never does. cl 19.51 gave one
# message before the flag too. Counted by hand: one message on cl 19.51,
# g++-14 and clang++-20; two on g++ and clang++ without the flag.
formula_add_negative_test(series_elementwise_short_first
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequireSeriesLengthsAgree<formula::ElementwiseBinaryNode")
formula_add_negative_test(series_elementwise_short_first_product
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequireSeriesLengthsAgree<formula::ElementwiseBinaryNode")
formula_add_negative_test(series_elementwise_wrong_dimension_first
    "the two sides of this addition or subtraction measure different"
    REJECT "the two series combined here have different lengths"
    "RequireAddendsAgree<formula::ElementwiseBinaryNode")
# The refusal passed upward along each other path, the second REJECT naming
# what only a second message prints on g++ and clang++: a refused right-hand
# operand, a negation, a running total, and a per-element rounding over a
# refused operand (each deletion-checked on both).
formula_add_negative_test(series_elementwise_short_on_the_right
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequireSeriesLengthsAgree<formula::SeriesVarNode<TotalMass, 5>")
formula_add_negative_test(series_elementwise_short_under_negation
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequireSeriesLengthsAgree<formula::ElementwiseUnaryNode")
formula_add_negative_test(series_elementwise_short_under_cumulative
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequireSeriesLengthsAgree<formula::CumulativeNode")
formula_add_negative_test(series_round_over_refused_operand
    "the two series combined here have different lengths"
    REJECT "the two sides of this addition or subtraction measure different"
    "RequirePlacesPerElement<")

# A per-element constant given fewer values than its stated length, through
# the factory and through aggregate initialisation of the node itself.
formula_add_negative_test(series_constant_short
    "this series constant was given a different number of elements than its length")
formula_add_negative_test(series_constant_short_no_factory
    "this series constant was given a different number of elements than its length")
formula_add_negative_test(series_constant_zero_length
    "this series constant has no elements"
    REJECT "no matching")
formula_add_negative_test(series_zero_length
    "this series has no elements"
    REJECT "no matching")

# Conformity: an envelope of the wrong length, through the factory and through
# aggregate initialisation of the check, and limits in a unit of the wrong
# dimension.
formula_add_negative_test(conformity_envelope_short
    "this envelope was given a different number of rows than its series has elements"
    REJECT "no matching")
formula_add_negative_test(conformity_envelope_short_no_factory
    "this envelope was given a different number of rows than its series has elements"
    REJECT "no matching")
formula_add_negative_test(conformity_unit_dimension_mismatch
    "this conformity check states its limits in a unit that does not measure the dimension of its subject"
    REJECT "this envelope was given a different number of rows")
# A single value as the subject, and an envelope of {},
# each refused once in the library's words.
formula_add_negative_test(conformity_single_value_subject
    "a conformity check judges each element of a series against its own row, and this subject is a single value"
    REJECT "no matching" "this conformity check states its limits in a unit that does not measure")
formula_add_negative_test(conformity_empty_envelope
    "this envelope was given a different number of rows than its series has elements"
    REJECT "no matching" "could not convert")

# Conformity compares, so it is Rational-only, as the
# snap, curve and binning are.
formula_add_negative_test(conformity_rep_not_rational
    "formula: a conformity check can only be evaluated with Rep = Rational")

# Snapping: an empty permitted set refuses once, with the breakpoint validation
# and the key check gated off; a descending set is the shipped breakpoint
# validation's refusal, with the key check gated off; a key of the wrong
# dimension.
formula_add_negative_test(snap_empty_permitted_set
    "this snap's permitted set is empty"
    REJECT "RequireBreakpoint" "RequireValidBreakpointTable" "key unit does not measure" "no matching")
formula_add_negative_test(snap_descending_permitted_set
    "formula: this breakpoint table's breakpoints do not strictly ascend"
    REJECT "key unit does not measure" "permitted set is empty"
           "must be initialized by a constant expression")
formula_add_negative_test(snap_key_dimension_mismatch
    "this snap's key unit does not measure the dimension of the expression it snaps"
    REJECT "breakpoints do not strictly ascend" "permitted set is empty")
# A snap evaluated in anything but Rational.
formula_add_negative_test(snap_rep_not_rational
    "formula: a snap can only be evaluated with Rep = Rational")

# Curves. Each mistake refused once; a curve, splice or interpolation
# over a refused operand asks nothing more.
formula_add_negative_test(curve_length_mismatch
    "formula: this curve pairs a domain and values of different lengths"
    REJECT "this curve is read at a point that does not measure")
formula_add_negative_test(splice_domain_dimension_mismatch
    "formula: the two curves spliced here have domains of different dimensions"
    REJECT "have values of different dimensions")
formula_add_negative_test(splice_value_dimension_mismatch
    "formula: the two curves spliced here have values of different dimensions"
    REJECT "have domains of different dimensions")
formula_add_negative_test(interpolate_at_dimension_mismatch
    "formula: this curve is read at a point that does not measure the dimension of its domain")
formula_add_negative_test(domain_empty
    "formula: this domain declares no points"
    REJECT "this curve pairs a domain and values of different lengths")
formula_add_negative_test(domain_descending
    "formula: this breakpoint table's breakpoints do not strictly ascend"
    REJECT "must be initialized by a constant expression" "this curve pairs a domain and values of different lengths")
formula_add_negative_test(curve_rep_not_rational
    "formula: a curve can only be evaluated with Rep = Rational")
# A single value given to curve() or splice() is
# refused once in the library's words, as sum and cumulative refuse one.
formula_add_negative_test(curve_of_single_value
    "formula: a curve pairs a series of points with a series of values, and this is a single value"
    REJECT "no matching" "this curve pairs a domain and values of different lengths"
           "this curve is read at a point that does not measure")
formula_add_negative_test(curve_of_single_values
    "formula: a curve pairs a series of points with a series of values, and this is a single value"
    REJECT "no matching")
formula_add_negative_test(splice_of_single_value
    "formula: splice joins two curves, and this is a single value, not a curve"
    REJECT "no matching" "have domains of different dimensions" "have values of different dimensions")

# Binning. More observations than the capacity; binned given a
# series; a key unit of another dimension; classes with a gap (the shipped
# band validation) and with none; a Rep other than Rational; and the four
# reads of raw observations in the wrong shape or capacity, each refused once
# in words true of it.
formula_add_negative_test(observations_over_capacity
    "more observations were listed than this set's capacity holds")
formula_add_negative_test(binned_series_not_observations
    "binned counts raw observations into classes, and this is not a set of observations"
    REJECT "no matching" "this binning's key unit" "not raw observations" "must be initialized by a constant expression")
formula_add_negative_test(binned_key_dimension_mismatch
    "this binning's key unit does not measure the dimension of the observations it bins"
    REJECT "gap or overlap" "declares no classes")
formula_add_negative_test(binned_class_gap
    "this band table has a gap or overlap between two adjacent bands"
    REJECT "this binning's key unit" "declares no classes")
formula_add_negative_test(binned_no_classes
    "this binning declares no classes"
    REJECT "this binning's key unit")
formula_add_negative_test(binned_rep_not_rational
    "a binning can only be evaluated with Rep = Rational")
formula_add_negative_test(observations_read_as_single
    "this environment holds raw observations for this quantity, which are neither a single value nor a series"
    REJECT "provides no value for this quantity" "holds a series for this quantity" "cannot convert" "no viable conversion" "could not convert")
formula_add_negative_test(observations_read_as_series
    "this environment holds raw observations for this quantity, which are neither a single value nor a series"
    REJECT "provides no value for this quantity" "holds a single value for this quantity" "cannot convert" "no viable conversion" "could not convert")
formula_add_negative_test(series_read_as_observations
    "this environment holds a single value or a series for this quantity, not raw observations"
    REJECT "provides no value for this quantity" "different capacity" "cannot convert" "no viable conversion" "could not convert")
formula_add_negative_test(observations_capacity_mismatch
    "this environment holds observations of a different capacity for this quantity than the one read"
    REJECT "provides no value for this quantity" "not raw observations" "cannot convert" "no viable conversion" "could not convert")

# Sum and cumulative given a single value, refused in this library's
# words by an overload that takes a Node for no other purpose. The REJECT
# pins that the refusal is not the compiler's own "no matching function".
formula_add_negative_test(series_sum_of_single_value
    "sum adds up the elements of a series, and this is a single value, not a series"
    REJECT "no matching")
formula_add_negative_test(series_cumulative_of_single_value
    "cumulative runs a total along a series, and this is a single value, not a series"
    REJECT "no matching" "this expression is a series, not a single value")
# The refusing cumulative returns a series already refused, so a sum over
# it, and checked_evaluate_series around it, add nothing; on cl the second case used to lose the library's words entirely.
formula_add_negative_test(series_sum_of_cumulative_of_single_value
    "cumulative runs a total along a series, and this is a single value, not a series"
    REJECT "sum adds up the elements of a series" "no matching")
formula_add_negative_test(series_cumulative_of_single_value_as_series
    "cumulative runs a total along a series, and this is a single value, not a series"
    REJECT "no matching" "this expression is a series, not a single value")
# A per-element rounding given a single value, likewise.
formula_add_negative_test(series_round_of_single_value
    "rounded_elementwise rounds each element of a series, and this is a single value, not a series"
    REJECT "no matching" "this expression is a series, not a single value")

# A per-element rounding with a table of the wrong length, which
# gates the unit check off, and one with a unit of the wrong dimension.
formula_add_negative_test(series_round_places_count_mismatch
    "are not a PlacesTable<N> of one DecimalPlaces per element of its series"
    REJECT "this rounding node names a unit that does not measure the dimension of the expression it rounds")
formula_add_negative_test(series_round_unit_dimension_mismatch
    "this rounding node names a unit that does not measure the dimension of the expression it rounds"
    REJECT "are not a PlacesTable<N> of one DecimalPlaces per element of its series")
# Past a refused table nothing indexes or loops over it: evaluated in a
# constant expression, rendered, documented and traced, a table of the wrong
# count, and a lone DecimalPlaces, each draw the one message; a table of the
# right count but the wrong type is told the truth.
formula_add_negative_test(series_round_places_count_mismatch_evaluated
    "are not a PlacesTable<N> of one DecimalPlaces per element of its series"
    REJECT "outside the bounds" "must be initialized by a constant expression" "no match" "not a range" "subscript"
    "was not declared")
formula_add_negative_test(series_round_places_not_a_table
    "are not a PlacesTable<N> of one DecimalPlaces per element of its series"
    REJECT "outside the bounds" "must be initialized by a constant expression" "no match" "not a range" "subscript"
    "was not declared")
formula_add_negative_test(series_round_places_int_array
    "are not a PlacesTable<N> of one DecimalPlaces per element of its series"
    REJECT "different number")

# A substitution for a quantity the method reads as a series: refused as
# such, and the "nobody reads it" refusal gated off, since a variant does
# read it.
formula_add_negative_test(overlay_constant_on_series_quantity
    "formula: this overlay fixes a quantity the method reads as a series; one constant cannot stand for a series"
    REJECT "overrides a quantity that no variant or constraint of the method uses")
formula_add_negative_test(overlay_derived_on_series_quantity
    "formula: this overlay derives a quantity the method reads as a series; one definition cannot stand for a series"
    REJECT "derives a quantity that no variant or constraint of the method uses")

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

# The class-body check, reached through the factory and around it. Each
# REJECTs the dimension messages of the nodes a rounded root could be
# mistaken for, or built from: a rounding node's unit check and the
# evaluator's result check.
formula_add_negative_test(rounded_sqrt_unit_dimension_mismatch
    "formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand"
    REJECT "formula: this rounding node names a unit that does not measure the dimension"
           "does not measure the dimension this expression computes"
           "formula: these two dimensions are not the same")

formula_add_negative_test(rounded_sqrt_no_factory_dimension_mismatch
    "formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand"
    REJECT "formula: this rounding node names a unit that does not measure the dimension"
           "does not measure the dimension this expression computes"
           "formula: these two dimensions are not the same")

# A unit with an offset (degrees Celsius) of the right dimension, and the
# same unit over the wrong dimension: the offset check is gated on the
# dimension matching, so the second draws the dimension message alone.
formula_add_negative_test(rounded_sqrt_unit_has_offset
    "formula: this rounded_sqrt names a unit with an offset, such as degrees Celsius"
    REJECT "formula: this rounded_sqrt names a unit whose square does not measure")

formula_add_negative_test(rounded_sqrt_offset_unit_dimension_mismatch
    "formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand"
    REJECT "formula: this rounded_sqrt names a unit with an offset")

# Critical values keyed by sample size. A malformed table is refused once,
# for its first defect, and the arity check is gated behind it; a valid
# table's arity is checked on both routes.
formula_add_negative_test(critical_value_sizes_descending
    "formula: this sample-size table's sizes do not strictly ascend"
    REJECT "this lookup table was given a different number of corrections")

formula_add_negative_test(critical_value_sizes_duplicate
    "formula: this sample-size table's sizes do not strictly ascend"
    REJECT "declares a size of zero")

formula_add_negative_test(critical_value_size_zero
    "formula: this sample-size table declares a size of zero"
    REJECT "do not strictly ascend")

formula_add_negative_test(critical_value_short
    "this lookup table was given a different number of corrections than it has rows"
    REJECT "sample-size table")

formula_add_negative_test(critical_value_short_no_factory
    "this lookup table was given a different number of corrections than it has rows"
    REJECT "sample-size table")

formula_add_negative_test(critical_value_count_not_scalar
    "formula: this critical value's count is not dimensionless"
    REJECT "sample-size table" "stated in a unit other than a bare number")

formula_add_negative_test(critical_value_count_in_euros
    "formula: this critical value's count is not dimensionless"
    REJECT "sample-size table" "stated in a unit other than a bare number")

# A dimensionless count in a unit other than a bare number. Gated on the
# count being dimensionless, so `count_not_scalar` above REJECTs this message.
formula_add_negative_test(critical_value_count_in_percent
    "formula: this critical value's count is stated in a unit other than a bare number"
    REJECT "is not dimensionless" "sample-size table")

formula_add_negative_test(critical_value_rep_not_rational
    "formula: a critical-value lookup node can only be evaluated with Rep = Rational")

# Precision limits. Each refusal is gated so that one mistake draws one
# message: the placeholder never asks an environment for a value, and a limit
# refused where it is declared is never evaluated.
formula_add_negative_test(precision_level_outside_limit
    "formula: precision_level is meaningful only inside the limit expression of precision_limit"
    REJECT "provides no value for this quantity")

formula_add_negative_test(precision_level_in_own_level
    "formula: this precision_limit's level expression reads precision_level"
    REJECT "precision_level is meaningful only inside" "provides no value for this quantity")

formula_add_negative_test(precision_level_dimension_mismatch
    "formula: this precision_limit's limit expression reads a precision_level whose quantity does not measure the dimension of the level expression"
    REJECT "reads a level of another dimension" "these two dimensions are not the same")

formula_add_negative_test(precision_limit_no_factory_level_dimension
    "formula: this precision_limit's limit expression reads a precision_level whose quantity does not measure the dimension of the level expression"
    REJECT "reads a level of another dimension")

formula_add_negative_test(precision_level_hidden_dimension_mismatch
    "formula: this precision_level reads a level of another dimension"
    REJECT "limit expression reads a precision_level whose quantity" "has no detail::LevelChildren specialisation")

# A node kind declared in namespace formula with no LevelChildren
# specialisation is refused, never walked as a leaf; the consumer's node just
# above, templated over a library type, is not.
formula_add_negative_test(precision_level_check_unlisted_library_kind
    "formula: this library node kind has no detail::LevelChildren specialisation"
    REJECT "reads precision_level" "reads a level of another dimension")

# The level check sees through the overlay's derived quantity: a placeholder
# an overlay puts into a level is refused as a level reading a level, where
# `apply` builds the rewritten limit -- nested or outermost -- and never as a
# placeholder evaluated where no limit binds it.
formula_add_negative_test(precision_level_derived_into_nested_level
    "formula: this precision_limit's level expression reads precision_level"
    REJECT "precision_level is meaningful only inside" "cannot see inside")

formula_add_negative_test(precision_level_derived_into_level
    "formula: this precision_limit's level expression reads precision_level"
    REJECT "precision_level is meaningful only inside" "cannot see inside")

# Sample statistics. A single value handed to one is refused once, and the
# refusing overload returns what the statistic would have been -- the value
# itself for a mean, a bare number for a count -- so nothing downstream, and
# no concept failure of the sample overload, adds a second message.
formula_add_negative_test(sample_mean_of_single_value
    "formula: a sample statistic needs a sample (a series or observations), not a single value"
    REJECT "no matching" "this expression is a series, not a single value"
           "this result quantity does not measure the dimension this expression computes")
formula_add_negative_test(sample_count_of_single_value
    "formula: a sample statistic needs a sample (a series or observations), not a single value"
    REJECT "no matching" "this expression is a series, not a single value"
           "this result quantity does not measure the dimension this expression computes")
formula_add_negative_test(sample_variance_of_single_value
    "formula: a sample statistic needs a sample (a series or observations), not a single value"
    REJECT "no matching" "this expression is a series, not a single value"
           "this result quantity does not measure the dimension this expression computes")
formula_add_negative_test(sample_range_of_single_value
    "formula: a sample statistic needs a sample (a series or observations), not a single value"
    REJECT "no matching" "this expression is a series, not a single value"
           "this result quantity does not measure the dimension this expression computes")
# Outlier rejection. Each refusal is gated so that one mistake draws one
# message: the positive counts, the limit's dimension and the sample's
# placeholders are asked only once the bounds are in order.
formula_add_negative_test(rejection_bounds_swapped
    "formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order"
    REJECT "declares AtMost<0>" "declares KeepAtLeast<0>" "does not measure what its criterion compares" "no matching")
formula_add_negative_test(rejection_bounds_swapped_zero
    "formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order"
    REJECT "declares AtMost<0>" "declares KeepAtLeast<0>")
formula_add_negative_test(rejection_bound_not_a_bound
    "formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order"
    REJECT "must be a class or namespace" "is not a member of" "prior to '::'" "no matching")
formula_add_negative_test(rejection_at_most_zero
    "formula: without_outliers declares AtMost<0>"
    REJECT "in that order")
formula_add_negative_test(rejection_keep_at_least_zero
    "formula: without_outliers declares KeepAtLeast<0>"
    REJECT "in that order")
formula_add_negative_test(rejection_limit_dimension_mismatch
    "formula: this rejection's limit does not measure what its criterion compares"
    REJECT "pass_mean whose quantity")
formula_add_negative_test(rejection_limit_and_pass_mean_dimension_mismatch
    "formula: this rejection's limit does not measure what its criterion compares"
    REJECT "pass_mean whose quantity")
formula_add_negative_test(pass_mean_wrong_dimension
    "formula: this rejection's limit reads a pass_mean whose quantity does not measure the determinations' dimension"
    REJECT "does not measure what its criterion compares")
formula_add_negative_test(rejection_result_dimension
    "formula: this result quantity does not measure what the rejection's determinations measure"
    REJECT "the dimension this expression computes")
formula_add_negative_test(rejection_under_double
    "formula: a rejection of outliers can only be evaluated with Rep = Rational"
    REJECT "no matching")
formula_add_negative_test(gap_to_range_every_exceeding
    "formula: gap_to_range examines only the lowest and the highest value; declare PerPass::MostExtreme"
    REJECT "does not measure what its criterion compares")
formula_add_negative_test(gap_to_range_limit_not_scalar
    "formula: this rejection's limit does not measure what its criterion compares"
    REJECT "gap_to_range examines only")
formula_add_negative_test(rejection_series_shorter_than_keep_at_least
    "formula: without_outliers keeps at least m determinations of a series that holds fewer"
    REJECT "declares KeepAtLeast<0>")
formula_add_negative_test(rejection_of_rejection
    "formula: without_outliers takes a series or observations as its sample, not another without_outliers"
    REJECT "keeps at least m")
formula_add_negative_test(rejection_observations_below_keep_at_least
    "formula: without_outliers keeps at least m determinations of observations that hold at most fewer"
    REJECT "of a series that holds fewer")
formula_add_negative_test(rejection_of_rejection_declared
    "formula: without_outliers takes a series or observations as its sample, not another without_outliers"
    REJECT "keeps at least m")
formula_add_negative_test(rejection_observations_below_keep_at_least_declared
    "formula: without_outliers keeps at least m determinations of observations that hold at most fewer"
    REJECT "of a series that holds fewer")
formula_add_negative_test(rejection_stddevs_keep_fewer_than_three
    "formula: deviation_in_stddevs needs at least 3 determinations in every pass it decides"
    REJECT "keeps at least m")
formula_add_negative_test(pass_mean_outside_rejection
    "formula: pass_mean and pass_count are meaningful only inside the limit expression of without_outliers"
    REJECT "provides no value for this quantity")
formula_add_negative_test(pass_mean_in_rejected_sample
    "formula: the sample this rejection reads mentions pass_mean or pass_count"
    REJECT "meaningful only inside")
formula_add_negative_test(rejection_of_single_value
    "formula: without_outliers rejects determinations from a sample"
    REJECT "no matching" "a sample statistic needs a sample")

# Phase 12's series-quantity refusal, reused unchanged for a statistic.
formula_add_negative_test(sample_mean_overlay_constant_on_sample
    "formula: this overlay fixes a quantity the method reads as a series; one constant cannot stand for a series"
    REJECT "overrides a quantity that no variant or constraint of the method uses" "cannot see inside")

# The same check sees through phase 12's series kinds: a placeholder added to
# every element of a series a nested level sums.
formula_add_negative_test(precision_level_in_nested_level_through_series
    "formula: this precision_limit's level expression reads precision_level"
    REJECT "precision_level is meaningful only inside")

formula_add_negative_test(precision_level_offset_unit
    "formula: this precision_level names a quantity whose unit has an offset"
    REJECT "reads a level of another dimension" "limit expression reads a precision_level whose quantity")

# `RepRounding<double>`'s own refusal, not one of the rounded root's: the node
# rounds through the representation's rounding, as every rounding node does.
formula_add_negative_test(rounded_sqrt_rep_double
    "formula: a rounding node cannot be evaluated with Rep = double"
    REJECT "formula: this rounded_sqrt names a unit")

# Same NAME-not-message shape as the exponent and symbol-length sentinels
# above: there is no static_assert to quote here, only a deleted default
# constructor, and every compiler's own wording for that differs (cl says
# "attempting to reference a deleted function", clang says "call to deleted
# constructor", GCC says "use of deleted function") -- but all three name the
# type whose constructor is deleted, so that name is the one substring common
# to all of them.
formula_add_negative_test(render_trace_no_step_limit "StepLimit")
# The same hole, with a number style named and the step limit left out: the
# style's default member initialiser must not make the limit optional.
formula_add_negative_test(render_trace_numbers_without_step_limit "StepLimit")
formula_add_negative_test(calculation_render_derivation_no_step_limit "StepLimit"
    REJECT "no matching" "cannot convert argument")
formula_add_negative_test(calculation_document_attempt_input
    "formula: attempt_input reads the determination recorded for the attempt that is running"
    EXPECT_COUNT 1
    REJECT "provides no value for this quantity" "no matching")
formula_add_negative_test(calculation_document_refused
    "formula: these definitions read one another in a cycle, so none of them can be calculated first"
    EXPECT_COUNT 1
    REJECT "formula: attempt_input reads the determination recorded for the attempt that is running"
           "provides no value for this quantity" "no matching")

# A bad spec in a literal std::format string is a compile error naming the
# guard format.hpp calls for that mistake, and no other guard.
formula_add_negative_test(format_places_without_mode
    "formula_number_format_needs_a_rounding_mode"
    REJECT "formula_number_format_places_out_of_range" "formula_number_format_spec_not_understood")
formula_add_negative_test(format_places_out_of_range
    "formula_number_format_places_out_of_range"
    REJECT "formula_number_format_needs_a_rounding_mode" "formula_number_format_spec_not_understood")
formula_add_negative_test(format_spec_not_understood
    "formula_number_format_spec_not_understood"
    REJECT "formula_number_format_needs_a_rounding_mode" "formula_number_format_places_out_of_range")

formula_add_negative_test(band_gap
    "formula: this band table has a gap or overlap between two adjacent bands"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(band_overlap
    "formula: this band table has a gap or overlap between two adjacent bands"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(band_inverted
    "formula: this band is not well-formed"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(band_zero_width
    "formula: this band is not well-formed"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(lookup_band_gap
    "formula: this band table has a gap or overlap between two adjacent bands"
    REJECT "must be initialized by a constant expression")

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

# An EMPTY list is the one wrong count the sentence above does not reach, on
# purpose: `{}` is what a default-constructibility probe asks for, and a
# refusal reachable from it stopped a method holding a lookup from compiling on
# clang++ (with libstdc++ or libc++) and on g++ -- see `Corrections` in
# lookup.hpp. It is refused by the wrong-count constructor's constraint
# instead, in the compiler's words, which every compiler spells with the
# type's name. The REJECT pins the route: with that constraint reverted, the
# empty list reaches the body again and prints the sentence -- measured on
# clang++ with both libraries, g++ 13 and 14, cl and clang-cl -- so this case
# catches the revert even on the compilers that accept the method either way.
formula_add_negative_test(lookup_empty_corrections_no_factory
    "Corrections<"
    REJECT "this lookup table was given a different number of corrections than it has rows")

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
#     is what fails when the pair filter in `detail::KeyPairCheck` skips a
#     table of two rows;
#   - _no_factory declares the node directly, which is the ONLY case that
#     tells a class-body `static_assert` from one in the factory -- the
#     factory returns the node by value, so every other case completes the
#     class either way.
formula_add_negative_test(exact_lookup_duplicate_key
    "formula: this exact lookup table declares the same key twice"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(exact_lookup_duplicate_key_two_rows
    "formula: this exact lookup table declares the same key twice"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(exact_lookup_duplicate_key_no_factory
    "formula: this exact lookup table declares the same key twice"
    REJECT "must be initialized by a constant expression")

# The scoped-enumeration requirement, at each of the three public entry points
# that takes a key: the node, the runtime validator and the predicate both are
# built on. One file each, so that removing any one guard is caught rather
# than masked by another still firing.
formula_add_negative_test(exact_lookup_unscoped_key
    "formula: an exact lookup's keys must be enumerators of a scoped enumeration")

# The REJECTs pin the two "report it once" guards in enumerator.hpp. The
# first REJECT of the misspelt case is the constant-expression message, which
# `!EnumeratorNameCustomization || ...` keeps from following the shape
# message -- pinned on cl and GCC only; clang stops at the first failed
# static_assert of a class template instantiation, so it never shows the
# second message with or without the guard. The rest are each compiler's own wording for the raw call that the
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
    "formula: EnumeratorName is specialised for a const-qualified enumeration")

formula_add_negative_test(enumerator_name_dangling_view
    "formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression")

# The TagName counterparts of the four above (`tag.hpp`), with the same
# REJECTs for the same two "report it once" guards -- measured again for this
# trait rather than assumed from that one, on cl, clang-cl, clang and GCC with
# each guard removed.
formula_add_negative_test(tag_name_misspelt_of
    "formula: this TagName specialisation does not have the shape the library reads"
    REJECT "is not usable in a constant expression"
           "no matching function for call to"
           "no matching overloaded function found")

formula_add_negative_test(tag_name_not_constexpr
    "formula: TagName<Tag>::of() is not usable in a constant expression"
    REJECT "must be initialized by a constant expression"
           "called in a constant expression"
           "expression did not evaluate to a constant")

# A TagName or EnumeratorName spelling that could forge a trace line: a
# bracketed clause of its own, or a line. One case per kind of character, as
# for a vocabulary's symbol.
formula_add_negative_test(tag_name_holds_bracket
    "formula: this TagName spelling holds a square bracket or a control character")
formula_add_negative_test(tag_name_holds_newline
    "formula: this TagName spelling holds a square bracket or a control character")
formula_add_negative_test(enumerator_name_holds_bracket
    "formula: this EnumeratorName spelling holds a square bracket or a control character")
formula_add_negative_test(enumerator_name_holds_control
    "formula: this EnumeratorName spelling holds a square bracket or a control character")

formula_add_negative_test(tag_name_cv_qualified
    "formula: TagName is specialised for a const-qualified tag")

formula_add_negative_test(tag_name_dangling_view
    "formula: TagName<Tag>::of() is not usable in a constant expression")

# A tag whose reflected name cannot be shown plainly (`RequireReadableTagName`,
# `tag.hpp`), one case per shape the two rules exist for. The first three are
# refused by the character rule and compile without it; the const argument is
# refused only by the normalizer's cv verdict (`NormalizedTypeName::qualified`),
# and compiles without that -- each deletion measured on cl, clang-cl, clang
# and GCC.
formula_add_negative_test(tag_name_unreadable_lambda
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

formula_add_negative_test(tag_name_unreadable_unnamed
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

formula_add_negative_test(tag_name_unreadable_function_argument
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

formula_add_negative_test(tag_name_unreadable_const_argument
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

# The same placeholders one level down, as a template argument, where the
# whole name starts with a letter. Refused on cl and GCC by the rule that
# every argument starts with a letter, a digit, `_` or `-`, and compiled
# there without it; clang is refused by the `(` of its own spelling either way.
formula_add_negative_test(tag_name_unreadable_nested_lambda
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

formula_add_negative_test(tag_name_unreadable_nested_unnamed
    "formula: this tag's name, as the compiler spells it, cannot be shown plainly")

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
    "formula: this breakpoint table's breakpoints do not strictly ascend"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(interpolating_lookup_descending_breakpoint
    "formula: this breakpoint table's breakpoints do not strictly ascend"
    REJECT "must be initialized by a constant expression")

formula_add_negative_test(interpolating_lookup_descending_breakpoint_no_factory
    "formula: this breakpoint table's breakpoints do not strictly ascend"
    REJECT "must be initialized by a constant expression")

# The other rule: a row whose key is not a rational number at all. Separate from
# the ordering rule because a one-row table has no pair for the ordering sweep
# to look at, and because "not ascending" would be a true statement about the
# wrong mistake.
formula_add_negative_test(interpolating_lookup_malformed_breakpoint
    "formula: this breakpoint table -- an interpolating lookup's rows, a snap's permitted set, or a curve's domain -- has a breakpoint whose key is not a rational number")

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

# Two distinct tags spelt the same by `TagName`: a trace could not say which
# variant ran but by its position.
formula_add_negative_test(method_tag_names_spelt_alike
    "formula: two variants of this method are spelt the same in a trace")

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
# A pin or a prune must cite the clause that makes it.
formula_add_negative_test(overlay_pin_without_citation
    "formula: pin_variant<Tag>() was given no citation")
formula_add_negative_test(overlay_prune_without_citation
    "formula: prune_variant<Tag>() was given no citation")

# Every overlay operation must cite the clause it comes from: one case per
# operation, besides the pin's and the prune's above.
formula_add_negative_test(overlay_constant_without_citation
    "formula: with_constant<Q>(value) was given no citation")
formula_add_negative_test(overlay_derived_without_citation
    "formula: add_derived<Q>(expression) was given no citation")
formula_add_negative_test(overlay_replacement_without_citation
    "formula: replace_variant<Tag>(expression) was given no citation")
formula_add_negative_test(overlay_rounding_without_citation
    "formula: with_rounding<U, Places, Mode>() was given no citation")
formula_add_negative_test(overlay_constraints_without_citation
    "formula: with_constraints(constraints(...)) was given no citation")

formula_add_negative_test(overlay_pin_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

formula_add_negative_test(overlay_prune_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

# Fixing a quantity the method never reads. "Never" is judged of the method
# the overlay PRODUCES, so the answer does not depend on operation order: each
# pair below lists the same two operations both ways round, and both are
# refused. In each pair, the order where the quantity is still read at the
# moment the constant is applied is the one a per-operation check would miss.
# The first case pins the message past its first clause.
formula_add_negative_test(overlay_constant_unused
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses; an override nobody reads would silently do nothing")

formula_add_negative_test(overlay_constant_after_its_variant_is_pruned
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_before_its_variant_is_pruned
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_after_pin
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_before_pin
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# One overlay both pinning and pruning, in either order. The pin-first case's
# REJECT pins that `apply` is not instantiated over the refused overlay:
# applied, its prune would be refused as naming a variant the pin removed.
# The prune-first case compiles cleanly even when applied, so its REJECT
# cannot fire; it is there only for the order.
formula_add_negative_test(overlay_pins_then_prunes
    "formula: this overlay both pins a variant and prunes one"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

formula_add_negative_test(overlay_prunes_then_pins
    "formula: this overlay both pins a variant and prunes one"
    REJECT "formula: this overlay pins or prunes a variant the method does not declare")

# A refused pin leaves the method as it was. The REJECT refuses the false
# "nothing reads it" a later constant would get if the pin carried on with
# the variant at position 0 instead.
formula_add_negative_test(overlay_pin_absent_then_constant
    "formula: this overlay pins or prunes a variant the method does not declare"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

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
# A role's displayed name must be identifier-like: an operator character through a scope,
# a template specialisation's own name through a record and a scope (one
# message for both, counted by hand), and a control character through a
# record alone.
formula_add_negative_test(record_role_name_operator
    "formula: this record role's displayed name is not identifier-like")
formula_add_negative_test(record_role_name_template_argument
    "formula: this record role's displayed name is not identifier-like")
formula_add_negative_test(record_role_name_control_character
    "formula: this record role's displayed name is not identifier-like")

# A role's name must start with a letter, and must not read as "this
# record", in any case.
# The second is identifier-like, so the identifier rule must not also fire:
# the REJECT. Through a record and a scope at once, it draws one message.
formula_add_negative_test(record_role_name_leading_digit
    "formula: this record role's displayed name is not identifier-like")
formula_add_negative_test(record_role_name_this_record
    "formula: this record role is displayed as 'this record'"
    EXPECT_COUNT 1
    REJECT "formula: this record role's displayed name is not identifier-like")
# The same, spelt as an identifier, and two roles displayed alike.
formula_add_negative_test(record_role_name_this_record_identifier
    "formula: this record role is displayed as 'this record'"
    EXPECT_COUNT 1
    REJECT "formula: this record role's displayed name is not identifier-like")
formula_add_negative_test(record_context_roles_named_alike
    "formula: this record_context binds two roles whose names are displayed alike"
    EXPECT_COUNT 1
    REJECT "formula: this record_context binds the same role to more than one record")

formula_add_negative_test(overlay_constant_unknown_node
    "formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# The same, with a prune afterwards that removes the variant holding the
# unknown node. The method produced then looks entirely known, so the REJECT
# pins that the unused-quantity check is gated on the method the overlay was
# applied to, not only on the one it produced.
formula_add_negative_test(overlay_constant_unknown_node_then_pruned
    "formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# The same unseeable node inside three scope types. The overlay sees through
# every scope, so the mistake is refused once, not once per scope type:
# EXPECT_COUNT pins one message, and gives three with
# the scope's rewrite deleted. Not counted under cl, which stops at the first
# failed static_assert in this chain and so reads 1 either way.
formula_add_negative_test(record_scope_overlay_one_refusal
    "formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside"
    EXPECT_COUNT 1
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# An overlay applied to a malformed method. The method is refused where it is
# declared, and `apply` gates its body on the method being well formed, as
# `evaluate_method` does; the REJECT refuses the compiler's own error for the
# `apply_operation` an ungated body would call with the parts out of order.
formula_add_negative_test(overlay_apply_arguments_out_of_order
    "formula: this method's variants are not a variants pack"
    REJECT "apply_operation")

# Two rounding overrides in one overlay, of different granularities -- two
# different types, which a repeat rule comparing types alone lets through,
# and the second silently replaces the first. The repeat rule compares each
# operation's identity, and every rounding override has the same one.
formula_add_negative_test(overlay_rounding_twice
    "formula: this overlay lists the same operation twice; two constants or definitions of one quantity, two pins, prunes or replacements of one variant, two rounding overrides, or two replacements of the constraints, leave the first silently doing nothing")

# Two replacements of the constraints in one overlay, holding different
# constraints: the same, for the method's one set of constraints.
formula_add_negative_test(overlay_constraints_twice
    "formula: this overlay lists the same operation twice")

# A quantity only the method's own constraint reads, fixed or defined, then
# constraints put in by the same overlay that read it plainly: refused as
# bypassed. Not as read by nothing -- the new constraint reads it -- and not
# as read both substituted and plainly, since nothing substituted is left,
# and not as listed before a use it never met, since it met one: the REJECTs.
formula_add_negative_test(overlay_constant_before_constraints_read_plainly
    "formula: this overlay fixes a quantity, and an operation listed after the constant removed every use it fixed and put back one that reads the quantity from the environment"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this method reads a quantity both where"
        "formula: this overlay fixes a quantity that nothing read where the constant is listed")

formula_add_negative_test(overlay_derived_before_constraints_read_plainly
    "formula: this overlay derives a quantity, and an operation listed after the definition removed every use it replaced and put back one that reads the quantity from the environment"
    REJECT "formula: this overlay derives a quantity that no variant or constraint of the method uses" "formula: this method reads a quantity both where"
        "formula: this overlay derives a quantity that nothing read where the definition is listed")

# The same shapes, where the substitution met no use where it applied: the
# quantity is read by nothing until a later operation of the overlay puts in
# a plain use -- constraints, a definition, a replaced variant -- or a
# definition cycle starts from a quantity nothing reads. Refused as listed
# before that use; not as read by nothing, which the produced method
# contradicts, nor as bypassed -- no operation removed a use the
# substitution reached: the REJECTs.
formula_add_negative_test(overlay_constant_never_read_then_constraints_read_it
    "formula: this overlay fixes a quantity that nothing read where the constant is listed, and an operation listed after it reads the quantity from the environment"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this overlay fixes a quantity, and an operation listed after the constant removed")

formula_add_negative_test(overlay_constant_never_read_then_derived_reads_it
    "formula: this overlay fixes a quantity that nothing read where the constant is listed, and an operation listed after it reads the quantity from the environment"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this overlay fixes a quantity, and an operation listed after the constant removed")

formula_add_negative_test(overlay_constant_never_read_then_replacement_reads_it
    "formula: this overlay fixes a quantity that nothing read where the constant is listed, and an operation listed after it reads the quantity from the environment"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this overlay fixes a quantity, and an operation listed after the constant removed")

formula_add_negative_test(overlay_derived_cycle_unread
    "formula: this overlay derives a quantity that nothing read where the definition is listed, and an operation listed after it reads the quantity from the environment"
    REJECT "formula: this overlay derives a quantity that no variant or constraint of the method uses" "formula: this overlay derives a quantity, and an operation listed after the definition removed")

# A quantity only the replaced constraints read, fixed before and after the
# replacement: the same method is produced, so the same refusal in either
# order.
formula_add_negative_test(overlay_constant_before_its_constraint_is_replaced
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_after_its_constraint_is_replaced
    "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

# A rounding override whose unit does not measure what the method reports is
# refused where the overlay applies it, in the words the method's own rule
# would be refused in -- alone, and followed by an operation that builds a
# method again, which must not raise it a second time. The count is not
# something a REJECT can say, but where the refusal is raised is: raised again
# by the method the constant built, it names that method's variants, which
# hold an `OverriddenConstantNode` -- the REJECT.
formula_add_negative_test(overlay_rounding_dimension_mismatch
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report")

formula_add_negative_test(overlay_rounding_dimension_mismatch_then_constant
    "formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report"
    REJECT "OverriddenConstantNode")

# A published layout that is not one: a repeated position, and a position at
# the count. One case per half of the rule -- the ordering half and the range
# half -- because each can be deleted while the other still refuses its own
# case; the swapped cases below pin the ordering half past mere distinctness.
# The positions are values, not
# types, so the refusal is a call no constant expression can make, and its
# name is the message -- see `detail::PublishedLayout` (`method.hpp`). Stated
# through `detail::PublishedLayoutAccess`, the one door to the check.
formula_add_negative_test(method_published_position_repeated
    "published_positions_must_increase_and_stay_below_the_published_count")

formula_add_negative_test(method_published_position_out_of_range
    "published_positions_must_increase_and_stay_below_the_published_count")

# The route `apply` carries a layout by: `select` copies the published
# positions of the variants a pin or a prune keeps, and holds the positions it
# keeps to the same rule at compile time.
formula_add_negative_test(method_published_selection_repeated
    "published_positions_must_increase_and_stay_below_the_published_count")

# Two variants swapped, by hand and through `select`. Distinct and below the
# count, so only the rule that positions increase refuses them -- which a pin
# or a prune always satisfies, since it keeps declaration order.
formula_add_negative_test(method_published_position_swapped
    "published_positions_must_increase_and_stay_below_the_published_count")

formula_add_negative_test(method_published_selection_swapped
    "published_positions_must_increase_and_stay_below_the_published_count")

# A layout stated from outside the library at all, well formed or not: in an
# aggregate initialiser, and assigned at run time. Only the library states
# one; the cases above reach the check through `detail::PublishedLayoutAccess`.
formula_add_negative_test(method_published_layout_stated_by_hand
    "formula: a variant's published position is stated only by the library")

formula_add_negative_test(method_published_layout_assigned_by_hand
    "formula: a variant's published position is stated only by the library")

# A layout selected by hand from another pack's: `select` is public so that it
# refuses in the library's words; `apply` reaches the selection through
# `detail::PublishedLayoutAccess`.
formula_add_negative_test(method_published_layout_selected_by_hand
    "formula: a variant's published position is stated only by the library")

# A method's own rule claiming to be a jurisdiction overlay's. Provenance is
# the library's to state: `rounding_rule<...>()` or `with_rounding` applied by
# an overlay, nothing else.
formula_add_negative_test(method_rounding_rule_provenance_hand_built
    "formula: a rounding rule's provenance is the library's to state, not an author's")

# The same claim made through a rounding node built by hand and put in a
# variant. The node holds a `RoundingRule`, not a provenance of its own, so the
# claim is refused by the rule's guard.
formula_add_negative_test(method_rounding_rule_node_provenance_hand_built
    "formula: a rounding rule's provenance is the library's to state, not an author's")

# Whose a method's constraints are is the library's to state: an origin
# stated by hand, and a method's own constraints wrapped by hand as a
# jurisdiction's, are both refused. `method(...)` declares the method's own,
# and `with_constraints` applied by an overlay a jurisdiction's.
formula_add_negative_test(method_constraint_origin_hand_built
    "formula: whose a method's constraints are is the library's to state, not an author's")

formula_add_negative_test(method_overlaid_constraints_hand_built
    "formula: whose a method's constraints are is the library's to state, not an author's")

# A malformed method checked: `check_method` gates its body as
# `evaluate_method` does, so the REJECT refuses the compiler's own error for
# the `check_all` an ungated body would call on the rounding rule.
formula_add_negative_test(method_check_arguments_out_of_order
    "formula: this method's rounding rule is not a rounding rule"
    REJECT "check_all")

# A rounding node built by hand around the default rule: it would make a trace
# say "(method default)" where there is no method. Only `evaluate_method`
# builds one.
formula_add_negative_test(method_rounding_rule_node_hand_built
    "formula: only evaluate_method builds a rounding rule node")

# Wholesale replacement and derived quantities. Each case's own file
# says what it pins; the REJECTs pin the gates between refusals, so that one
# mistake gets one message. Both orders of a replacement and a prune are
# refused in the same words, and so are both orders of a definition and a
# prune, because each is judged against the method the overlay produces. The
# three hand-built cases pin that only an overlay builds the nodes a trace
# reads "fixed", "derived" or "replaced" from.
formula_add_negative_test(overlay_replacement_changes_quantity
    "formula: this overlay replaces a variant with a formula of a different dimension from the method's"
    REJECT "formula: two variants of this method measure different dimensions")

formula_add_negative_test(overlay_replacement_absent_variant
    "formula: this overlay replaces a variant the method does not declare"
    REJECT "formula: this overlay replaces a variant that the method it produces does not hold")

formula_add_negative_test(overlay_replacement_then_pruned
    "formula: this overlay replaces a variant that the method it produces does not hold"
    REJECT "formula: this overlay replaces a variant the method does not declare")

formula_add_negative_test(overlay_replacement_after_prune
    "formula: this overlay replaces a variant that the method it produces does not hold"
    REJECT "formula: this overlay replaces a variant the method does not declare")

formula_add_negative_test(overlay_replacement_tag_cv_qualified
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay replaces a variant the method does not declare")

formula_add_negative_test(overlay_replacement_twice
    "formula: this overlay lists the same operation twice")

formula_add_negative_test(overlay_derived_unused
    "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_before_its_variant_is_pruned
    "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_after_its_variant_is_pruned
    "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_dimension_mismatch
    "formula: this overlay derives a quantity from an expression of a different dimension"
    REJECT "formula: this method reads a quantity both where an overlay fixed or derived it" "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_self_reference
    "formula: this overlay derives a quantity from an expression that reads the quantity itself"
    REJECT "formula: this method reads a quantity both where an overlay fixed or derived it" "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_definition_unknown_node
    "formula: this overlay derives a quantity in an expression holding a node kind it cannot see inside"
    REJECT "formula: this overlay derives a quantity from an expression that reads the quantity itself" "formula: this method reads a quantity both where an overlay fixed or derived it" "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_derived_method_unknown_node
    "formula: this overlay derives a quantity in an expression holding a node kind it cannot see inside"
    REJECT "formula: this overlay overrides a constant" "formula: this overlay derives a quantity that no variant")

# A definition cycle across two quantities in one overlay: the second
# definition puts a plain use of the first quantity back into its definition.
# The first definition's node is still there beside that plain use, so the
# REJECT pins that this is not also reported as bypassed.
formula_add_negative_test(overlay_derived_cycle
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "formula: this overlay derives a quantity, and an operation listed after the definition removed")

# A refused replacement leaves the method without it, so the substitution
# checks wait on every replacement having been applied; the replacement's
# own dimension and held rules are judged against the result, so one mistake
# gets one message in either order. The stacked cases pin that the rule a
# substitution be in effect wherever the method reads its quantity is judged
# against the whole produced method, across overlays applied in turn.
formula_add_negative_test(overlay_replacement_absent_then_constant
    "formula: this overlay replaces a variant the method does not declare"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_replacement_tag_cv_qualified_then_constant
    "formula: this tag is not a plain class type"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this overlay replaces a variant the method does not declare")

formula_add_negative_test(overlay_replacement_dimension_then_derived
    "formula: this overlay replaces a variant with a formula of a different dimension from the method's"
    REJECT "formula: this overlay derives a quantity that no variant or constraint of the method uses" "formula: two variants of this method measure different dimensions")

formula_add_negative_test(overlay_replacement_dimension_then_pruned
    "formula: this overlay replaces a variant with a formula of a different dimension from the method's"
    REJECT "formula: this overlay replaces a variant that the method it produces does not hold")

formula_add_negative_test(overlay_replacement_pruned_then_dimension
    "formula: this overlay replaces a variant with a formula of a different dimension from the method's"
    REJECT "formula: this overlay replaces a variant that the method it produces does not hold")

formula_add_negative_test(overlay_stacked_cycle
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "formula: this overlay derives a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_stacked_constant_read_again
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses")

formula_add_negative_test(overlay_constant_reintroduced
    "formula: this overlay fixes a quantity, and an operation listed after the constant removed every use it fixed and put back one that reads the quantity from the environment"
    REJECT "formula: this overlay overrides a quantity that no variant or constraint of the method uses" "formula: this method reads a quantity both where"
        "formula: this overlay fixes a quantity that nothing read where the constant is listed")

# A refusing constructor explicitly specialised by user code, to forge a
# provenance with no `detail::` written. The specialisation cannot match the
# constructor -- whose last parameter is `detail::ProvenanceStatedByAuthor` --
# so it is the compiler, not the library, that refuses it, in its own words;
# each case expects the type's name, which every compiler's refusal names.
formula_add_negative_test(method_constraint_origin_specialised "ConstraintOrigin")
formula_add_negative_test(method_rounding_rule_specialised "RoundingRule")
formula_add_negative_test(method_overlaid_constraints_specialised "OverlaidConstraints")
formula_add_negative_test(method_rounding_rule_node_specialised "RoundingRuleNode")
formula_add_negative_test(overlay_node_fixed_specialised "OverriddenConstantNode")
formula_add_negative_test(overlay_node_derived_specialised "DerivedQuantityNode")
formula_add_negative_test(overlay_node_replaced_specialised "ReplacedVariantNode")

formula_add_negative_test(overlay_node_hand_built_fixed
    "formula: only an overlay builds this node")

formula_add_negative_test(overlay_node_hand_built_derived
    "formula: only an overlay builds this node")

formula_add_negative_test(overlay_node_hand_built_replaced
    "formula: only an overlay builds this node")

# A vocabulary that renames one quantity twice, and one holding something that
# is not an entry. Each REJECTs the other's message: the duplicate check is
# gated on the entry check, so a pack refused as not entries is not also
# refused as a duplicate (`RequireVocabularyEntries`, `vocabulary.hpp`). The
# second also REJECTs `quantity'`, which is how the compilers' own errors name
# the `Es::quantity` a pack of non-entries does not have. Measured: removing
# the gate in `renames_distinct_quantities` makes this case fail on cl 19.51
# and g++ 13.3 (not clang++ 20.1.8); removing the one in
# `ScopedVocabulary::symbol` makes it fail on clang++ 20.1.8 (not the other
# two). Each gate is pinned on at least one leg.
formula_add_negative_test(vocabulary_duplicate_quantity
    "formula: this vocabulary renames the same quantity twice"
    REJECT "formula: a vocabulary holds only")
formula_add_negative_test(vocabulary_not_an_entry
    "formula: a vocabulary holds only entries made by renames<Q>(symbol)"
    REJECT "formula: this vocabulary renames the same quantity twice" "quantity'")

# An empty symbol, which would leave a blank where the quantity stands. It has
# no characters, so it is vacuously all whitespace too; the REJECT pins that
# `renames` refuses it once, as empty, and not again as blank.
formula_add_negative_test(vocabulary_empty_symbol
    "formula: renames<Q>(\"\") gives the quantity an empty symbol"
    REJECT "renames_symbol_must_not_be_all_whitespace")

# `Q const` names the same quantity as `Q`, so the two renamed together are a
# double rename -- the cv-blind matching `renamesOf` claims, pinned.
formula_add_negative_test(vocabulary_duplicate_cv
    "formula: this vocabulary renames the same quantity twice")

# Symbols `renames` refuses in this library's words. A buffer that is not
# const, whatever its storage, meets the overload that exists to refuse it; the
# other three are values, not types, so each is refused by a call to a
# function whose name is the refusal (`vocabulary.hpp`). The unterminated
# array and the embedded NUL are two separate checks in `renames`, so each
# has its own case.
formula_add_negative_test(vocabulary_symbol_mutable_buffer
    "formula: renames<Q> was given a buffer that is not const")
formula_add_negative_test(vocabulary_symbol_embedded_nul
    "renames_symbol_must_be_a_string_with_no_embedded_nul")
formula_add_negative_test(vocabulary_symbol_unterminated
    "renames_symbol_must_be_a_string_with_no_embedded_nul")
formula_add_negative_test(vocabulary_symbol_whitespace
    "renames_symbol_must_not_be_all_whitespace")

# A symbol that could forge a trace's provenance: a bracketed clause of its
# own, or a whole line. One case per shape, because `[` and a newline are two
# different kinds of character to the check.
formula_add_negative_test(vocabulary_symbol_bracket
    "renames_symbol_must_not_hold_a_bracket_or_a_control_character")
formula_add_negative_test(vocabulary_symbol_newline
    "renames_symbol_must_not_hold_a_bracket_or_a_control_character")

# A render_node of the library's own that takes no vocabulary. Declared in
# `formula` before render.hpp, where qualified lookup finds it as it finds
# every one of the library's (`detail::render_in_vocabulary`).
formula_add_negative_test(render_node_without_vocabulary
    "formula: a render_node of this library takes no vocabulary")

# A symbol in a const buffer on the stack, which a trace step would keep a
# dangling view of. `renames` is `consteval` and takes a character array, so
# the argument must be usable in a constant expression, and nothing in this
# library can phrase that refusal: storage duration is neither a type nor a
# value a constant expression can inspect, so the text is the compilers' own.
# (A buffer that is not const is refused in the library's words, above.)
# Measured: cl 19.51 and clang-cl 22.1.3 and clang++ 20.1.8 say "is not a
# constant expression", g++ 13.3 says "is not usable in a constant
# expression" -- the case now fails while `renames` reads the buffer's
# characters, which g++ refuses first. Their shared words are the EXPECT. Not
# measured on CI's g++-14 or Apple clang. Deleting `consteval` makes the case
# compile.
formula_add_negative_test(vocabulary_symbol_not_static "a constant expression")

# Phase 14: records and the context. Each case's own comment says why its
# mistake is the only thing wrong with it.
formula_add_negative_test(record_context_duplicate_role
    "formula: this record_context binds the same role to more than one record"
    REJECT "RequireThisRecordFirst" "RequirePlainRole")

formula_add_negative_test(record_context_first_not_this_record
    "formula: the first record of a record_context must be record<ThisRecord>(...)"
    REJECT "RequireDistinctRoles" "RequirePlainRole")

formula_add_negative_test(record_role_not_plain
    "formula: this record role is not a plain class type")

# A context is its own record's environment, so a quantity that record lacks
# is refused as it is for the plain environment, by RequireProvided. Its
# message is counted once by hand on cl, clang-cl, g++ 13 and clang++ 20.1.8;
# REJECT cannot refuse a second copy of it.
formula_add_negative_test(record_context_missing_quantity
    "formula: this environment provides no value for this quantity"
    REJECT "RequireThisRecordFirst" "RequireDistinctRoles" "RequirePlainRole")

# One mistake, a cv-qualified ThisRecord, must draw one message: the
# first-record rule ignores cv-qualifiers so that only the role rule reports
# it. Deleting that `remove_cv_t` makes the REJECT fire.
formula_add_negative_test(record_context_this_record_not_plain
    "formula: this record role is not a plain class type"
    REJECT "RequireThisRecordFirst")

# The compiler's own words, since the strong types are the guard and no
# library static_assert fires (REJECT "formula: " holds every library message
# to that). Measured: cl says "cannot convert argument 1 from
# 'formula::TestId' to 'formula::SampleId'", g++ 13 "could not convert ... from
# 'formula::TestId' to 'formula::SampleId'", clang++ 20.1.8 and clang-cl 22.1.3 "no
# viable conversion from 'TestId' to 'SampleId'". Not "no matching function":
# record_key is one non-template function, and none of the four words it that
# way. g++-14 and Apple clang were not measured.
# The qualifier differs, so the shared text stops at the second quote.
formula_add_negative_test(record_key_swapped "TestId' to '" REJECT "formula: ")

# What `explicit` on SampleId and TestId refuses, which the distinct types
# alone do not: a bare integer for a key. One case per key, each with the
# other key well formed, so that each `explicit` is the only thing refusing
# its case: removing it makes that case compile. Again the compiler's own
# words: cl says "cannot convert argument N from 'int' to
# 'formula::SampleId'", g++ 13 "could not convert '17' from 'int' to
# 'formula::SampleId'", clang++ 20.1.8 and clang-cl 22.1.3 "no viable
# conversion from 'int' to 'SampleId'" (TestId likewise).
formula_add_negative_test(record_key_sample_from_integer "'int' to '" REJECT "formula: ")
formula_add_negative_test(record_key_test_from_integer "'int' to '" REJECT "formula: ")

# Reading from another record through from_record<Role>.
# Each refusal is gated so that it is the only message: the evaluator does
# not dispatch the operand after refusing, which each case's REJECT of
# RequireProvided holds it to (each case's operand names a quantity the
# environment it would fall back to does not hold), and messages were counted
# by hand. Removing any of the three gates
# makes its cases fail on their REJECT under g++ 13 and clang++ 20.1.8;
# cl-debug showed one message either way, so only those legs see a cascade.
formula_add_negative_test(record_scope_plain_environment
    "formula: this formula reads from another record, but was evaluated against an environment that holds only one"
    REJECT "RequireProvided" "RequireBoundRole" "RequireForeignRole")

formula_add_negative_test(record_scope_role_not_bound
    "formula: this record_context binds no record to this role"
    REJECT "RequireProvided" "RequireRecordContext" "RequireForeignRole")

# After the refusal record<Role>() returns this record, so the key read after
# it draws nothing more. Without that fallback the tuple lookup adds errors
# of its own ("tuple index must be in range" and a deleted `get` on g++ 13
# and clang++ 20.1.8, tuple_element instantiations on cl); the "tuple"
# REJECT catches them on all three, measured.
formula_add_negative_test(record_context_role_not_bound
    "formula: this record_context binds no record to this role"
    REJECT "RequireThisRecordFirst" "RequireDistinctRoles" "tuple")

formula_add_negative_test(record_scope_this_record
    "formula: from_record<ThisRecord> reads this record as though it were another"
    REJECT "RequireBoundRole" "RequireRecordContext" "RequireProvided")

# One mistake, a cv-qualified ThisRecord in a scope, draws one message: the
# evaluator returns at once for a role naming this record, so the bound-role
# rule -- no context binds `ThisRecord const` -- is never asked.
formula_add_negative_test(record_scope_this_record_const
    "formula: from_record<ThisRecord> reads this record as though it were another"
    REJECT "RequireBoundRole" "RequireRecordContext" "RequireProvided")

# A RecordOrigin stated by hand. The refusing constructor is a template with
# a defaulted placeholder parameter, so that its static_assert waits for a
# call: RecordOrigin is not a template, and a non-dependent assertion would
# fire at the header's own definition.
formula_add_negative_test(record_origin_by_hand
    "formula: a record origin is the library's to state, not an author's")

# The same forgery by explicit specialisation, which compiled while the
# refusing constructor was a member template. The compiler's own words, since
# no library static_assert is involved.
formula_add_negative_test(record_origin_specialised "RecordOrigin" REJECT "formula: ")

# Lineage requirements. Each case's own comment says why its mistake is the
# only thing wrong with it; messages were counted by hand.
formula_add_negative_test(lineage_requirement_empty
    "formula: this lineage requirement names no attribute"
    REJECT "RequireAttributesNamedOnce")

formula_add_negative_test(lineage_attribute_twice
    "formula: this lineage requirement names the same attribute more than once"
    REJECT "RequireSomeAttribute")

formula_add_negative_test(lineage_record_declares_twice
    "formula: this record declares the same lineage attribute more than once"
    REJECT "RequireLineageEntries" "RequirePlainRole")

# The duplicate rule is gated on every entry being a lineage entry; without
# the gate the non-entry would also draw it.
formula_add_negative_test(lineage_record_entry_not_lineage
    "formula: a record takes only lineage entries after its environment"
    REJECT "RequireAttributesDeclaredOnce")

formula_add_negative_test(lineage_against_own_role
    "formula: this lineage requirement compares the record it reads with itself"
    REJECT "RequireBoundRole" "RequireDeclaredAttribute")

# An attribute neither record declares: one message, not one per record. The
# compared record is asked only once the read record is known to declare the
# attribute; without that gate g++ and clang++ report both.
formula_add_negative_test(lineage_attribute_undeclared
    "formula: this lineage requirement compares an attribute the record does not declare"
    REJECT "RequireProvided" "RequireBoundRole")

formula_add_negative_test(record_scope_requirement_not_lineage
    "formula: a record scope's requirement is not a lineage requirement"
    REJECT "RequireForeignRole" "RequireComparandNotSubject")

# A sink defining only one of the two scope hooks. After the refusal the
# evaluator reports to neither, so nothing else follows.
formula_add_negative_test(record_scope_sink_one_hook
    "formula: this sink defines only one of record_entered and lineage_checked"
    REJECT "RequireProvided" "RequireBoundRole")

formula_add_negative_test(record_scope_nested
    "formula: this formula reads from another record, but was evaluated against an environment that holds only one"
    REJECT "RequireBoundRole" "RequireProvided" "RequireForeignRole")
# The same nesting, put there by an overlay's add_derived: the refusal's words
# name it, since the author did evaluate against a context.
formula_add_negative_test(record_scope_nested_by_overlay
    "a from_record sits inside another from_record, perhaps put there by an overlay's add_derived"
    EXPECT_COUNT 1
    REJECT "RequireBoundRole" "RequireProvided" "RequireForeignRole")

formula_add_negative_test(record_unbound_this_record
    "formula: the record being evaluated cannot be unbound"
    REJECT "RequirePlainRole")

# After this refusal `record()` returns a record over the context's own
# record's environment, so that using it -- its key, or evaluating through
# it -- draws nothing more. The message is counted once by hand on cl,
# clang-cl, g++ 13 and clang++ 20.1.8; REJECT cannot refuse a second copy of it, and REJECTs RequireProvided, the message
# an evaluation through a record with no values would add.
formula_add_negative_test(record_environment_is_context
    "formula: a record holds its values in an environment(...), and this is not one"
    REJECT "RequirePlainRole" "RequireThisRecordFirst" "RequireDistinctRoles" "RequireProvided")

# A bare Measured where environment(...) was meant, and a formula of three
# quantities evaluated through a context over that record. One message: the
# refused record holds detail::AbsentEnvironment, which answers every
# quantity as absent without a RequireProvided of its own.
# A read from another record over a series itself: refused with one
# message, the refused scope answered as absent so the formula it is used in
# draws nothing more; and a series read through a record whose values were
# refused, which AbsentEnvironment answers without a get_series error.
formula_add_negative_test(record_scope_of_series
    "formula: from_record was given a series, raw observations or a curve"
    EXPECT_COUNT 1)
formula_add_negative_test(record_series_through_refused_record
    "formula: a record holds its values in an environment(...), and this is not one"
    EXPECT_COUNT 1
    REJECT "get_series")
# The same refusal, rendered, documented and overlaid: every walk answers the
# refused scope, as phase 12's walks answer a refused series, so each stays
# one message. cl stops at the first failed static_assert, so only the
# overlay's REJECT -- a library static_assert of its own -- can fire there;
# g++, clang++ and clang-cl see all three.
formula_add_negative_test(record_scope_of_series_rendered
    "formula: from_record was given a series, raw observations or a curve"
    EXPECT_COUNT 1
    REJECT "render_node")
formula_add_negative_test(record_scope_of_series_documented
    "formula: from_record was given a series, raw observations or a curve"
    EXPECT_COUNT 1
    REJECT "render_node" "collect")
formula_add_negative_test(record_scope_of_series_overlaid
    "formula: from_record was given a series, raw observations or a curve"
    EXPECT_COUNT 1
    REJECT "cannot see inside")
# The lineage form of the same refusal: without
# its overload the call finds no from_record at all, and no library message.
formula_add_negative_test(record_scope_of_series_with_lineage
    "formula: from_record was given a series, raw observations or a curve"
    EXPECT_COUNT 1)

formula_add_negative_test(record_values_not_environment
    "formula: a record holds its values in an environment(...), and this is not one"
    REJECT "RequireProvided" "RequirePlainRole" "RequireThisRecordFirst" "RequireDistinctRoles")

# The same mistake one level up: a context given as this record's values,
# and a formula then evaluated through the context built over it. One
# message, and no RequireProvided for each quantity read. With the
# context branch of `record()` removed, both this case and the one above
# fail on their RequireProvided REJECT under g++ 13 and clang++ 20.1.8;
# cl-debug showed one message either way, so only those two legs can see
# that cascade.
formula_add_negative_test(record_context_over_context
    "formula: a record holds its values in an environment(...), and this is not one"
    REJECT "RequireProvided" "RequirePlainRole" "RequireThisRecordFirst" "RequireDistinctRoles")

# Opaque operations (`opaque.hpp`). Each refusal is gated on the ones before
# it, so one mistake draws one message; every REJECT names the message the
# next check down would add.
formula_add_negative_test(opaque_blank_name
    "formula: an opaque operation's name must say something; one that is empty or blank leaves the trace unable to say what ran"
    REJECT "may hold only ASCII letters, digits and single spaces")
formula_add_negative_test(opaque_unreadable_name
    "formula: an opaque operation's name and output names may hold only ASCII letters, digits and single spaces, so that every dialect shows them as written"
    REJECT "declares two outputs with the same name")
formula_add_negative_test(opaque_duplicate_output
    "formula: an opaque operation declares two outputs with the same name; opaque_output could not tell them apart"
    REJECT "may hold only ASCII letters, digits and single spaces")
formula_add_negative_test(opaque_no_outputs
    "formula: an opaque operation declares no outputs; a call with nothing to use has nothing to trace"
    REJECT "declares two outputs with the same name")
formula_add_negative_test(opaque_unknown_output
    "formula: this opaque operation has no output of that name; the operation and the name asked for appear in this diagnostic as the template arguments of RequireOpaqueOutputNamed"
    REJECT "this result quantity does not measure the dimension")
# g++ 14.2 followed the noexcept refusal with seven errors of its own when the
# evaluator's body was not gated (phase 15's spike, step 7): the REJECTs.
formula_add_negative_test(opaque_throwing_compute
    "formula: an opaque operation's compute must be noexcept; evaluation cannot throw, and an exception escaping it would end the program"
    REJECT "~expected" "flows off the end")
formula_add_negative_test(opaque_wrong_arity
    "formula: this opaque call passes a different number of inputs than the operation declares"
    REJECT "passes an input of another shape" "does not accept inputs of these dimensions")
formula_add_negative_test(opaque_wrong_shape
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "does not accept inputs of these dimensions" "compute cannot be called")
formula_add_negative_test(opaque_series_lengths_differ
    "formula: this opaque operation's series and curve inputs differ in length"
    REJECT "does not accept inputs of these dimensions")
formula_add_negative_test(opaque_dimensions_refused
    "formula: this opaque operation does not accept inputs of these dimensions"
    REJECT "compute cannot be called" "must be noexcept")
formula_add_negative_test(opaque_compute_signature
    "formula: this opaque operation's compute cannot be called with the values its declared inputs give"
    REJECT "must be noexcept")
formula_add_negative_test(opaque_output_as_series
    "formula: an opaque call is not a value, since an operation may have several outputs; choose one with opaque_output<\"name\">(call)"
    REJECT "this expression is a series, not a single value")
# Least squares (`least_squares.hpp`): two loose series are refused in this
# library's words, not the compiler's; a fit in double is refused once, in the
# curve's words.
formula_add_negative_test(least_squares_not_a_curve
    "formula: linear_least_squares fits a curve; pair the domain and the values with curve(domain, values)"
    REJECT "no matching overloaded function" "no matching function" "RequireResultDimension")
# A fit with no citation is refused in the library's words; a fit over a
# refused curve says only the curve's.
formula_add_negative_test(least_squares_uncited
    "formula: linear_least_squares needs a citation, the reason the method fits a line here; pass {} when it gives none"
    REJECT "no matching overloaded function" "no matching function" "RequireResultDimension")
formula_add_negative_test(least_squares_refused_curve
    "formula: this curve pairs a domain and values of different lengths"
    REJECT "RequireResultDimension" "output_dimensions" "compute cannot be called")
formula_add_negative_test(least_squares_in_double
    "formula: a curve can only be evaluated with Rep = Rational"
    REJECT "must be noexcept" "compute cannot be called")
# A constant for a quantity read as a series inside an opaque call: the
# rewrite sees through the call to its inputs, so the series refusal is the
# one message -- not "cannot see inside", and not "nobody reads it".
formula_add_negative_test(overlay_constant_inside_opaque_series
    "formula: this overlay fixes a quantity the method reads as a series; one constant cannot stand for a series"
    REJECT "cannot see inside" "overrides a quantity that no variant or constraint of the method uses")
# A refused opaque call's output stands in with a dimensionless dimension;
# every check over it is silent (`detail::refused_already`), so the one
# mistake draws one message. Each REJECT names the check that would repeat it.
formula_add_negative_test(opaque_refused_result_dimension
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this result quantity does not measure the dimension")
formula_add_negative_test(opaque_refused_added
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "the two sides of this addition or subtraction measure different" "this result quantity does not measure the dimension")
formula_add_negative_test(opaque_refused_compared
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "the two sides of this comparison measure different dimensions")
formula_add_negative_test(opaque_refused_branch
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "the two branches of this when() measure different dimensions")
formula_add_negative_test(opaque_refused_escape
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this numeric_value_of names a unit that does not measure")
formula_add_negative_test(opaque_refused_lookup
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this lookup table's key unit does not measure")
formula_add_negative_test(opaque_refused_rounded
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this rounding node names a unit that does not measure")
formula_add_negative_test(opaque_refused_snapped
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this snap's key unit does not measure")
formula_add_negative_test(opaque_refused_interpolation_point
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this curve is read at a point that does not measure")
formula_add_negative_test(opaque_refused_variants
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "two variants of this method measure different dimensions")
formula_add_negative_test(opaque_refused_rounding_rule
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this method's rounding rule rounds in a unit that does not measure")
formula_add_negative_test(opaque_refused_replacement
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this overlay replaces a variant with a formula of a different dimension")
formula_add_negative_test(opaque_refused_derivation
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "this overlay derives a quantity from an expression of a different dimension")

# A curve operation whose output_dimensions takes one dimension, not two: refused
# once in the library's words, not in the compiler's "no matching function".
formula_add_negative_test(opaque_curve_dimensions_declared
    "formula: this opaque operation's output_dimensions must be a static function taking std::array<Dimension, D>"
    REJECT "no matching overloaded function" "no matching function" "may hold only ASCII letters")
# An output node built by hand at a position its operation does not declare.
formula_add_negative_test(opaque_forged_output_position
    "formula: this opaque output's position names no output of its operation"
    REJECT "subscript")
# The sentinel position built by hand over a sound call: refused in its own
# words, never silently.
formula_add_negative_test(opaque_forged_sentinel_position
    "formula: this opaque output's position names no output of its operation"
    REJECT "subscript" "RequireAddendsAgree" "RequireResultDimension")
# Overlays over opaque calls: a refused call is never rebuilt,
# and a substitution inside a call is seen by every rule over the method.
formula_add_negative_test(overlay_constant_inside_refused_opaque
    "formula: this opaque operation does not accept inputs of these dimensions"
    REJECT "no matching overloaded function" "no matching function" "cannot see inside")
formula_add_negative_test(overlay_derivation_of_refused_opaque
    "formula: this opaque operation does not accept inputs of these dimensions"
    REJECT "no matching overloaded function" "no matching function" "cannot see inside")
formula_add_negative_test(overlay_substitution_inside_opaque_across_overlays
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "cannot see inside")
formula_add_negative_test(overlay_constant_bypassed_inside_opaque
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "cannot see inside")
formula_add_negative_test(overlay_constant_before_derivation_inside_opaque
    "formula: this overlay fixes a quantity that nothing read where the constant is listed"
    REJECT "cannot see inside")

# Bounded retry (`retry.hpp`): each refusal is gated on the ones before it,
# so that one mistake draws one message.
formula_add_negative_test(retry_zero_attempts
    "formula: a retry allows no attempts at all; MaxAttempts must be at least one"
    REJECT "allows at most 64 attempts" "second attempt allows only one attempt")
formula_add_negative_test(retry_over_cap
    "formula: a retry allows at most 64 attempts; the methods this shape exists for repeat a step a few times, and a larger count is almost always a typo"
    REJECT "allows no attempts at all" "second attempt allows only one attempt")
formula_add_negative_test(retry_second_attempt_of_one
    "formula: a retry judged from its second attempt allows only one attempt, so its acceptance could never be judged"
    REJECT "allows no attempts at all" "allows at most 64 attempts")
formula_add_negative_test(retry_attempt_dimension
    "formula: this retry's attempt expression does not compute the dimension of its result quantity"
    REJECT "RequireResultDimension" "RequireComparandsAgree" "no matching")
formula_add_negative_test(retry_starting_dimension
    "formula: this retry's starting value does not compute the dimension of its result quantity"
    REJECT "does not compute the dimension of its result quantity; each attempt" "no matching")
formula_add_negative_test(retry_accept_not_predicate
    "formula: a retry's acceptance is a comparison that holds or does not"
    REJECT "no matching" "checked_evaluate_predicate")
formula_add_negative_test(retry_attempt_not_node
    "formula: a retry's attempt is one expression that produces one value"
    REJECT "no matching" "does not compute the dimension")
formula_add_negative_test(retry_this_attempt_in_attempt
    "formula: this_attempt is the value an attempt produced, known only when judging it"
    REJECT "only meaningful inside a retry" "no matching")
formula_add_negative_test(retry_in_double
    "formula: a retry can only be evaluated with Rep = Rational"
    REJECT "no matching" "only meaningful inside a retry")
formula_add_negative_test(retry_previous_outside
    "formula: previous_attempt, this_attempt and attempt_number are only meaningful inside a retry"
    REJECT "RequireProvided" "no matching")

# Retry review fixes: a context node names the retry's own quantity and is
# read inside an attempt; a retry is evaluated at the top; its start and verdict
# are what they say.
formula_add_negative_test(retry_previous_other_quantity
    "formula: previous_attempt and this_attempt name the retry's own result quantity; this one names another"
    EXPECT_COUNT 1
    REJECT "only meaningful inside a retry" "does not compute the dimension" "template arguments of RequireRetriedQuantity")
formula_add_negative_test(retry_previous_other_dimension
    "formula: previous_attempt and this_attempt name the retry's own result quantity; this one names another"
    EXPECT_COUNT 1
    REJECT "only meaningful inside a retry" "does not compute the dimension" "template arguments of RequireRetriedQuantity")
formula_add_negative_test(retry_context_in_starting_value
    "formula: a retry's starting value is evaluated before its first attempt, so no attempt exists yet"
    EXPECT_COUNT 1
    REJECT "only meaningful inside a retry" "no matching")
formula_add_negative_test(retry_start_not_starting_value
    "formula: a retry starts from starting_from(expression), or from nothing; this starting value is neither"
    REJECT "no matching" "does not compute the dimension")
formula_add_negative_test(retry_empty_verdict
    "formula_retry_verdict_must_say_something"
    REJECT "static assertion failed")
formula_add_negative_test(retry_in_checked_evaluate
    "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula"
    REJECT "no matching")
formula_add_negative_test(retry_in_evaluate
    "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula"
    REJECT "no matching")
formula_add_negative_test(retry_as_variant
    "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula"
    REJECT "no matching" "RequireVariantsAgree" "RequireRoundingRuleMeasuresVariants")
formula_add_negative_test(retry_in_arithmetic
    "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula"
    REJECT "no matching" "invalid operands" "no match for" "RequireResultDimension")
formula_add_negative_test(retry_render_other_quantity
    "formula: previous_attempt and this_attempt name the retry's own result quantity; this one names another"
    EXPECT_COUNT 1
    REJECT "template arguments of RequireRetriedQuantity" "does not compute the dimension")
# Only this library's node kinds can claim to be refused already.
formula_add_negative_test(consumer_refused_member
    "formula: the two sides of this addition or subtraction measure different dimensions"
    EXPECT_COUNT 1
    REJECT "no matching")
formula_add_negative_test(retry_render_context_in_starting_value
    "formula: a retry's starting value is evaluated before its first attempt, so no attempt exists yet"
    EXPECT_COUNT 1
    REJECT "names another" "only meaningful inside a retry" "does not compute the dimension")
formula_add_negative_test(retry_render_this_attempt_in_attempt
    "formula: this_attempt is the value an attempt produced, known only when judging it"
    EXPECT_COUNT 1
    REJECT "only meaningful inside a retry" "names another" "does not compute the dimension")
formula_add_negative_test(retry_previous_in_record_scope
    "inside from_record<Role>(...) the other record's data is read, where no attempt runs"
    EXPECT_COUNT 1
    REJECT "no matching")
formula_add_negative_test(retry_attempt_input_in_record_scope
    "inside from_record<Role>(...) the other record's data is read, where no attempt runs"
    EXPECT_COUNT 1
    REJECT "no matching")
formula_add_negative_test(retry_compared
    "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula"
    EXPECT_COUNT 1
    REJECT "no match for" "invalid operands" "no matching")
formula_add_negative_test(retry_attempt_input_length
    "this environment holds a series of a different length for this quantity than the one read"
    EXPECT_COUNT 1
    REJECT "provides no value for this quantity" "no matching")
formula_add_negative_test(retry_attempt_input_outside
    "formula: attempt_input reads the determination recorded for the attempt that is running"
    REJECT "provides no value for this quantity" "no matching")
formula_add_negative_test(retry_attempt_input_in_starting_value
    "formula: a retry's starting value is evaluated before its first attempt, so there is no recorded determination"
    REJECT "only meaningful inside a retry" "provides no value for this quantity" "no matching")
formula_add_negative_test(retry_attempt_input_documented_outside
    "formula: attempt_input reads the determination recorded for the attempt that is running"
    EXPECT_COUNT 1
    REJECT "provides no value for this quantity" "no matching")

# A definition, refused where it is written.
formula_add_negative_test(calculation_definition_dimension_mismatch
    "formula: this definition's expression measures a different dimension from the quantity it defines"
    EXPECT_COUNT 1
    REJECT "formula: this definition holds a node kind the calculation cannot see inside" "no matching")
formula_add_negative_test(calculation_definition_of_series
    "formula: define<Q> takes an expression of one value, and this is a series"
    EXPECT_COUNT 1
    REJECT "formula: this definition's expression measures a different dimension from the quantity it defines"
           "formula: a calculation holds single values, and this definition reads a quantity as a series"
           "no matching")
formula_add_negative_test(calculation_definition_reads_series
    "formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations"
    EXPECT_COUNT 1
    REJECT "formula: this definition holds a node kind the calculation cannot see inside"
           "has no detail::LevelChildren specialisation"
           "formula: this definition's expression measures a different dimension from the quantity it defines"
           "no matching")
formula_add_negative_test(calculation_definition_reads_observations
    "formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations"
    EXPECT_COUNT 1
    REJECT "formula: this definition holds a node kind the calculation cannot see inside"
           "has no detail::LevelChildren specialisation" "no matching")
formula_add_negative_test(calculation_definition_reads_record
    "formula: a calculation's definitions read the worksheet's own values, and this one reads from another record with from_record"
    EXPECT_COUNT 1
    REJECT "formula: this definition holds a node kind the calculation cannot see inside"
           "has no detail::LevelChildren specialisation" "no matching")
formula_add_negative_test(calculation_definition_unseen_node
    "formula: this definition holds a node kind the calculation cannot see inside"
    EXPECT_COUNT 1
    REJECT "has no detail::LevelChildren specialisation"
           "formula: this definition's expression measures a different dimension from the quantity it defines"
           "no matching")
formula_add_negative_test(calculation_library_kind_without_entry
    "formula: this library node kind has no detail::LevelChildren specialisation"
    EXPECT_COUNT 1
    REJECT "formula: this definition holds a node kind the calculation cannot see inside")

# A calculation, refused where it is written, and its queries.
formula_add_negative_test(calculation_argument_not_a_definition
    "formula: calculation(...) takes only definitions made by define<Q>(expression)"
    EXPECT_COUNT 1
    REJECT "formula: a calculation defines at least one quantity"
           "formula: this calculation neither defines nor reads this quantity"
           "no type named" "is not a member" "no matching")
formula_add_negative_test(calculation_of_nothing
    "formula: a calculation defines at least one quantity; calculation() with no definitions calculates nothing"
    EXPECT_COUNT 1
    REJECT "formula: calculation(...) takes only definitions made by define<Q>(expression)" "no matching")
formula_add_negative_test(calculation_defines_twice
    "formula: this calculation defines the same quantity more than once"
    EXPECT_COUNT 1
    REJECT "formula: this definition reads the quantity it defines"
           "formula: these definitions read one another in a cycle"
           "formula: a calculation holds at most 64 quantities"
           "formula: this calculation neither defines nor reads this quantity")
formula_add_negative_test(calculation_self_reference
    "formula: this definition reads the quantity it defines, so it can never be calculated"
    EXPECT_COUNT 1
    REJECT "formula: these definitions read one another in a cycle"
           "formula: this calculation neither defines nor reads this quantity")
formula_add_negative_test(calculation_cycle
    "formula: these definitions read one another in a cycle, so none of them can be calculated first"
    EXPECT_COUNT 1
    REJECT "formula: this definition reads the quantity it defines"
           "formula: this calculation neither defines nor reads this quantity")
formula_add_negative_test(calculation_over_capacity
    "formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more"
    EXPECT_COUNT 1
    REJECT "formula: these definitions read one another in a cycle"
           "formula: this calculation neither defines nor reads this quantity" "shift")
formula_add_negative_test(calculation_query_unknown_quantity
    "formula: this calculation neither defines nor reads this quantity"
    EXPECT_COUNT 1
    REJECT "no matching")
formula_add_negative_test(calculation_defines_twice_reading_itself
    "formula: this calculation defines the same quantity more than once"
    EXPECT_COUNT 1
    REJECT "formula: this definition reads the quantity it defines"
           "formula: these definitions read one another in a cycle")
formula_add_negative_test(calculation_over_capacity_reading_itself
    "formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more"
    EXPECT_COUNT 1
    REJECT "formula: this definition reads the quantity it defines"
           "formula: these definitions read one another in a cycle")
formula_add_negative_test(calculation_reads_itself_on_a_cycle
    "formula: this definition reads the quantity it defines, so it can never be calculated"
    EXPECT_COUNT 1
    REJECT "formula: these definitions read one another in a cycle")
formula_add_negative_test(calculation_query_after_refused_definition
    "formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations"
    EXPECT_COUNT 1
    REJECT "formula: this calculation neither defines nor reads this quantity"
           "formula: these definitions read one another in a cycle")

# A worksheet, refused where it is made, set or asked.
formula_add_negative_test(worksheet_input_missing
    "formula: this worksheet's environment provides no value for an input of its calculation"
    EXPECT_COUNT 1
    REJECT "formula: this environment provides no value for this quantity" "no matching")
formula_add_negative_test(worksheet_entry_unread
    "formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment provides no value for an input of its calculation" "no matching")
formula_add_negative_test(worksheet_entry_misnamed
    "formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment provides no value for an input of its calculation" "no matching")
formula_add_negative_test(worksheet_calculated_as_measurement
    "formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines"
           "formula: this worksheet's environment provides no value for an input of its calculation"
           "no matching")
formula_add_negative_test(worksheet_set_calculated_as_measurement
    "formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement"
    EXPECT_COUNT 1
    REJECT "formula: set() names a quantity this worksheet's calculation neither reads nor defines" "no matching")
formula_add_negative_test(worksheet_series_entry
    "formula: a worksheet holds single values, and this entry is a series or raw observations"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment provides no value for an input of its calculation"
           "holds a series for this quantity, not a single value"
           "no matching")
formula_add_negative_test(worksheet_set_series
    "formula: a worksheet holds single values, and this entry is a series or raw observations"
    EXPECT_COUNT 1
    REJECT "formula: set() names a quantity this worksheet's calculation neither reads nor defines"
           "holds a series for this quantity, not a single value"
           "to non-scalar type" "no viable conversion" "cannot convert from"
           "no matching")
formula_add_negative_test(worksheet_set_twice
    "formula: set() was given the same quantity more than once"
    EXPECT_COUNT 1
    REJECT "formula: set() names a quantity this worksheet's calculation neither reads nor defines" "tuple index"
           "no matching")
formula_add_negative_test(worksheet_set_unknown
    "formula: set() names a quantity this worksheet's calculation neither reads nor defines"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines"
           "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
           "tuple index" "no matching")
formula_add_negative_test(worksheet_result_unknown
    "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
    EXPECT_COUNT 1
    REJECT "formula: this calculation neither defines nor reads this quantity"
           "formula: clear_override or is_overridden names an input of the calculation"
           "no matching")
formula_add_negative_test(worksheet_is_overridden_unknown
    "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
    EXPECT_COUNT 1
    REJECT "formula: this calculation neither defines nor reads this quantity"
           "formula: clear_override or is_overridden names an input of the calculation"
           "no matching")
formula_add_negative_test(worksheet_clear_override_unknown
    "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
    EXPECT_COUNT 1
    REJECT "formula: this calculation neither defines nor reads this quantity"
           "formula: clear_override or is_overridden names an input of the calculation"
           "no matching")
formula_add_negative_test(worksheet_is_overridden_input
    "formula: clear_override or is_overridden names an input of the calculation"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's calculation neither defines nor reads the quantity asked for" "no matching")
formula_add_negative_test(worksheet_clear_input_override
    "formula: clear_override or is_overridden names an input of the calculation"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's calculation neither defines nor reads the quantity asked for" "no matching")
formula_add_negative_test(worksheet_of_refused_calculation
    "formula: these definitions read one another in a cycle, so none of them can be calculated first"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet's environment"
           "formula: set() names a quantity this worksheet's calculation neither reads nor defines"
           "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
           "formula: clear_override or is_overridden names an input of the calculation"
           "no matching")
formula_add_negative_test(worksheet_explain_unknown
    "formula: this worksheet's calculation neither defines nor reads the quantity asked for"
    EXPECT_COUNT 1
    REJECT "formula: this calculation neither defines nor reads this quantity" "no matching")
formula_add_negative_test(worksheet_view_missed_read
    "formula: this environment provides no value for this quantity"
    EXPECT_COUNT 1
    REJECT "formula: this worksheet" "no matching")
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

# Every symbol render(), document() and the trace write goes through the
# vocabulary they were given -- see the script for why the trace is the case
# that needs a check.
# `consumer_globals_tests.cpp` guards the headers it includes against hiding
# a consumer's global (cl C4459); this fails when a public header is missing
# from its include list.
add_test(NAME hygiene.consumer-globals
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckConsumerGlobalsIncludes.cmake")

add_test(NAME hygiene.vocabulary-reach
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckVocabularyReach.cmake")

add_test(NAME hygiene.changelog
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckChangelog.cmake")

add_test(NAME hygiene.no-real-standards
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckNoRealStandards.cmake")

# The same check on a tree whose .git names a git directory git cannot read,
# as a worktree made by git for Windows does when read from WSL: scanned, not
# refused. See the script for the fixture.
add_test(NAME hygiene.no-real-standards-fallback
    COMMAND "${CMAKE_COMMAND}"
            -D "CHECK_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/CheckNoRealStandards.cmake"
            -D "WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/no-real-standards-fallback"
            -P "${PROJECT_SOURCE_DIR}/cmake/TestNoRealStandardsFallback.cmake")

# A compiler diagnostic quoted in docs/ names a header and a line number, and
# the number goes stale the moment anyone inserts a line above it -- which
# happened twice in this repository before anything checked. See the script's
# own comment for both instances and for what it does and does not verify.
add_test(NAME hygiene.documented-diagnostics
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckDocumentedDiagnostics.cmake")

# The words of a library refusal quoted in docs/, which the check above never
# reads: each must still be a message some header states, verbatim.
add_test(NAME hygiene.documented-diagnostic-text
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckDocumentedDiagnosticText.cmake")
