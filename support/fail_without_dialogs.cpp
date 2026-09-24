// SPDX-License-Identifier: Apache-2.0
//
// Makes a dying test or example die QUIETLY: a message on stderr and a non-zero
// exit code, never a modal dialog box.
//
// Linked into the test executable and every example. Not part of the library and
// not installed -- a library has no business changing a host process's error
// handling, and this file exists only so that OUR OWN executables behave when
// they fail.
//
// Why it exists: this project deliberately fails fast in several places. The
// dimension and unit sentinels call `std::abort()` on a programming error, and a
// dereference of a valueless `std::expected` trips the Microsoft STL's
// `_STL_VERIFY`, which also aborts. Under the MSVC debug runtime both raise a
// modal "Debug Error!" window. On a developer's desktop that window stops the
// world until somebody clicks it; on a CI runner nothing clicks it, so the job
// burns its entire timeout and reports nothing useful. Either way the failure is
// invisible in the one place you would look for it, which is the test output.
//
// This is not hypothetical and it is not only about mutation testing. Any
// regression that makes a `checked_` function start returning an error will hit
// the same path in a test that unwraps the result, and a suite that hangs
// instead of going red is worse than one that simply fails.
//
// The whole file is a no-op off Windows, where `abort()` raises SIGABRT and the
// shell reports it.

#if defined(_WIN32)

    #include <cstdint>
    #include <cstdio>
    #include <cstdlib>
    #include <initializer_list>

    #include <crtdbg.h>

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>

namespace
{

/// The CRT calls this instead of showing its own dialog when it is handed an
/// invalid argument -- a bad printf format, an out-of-range index in a checked
/// iterator, and so on. Writing to stderr and leaving keeps the diagnosis in the
/// test log, where somebody will actually read it.
void report_invalid_parameter(wchar_t const* expression,
                              wchar_t const* function,
                              wchar_t const* file,
                              unsigned int line,
                              std::uintptr_t reserved) noexcept
{
    (void) expression;
    (void) function;
    (void) file;
    (void) line;
    (void) reserved;
    std::fputs("formula: the C runtime rejected an invalid parameter; exiting without a dialog\n", stderr);
    std::fflush(stderr);
    // _Exit, not abort: abort would re-enter the very handling this file is
    // configuring, and there is nothing left worth unwinding.
    std::_Exit(3);
}

struct FailWithoutDialogs
{
    FailWithoutDialogs() noexcept
    {
        // Runs before main, being a namespace-scope object in a translation unit
        // linked directly into the executable. Catch2 supplies main, so there is
        // no other hook to use.

        // 1. abort() must not raise the "abnormal program termination" window,
        //    and must not hand the process to Windows Error Reporting either.
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

        // 2. Debug-CRT assertions report to stderr instead of a message box.
        //    This is the path _STL_VERIFY takes on a valueless std::expected.
        //    In a Release (NDEBUG) build, <crtdbg.h> turns _CrtSetReportMode
        //    and _CrtSetReportFile into no-op macros that discard their
        //    arguments entirely, which otherwise leaves `report` unused and
        //    trips /WX. [[maybe_unused]] covers both configurations.
        for ([[maybe_unused]] int const report: { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
        {
            _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
        }

        // 3. Invalid CRT parameters reach our handler rather than a dialog.
        _set_invalid_parameter_handler(&report_invalid_parameter);

        // 4. And the operating system's own boxes -- "program has stopped
        //    working", and the missing-disk one -- stay shut.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    }
};

FailWithoutDialogs const Installed {};

} // namespace

#endif
