# SPDX-License-Identifier: Apache-2.0
# Every surface that writes a quantity's symbol writes it through the
# vocabulary it was given (`vocabulary.hpp`), never straight off `Describe`.
#
# A symbol read directly is a defect no page shows: the page is rendered in
# the jurisdiction's words and the one surface that read `Describe` is not.
# The trace is the case that made this a check rather than a habit -- its
# symbols are written by the sink at evaluation time, so a renderer threaded
# with the vocabulary says nothing about them -- and a new step kind that
# names a quantity copies the old line as naturally as not.
#
# **This is a tripwire for the ordinary spellings, not the guarantee.** The
# guarantee is behavioural: `test/vocabulary_tests.cpp` puts every node kind
# through `render`, `document` and the trace under a crossed-over vocabulary,
# and `render.hpp` refuses a one-argument `render_node` of this library's at
# compile time. What this script refuses, outside a whole-line comment, in
# the five headers below:
#
#  - `::symbol` as a whole name, which is how `Describe<Q>::symbol` (at any
#    nesting of template arguments), `N::quantity::symbol` (a CRTP quantity's
#    inherited member) and an alias of `Describe` all spell it;
#  - `quantity_symbol(`, `Measured`'s accessor for the same text;
#  - `DefaultVocabulary {`, outside the public overloads that forward to
#    their vocabulary-taking counterparts -- a surface that resolves through
#    the default vocabulary has thrown away the one it was given;
#  - a one-argument `render<...>(x)` call, outside the public plain-text
#    overloads that forward to their dialect counterparts -- a
#    sub-expression rendered that way is in the declared symbols;
#  - `symbol_of<...>()` with no argument, which reads `Describe<Q>::symbol`
#    through the default vocabulary and ignores the one the surface was given
#    (angle brackets and one level of parentheses inside the template argument
#    are matched, as in `Wrapper<Q>` and `decltype(x)`; deeper parentheses, as
#    in `decltype(f(x))`, are not caught, and nor is a template argument split
#    across lines, since the match stops at a line's end);
#  - a two-argument `render<...>(x, renderOptions)` or
#    `document<...>(x, renderOptions)` call, outside the public overloads that
#    forward `RenderOptions` -- it names no vocabulary, so it resolves the
#    default one.
#
# What it cannot see: a spelling none of these match. That is why it is not
# the guarantee.

set(surfaces
    "${SOURCE_DIR}/include/formula-cpp/calculation.hpp"
    "${SOURCE_DIR}/include/formula-cpp/render.hpp"
    "${SOURCE_DIR}/include/formula-cpp/document.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace_render.hpp")

set(offenders "")
foreach(file IN LISTS surfaces)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR
            "vocabulary check: ${file} does not exist. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
            "header moved. A check that examines nothing is a check that lies.")
    endif()
    file(READ "${file}" contents)
    file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")

    # Whole-line comments: the doc comments in these headers name
    # `Describe<Q>::symbol` and one-argument calls freely.
    string(REGEX REPLACE "\n[ \t]*//[^\n]*" "\n" code "${contents}")
    # The public forwarding overloads, each exactly one line of this shape --
    # the ones that take `RenderOptions` pass them on.
    string(REGEX REPLACE "\n[ \t]*return (render|document)<[A-Za-z:]+>[(]node, DefaultVocabulary {}(, renderOptions)?[)];" "\n" code
                         "${code}")
    string(REGEX REPLACE "\n[ \t]*return render<Dialect::Plain>[(]node[)];" "\n" code "${code}")

    if(code MATCHES "::symbol[^_A-Za-z0-9]")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- a symbol read past the vocabulary")
    endif()
    if(code MATCHES "quantity_symbol[(]")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- a symbol read past the vocabulary")
    endif()
    if(code MATCHES "DefaultVocabulary *{")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- the given vocabulary thrown away")
    endif()
    if(code MATCHES "[^_A-Za-z0-9]render<[^<>()]*>[(][^,()]*[)]")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- rendered without the vocabulary")
    endif()
    if(code MATCHES "symbol_of<([^();{}\n]|[(][^();{}\n]*[)])*>[(][ \t]*[)]")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- a symbol read through the default vocabulary")
    endif()
    if(code MATCHES "[^_A-Za-z0-9](render|document)(<[^<>()]*>)?[(][^,()]*, renderOptions[)]")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0} -- rendered without naming the vocabulary")
    endif()
endforeach()

if(offenders)
    message(FATAL_ERROR
        "a symbol is written without the vocabulary the surface was given -- resolve it with "
        "symbol_of<Q>(vocabulary), and render a sub-expression with render<D>(x, vocabulary):${offenders}")
endif()
