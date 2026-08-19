/*
 *  SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisUsageLogger.h"

#include <PkDebug.h>
#include <PkDateTime.h>

#include <PkThread.h>

#include <KritaVersionWrapper.h>

#ifdef Q_OS_WIN
#include "KisWindowsPackageUtils.h"
#include <windows.h>
#include <versionhelpers.h>
#endif

#ifdef Q_OS_ANDROID
#include <KisAndroidExitInfo.h>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtAndroidExtras/QtAndroid>
#endif
#endif

#ifdef Q_OS_MACOS
#include "KisMacosEntitlements.h"
#endif

#include <clocale>

static KisUsageLogger *s_instance()
{
    static KisUsageLogger instance;
    return &instance;
}

const PkString KisUsageLogger::s_sectionHeader("================================================================================\n");

struct KisUsageLogger::Private {
    bool active {false};
    PkFile logFile;
    PkFile sysInfoFile;
};

KisUsageLogger::KisUsageLogger()
    : d(new Private)
{
    if (!PkFileInfo(PkStandardPaths::writableLocation(PkStandardPaths::GenericDataLocation)).exists()) {
        PkDir().mkpath(PkStandardPaths::writableLocation(PkStandardPaths::GenericDataLocation));
    }
    d->logFile.setFileName(PkStandardPaths::writableLocation(PkStandardPaths::GenericDataLocation) + "/krita.log");
    d->sysInfoFile.setFileName(PkStandardPaths::writableLocation(PkStandardPaths::GenericDataLocation) + "/krita-sysinfo.log");

    PkFileInfo fi(d->logFile.fileName());
    if (fi.size() > 100 * 1000 * 1000) { // 100 mb seems a reasonable max
        if (d->logFile.open(PkStream::WriteOnly | PkStream::Truncate)) {
            d->logFile.close();
        } else {
            qWarning() << "Could not clear the >100MB" << d->logFile.fileName() << ":" << d->logFile.errorString();
        }
    }
    else {
        rotateLog();
    }

    if (!d->logFile.open(PkFile::Append | PkFile::Text)) {
        qWarning() << "Could not open" << d->logFile.fileName() << "for writing:" << d->logFile.errorString();
    }
    if (!d->sysInfoFile.open(PkFile::WriteOnly | PkFile::Text)) {
        qWarning() << "Could not open" << d->sysInfoFile.fileName() << "for writing:" << d->sysInfoFile.errorString();
    }
}

KisUsageLogger::~KisUsageLogger()
{
    if (d->active) {
        close();
    }
}

void KisUsageLogger::initialize()
{
    s_instance()->d->active = true;

    PkString systemInfo = basicSystemInfo();
    s_instance()->d->sysInfoFile.write(systemInfo.toUtf8());
}

PkString KisUsageLogger::basicSystemInfo()
{
    PkString systemInfo;

    // NOTE: This is intentionally not translated!

    // Krita version info
    systemInfo.append("Krita\n");
    systemInfo.append("\n Version: ").append(KritaVersionWrapper::versionString(true));
#ifdef Q_OS_WIN
    {
        using namespace KisWindowsPackageUtils;
        PkString packageFamilyName;
        PkString packageFullName;
        systemInfo.append("\n Installation type: ");
        if (tryGetCurrentPackageFamilyName(&packageFamilyName) && tryGetCurrentPackageFullName(&packageFullName)) {
            systemInfo.append("Store / MSIX package\n    Family Name: ")
                .append(packageFamilyName)
                .append("\n    Full Name: ")
                .append(packageFullName);
        } else {
            systemInfo.append("installer / portable package");
        }
    }
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Attribute does nothing on Qt6
    systemInfo.append("\n Hidpi: ").append(PkCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling) ? "true" : "false");
#endif
#ifdef Q_OS_MACOS
    KisMacosEntitlements entitlements;
    systemInfo.append("\n Sandbox: ").append((entitlements.sandbox()) ? "true" : "false");
#endif
    systemInfo.append("\n\n");

    systemInfo.append("Qt\n");
    systemInfo.append("\n  Version (compiled): ").append(QT_VERSION_STR);
    systemInfo.append("\n  Version (loaded): ").append(qVersion());
    systemInfo.append("\n\n");

    // OS information
    systemInfo.append("OS Information\n");
    systemInfo.append("\n  Build ABI: ").append(PkSysInfo::buildAbi());
    systemInfo.append("\n  Build CPU: ").append(PkSysInfo::buildCpuArchitecture());
    systemInfo.append("\n  CPU: ").append(PkSysInfo::currentCpuArchitecture());
    systemInfo.append("\n  Kernel Type: ").append(PkSysInfo::kernelType());
    systemInfo.append("\n  Kernel Version: ").append(PkSysInfo::kernelVersion());
    systemInfo.append("\n  Pretty Productname: ").append(PkSysInfo::prettyProductName());
    systemInfo.append("\n  Product Type: ").append(PkSysInfo::productType());
    systemInfo.append("\n  Product Version: ").append(PkSysInfo::productVersion());

#ifdef Q_OS_ANDROID
    PkString manufacturer =
        PkAndroidJniObject::getStaticObjectField("android/os/Build", "MANUFACTURER", "Ljava/lang/String;").toString();
    const PkString model =
        PkAndroidJniObject::getStaticObjectField("android/os/Build", "MODEL", "Ljava/lang/String;").toString();
    manufacturer[0] = manufacturer[0].toUpper();
    systemInfo.append("\n  Product Model: ").append(manufacturer + " " + model);
#elif defined(Q_OS_LINUX)
    systemInfo.append("\n  Desktop: ").append(qgetenv("XDG_CURRENT_DESKTOP"));

    systemInfo.append("\n  Appimage build: ").append(qEnvironmentVariableIsSet("APPIMAGE") ? "Yes" : "No");
#elif defined(Q_OS_WIN)
    systemInfo.append("\n  Result of IsWindows10OrGreater(): ").append(IsWindows10OrGreater() ? "Yes" : "No");
#endif
    systemInfo.append("\n\n");

    return systemInfo;
}

void KisUsageLogger::writeLocaleSysInfo()
{
    if (!s_instance()->d->active) {
        return;
    }
    PkString systemInfo;
    systemInfo.append("Locale\n");
    systemInfo.append("\n  Languages: ").append(KLocalizedString::languages().join(", "));
    systemInfo.append("\n  C locale: ").append(std::setlocale(LC_ALL, nullptr));
    systemInfo.append("\n  PkLocale current: ").append(PkLocale().bcp47Name());
    systemInfo.append("\n  PkLocale system: ").append(PkLocale::system().bcp47Name());
    const PkTextCodec *codecForLocale = PkTextCodec::codecForLocale();
    systemInfo.append("\n  PkTextCodec for locale: ").append(codecForLocale->name());
#ifdef Q_OS_WIN
    {
        systemInfo.append("\n  Process ACP: ");
        CPINFOEXW cpInfo {};
        if (GetCPInfoExW(CP_ACP, 0, &cpInfo)) {
            systemInfo.append(PkString::fromWCharArray(cpInfo.CodePageName));
        } else {
            // Shouldn't happen, but just in case
            systemInfo.append(PkString::number(GetACP()));
        }
        wchar_t lcData[2];
        int result = GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, lcData, sizeof(lcData) / sizeof(lcData[0]));
        if (result == 2) {
            systemInfo.append("\n  System locale default ACP: ");
            int systemACP = lcData[1] << 16 | lcData[0];
            if (systemACP == CP_ACP) {
                systemInfo.append("N/A");
            } else if (GetCPInfoExW(systemACP, 0, &cpInfo)) {
                systemInfo.append(PkString::fromWCharArray(cpInfo.CodePageName));
            } else {
                // Shouldn't happen, but just in case
                systemInfo.append(PkString::number(systemACP));
            }
        }
    }
#endif
    systemInfo.append("\n\n");
    s_instance()->d->sysInfoFile.write(systemInfo.toUtf8());
}

void KisUsageLogger::close()
{
    log("CLOSING SESSION");
    s_instance()->d->active = false;
    s_instance()->d->logFile.flush();
    s_instance()->d->logFile.close();
    s_instance()->d->sysInfoFile.flush();
    s_instance()->d->sysInfoFile.close();
}

void KisUsageLogger::log(const PkString &message)
{
    if (!s_instance()->d->active) return;
    if (!s_instance()->d->logFile.isOpen()) return;

    s_instance()->d->logFile.write(PkDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8());
    s_instance()->d->logFile.write(": ");
    write(message);
}

void KisUsageLogger::write(const PkString &message)
{
    if (!s_instance()->d->active) return;
    if (!s_instance()->d->logFile.isOpen()) return;

    s_instance()->d->logFile.write(message.toUtf8());
    s_instance()->d->logFile.write("\n");

    s_instance()->d->logFile.flush();
}

void KisUsageLogger::writeSysInfo(const PkString &message)
{
    if (!s_instance()->d->active) return;
    if (!s_instance()->d->sysInfoFile.isOpen()) return;

    s_instance()->d->sysInfoFile.write(message.toUtf8());
    s_instance()->d->sysInfoFile.write("\n");

    s_instance()->d->sysInfoFile.flush();

}

void KisUsageLogger::writeHeader()
{
    Q_ASSERT(s_instance()->d->sysInfoFile.isOpen());
    s_instance()->d->logFile.write(s_sectionHeader.toUtf8());

    PkString sessionHeader = PkString("SESSION: %1. Executing %2\n\n")
            .arg(PkDateTime::currentDateTime().toString(Qt::RFC2822Date))
            .arg(qApp->arguments().join(' '));

    s_instance()->d->logFile.write(sessionHeader.toUtf8());

    PkString KritaAndQtVersion;
    KritaAndQtVersion.append("Krita Version: ").append(KritaVersionWrapper::versionString(true))
            .append(", Qt version compiled: ").append(QT_VERSION_STR)
            .append(", loaded: ").append(qVersion())
            .append(". Process ID: ")
            .append(PkString::number(qApp->applicationPid())).append("\n");

    KritaAndQtVersion.append("-- -- -- -- -- -- -- --\n");
    s_instance()->d->logFile.write(KritaAndQtVersion.toUtf8());
    s_instance()->d->logFile.flush();
    log(PkString("Style: %1. Available styles: %2")
        .arg(qApp->style()->objectName(),
             PkStyleFactory::keys().join(", ")));

#ifdef Q_OS_ANDROID
    KisAndroidExitInfo androidExitInfo = KisAndroidExitInfo::getLast();
    if (androidExitInfo.isValid()) {
        log(PkString("Last exit: %1").arg(androidExitInfo.buildLogString()));
    }
#endif
}

PkString KisUsageLogger::screenInformation()
{
    PkList<PkScreen*> screens = qApp->screens();

    PkString info;
    info.append("Display Information");
    info.append("\nNumber of screens: ").append(PkString::number(screens.size()));

    for (int i = 0; i < screens.size(); ++i ) {
        PkScreen *screen = screens[i];
        info.append("\n\tScreen: ").append(PkString::number(i));
        info.append("\n\t\tName: ").append(screen->name());
        info.append("\n\t\tDepth: ").append(PkString::number(screen->depth()));
        info.append("\n\t\tScale: ").append(PkString::number(screen->devicePixelRatio()));
        info.append("\n\t\tPhysical DPI").append(PkString::number(screen->physicalDotsPerInch()));
        info.append("\n\t\tLogical DPI").append(PkString::number(screen->logicalDotsPerInch()));
        info.append("\n\t\tPhysical Size: ").append(PkString::number(screen->physicalSize().width()))
                .append(", ")
                .append(PkString::number(screen->physicalSize().height()));
        info.append("\n\t\tPosition: ").append(PkString::number(screen->geometry().x()))
                .append(", ")
                .append(PkString::number(screen->geometry().y()));
        info.append("\n\t\tResolution in pixels: ").append(PkString::number(screen->geometry().width()))
                .append("x")
                .append(PkString::number(screen->geometry().height()));
        info.append("\n\t\tManufacturer: ").append(screen->manufacturer());
        info.append("\n\t\tModel: ").append(screen->model());
        info.append("\n\t\tRefresh Rate: ").append(PkString::number(screen->refreshRate()));
        info.append("\n\t\tSerial Number: ").append(screen->serialNumber());

    }
    info.append("\n");
    return info;
}

void KisUsageLogger::rotateLog()
{
    if (!d->logFile.exists()) { return; }

    // Check for CLOSING SESSION
    if (d->logFile.open(PkFile::ReadOnly)) {
        PkString log = PkString::fromUtf8(d->logFile.readAll());
        if (!log.split(s_sectionHeader).last().contains("CLOSING SESSION")) {
            log.append("\nKRITA DID NOT CLOSE CORRECTLY\n");
            PkString crashLog = PkStandardPaths::writableLocation(PkStandardPaths::GenericConfigLocation) + PkString("/kritacrash.log");
            PkFile f(crashLog);
            if (f.open(PkFile::ReadOnly)) {
                PkString crashes = PkString::fromUtf8(f.readAll());
                f.close();

                PkStringList crashlist = crashes.split("-------------------");
                log.append(PkString("\nThere were %1 crashes in total in the crash log.\n").arg(crashlist.size()));

                if (crashes.size() > 0) {
                    log.append(crashlist.last());
                }
            }
            d->logFile.close();
            if (d->logFile.open(PkFile::WriteOnly)) {
                d->logFile.write(log.toUtf8());
            }
        }
        d->logFile.flush();
        d->logFile.close();
    }

    // Rotate
    if (d->logFile.open(PkFile::ReadOnly)) {
        PkString log = PkString::fromUtf8(d->logFile.readAll());
        d->logFile.close();
        PkStringList logItems = log.split("SESSION:");
        PkStringList keptItems;
        int sectionCount = logItems.size();
        if (sectionCount > s_maxLogs) {
            for (int i = sectionCount - s_maxLogs; i < sectionCount; ++i) {
                if (logItems.size() > i ) {
                    keptItems.append(logItems[i]);
                }
            }

            if (d->logFile.open(PkFile::WriteOnly)) {
                d->logFile.write(keptItems.join("\nSESSION:").toUtf8());
                d->logFile.flush();
                d->logFile.close();
            }
        }
    }
}

