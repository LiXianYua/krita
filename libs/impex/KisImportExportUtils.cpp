/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisImportExportUtils.h"

// KisImportExportBackend.h 尚未剥离（Task 7 范围），其字节数组类型无 pk compat 头，
// 直接包含会编译失败。此处用最小类声明替代（只含本文件用到的 chooseColorSpace，
// 其余三个纯虚接口壳闭包内无人调用），待 Task 7 完成后切回真包含。
// #include "KisImportExportBackend.h"
class PkWidget;
class KoColorSpace;
class KisImportExportUiServices {
public:
    virtual ~KisImportExportUiServices() = default;
    virtual bool chooseColorSpace(PkWidget *, const KoColorSpace *, const KoColorSpace **, int *, int *) = 0;
};
KisImportExportUiServices *kisImportExportUiServices();

#include <KoColorSpaceRegistry.h>
#include <KoColorSpace.h>
#include <KoColorProfile.h>
#include "kis_image.h"
#include "KisImportUserFeedbackInterface.h"

namespace KritaUtils {

KisImportExportErrorCode workaroundUnsuitableImageColorSpace(KisImageSP image,
                                                             KisImportUserFeedbackInterface *feedbackInterface,
                                                             KisImageBarrierLock &lock)
{
    const KoColorSpace *replacementColorSpace = 0;
    KoColorConversionTransformation::Intent replacementColorSpaceIntent = KoColorConversionTransformation::internalRenderingIntent();
    KoColorConversionTransformation::ConversionFlags replacementColorSpaceConversionFlags = KoColorConversionTransformation::internalConversionFlags();

    const KoColorSpace *colorSpace = image->colorSpace();
    const KoColorProfile *profile = colorSpace->profile();

    if (profile && !profile->isSuitableForOutput()) {
        /// The profile has no reverse mapping into for the described color space,
        /// so we cannot use it in Krita. We need to ask the user to convert the image
        /// right on loading

        KIS_SAFE_ASSERT_RECOVER_NOOP(feedbackInterface);
        if (feedbackInterface) {
            KisImportUserFeedbackInterface::Result result =
                feedbackInterface->askUser([&] (PkWidget *parent) {
                    const KoColorSpace* fallbackColorSpace =
                        KoColorSpaceRegistry::instance()->colorSpace(
                            colorSpace->colorModelId().id(),
                            colorSpace->colorDepthId().id(),
                            nullptr);

                    int intent = int(replacementColorSpaceIntent);
                    int flags = int(replacementColorSpaceConversionFlags);
                    const bool accepted = kisImportExportUiServices() &&
                        kisImportExportUiServices()->chooseColorSpace(parent, fallbackColorSpace,
                                                                      &replacementColorSpace,
                                                                      &intent, &flags);
                    replacementColorSpaceIntent = KoColorConversionTransformation::Intent(intent);
                    replacementColorSpaceConversionFlags = KoColorConversionTransformation::ConversionFlags(flags);
                    return accepted;
                });

            if (result == KisImportUserFeedbackInterface::SuppressedByBatchMode) {
                return ImportExportCodes::FormatColorSpaceUnsupported;
            } else if (result == KisImportUserFeedbackInterface::UserCancelled) {
                return ImportExportCodes::Cancelled;
            }
        }
    }

    if (replacementColorSpace) {
        /**
         * Here is an extremely tricky part! First we start the conversion
         * stroke, and only **after that** we unlock the image. The point is
         * that KisDelayedUpdateNodeInterface-based nodes are forbidden to
         * start their update jobs while the image is locked, so this
         * guarantees that no stroke will be started before we actually convert
         * the image into something usable
         */
        image->convertImageColorSpace(replacementColorSpace,
                                        replacementColorSpaceIntent,
                                        replacementColorSpaceConversionFlags);
        lock.unlock();
        image->waitForDone();
    }

    return ImportExportCodes::OK;
}

}
