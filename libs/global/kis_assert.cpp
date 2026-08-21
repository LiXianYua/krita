/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_assert.h"

#include <PkString.h>

#include <KisUsageLogger.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "config-safe-asserts.h"

/**
 * TODO: Add automatic saving of the documents
 *
 * Requirements:
 * 1) Should save all open KisDocument objects
 * 2) Should *not* overwrite original document since the saving
 *    process may fail.
 * 3) Should *not* overwrite any autosaved documents since the saving
 *    process may fail.
 * 4) Double-fault tolerance! Assert during emergency saving should not
 *    lead to an infinite loop.
 */

void kis_assert_common(const char *assertion, const char *file, int line, bool fatal, bool isIgnorable)
{
    PkString shortMessage =
        PkString("%4ASSERT (krita): \"%1\" in file %2, line %3")
        .arg(assertion)
        .arg(file)
        .arg(line)
        .arg(isIgnorable ? "SAFE " : "");

    KisUsageLogger::log(shortMessage);

    const char *const noAssertMessage = std::getenv("KRITA_NO_ASSERT_MSG");
    const bool suppressAssertMessage =
        noAssertMessage && std::strtol(noAssertMessage, nullptr, 10) != 0;
    bool forceCrashOnSafeAsserts = false;

#ifdef CRASH_ON_SAFE_ASSERTS
    forceCrashOnSafeAsserts |= CRASH_ON_SAFE_ASSERTS;
#endif

    if (!suppressAssertMessage) {
        const std::string utf8Message = shortMessage.PkToUtf8();
        std::fprintf(stderr, "%s\n", utf8Message.c_str());
        std::fflush(stderr);
    }

    if (fatal || !isIgnorable || forceCrashOnSafeAsserts) {
        std::abort();
    }
}

void kis_assert_recoverable(const char *assertion, const char *file, int line)
{
    kis_assert_common(assertion, file, line, false, false);
}

void kis_safe_assert_recoverable(const char *assertion, const char *file, int line)
{
    kis_assert_common(assertion, file, line, false, true);
}

void kis_assert_exception(const char *assertion, const char *file, int line)
{
    kis_assert_common(assertion, file, line, true, false);
}

void kis_assert_x_exception(const char *assertion,
                            const char *where,
                            const char *what,
                            const char *file, int line)
{
    PkString res =
        PkString("ASSERT failure in %1: \"%2\" (%3)")
        .arg(where, what, assertion);

    const std::string utf8Message = res.PkToUtf8();
    kis_assert_common(utf8Message.c_str(), file, line, true, false);
}
