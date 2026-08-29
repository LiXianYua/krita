/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_image_config.h"
#include "kis_image_config_paths.h"

#include <PkSharedConfig.h>
#include <PkScopedPointer.h>
#include <PkThread.h>

// A few not-yet-explicit image headers still carry Qt declaration macros even
// though their types are already Pk types.  Keep the compatibility local to
// this translation unit; none of these macros provide behavior.
#ifndef Q_DECLARE_METATYPE
#define Q_DECLARE_METATYPE(...)
#endif
#ifndef Q_OBJECT
#define Q_OBJECT
#endif
#ifndef Q_SIGNALS
#define Q_SIGNALS public
#endif
#ifndef Q_DISABLE_COPY
#define Q_DISABLE_COPY(Class) \
    Class(const Class &) = delete; \
    Class &operator=(const Class &) = delete;
#endif
#ifndef Q_ASSERT_X
#define Q_ASSERT_X(condition, where, what) Q_ASSERT(condition)
#endif
#ifndef Q_DECL_DEPRECATED
#define Q_DECL_DEPRECATED [[deprecated]]
#endif

#include <KoConfig.h>
#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorConversionTransformation.h>
#include <KisProofingConfiguration.h>
#include <kis_properties_configuration.h>

#include <KisImageConfigNotifier.h>
#include "kis_debug.h"

#include "kis_global.h"
#include <KisGlobalFileSystem.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef __ANDROID__
#include <KisAndroidUtils.h>
#endif

namespace {

void cleanOldImageCursorStyleKeys(PkConfigGroup config)
{
    if (config.hasKey("newCursorStyle") && config.hasKey("newOutlineStyle")) {
        config.deleteEntry("cursorStyleDef");
    }
}

PkString numberString(int value)
{
    return PkString(std::to_string(value).c_str());
}

PkString dynamicKey(const PkString &prefix, const PkString &suffix)
{
    return prefix + suffix;
}

std::filesystem::path toPath(const PkString &path)
{
    return std::filesystem::u8path(path.PkToUtf8());
}

PkString fromPath(const std::filesystem::path &path)
{
    return PkString(path.u8string().c_str());
}

PkString homeDirectory()
{
    return fromPath(KisGlobalFileSystem::detail::homePath());
}

PkString temporaryDirectory()
{
    std::error_code error;
    const std::filesystem::path path = std::filesystem::temp_directory_path(error);
    if (!error && !path.empty()) {
        return fromPath(path);
    }
    return homeDirectory();
}

struct PlatformSwapPathPolicy
{
    std::filesystem::path stableDefault;
    KisImageConfigPaths::TransientFallback transientFallback;
};

PlatformSwapPathPolicy platformSwapPathPolicy(const PkString &suffix)
{
#ifdef __APPLE__
    return {
        KisGlobalFileSystem::writableLocation(KisGlobalFileSystem::Location::AppData) /
            toPath(suffix),
        KisImageConfigPaths::TransientFallback::Reject,
    };
#else
    (void)suffix;
    return {std::filesystem::path(), KisImageConfigPaths::TransientFallback::Allow};
#endif
}

}

KisImageConfig::KisImageConfig(bool readOnly)
    : m_config(PkSharedConfig::openConfig()->group(PkString()))
    , m_readOnly(readOnly)
{
    if (!readOnly) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(PkThread::mainThreadId() == PkThread::currentThreadId());
    }
#ifdef __APPLE__
    // clear /var/folders/ swap path set by old broken Krita swap implementation in order to use new default swap dir.
    PkString swap = m_config.readEntry("swaplocation", "");
    if (swap.startsWith("/var/folders/")) {
        m_config.deleteEntry("swaplocation");
    }
#endif
}

KisImageConfig::~KisImageConfig()
{
    if (m_readOnly) return;

    if (PkThread::mainThreadId() != PkThread::currentThreadId()) {
        dbgKrita << "KisImageConfig: requested config synchronization from nonGUI thread! Called from" << kisBacktrace();
        return;
    }

    m_config.sync();
}

bool KisImageConfig::enableProgressReporting(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("enableProgressReporting", true) : true;
}

void KisImageConfig::setEnableProgressReporting(bool value)
{
    m_config.writeEntry("enableProgressReporting", value);
}

bool KisImageConfig::enablePerfLog(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("enablePerfLog", false) :false;
}

void KisImageConfig::setEnablePerfLog(bool value)
{
    m_config.writeEntry("enablePerfLog", value);
}

qreal KisImageConfig::transformMaskOffBoundsReadArea() const
{
    return m_config.readEntry("transformMaskOffBoundsReadArea", 0.5);
}

int KisImageConfig::updatePatchHeight() const
{
    int patchHeight = m_config.readEntry("updatePatchHeight", 512);
    if (patchHeight <= 0) return 512;
    return patchHeight;
}

void KisImageConfig::setUpdatePatchHeight(int value)
{
    m_config.writeEntry("updatePatchHeight", value);
}

int KisImageConfig::updatePatchWidth() const
{
    int patchWidth = m_config.readEntry("updatePatchWidth", 512);
    if (patchWidth <= 0) return 512;
    return patchWidth;
}

void KisImageConfig::setUpdatePatchWidth(int value)
{
    m_config.writeEntry("updatePatchWidth", value);
}

qreal KisImageConfig::maxCollectAlpha() const
{
    return m_config.readEntry("maxCollectAlpha", 2.5);
}

qreal KisImageConfig::maxMergeAlpha() const
{
    return m_config.readEntry("maxMergeAlpha", 1.);
}

qreal KisImageConfig::maxMergeCollectAlpha() const
{
    return m_config.readEntry("maxMergeCollectAlpha", 1.5);
}

qreal KisImageConfig::schedulerBalancingRatio() const
{
    /**
     * updates-queue-size / strokes-queue-size
     */
    return m_config.readEntry("schedulerBalancingRatio", 100.);
}

void KisImageConfig::setSchedulerBalancingRatio(qreal value)
{
    m_config.writeEntry("schedulerBalancingRatio", value);
}

int KisImageConfig::maxSwapSize(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("maxSwapSize", 4096) : 4096; // in MiB
}

void KisImageConfig::setMaxSwapSize(int value)
{
    m_config.writeEntry("maxSwapSize", value);
}

int KisImageConfig::swapSlabSize() const
{
    return m_config.readEntry("swapSlabSize", 64); // in MiB
}

void KisImageConfig::setSwapSlabSize(int value)
{
    m_config.writeEntry("swapSlabSize", value);
}

int KisImageConfig::swapWindowSize() const
{
    return m_config.readEntry("swapWindowSize", 16); // in MiB
}

void KisImageConfig::setSwapWindowSize(int value)
{
    m_config.writeEntry("swapWindowSize", value);
}

int KisImageConfig::tilesHardLimit() const
{
    qreal hp = qreal(memoryHardLimitPercent()) / 100.0;
    qreal pp = qreal(memoryPoolLimitPercent()) / 100.0;

    return totalRAM() * hp * (1 - pp);
}

int KisImageConfig::tilesSoftLimit() const
{
    qreal sp = qreal(memorySoftLimitPercent()) / 100.0;

    return tilesHardLimit() * sp;
}

int KisImageConfig::poolLimit() const
{
    qreal hp = qreal(memoryHardLimitPercent()) / 100.0;
    qreal pp = qreal(memoryPoolLimitPercent()) / 100.0;

    return totalRAM() * hp * pp;
}

qreal KisImageConfig::memoryHardLimitPercent(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("memoryHardLimitPercent", 50.) : 50.;
}

void KisImageConfig::setMemoryHardLimitPercent(qreal value)
{
    m_config.writeEntry("memoryHardLimitPercent", value);
}

qreal KisImageConfig::memorySoftLimitPercent(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("memorySoftLimitPercent", 2.) : 2.;
}

void KisImageConfig::setMemorySoftLimitPercent(qreal value)
{
    m_config.writeEntry("memorySoftLimitPercent", value);
}

qreal KisImageConfig::memoryPoolLimitPercent(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("memoryPoolLimitPercent", 0.0) : 0.0;
}

void KisImageConfig::setMemoryPoolLimitPercent(qreal value)
{
    m_config.writeEntry("memoryPoolLimitPercent", value);
}

PkString KisImageConfig::safelyGetWritableTempLocation(const PkString &suffix, const PkString &configKey, bool requestDefault) const
{
    const PlatformSwapPathPolicy policy = platformSwapPathPolicy(suffix);
    const std::filesystem::path stableDefault = policy.stableDefault;
    if (!stableDefault.empty()) {
        KisImageConfigPaths::ensureDirectory(stableDefault);
    }
    const std::filesystem::path transientDefault = toPath(temporaryDirectory());
    const std::filesystem::path homeFallback = toPath(homeDirectory());
    const std::filesystem::path platformDefault =
        stableDefault.empty() ? transientDefault : stableDefault;
    if (requestDefault) {
        return fromPath(platformDefault);
    }
    PkString configuredSwap = m_config.readEntry(configKey, fromPath(platformDefault));
#ifdef __APPLE__
    if (configuredSwap.startsWith("/var/folders/")) {
        configuredSwap = fromPath(platformDefault);
    }
#endif
    const std::filesystem::path preferred = configuredSwap.isEmpty()
        ? platformDefault : toPath(configuredSwap);

    const std::filesystem::path lastResort =
        policy.transientFallback == KisImageConfigPaths::TransientFallback::Reject
        ? stableDefault : preferred;
    const KisImageConfigPaths::LocationSelection selection =
        KisImageConfigPaths::selectWritableLocation(
        preferred, stableDefault, transientDefault, homeFallback, lastResort,
        policy.transientFallback,
        [](const std::filesystem::path &location) {
            // QFileInfo::isWritable() is insufficient on NTFS, so preserve the
            // official implementation's final authority: create and remove a
            // real temporary file in each proposed directory.
            return KisImageConfigPaths::probeWritableDirectory(location);
        });
    const PkString chosenLocation = fromPath(selection.location);

    if (!selection.probeSucceeded) {
        qCritical() << "CRITICAL: no writable location for a swap file found";
    }

    if (chosenLocation != fromPath(preferred)) {
        qWarning() << "WARNING: configured swap location is not writable, using a fall-back location" << fromPath(preferred) << "->" << chosenLocation;
    }

    return chosenLocation;
}


PkString KisImageConfig::swapDir(bool requestDefault)
{
    return safelyGetWritableTempLocation("swap", "swaplocation", requestDefault);
}

void KisImageConfig::setSwapDir(const PkString &swapDir)
{
    m_config.writeEntry("swaplocation", swapDir);
}

int KisImageConfig::numberOfOnionSkins() const
{
    return m_config.readEntry("numberOfOnionSkins", 10);
}

void KisImageConfig::setNumberOfOnionSkins(int value)
{
    m_config.writeEntry("numberOfOnionSkins", value);
}

int KisImageConfig::onionSkinTintFactor() const
{
    return m_config.readEntry("onionSkinTintFactor", 192);
}

void KisImageConfig::setOnionSkinTintFactor(int value)
{
    m_config.writeEntry("onionSkinTintFactor", value);
}

int KisImageConfig::onionSkinOpacity(int offset, bool requestDefault) const
{
    int value = m_config.readEntry(dynamicKey(PkString("onionSkinOpacity_"), numberString(offset)), -1);

    if (value < 0 || requestDefault) {
        const int num = numberOfOnionSkins();
        if (num > 0) {
            const qreal dx = qreal(qAbs(offset)) / num;
            value = 0.7 * exp(-pow2(dx) / 0.5) * 255;
        }
    }

    return value;
}

void KisImageConfig::setOnionSkinOpacity(int offset, int value)
{
    m_config.writeEntry(dynamicKey(PkString("onionSkinOpacity_"), numberString(offset)), value);
}

bool KisImageConfig::onionSkinState(int offset) const
{
    bool enableByDefault = (qAbs(offset) <= 2);
    return m_config.readEntry(dynamicKey(PkString("onionSkinState_"), numberString(offset)), enableByDefault);
}

void KisImageConfig::setOnionSkinState(int offset, bool value)
{
    m_config.writeEntry(dynamicKey(PkString("onionSkinState_"), numberString(offset)), value);
}

PkColor KisImageConfig::onionSkinTintColorBackward() const
{
    return m_config.readEntry("onionSkinTintColorBackward", PkColor(255, 0, 0));
}

void KisImageConfig::setOnionSkinTintColorBackward(const PkColor &value)
{
    m_config.writeEntry("onionSkinTintColorBackward", value);
}

PkColor KisImageConfig::onionSkinTintColorForward() const
{
    return m_config.readEntry("oninSkinTintColorForward", PkColor(0, 255, 0));
}

void KisImageConfig::setOnionSkinTintColorForward(const PkColor &value)
{
    m_config.writeEntry("oninSkinTintColorForward", value);
}

bool KisImageConfig::autoKeyEnabled(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("lazyFrameCreationEnabled", true) : true;
}

void KisImageConfig::setAutoKeyEnabled(bool value)
{
    m_config.writeEntry("lazyFrameCreationEnabled", value);
}

bool KisImageConfig::autoKeyModeDuplicate(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("lazyFrameModeDuplicate", true) : true;
}

void KisImageConfig::setAutoKeyModeDuplicate(bool value)
{
    m_config.writeEntry("lazyFrameModeDuplicate", value);
}

#if defined __linux__
#include <sys/sysinfo.h>
#elif defined __HAIKU__
#include <OS.h>
#elif defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
#include <sys/sysctl.h>
#elif defined _WIN32
#include <windows.h>
#elif defined __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

int KisImageConfig::totalRAM()
{
    // let's think that default memory size is 1000MiB
    int totalMemory = 1000; // MiB
    int error = 1;

#if defined __linux__
    struct sysinfo info;

    error = sysinfo(&info);
    if(!error) {
        totalMemory = info.totalram * info.mem_unit / (1UL << 20);
    }
#elif defined __HAIKU__
	system_info info;
	error = get_system_info(&info) == B_OK ? 0 : 1;
	if (!error) {
		uint64_t size = (info.max_pages * B_PAGE_SIZE);
	totalMemory = size >> 20;
	}
#elif defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
    u_long physmem;
#   if defined HW_PHYSMEM64 // NetBSD only
    int mib[] = {CTL_HW, HW_PHYSMEM64};
#   else
    int mib[] = {CTL_HW, HW_PHYSMEM};
#   endif
    size_t len = sizeof(physmem);

    error = sysctl(mib, 2, &physmem, &len, 0, 0);
    if(!error) {
        totalMemory = physmem >> 20;
    }
#elif defined _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    error  = !GlobalMemoryStatusEx(&status);

    if (!error) {
        totalMemory = status.ullTotalPhys >> 20;
    }

    // For 32 bit windows, the total memory available is at max the 2GB per process memory limit.
#   if defined ENV32BIT
    totalMemory = qMin(totalMemory, 2000);
#   endif
#elif defined __APPLE__
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof(mib) / sizeof(mib[0]);
    uint64_t size;
    size_t len = sizeof(size);

    errno = 0;
    if (sysctl(mib, namelen, &size, &len, 0, 0) >= 0) {
        totalMemory = size >> 20;
        error = 0;
    }
    else {
        dbgKrita << "sysctl(\"hw.memsize\") raised error" << strerror(errno);
    }
#endif

    if (error) {
        warnKrita << "Cannot get the size of your RAM. Using 1 GiB by default.";
    }

    return totalMemory;
}

bool KisImageConfig::showAdditionalOnionSkinsSettings(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("showAdditionalOnionSkinsSettings", true) : true;
}

void KisImageConfig::setShowAdditionalOnionSkinsSettings(bool value)
{
    m_config.writeEntry("showAdditionalOnionSkinsSettings", value);
}

int KisImageConfig::defaultFrameColorLabel() const
{
    return m_config.readEntry("defaultFrameColorLabel", 0);
}

void KisImageConfig::setDefaultFrameColorLabel(int label)
{
    m_config.writeEntry("defaultFrameColorLabel", label);
}

KisProofingConfigurationSP KisImageConfig::defaultProofingconfiguration(bool requestDefault)
{
    KisProofingConfiguration *proofingConfig= new KisProofingConfiguration();
    if (!requestDefault) {
        proofingConfig->proofingProfile = m_config.readEntry("defaultProofingProfileName", "Chemical proof");
        proofingConfig->proofingModel = m_config.readEntry("defaultProofingProfileModel", "CMYKA");
        proofingConfig->proofingDepth = m_config.readEntry("defaultProofingProfileDepth", "U8");
        proofingConfig->displayIntent = KoColorConversionTransformation::Intent(m_config.readEntry("defaultProofingProfileIntent", int(INTENT_ABSOLUTE_COLORIMETRIC)));
        proofingConfig->conversionIntent = KoColorConversionTransformation::Intent(m_config.readEntry("defaultProofingConversionIntent", int(INTENT_RELATIVE_COLORIMETRIC)));
        proofingConfig->useBlackPointCompensationFirstTransform = m_config.readEntry("defaultProofingBlackpointCompensation", true);

        proofingConfig->displayFlags.setFlag(KoColorConversionTransformation::BlackpointCompensation, m_config.readEntry("defaultProofingDisplayBlackpointCompensation", true));
        proofingConfig->displayMode = KisProofingConfiguration::DisplayTransformState(m_config.readEntry("defaultProofingDisplayMode", int(KisProofingConfiguration::Paper)));
        PkColor def(0, 255, 0);
        def = m_config.readEntry("defaultProofingGamutwarning", def);
        KoColor col(KoColorSpaceRegistry::instance()->rgb8());
        col.fromQColor(def);
        col.setOpacity(1.0);
        proofingConfig->warningColor = col;
        proofingConfig->setLegacyAdaptationState(m_config.readEntry("defaultProofingAdaptationState", 1.0));
    }
    return toQShared(proofingConfig);
}

void KisImageConfig::setDefaultProofingConfig(const KisProofingConfiguration &config)
{
    if (*defaultProofingconfiguration() == config) return;

    m_config.writeEntry("defaultProofingProfileName", config.proofingProfile);
    m_config.writeEntry("defaultProofingProfileModel", config.proofingModel);
    m_config.writeEntry("defaultProofingProfileDepth", config.proofingDepth);
    m_config.writeEntry("defaultProofingConversionIntent", int(config.conversionIntent));
    m_config.writeEntry("defaultProofingBlackpointCompensation", config.useBlackPointCompensationFirstTransform);
    PkColor c;
    c = config.warningColor.toQColor();
    m_config.writeEntry("defaultProofingGamutwarning", c);
    m_config.writeEntry("defaultProofingAdaptationState", config.legacyAdaptationState());
    m_config.writeEntry("defaultProofingDisplayBlackpointCompensation", config.displayFlags.testFlag(KoColorConversionTransformation::BlackpointCompensation));
    m_config.writeEntry("defaultProofingProfileIntent", int(config.displayIntent));
    m_config.writeEntry("defaultProofingDisplayMode", int(config.displayMode));

    KisImageConfigNotifier::instance()->notifyGlobalProofingConfigChanged();
}

bool KisImageConfig::useLodForColorizeMask(bool requestDefault) const
{
    return !requestDefault ?
        m_config.readEntry("useLodForColorizeMask", false) : false;
}

void KisImageConfig::setUseLodForColorizeMask(bool value)
{
    m_config.writeEntry("useLodForColorizeMask", value);
}

int KisImageConfig::maxNumberOfThreads(bool defaultValue) const
{
    return (defaultValue ? PkThread::idealThreadCount() : m_config.readEntry("maxNumberOfThreads", PkThread::idealThreadCount()));
}

void KisImageConfig::setMaxNumberOfThreads(int value)
{
    if (value == PkThread::idealThreadCount()) {
        m_config.deleteEntry("maxNumberOfThreads");
    } else {
        m_config.writeEntry("maxNumberOfThreads", value);
    }
}

int KisImageConfig::frameRenderingClones(bool defaultValue) const
{
    const int defaultClonesCount = qMax(1, maxNumberOfThreads(defaultValue) / 2);
    return defaultValue ? defaultClonesCount : m_config.readEntry("frameRenderingClones", defaultClonesCount);
}

void KisImageConfig::setFrameRenderingClones(int value)
{
    m_config.writeEntry("frameRenderingClones", value);
}

int KisImageConfig::frameRenderingTimeout(bool defaultValue) const
{
    const int defaultFrameRenderingTimeout = 30000; // 30 ms
    return defaultValue ? defaultFrameRenderingTimeout : m_config.readEntry("frameRenderingTimeout", defaultFrameRenderingTimeout);
}

void KisImageConfig::setFrameRenderingTimeout(int value)
{
    m_config.writeEntry("frameRenderingTimeout", value);
}

int KisImageConfig::fpsLimit(bool defaultValue) const
{
    int limit = defaultValue ? 100 : m_config.readEntry("fpsLimit", 100);
    return limit > 0 ? limit : 1;
}

void KisImageConfig::setFpsLimit(int value)
{
    m_config.writeEntry("fpsLimit", value);
}

bool KisImageConfig::detectFpsLimit(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("detectFpsLimit", true);
}

void KisImageConfig::setDetectFpsLimit(bool value)
{
    m_config.writeEntry("detectFpsLimit", value);
}

bool KisImageConfig::useOnDiskAnimationCacheSwapping(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("useOnDiskAnimationCacheSwapping", true);
}

void KisImageConfig::setUseOnDiskAnimationCacheSwapping(bool value)
{
    m_config.writeEntry("useOnDiskAnimationCacheSwapping", value);
}

PkString KisImageConfig::animationCacheDir(bool defaultValue) const
{
    return safelyGetWritableTempLocation("animation_cache", "animationCacheDir", defaultValue);
}

void KisImageConfig::setAnimationCacheDir(const PkString &value)
{
    m_config.writeEntry("animationCacheDir", value);
}

bool KisImageConfig::useAnimationCacheFrameSizeLimit(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("useAnimationCacheFrameSizeLimit", true);
}

void KisImageConfig::setUseAnimationCacheFrameSizeLimit(bool value)
{
    m_config.writeEntry("useAnimationCacheFrameSizeLimit", value);
}

int KisImageConfig::animationCacheFrameSizeLimit(bool defaultValue) const
{
    return defaultValue ? 2500 : m_config.readEntry("animationCacheFrameSizeLimit", 2500);
}

void KisImageConfig::setAnimationCacheFrameSizeLimit(int value)
{
    m_config.writeEntry("animationCacheFrameSizeLimit", value);
}

bool KisImageConfig::useAnimationCacheRegionOfInterest(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("useAnimationCacheRegionOfInterest", true);
}

void KisImageConfig::setUseAnimationCacheRegionOfInterest(bool value)
{
    m_config.writeEntry("useAnimationCacheRegionOfInterest", value);
}

qreal KisImageConfig::animationCacheRegionOfInterestMargin(bool defaultValue) const
{
    return defaultValue ? 0.25 : m_config.readEntry("animationCacheRegionOfInterestMargin", 0.25);
}

void KisImageConfig::setAnimationCacheRegionOfInterestMargin(qreal value)
{
    m_config.writeEntry("animationCacheRegionOfInterestMargin", value);
}

qreal KisImageConfig::selectionOutlineOpacity(bool defaultValue) const
{
    return defaultValue ? 1.0 : m_config.readEntry("selectionOutlineOpacity", 1.0);
}

void KisImageConfig::setSelectionOutlineOpacity(qreal value)
{
    m_config.writeEntry("selectionOutlineOpacity", value);
}

PkColor KisImageConfig::selectionOverlayMaskColor(bool defaultValue) const
{
    PkColor def(255, 0, 0, 128);
    return (defaultValue ? def : m_config.readEntry("selectionOverlayMaskColor", def));
}

void KisImageConfig::setSelectionOverlayMaskColor(const PkColor &color)
{
    m_config.writeEntry("selectionOverlayMaskColor", color);
}

int KisImageConfig::maxBrushSize(bool defaultValue) const
{
    return !defaultValue ? m_config.readEntry("maximumBrushSize", 1000) : 1000;
}

void KisImageConfig::setMaxBrushSize(int value)
{
    m_config.writeEntry("maximumBrushSize", value);
}

int KisImageConfig::maxMaskingBrushSize() const
{
    return qMin(15000, 3 * maxBrushSize());
}

CursorStyle KisImageConfig::newCursorStyle(bool defaultValue) const
{
    if (defaultValue) {
        return CURSOR_STYLE_NO_CURSOR;
    }

    int style = m_config.readEntry("newCursorStyle", int(-1));

    if (style < 0) {
        style = m_config.readEntry("cursorStyleDef", int(OLD_CURSOR_STYLE_OUTLINE));

        switch (style) {
        case OLD_CURSOR_STYLE_TOOLICON:
            style = CURSOR_STYLE_TOOLICON;
            break;
        case OLD_CURSOR_STYLE_CROSSHAIR:
        case OLD_CURSOR_STYLE_OUTLINE_CENTER_CROSS:
            style = CURSOR_STYLE_CROSSHAIR;
            break;
        case OLD_CURSOR_STYLE_POINTER:
            style = CURSOR_STYLE_POINTER;
            break;
        case OLD_CURSOR_STYLE_OUTLINE:
        case OLD_CURSOR_STYLE_NO_CURSOR:
            style = CURSOR_STYLE_NO_CURSOR;
            break;
        case OLD_CURSOR_STYLE_SMALL_ROUND:
        case OLD_CURSOR_STYLE_OUTLINE_CENTER_DOT:
            style = CURSOR_STYLE_SMALL_ROUND;
            break;
        case OLD_CURSOR_STYLE_TRIANGLE_RIGHTHANDED:
        case OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_RIGHTHANDED:
            style = CURSOR_STYLE_TRIANGLE_RIGHTHANDED;
            break;
        case OLD_CURSOR_STYLE_TRIANGLE_LEFTHANDED:
        case OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_LEFTHANDED:
            style = CURSOR_STYLE_TRIANGLE_LEFTHANDED;
            break;
        default:
            style = -1;
        }
    }

    cleanOldImageCursorStyleKeys(m_config);

    if (style < 0 || style >= N_CURSOR_STYLE_SIZE) {
        style = CURSOR_STYLE_NO_CURSOR;
    }

    return static_cast<CursorStyle>(style);
}

OutlineStyle KisImageConfig::newOutlineStyle(bool defaultValue) const
{
    if (defaultValue) {
        return OUTLINE_FULL;
    }

    int style = m_config.readEntry("newOutlineStyle", int(-1));

    if (style < 0) {
        style = m_config.readEntry("cursorStyleDef", int(OLD_CURSOR_STYLE_OUTLINE));

        switch (style) {
        case OLD_CURSOR_STYLE_TOOLICON:
        case OLD_CURSOR_STYLE_CROSSHAIR:
        case OLD_CURSOR_STYLE_POINTER:
        case OLD_CURSOR_STYLE_NO_CURSOR:
        case OLD_CURSOR_STYLE_SMALL_ROUND:
        case OLD_CURSOR_STYLE_TRIANGLE_RIGHTHANDED:
        case OLD_CURSOR_STYLE_TRIANGLE_LEFTHANDED:
            style = OUTLINE_NONE;
            break;
        case OLD_CURSOR_STYLE_OUTLINE:
        case OLD_CURSOR_STYLE_OUTLINE_CENTER_DOT:
        case OLD_CURSOR_STYLE_OUTLINE_CENTER_CROSS:
        case OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_RIGHTHANDED:
        case OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_LEFTHANDED:
            style = OUTLINE_FULL;
            break;
        default:
            style = -1;
        }
    }

    cleanOldImageCursorStyleKeys(m_config);

    if (style < 0 || style >= N_OUTLINE_STYLE_SIZE) {
        style = OUTLINE_FULL;
    }

    return static_cast<OutlineStyle>(style);
}

bool KisImageConfig::separateEraserCursor(bool defaultValue) const
{
    return defaultValue ? false : m_config.readEntry("separateEraserCursor", false);
}

CursorStyle KisImageConfig::eraserCursorStyle(bool defaultValue) const
{
    if (defaultValue) {
        return CURSOR_STYLE_ERASER;
    }

    int style = m_config.readEntry("eraserCursorStyle", int(-1));
    if (style < 0 || style >= N_CURSOR_STYLE_SIZE) {
        style = CURSOR_STYLE_ERASER;
    }
    return static_cast<CursorStyle>(style);
}

OutlineStyle KisImageConfig::eraserOutlineStyle(bool defaultValue) const
{
    if (defaultValue) {
        return OUTLINE_FULL;
    }

    int style = m_config.readEntry("eraserOutlineStyle", int(-1));
    if (style < 0 || style >= N_OUTLINE_STYLE_SIZE) {
        style = OUTLINE_FULL;
    }
    return static_cast<OutlineStyle>(style);
}

PkString KisImageConfig::pressureTabletCurve(bool defaultValue) const
{
    PkString fallback("0,0;1,1;");
#ifdef __ANDROID__
    if (KisAndroidUtils::looksLikeXiaomiDevice()) {
        fallback = PkString("0,0;0.7,1;");
    }
#endif
    return defaultValue ? fallback : m_config.readEntry("tabletPressureCurve", fallback);
}

bool KisImageConfig::showOutlineWhilePainting(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("ShowOutlineWhilePainting", true);
}

bool KisImageConfig::forceAlwaysFullSizedOutline(bool defaultValue) const
{
    return defaultValue ? false : m_config.readEntry("forceAlwaysFullSizedOutline", false);
}

bool KisImageConfig::showEraserOutlineWhilePainting(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("ShowEraserOutlineWhilePainting", true);
}

bool KisImageConfig::forceAlwaysFullSizedEraserOutline(bool defaultValue) const
{
    return defaultValue ? false : m_config.readEntry("forceAlwaysFullSizedEraserOutline", false);
}

qreal KisImageConfig::outlineSizeMinimum(bool defaultValue) const
{
    return defaultValue ? 1.0 : m_config.readEntry("OutlineSizeMinimum", 1.0);
}

KisImageConfig::TouchPainting KisImageConfig::touchPainting(bool defaultValue) const
{
    const int value = defaultValue
        ? int(TOUCH_PAINTING_AUTO)
        : m_config.readEntry("touchPainting", int(TOUCH_PAINTING_AUTO));
    return static_cast<TouchPainting>(value);
}

bool KisImageConfig::disableTouchOnCanvas(bool tabletInputReceived) const
{
    switch (touchPainting()) {
    case TOUCH_PAINTING_ENABLED:
        return false;
    case TOUCH_PAINTING_DISABLED:
        return true;
    default:
        return tabletInputReceived;
    }
}

int KisImageConfig::lineSmoothingType(bool defaultValue) const
{
    return defaultValue ? 1 : m_config.readEntry("LineSmoothingType", 1);
}

void KisImageConfig::setLineSmoothingType(int value)
{
    m_config.writeEntry("LineSmoothingType", value);
}

qreal KisImageConfig::lineSmoothingDistanceMin(bool defaultValue) const
{
    return defaultValue ? 50.0 : m_config.readEntry("LineSmoothingDistanceMin", 50.0);
}

void KisImageConfig::setLineSmoothingDistanceMin(qreal value)
{
    m_config.writeEntry("LineSmoothingDistanceMin", value);
}

qreal KisImageConfig::lineSmoothingDistanceMax(bool defaultValue) const
{
    return defaultValue ? 50.0 : m_config.readEntry("LineSmoothingDistanceMax", 50.0);
}

void KisImageConfig::setLineSmoothingDistanceMax(qreal value)
{
    m_config.writeEntry("LineSmoothingDistanceMax", value);
}

bool KisImageConfig::lineSmoothingDistanceKeepAspectRatio(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("LineSmoothingDistanceKeepAspectRatio", true);
}

void KisImageConfig::setLineSmoothingDistanceKeepAspectRatio(bool value)
{
    m_config.writeEntry("LineSmoothingDistanceKeepAspectRatio", value);
}

qreal KisImageConfig::lineSmoothingTailAggressiveness(bool defaultValue) const
{
    return defaultValue ? 0.15 : m_config.readEntry("LineSmoothingTailAggressiveness", 0.15);
}

void KisImageConfig::setLineSmoothingTailAggressiveness(qreal value)
{
    m_config.writeEntry("LineSmoothingTailAggressiveness", value);
}

bool KisImageConfig::lineSmoothingSmoothPressure(bool defaultValue) const
{
    return defaultValue ? false : m_config.readEntry("LineSmoothingSmoothPressure", false);
}

void KisImageConfig::setLineSmoothingSmoothPressure(bool value)
{
    m_config.writeEntry("LineSmoothingSmoothPressure", value);
}

bool KisImageConfig::lineSmoothingScalableDistance(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("LineSmoothingScalableDistance", true);
}

void KisImageConfig::setLineSmoothingScalableDistance(bool value)
{
    m_config.writeEntry("LineSmoothingScalableDistance", value);
}

qreal KisImageConfig::lineSmoothingDelayDistance(bool defaultValue) const
{
    return defaultValue ? 50.0 : m_config.readEntry("LineSmoothingDelayDistance", 50.0);
}

void KisImageConfig::setLineSmoothingDelayDistance(qreal value)
{
    m_config.writeEntry("LineSmoothingDelayDistance", value);
}

bool KisImageConfig::lineSmoothingUseDelayDistance(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("LineSmoothingUseDelayDistance", true);
}

void KisImageConfig::setLineSmoothingUseDelayDistance(bool value)
{
    m_config.writeEntry("LineSmoothingUseDelayDistance", value);
}

bool KisImageConfig::lineSmoothingFinishStabilizedCurve(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("LineSmoothingFinishStabilizedCurve", true);
}

void KisImageConfig::setLineSmoothingFinishStabilizedCurve(bool value)
{
    m_config.writeEntry("LineSmoothingFinishStabilizedCurve", value);
}

bool KisImageConfig::lineSmoothingStabilizeSensors(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("LineSmoothingStabilizeSensors", true);
}

void KisImageConfig::setLineSmoothingStabilizeSensors(bool value)
{
    m_config.writeEntry("LineSmoothingStabilizeSensors", value);
}

int KisImageConfig::stabilizerSampleSize(bool defaultValue) const
{
#ifdef _WIN32
    const int defaultSampleSize = 50;
#else
    const int defaultSampleSize = 15;
#endif
    return defaultValue ? defaultSampleSize
                        : m_config.readEntry("stabilizerSampleSize", defaultSampleSize);
}

bool KisImageConfig::stabilizerDelayedPaint(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("stabilizerDelayedPaint", true);
}

bool KisImageConfig::compressKra(bool defaultValue) const
{
    return defaultValue ? false : m_config.readEntry("compressLayersInKra", false);
}

void KisImageConfig::setCompressKra(bool compress)
{
    m_config.writeEntry("compressLayersInKra", compress);
}

bool KisImageConfig::renameMergedLayers(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("renameMergedLayers", true);
}

void KisImageConfig::setRenameMergedLayers(bool value)
{
    m_config.writeEntry("renameMergedLayers", value);
}

bool KisImageConfig::renameDuplicatedLayers(bool defaultValue) const
{
    return defaultValue ? true : m_config.readEntry("renameDuplicatedLayers", true);
}

void KisImageConfig::setRenameDuplicatedLayers(bool value)
{
    m_config.writeEntry("renameDuplicatedLayers", value);
}

PkString KisImageConfig::exportConfigurationXML(const PkString &exportConfigId, bool defaultValue) const
{
    return (defaultValue ? PkString() : m_config.readEntry(dynamicKey(PkString("ExportConfiguration-"), exportConfigId), PkString()));
}

bool KisImageConfig::hasExportConfiguration(const PkString &exportConfigID)
{
    return m_config.hasKey(dynamicKey(PkString("ExportConfiguration-"), exportConfigID));
}

KisPropertiesConfigurationSP KisImageConfig::exportConfiguration(const PkString &exportConfigId, bool defaultValue) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    const PkString xmlData = exportConfigurationXML(exportConfigId, defaultValue);
    cfg->fromXML(xmlData);
    return cfg;
}

void KisImageConfig::setExportConfiguration(const PkString &exportConfigId, KisPropertiesConfigurationSP properties)
{
    const PkString exportConfig = properties->toXML();
    PkString configId = dynamicKey(PkString("ExportConfiguration-"), exportConfigId);
    m_config.writeEntry(configId, exportConfig);
}

void KisImageConfig::resetConfig()
{
    PkConfigGroup config = PkSharedConfig::openConfig()->group(PkString());
    config.deleteGroup();
}
