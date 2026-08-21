/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_debug.h"

#include "config-debug.h"

#include <PkString.h>

#if HAVE_BACKTRACE
#include <execinfo.h>
#ifdef __GNUC__
#define HAVE_BACKTRACE_DEMANGLE
#include <cxxabi.h>
#endif
#endif

#include <string>
#include <sstream>
#include <cstring>

#if HAVE_BACKTRACE
static PkString maybeDemangledName(char *name)
{
#ifdef HAVE_BACKTRACE_DEMANGLE
    const int len = strlen(name);
    std::string in(name, len);
    const int mangledNameStart = in.find("(_");
    if (mangledNameStart >= 0) {
        const int mangledNameEnd = in.find('+', mangledNameStart + 2);
        if (mangledNameEnd >= 0) {
            int status;
            name[mangledNameEnd] = 0;
            char *demangled = abi::__cxa_demangle(name + mangledNameStart + 1, 0, 0, &status);
            name[mangledNameEnd] = '+';
            if (demangled) {
                std::string ret = std::string(name, mangledNameStart + 1) +
                                  std::string(demangled) +
                                  std::string(name + mangledNameEnd, len - mangledNameEnd);
                free(demangled);
                return PkString(ret.c_str());
            }
        }
    }
#endif
    return PkString(name);
}
#endif

PkString kisBacktrace()
{
    PkString s;
#if HAVE_BACKTRACE
    void *trace[256];
    int n = backtrace(trace, 256);
    if (!n) {
        return s;
    }
    char **strings = backtrace_symbols(trace, n);

    std::ostringstream oss;
    oss << "[\n";

    for (int i = 0; i < n; ++i)
        oss << "\t" << i << ": " << maybeDemangledName(strings[i]).PkToUtf8() << "\n";
    oss << "]\n";
    free(strings);

    s = PkString(oss.str().c_str());
#endif
    return s;
}

PK_LOGGING_CATEGORY(_30009, "krita.lib.resources", PkLogInfo)
PK_LOGGING_CATEGORY(_30010, "krita.db.migration", PkLogInfo)
PK_LOGGING_CATEGORY(_41000, "krita.general", PkLogInfo)
PK_LOGGING_CATEGORY(_41001, "krita.core", PkLogInfo)
PK_LOGGING_CATEGORY(_41002, "krita.registry", PkLogInfo)
PK_LOGGING_CATEGORY(_41003, "krita.tools", PkLogInfo)
PK_LOGGING_CATEGORY(_41004, "krita.tiles", PkLogInfo)
PK_LOGGING_CATEGORY(_41005, "krita.filters", PkLogInfo)
PK_LOGGING_CATEGORY(_41006, "krita.plugins", PkLogInfo)
PK_LOGGING_CATEGORY(_41007, "krita.ui", PkLogInfo)
PK_LOGGING_CATEGORY(_41008, "krita.file", PkLogInfo)
PK_LOGGING_CATEGORY(_41009, "krita.math", PkLogInfo)
PK_LOGGING_CATEGORY(_41010, "krita.render", PkLogInfo)
PK_LOGGING_CATEGORY(_41011, "krita.scripting", PkLogInfo)
PK_LOGGING_CATEGORY(_41012, "krita.input", PkLogInfo)
PK_LOGGING_CATEGORY(_41013, "krita.action", PkLogInfo)
PK_LOGGING_CATEGORY(_41014, "krita.tabletlog", PkLogDebug)
PK_LOGGING_CATEGORY(_41015, "krita.opengl", PkLogInfo)
PK_LOGGING_CATEGORY(_41016, "krita.metadata", PkLogInfo)
PK_LOGGING_CATEGORY(_41017, "krita.android", PkLogDebug)
PK_LOGGING_CATEGORY(_41018, "krita.locale", PkLogInfo)

PkString __methodName(const char *_prettyFunction)
{
    std::string prettyFunction(_prettyFunction);

    size_t colons = prettyFunction.find("::");
    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return PkString((prettyFunction.substr(begin,end) + "()").c_str());
}
