/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "KisAndroidUtils.h"
#include "KisAndroidLogHandler.h"
#include <kis_debug.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

#else

using PkJniEnvironment = PkAndroidJniEnvironment;
using PkJniObject = PkAndroidJniObject;
#endif

namespace KisAndroidUtils
{

void performInitialSetup()
{
    KisAndroidLogHandler::handler_init();

    PkJniObject activity = PkJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                             "activity",
                                                             "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("copyAssets", "()V");
    } else {
        qWarning("performInitialSetup: activity not valid");
    }
}

bool looksLikeXiaomiDevice()
{
    // The device isn't going to change, so let's cache the slow JNI call.
    static bool checked;
    static bool result;
    if (!checked) {
        checked = true;
        result =
            PkJniObject::callStaticMethod<jboolean>("org/krita/android/MainActivity", "looksLikeXiaomiDevice", "()Z");
    }
    return result;
}

bool isLowMemoryKillReportSupported()
{
    // The support is device-bound and will never change, so cache the JNI call.
    static bool checked;
    static bool result;
    if (!checked) {
        checked = true;
        result = PkJniObject::callStaticMethod<jboolean>("org/krita/android/MainActivity",
                                                        "isLowMemoryKillReportSupported",
                                                        "()Z");
    }
    return result;
}

void clearJniException(const PkString &location)
{
    PkJniEnvironment env;
    if (env->ExceptionCheck()) {
        warnKrita << "JNI exception occurred" << location;
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

bool isInFullScreen()
{
    PkJniObject activity = PkJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                             "activity",
                                                             "()Landroid/app/Activity;");
    KisAndroidUtils::clearJniException(PkString("getting activity in isInFullScreen"));
    if (activity.isValid()) {
        bool fullScreen = activity.callMethod<jboolean>("isInFullScreen", "()Z");
        KisAndroidUtils::clearJniException(PkString("calling isInFullScreen"));
        return fullScreen;
    } else {
        qWarning("isInFullScreen: activity not valid");
        return false;
    }
}

void setFullScreen(bool fullScreen)
{
    PkJniObject activity = PkJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                             "activity",
                                                             "()Landroid/app/Activity;");
    KisAndroidUtils::clearJniException(PkString("getting activity in setFullScreen"));
    if (activity.isValid()) {
        activity.callMethod<void>("setFullScreenOnUiThread", "(Z)V", jboolean(fullScreen));
        KisAndroidUtils::clearJniException(PkString("calling setFullScreenOnUiThread"));
    } else {
        qWarning("setFullScreen: activity not valid");
    }
}

} // namespace KisAndroidUtils
