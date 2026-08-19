/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "KisAndroidUtils.h"
#include "KisAndroidExitInfo.h"

#include <QtAndroid>

KisAndroidExitInfo KisAndroidExitInfo::getLast()
{
    PkAndroidJniEnvironment env;
    PkAndroidJniObject activity = PkAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity",
                                                                           "()Landroid/app/Activity;");
    if (env->ExceptionCheck()) {
        qWarning("KisAndroidExitInfo::getLast: JNI exception in activity");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return KisAndroidExitInfo();
    } else if (!activity.isValid()) {
        qWarning("KisAndroidExitInfo::getLast: activity not valid");
        return KisAndroidExitInfo();
    }

    PkAndroidJniObject exitInfo =
        activity.callObjectMethod("getLastApplicationExitInfo", "()Landroid/app/ApplicationExitInfo;");
    if (env->ExceptionCheck()) {
        qWarning("KisAndroidExitInfo::getLast: JNI exception in getLastApplicationExitInfo");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return KisAndroidExitInfo();
    } else if (!exitInfo.isValid()) {
        qWarning("KisAndroidExitInfo::getLast: exit info not valid");
        return KisAndroidExitInfo();
    }

    int reasonCode = exitInfo.callMethod<jint>("getReason", "()I");
    if (env->ExceptionCheck()) {
        qWarning("KisAndroidExitInfo::getLast: JNI exception in getReason");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return KisAndroidExitInfo();
    }

    int exitOrSignalCode = exitInfo.callMethod<jint>("getStatus", "()I");
    if (env->ExceptionCheck()) {
        qWarning("KisAndroidExitInfo::getLast: JNI exception in getStatus");
        env->ExceptionDescribe();
        env->ExceptionClear();
        exitOrSignalCode = -1;
    }

    int importanceCode = exitInfo.callMethod<jint>("getImportance", "()I");
    if (env->ExceptionCheck()) {
        qWarning("KisAndroidExitInfo::getLast: JNI exception in getImportance");
        env->ExceptionDescribe();
        env->ExceptionClear();
        importanceCode = -1;
    }

    PkString description;
    {
        PkAndroidJniObject descriptionObject = exitInfo.callObjectMethod("getDescription", "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
            qWarning("KisAndroidExitInfo::getLast: JNI exception in getDescription");
            env->ExceptionDescribe();
            env->ExceptionClear();
        } else if (descriptionObject.isValid()) {
            description = descriptionObject.toString();
        }
    }

    return KisAndroidExitInfo(reasonCode, exitOrSignalCode, importanceCode, description);
}

KisAndroidExitInfo::KisAndroidExitInfo()
    : m_reasonCode(int(Reason::Unknown))
    , m_exitOrSignalCode(-1)
    , m_importanceCode(-1)
    , m_valid(false)
{
}

KisAndroidExitInfo::KisAndroidExitInfo(int reasonCode,
                                       int exitOrSignalCode,
                                       int importanceCode,
                                       const PkString &description)
    : m_description(description)
    , m_reasonCode(reasonCode)
    , m_exitOrSignalCode(exitOrSignalCode)
    , m_importanceCode(importanceCode)
    , m_valid(true)
{
}

PkString KisAndroidExitInfo::buildLogString() const
{
    if (!isValid()) {
        return PkString();
    }

    PkString message;

    message.append(PkString("reason %1").arg(m_reasonCode));
    switch (m_reasonCode) {
    case int(Reason::Unknown):
        message.append(PkString(" (UNKNOWN)"));
        break;
    case int(Reason::ExitSelf):
        message.append(PkString(" (EXIT_SELF)"));
        break;
    case int(Reason::Signaled):
        message.append(PkString(" (SIGNALED)"));
        break;
    case int(Reason::LowMemory):
        message.append(PkString(" (LOW_MEMORY)"));
        break;
    case int(Reason::Crash):
        message.append(PkString(" (CRASH)"));
        break;
    case int(Reason::CrashNative):
        message.append(PkString(" (CRASH_NATIVE)"));
        break;
    case int(Reason::Anr):
        message.append(PkString(" (ANR)"));
        break;
    case int(Reason::InitializationFailure):
        message.append(PkString(" (INITIALIZATION_FAILURE)"));
        break;
    case int(Reason::PermissionChange):
        message.append(PkString(" (PERMISSION_CHANGE)"));
        break;
    case int(Reason::ExcessiveResourceUsage):
        message.append(PkString(" (EXCESSIVE_RESOURCE_USAGE)"));
        break;
    case int(Reason::UserRequested):
        message.append(PkString(" (USER_REQUESTED)"));
        break;
    case int(Reason::UserStopped):
        message.append(PkString(" (USER_STOPPED)"));
        break;
    case int(Reason::DependencyDied):
        message.append(PkString(" (DEPENDENCY_DIED)"));
        break;
    case int(Reason::Other):
        message.append(PkString(" (OTHER)"));
        break;
    case int(Reason::Freezer):
        message.append(PkString(" (FREEZER)"));
        break;
    case int(Reason::PackageStateChange):
        message.append(PkString(" (PACKAGE_STATE_CHANGE)"));
        break;
    case int(Reason::PackageUpdated):
        message.append(PkString(" (PACKAGE_UPDATED)"));
        break;
    default:
        break;
    }

    message.append(PkString(", importance %1").arg(m_importanceCode));
    switch (m_importanceCode) {
    case int(Importance::Foreground):
        message.append(PkString(" (FOREGROUND)"));
        break;
    case int(Importance::ForegroundService):
        message.append(PkString(" (FOREGROUND_SERVICE)"));
        break;
    case int(Importance::PerceptiblePre26):
        message.append(PkString(" (PERCEPTIBLE_PRE_26)"));
        break;
    case int(Importance::TopSleepingPre28):
        message.append(PkString(" (TOP_SLEEPING_PRE_28)"));
        break;
    case int(Importance::Visible):
        message.append(PkString(" (VISIBLE)"));
        break;
    case int(Importance::Perceptible):
        message.append(PkString(" (PERCEPTIBLE)"));
        break;
    case int(Importance::Service):
        message.append(PkString(" (SERVICE)"));
        break;
    case int(Importance::TopSleeping):
        message.append(PkString(" (TOP_SLEEPING)"));
        break;
    case int(Importance::CantSaveState):
        message.append(PkString(" (CANT_SAVE_STATE)"));
        break;
    case int(Importance::Cached):
        message.append(PkString(" (CACHED)"));
        break;
    case int(Importance::Empty):
        message.append(PkString(" (EMPTY)"));
        break;
    case int(Importance::Gone):
        message.append(PkString(" (GONE)"));
        break;
    default:
        break;
    }

    message.append(PkString(", exit code/signal %1").arg(m_exitOrSignalCode));

    message.append(PkString(", low-memory kill report "));
    if (KisAndroidUtils::isLowMemoryKillReportSupported()) {
        message.append(PkString("supported"));
    } else {
        message.append(PkString("not supported"));
    }

    if (m_description.isEmpty()) {
        message.append(PkString(", no description"));
    } else {
        message.append(PkString(", description '%1'").arg(m_description));
    }

    return message;
}
