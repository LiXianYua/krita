/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisImportExportManager.h"
#include "KisImportExportBackend.h"

#include <PkFileStream.h>
#include <PkSharedPointer.h>
#include <PkStream.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkAuxTypes.h>

#include <KisMimeDatabase.h>
#include <KisUsageLogger.h>
#include <KoColorProfile.h>
#include <KoColorProfileConstants.h>
#include <KoProgressUpdater.h>
#include <kis_assert.h>
#include <kis_debug.h>
#include <kis_image.h>
#include <kis_iterator_ng.h>
#include <kis_layer_utils.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>

#include "KisImportExportErrorCode.h"
#include "KisImportExportFilter.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <future>

// 本地数值格式化与字节互转辅助（与 KisDocument.cpp 同款）。
static PkString pkNumber(int v) { char buf[32]; snprintf(buf, sizeof(buf), "%d", v); return PkString(buf); }
static PkString pkNumber(long long v) { char buf[32]; snprintf(buf, sizeof(buf), "%lld", v); return PkString(buf); }
static PkString pkNumber(long v) { char buf[32]; snprintf(buf, sizeof(buf), "%ld", v); return PkString(buf); }
static PkString pkNumber(double v) { char buf[64]; snprintf(buf, sizeof(buf), "%g", v); return PkString(buf); }
static PkString pkFromByteArray(const PkByteArray &ba) {
    return PkString::PkFromUtf8(ba.constData(), ba.size());
}
static PkByteArray pkToByteArray(const PkString &s) {
    std::string u = s.PkToUtf8();
    return PkByteArray(u.data(), (int)u.size());
}

class KisImportExportManager::Private
{
public:
    KoUpdaterPtr updater;

    PkString cachedExportFilterMimeType;
    PkSharedPointer<KisImportExportFilter> cachedExportFilter;
};

struct KisImportExportManager::ConversionResult {
    ConversionResult()
    {
    }

    ConversionResult(std::future<KisImportExportErrorCode> futureStatus)
        : m_isAsync(true),
          m_futureStatus(std::move(futureStatus))
    {
    }

    ConversionResult(KisImportExportErrorCode status)
        : m_isAsync(false),
          m_status(status)
    {
    }

    bool isAsync() const {
        return m_isAsync;
    }

    std::future<KisImportExportErrorCode> futureStatus() {
        // if the result is not async, then it means some failure happened,
        // just return a default-constructed future
        KIS_SAFE_ASSERT_RECOVER_NOOP(m_isAsync || !m_status.isOk());

        return std::move(m_futureStatus);
    }

    KisImportExportErrorCode status() const {
        return m_status;
    }

    void setStatus(KisImportExportErrorCode value) {
        m_status = value;
    }
private:
    bool m_isAsync = false;
    std::future<KisImportExportErrorCode> m_futureStatus;
    KisImportExportErrorCode m_status = ImportExportCodes::InternalError;
};


// ---- S9 静态注册点 ----
// 原 KoJsonTrader 动态插件发现（D-12 已删）改为静态注册表：registerFilter() 写入，
// supportedMimeTypes()/filterForMimeType() 读取。注册表当前为空；S9（插件静态
// 注册）落地前：supportedMimeTypes() 返回空列表、filterForMimeType() 返回 nullptr
// （导出路径在 KisDocument.cpp:946/1520 的调用点 gracefully 失败）。
static PkList<KisImportExportManager::FilterRegistration> &importExportFilterRegistry()
{
    static PkList<KisImportExportManager::FilterRegistration> registry;
    return registry;
}

void KisImportExportManager::registerFilter(const FilterRegistration &registration)
{
    importExportFilterRegistry().append(registration);
}


KisImportExportManager::KisImportExportManager(KisDocument* document)
    : m_backend(createKisImportExportBackend(document))
    , d(new Private)
{
}

KisImportExportManager::~KisImportExportManager()
{
    delete d;
}

KisImportExportErrorCode KisImportExportManager::importDocument(const PkString& location, const PkString& mimeType)
{
    ConversionResult result = convert(Import, location, location, mimeType, false, 0, false);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!result.isAsync(), ImportExportCodes::InternalError);

    return result.status();
}

KisImportExportErrorCode KisImportExportManager::exportDocument(const PkString& location, const PkString& realLocation, const PkByteArray& mimeType, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    ConversionResult result = convert(Export, location, realLocation, pkFromByteArray(mimeType), showWarnings, exportConfiguration, false, isAdvancedExporting);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!result.isAsync(), ImportExportCodes::InternalError);

    return result.status();
}

std::future<KisImportExportErrorCode> KisImportExportManager::exportDocumentAsync(const PkString &location, const PkString &realLocation, const PkByteArray &mimeType,
                                                                                 KisImportExportErrorCode &status, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    ConversionResult result = convert(Export, location, realLocation, pkFromByteArray(mimeType), showWarnings, exportConfiguration, true, isAdvancedExporting);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(result.isAsync() ||
                                         !result.status().isOk(), std::future<KisImportExportErrorCode>());

    status = result.status();
    return result.futureStatus();
}

// The static method to figure out to which parts of the
// graph this mimetype has a connection to.
PkStringList KisImportExportManager::supportedMimeTypes(Direction direction)
{
    // 原实现经 KoJsonTrader 动态发现插件并缓存（m_importMimeTypes/m_exportMimeTypes）。
    // 缓存已移除：静态注册表可能在首次调用后被 S9 填充，缓存会让结果陈旧。
    // 现改为每次遍历静态注册表（当前为空 → 返回空列表）。
    PkStringList result;
    for (const auto &registration : importExportFilterRegistry()) {
        const PkStringList &types =
                direction == Import ? registration.importMimeTypes : registration.exportMimeTypes;
        for (const PkString &mimeType : types) {
            if (!result.contains(mimeType)) {
                result.append(mimeType);
            }
        }
    }
    return result;
}

KisImportExportFilter *KisImportExportManager::filterForMimeType(const PkString &mimetype, KisImportExportManager::Direction direction)
{
    // 原实现经 KoJsonTrader + 工厂动态实例化插件（D-12 已删）。现改为查静态
    // 注册表：选 weight 最高且声明支持该 mimetype 的注册项，实例化并返回。
    // 注册表为空 → 返回 nullptr（S9 落地前导出路径 graceful 失败）。
    int weight = -1;
    KisImportExportFilter *filter = 0;

    for (const auto &registration : importExportFilterRegistry()) {
        const PkStringList &types =
                direction == Export ? registration.exportMimeTypes : registration.importMimeTypes;
        if (!types.contains(mimetype)) {
            continue;
        }
        if (registration.weight <= weight) {
            continue;
        }
        KisImportExportFilter *candidate = registration.factory ? registration.factory() : 0;
        if (!candidate) {
            continue;
        }
        delete filter;
        filter = candidate;
        weight = registration.weight;
    }

    if (filter) {
        filter->setMimeType(mimetype);
    }
    return filter;
}

bool KisImportExportManager::batchMode(void) const
{
    return m_backend->batchMode();
}

void KisImportExportManager::setUpdater(KoUpdaterPtr updater)
{
    d->updater = updater;
}

PkString KisImportExportManager::askForAudioFileName(const PkString &defaultDir, PkWidget *parent)
{
    return kisImportExportUiServices()->askForAudioFileName(defaultDir, parent);
}

PkString KisImportExportManager::getUriForAdditionalFile(const PkString &defaultUri, PkWidget *parent)
{
    return kisImportExportUiServices()->getUriForAdditionalFile(defaultUri, parent);
}

KisImportExportManager::ConversionResult KisImportExportManager::convert(KisImportExportManager::Direction direction, const PkString &location, const PkString& realLocation, const PkString &mimeType, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration, bool isAsync, bool isAdvancedExporting)
{
    // export configuration is supported for export only
    KIS_SAFE_ASSERT_RECOVER_NOOP(direction == Export || !bool(exportConfiguration));

    PkString typeName = mimeType;
    if (typeName.isEmpty()) {
        typeName = KisMimeDatabase::mimeTypeForFile(location, direction == KisImportExportManager::Export ? false : true);
    }

    PkSharedPointer<KisImportExportFilter> filter;

    /**
     * Fetching a filter from the registry is a really expensive operation,
     * because it blocks all the threads. Cache the filter if possible.
     */
    if (direction == KisImportExportManager::Export &&
            d->cachedExportFilter &&
            d->cachedExportFilterMimeType == typeName) {

        filter = d->cachedExportFilter;
    } else {

        filter = PkSharedPointer<KisImportExportFilter>(filterForMimeType(typeName, direction));

        if (direction == Export) {
            d->cachedExportFilter = filter;
            d->cachedExportFilterMimeType = typeName;
        }
    }

    if (!filter) {
        return KisImportExportErrorCode(ImportExportCodes::FileFormatNotSupported);
    }

    filter->setFilename(location);
    filter->setRealFilename(realLocation);
    filter->setBatchMode(batchMode());
    filter->setMimeType(typeName);

    if (direction == Import) {
        filter->setImportUserFeedBackInterface(m_backend->createImportFeedback());
    }

    if (!d->updater.isNull()) {
        // WARNING: The updater is not guaranteed to be persistent! If you ever want
        // to add progress reporting to "Save also as .kra", make sure you create
        // a separate KoProgressUpdater for that!

        // WARNING2: the failsafe completion of the updater happens in the destructor
        // the filter.

        filter->setUpdater(d->updater);
    }

    PkByteArray from, to;
    if (direction == Export) {
        from = m_backend->nativeFormatMimeType();
        to = pkToByteArray(typeName);
    }
    else {
        from = pkToByteArray(typeName);
        to = m_backend->nativeFormatMimeType();
    }

    KIS_ASSERT_RECOVER_RETURN_VALUE(
                direction == Import || direction == Export,
                KisImportExportErrorCode(ImportExportCodes::InternalError)); // "bad conversion graph"

    ConversionResult result = KisImportExportErrorCode(ImportExportCodes::OK);
    if (direction == Import) {

        KisUsageLogger::log(PkString("Importing %1 to %2. Location: %3. Real location: %4. Batchmode: %5")
                                .arg(pkFromByteArray(from), pkFromByteArray(to), location)
                                .arg(realLocation)
                                .arg(pkNumber(static_cast<int>(batchMode()))));

        // async importing is not yet supported!
        KIS_SAFE_ASSERT_RECOVER_NOOP(!isAsync);

        // FIXME: Dmitry says "this progress reporting code never worked. Initial idea was to implement it his way, but I stopped and didn't finish it"
        if (0 && !batchMode()) {
            result = m_backend->runAction(PkString("Opening document..."), std::bind(&KisImportExportManager::doImport, this, location, filter));
        } else {
            result = doImport(location, filter);
        }
        if (result.status().isOk()) {
            KisImageSP image = m_backend->image();
            if (image) {
                KIS_SAFE_ASSERT_RECOVER(image->colorSpace() != nullptr && image->colorSpace()->profile() != nullptr)
                {
                    qWarning() << "Loaded a profile-less file without a fallback. Rejecting image "
                                  "opening";
                    return KisImportExportErrorCode(ImportExportCodes::InternalError);
                }
                KisUsageLogger::log(PkString("Loaded image from %1. Size: %2 * %3 pixels, %4 dpi. Color "
                                             "model: %6 %5 (%7). Layers: %8")
                                        .arg(pkFromByteArray(from))
                                        .arg(pkNumber(image->width()))
                                        .arg(pkNumber(image->height()))
                                        .arg(pkNumber(image->xRes()))
                                        .arg(image->colorSpace()->colorModelId().name())
                                        .arg(image->colorSpace()->colorDepthId().name())
                                        .arg(image->colorSpace()->profile()->name())
                                        .arg(pkNumber(image->nlayers())));
            } else {
                qWarning() << "The filter returned OK, but there is no image";
            }

        }
        else {
            KisUsageLogger::log(PkString("Failed to load image from %1").arg(pkFromByteArray(from)));
        }

    }
    else /* if (direction == Export) */ {
        if (!exportConfiguration) {
            exportConfiguration = filter->lastSavedConfiguration(from, to);
        }

        if (exportConfiguration) {
            fillStaticExportConfigurationProperties(exportConfiguration);
        }

        bool alsoAsKra = false;
        bool askUser = askUserAboutExportConfiguration(filter, exportConfiguration,
                                                       from, to,
                                                       batchMode(), showWarnings,
                                                       &alsoAsKra, isAdvancedExporting);


        if (!batchMode() && !askUser) {
            return KisImportExportErrorCode(ImportExportCodes::Cancelled);
        }

        KisUsageLogger::log(
            PkString("Converting from %1 to %2. Location: %3. Real location: %4. Batchmode: %5. Configuration: %6")
                .arg(pkFromByteArray(from), pkFromByteArray(to), location)
                .arg(realLocation)
                .arg(pkNumber(static_cast<int>(batchMode())))
                .arg(exportConfiguration ? exportConfiguration->toXML() : PkString("none")));

        const PkString alsoAsKraLocation = alsoAsKra ? getAlsoAsKraLocation(location) : PkString();
        if (isAsync) {
            // 原 QtConcurrent::run 从线程池取线程；std::async(std::launch::async) 每次
            // 调用新起一个线程（无池）。语义一致：doExport 在后台线程跑，返回的 future
            // 由调用方（KisDocument 的 PkTimer 轮询）收尾。行为差异登记在 Task 8 报告。
            result = ConversionResult(std::async(std::launch::async,
                                                 std::bind(&KisImportExportManager::doExport, this, location, filter,
                                                           exportConfiguration, alsoAsKraLocation)));

            // we should explicitly report that the exporting has been initiated
            result.setStatus(ImportExportCodes::OK);

        } else if (!batchMode()) {
            result = m_backend->runAction(PkString("Saving document..."), std::bind(&KisImportExportManager::doExport, this, location, filter,
                                           exportConfiguration, alsoAsKraLocation));
        } else {
            result = doExport(location, filter, exportConfiguration, alsoAsKraLocation);
        }

        if (exportConfiguration && !batchMode()) {
            m_backend->saveExportConfiguration(pkToByteArray(typeName), exportConfiguration);
        }
    }
    return result;
}

void KisImportExportManager::fillStaticExportConfigurationProperties(KisPropertiesConfigurationSP exportConfiguration, KisImageSP image)
{
    KisPaintDeviceSP dev = image->projection();
    const KoColorSpace* cs = dev->colorSpace();
    const bool isThereAlpha =
            KisPainter::checkDeviceHasTransparency(image->projection());

    exportConfiguration->setProperty(KisImportExportFilter::ImageContainsTransparencyTag, isThereAlpha);
    exportConfiguration->setProperty(KisImportExportFilter::ColorModelIDTag, cs->colorModelId().id());
    exportConfiguration->setProperty(KisImportExportFilter::ColorDepthIDTag, cs->colorDepthId().id());

    // 原 contains("srgb", 大小写不敏感)：PkString 无 CaseInsensitive 标志，用
    // toLower() 等价；"g10" 检测原为大小写敏感，保持原语义。
    const PkString profileName = cs->profile()->name();
    const bool sRGB =
            (profileName.toLower().contains(PkString("srgb")) &&
             !profileName.contains(PkString("g10")));
    exportConfiguration->setProperty(KisImportExportFilter::sRGBTag, sRGB);

    ColorPrimaries primaries = cs->profile()->getColorPrimaries();
    if (primaries >= PRIMARIES_ADOBE_RGB_1998) {
        primaries = PRIMARIES_UNSPECIFIED;
    }
    TransferCharacteristics transferFunction =
        cs->profile()->getTransferCharacteristics();
    if (transferFunction >= TRC_GAMMA_1_8) {
        transferFunction = TRC_UNSPECIFIED;
    }
    exportConfiguration->setProperty(KisImportExportFilter::CICPPrimariesTag,
                                     static_cast<int>(primaries));
    exportConfiguration->setProperty(
        KisImportExportFilter::CICPTransferCharacteristicsTag,
        static_cast<int>(transferFunction));
    exportConfiguration->setProperty(KisImportExportFilter::HDRTag, cs->hasHighDynamicRange());
}


void KisImportExportManager::fillStaticExportConfigurationProperties(KisPropertiesConfigurationSP exportConfiguration)
{
    return fillStaticExportConfigurationProperties(exportConfiguration, m_backend->image());
}

bool KisImportExportManager::askUserAboutExportConfiguration(
        PkSharedPointer<KisImportExportFilter> filter,
        KisPropertiesConfigurationSP exportConfiguration,
        const PkByteArray &from,
        const PkByteArray &to,
        const bool batchMode,
        const bool showWarnings,
        bool *alsoAsKra,
        bool isAdvancedExporting)
{
    return m_backend->askExportConfiguration(filter.data(), exportConfiguration, from, to,
                                             batchMode, showWarnings, alsoAsKra,
                                             isAdvancedExporting);
}

KisImportExportErrorCode KisImportExportManager::doImport(const PkString &location, PkSharedPointer<KisImportExportFilter> filter)
{
    // 原打开前先做文件存在性检查。
    if (!std::filesystem::exists(location.PkToUtf8())) {
        return ImportExportCodes::FileNotExist;
    }

    PkFileStream file(location);
    if (filter->supportsIO() && !file.open(PkStream::ReadOnly)) {
        // 原 KisImportExportErrorCannotRead(file.error())：PkStream 无错误码 API，
        // 统一归一为 PkOpenError（Task 4 先例）。
        return KisImportExportErrorCode(KisImportExportErrorCannotRead(PkOpenError));
    }

    KisImportExportErrorCode status = filter->convert(m_backend->document(), &file, KisPropertiesConfigurationSP());

    if (file.isOpen()) {
        file.close();
    }

    return status;
}

KisImportExportErrorCode KisImportExportManager::doExport(const PkString &location,
                                                          PkSharedPointer<KisImportExportFilter> filter,
                                                          KisPropertiesConfigurationSP exportConfiguration,
                                                          const PkString alsoAsKraLocation)
{
    KisImportExportErrorCode status =
            doExportImpl(location, filter, exportConfiguration);

    if (!alsoAsKraLocation.isEmpty() && status.isOk()) {
        PkByteArray mime = m_backend->nativeFormatMimeType();
        PkSharedPointer<KisImportExportFilter> filter(
                    filterForMimeType(pkFromByteArray(mime), Export));

        KIS_SAFE_ASSERT_RECOVER_NOOP(filter);

        if (filter) {
            filter->setFilename(alsoAsKraLocation);

            KisPropertiesConfigurationSP kraExportConfiguration =
                    filter->lastSavedConfiguration(mime, mime);

            status = doExportImpl(alsoAsKraLocation, filter, kraExportConfiguration);
        } else {
            status = ImportExportCodes::FileFormatIncorrect;
        }
    }

    return status;
}

// 剥离说明：Linux 分支原用原子写 API（临时文件 + commit 时改名，规避网盘对目标
// 文件的锁）；macOS/Android 沙箱里原子写不可靠，改走「临时文件 + 复制」。本实现
// Linux 分支用 PkFileStream 直接写目标（WriteOnly|Truncate），原子改名语义不保留
// （行为差异登记在 Task 8 报告）；非 Linux 分支保留临时文件结构。
#if !(defined(_WIN32) || defined(__APPLE__) || defined(__ANDROID__))

KisImportExportErrorCode KisImportExportManager::doExportImpl(const PkString &location, PkSharedPointer<KisImportExportFilter> filter, KisPropertiesConfigurationSP exportConfiguration)
{
    // Linux（主分支）。原原子写 API 的 setDirectWriteFallback(true) 原意是退化直写，
    // 这里恒直写目标文件。原 getFileOpenError(file)：PkStream 无错误码 API，归一
    // PkOpenError。
    PkFileStream file(location);
    if (filter->supportsIO() && !file.open(PkStream::WriteOnly | PkStream::Truncate)) {
        return KisImportExportErrorCannotWrite(PkOpenError);
    }

    KisImportExportErrorCode status = filter->convert(m_backend->document(), &file, exportConfiguration);

    if (filter->supportsIO()) {
        if (!status.isOk()) {
            // 原 cancelWriting() 取消落盘；直写模式下无撤销手段（目标已写入部分
            // 内容，与原子写 API 的直写退化模式一致），仍 close 落盘。
            file.close();
        } else {
            // 原 commit() 改名失败检测不保留：直写无 rename 步骤可失败，
            // flush() 失败视为写盘失败。
            if (!file.flush()) {
                qWarning() << "Could not flush export file" << location;
                status = KisImportExportErrorCannotWrite(PkOpenError);
            }
            file.close();
        }
    }

    if (status.isOk()) {
        // Do some minimal verification
        PkString verificationResult = filter->verify(location);
        if (!verificationResult.isEmpty()) {
            status = KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
            m_backend->setErrorMessage(verificationResult);
        }
    }

    return status;
}

#else

KisImportExportErrorCode KisImportExportManager::doExportImpl(const PkString &location, PkSharedPointer<KisImportExportFilter> filter, KisPropertiesConfigurationSP exportConfiguration)
{
    // 非 Linux（Windows/macOS/Android 沙箱）：先写唯一临时路径，再复制到目标。
    // 原临时文件 API 由 Qt 保证全局唯一；PkFileStream 无自动临时文件，用
    // temp_directory_path() + 进程内计数器构造唯一路径。
    static long long s_tmpCounter = 0;
    const PkString tmpLocation =
            PkString(std::filesystem::temp_directory_path().string().c_str()) +
            PkString("/.kra_tmp_") + pkNumber(++s_tmpCounter) + PkString(".kra");
    PkFileStream file(tmpLocation);
    if (filter->supportsIO() && !file.open(PkStream::ReadWrite | PkStream::Truncate)) {
        return KisImportExportErrorCannotWrite(PkOpenError);
    }

    KisImportExportErrorCode status = filter->convert(m_backend->document(), &file, exportConfiguration);

    if (filter->supportsIO()) {
        if (status.isOk()) {
#if defined(__ANDROID__)
            // Android 沙箱：目标文件必须显式截断（打开写不自动清空旧内容），
            // 手动逐块拷贝 + 长度校验（原 File::copy 在沙箱里不可靠）。
            if (file.isOpen()) {
                if (!file.flush()) {
                    return KisImportExportErrorCannotWrite(PkOpenError);
                }
            } else if (!file.open(PkStream::ReadWrite)) {
                return KisImportExportErrorCannotWrite(PkOpenError);
            }

            // 期望写入长度，供最后校验。
            const PkStream::pk_int64 expectedSize = file.size();
            if (expectedSize < 0 || !file.seek(0)) {
                return KisImportExportErrorCannotWrite(PkOpenError);
            }

            // 目标文件显式 Truncate。
            PkFileStream target(location);
            if (!target.open(PkStream::WriteOnly | PkStream::Truncate)) {
                return KisImportExportErrorCannotWrite(PkOpenError);
            }

            // 手动按 BUFSIZ 分块读写。
            char buf[BUFSIZ];
            PkStream::pk_int64 totalWritten = 0;
            while (true) {
                PkStream::pk_int64 read = file.read(buf, BUFSIZ);
                if (read < 0) {
                    return KisImportExportErrorCannotWrite(PkOpenError);
                } else if (read == 0) {
                    break;
                } else {
                    PkStream::pk_int64 written = target.write(buf, read);
                    if (written < 0) {
                        return KisImportExportErrorCannotWrite(PkOpenError);
                    }
                    totalWritten += written;
                }
            }

            file.close();
            if (!target.flush()) {
                return KisImportExportErrorCannotWrite(PkOpenError);
            }
            target.close();

            // 校验实际写入长度。
            if (totalWritten != expectedSize) {
                return KisImportExportErrorCannotWrite(PkCopyError);
            }
#else
            // Windows/macOS 非 Android：临时文件复制到目标（覆盖已有文件）。
            file.flush();
            file.close();
            std::error_code ec;
            std::filesystem::copy_file(tmpLocation.PkToUtf8(), location.PkToUtf8(),
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                // 原 File::copy 失败后保留临时文件便于排查；PkFileStream 无
                // autoRemove，失败即报错，临时文件由 OS 清理。
                return KisImportExportErrorCannotWrite(PkOpenError);
            }
#endif
        }
    }

    if (status.isOk()) {
        // Do some minimal verification
        PkString verificationResult = filter->verify(location);
        if (!verificationResult.isEmpty()) {
            status = KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
            m_backend->setErrorMessage(verificationResult);
        }
    }

    return status;
}

#endif

PkString KisImportExportManager::getAlsoAsKraLocation(const PkString location) const
{
#if defined(__ANDROID__)
    return getUriForAdditionalFile(location, nullptr);
#else
    return location + PkString(".kra");
#endif
}
