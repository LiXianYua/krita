/*
 *  SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISUSAGELOGGER_H
#define KISUSAGELOGGER_H

#include <PkString.h>
#include <PkScopedPointer.h>

#include "kritaglobal_export.h"

/**
 * @brief The KisUsageLogger class logs messages to a logfile
 */
class KRITAGLOBAL_EXPORT KisUsageLogger
{

public:

    KisUsageLogger();
    ~KisUsageLogger();

    static void initialize();
    static void close();

    /// basic system information
    ///    (there is other information spread in the code
    ///     check usages of writeSysInfo for details)
    static PkString basicSystemInfo();

    static void writeLocaleSysInfo();

    /// Logs with date/time
    static void log(const PkString &message);

    /// Writes without date/time
    static void write(const PkString &message);

    /// Writes to the system information file and Krita log
    static void writeSysInfo(const PkString &message);

    static void writeHeader();

    /// Screen enumeration belongs to the UI; the headless core reports that
    /// display information is unavailable.
    static PkString screenInformation();

private:

    void rotateLog();

    KisUsageLogger(const KisUsageLogger &) = delete;
    KisUsageLogger &operator=(const KisUsageLogger &) = delete;

    struct Private;
    const PkScopedPointer<Private> d;

    static const PkString s_sectionHeader;
    static const int s_maxLogs {20};

};

#endif // KISUSAGELOGGER_H
