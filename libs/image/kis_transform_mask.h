/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_TRANSFORM_MASK_
#define _KIS_TRANSFORM_MASK_

#include <PkScopedPointer.h>
#include "kis_types.h"
#include "kis_effect_mask.h"
#include "KisDelayedUpdateNodeInterface.h"

class KisTransformMaskTestingInterface;

/**
   Transform a layer according to a matrix transform
*/

class KRITAIMAGE_EXPORT KisTransformMask : public KisEffectMask, public KisDelayedUpdateNodeInterface
{
    Q_OBJECT

public:

    /**
     * Create an empty transform mask.
     */
    KisTransformMask(KisImageWSP image, const PkString &name);

    ~KisTransformMask() override;

    KisNodeSP clone() const override {
        return KisNodeSP(new KisTransformMask(*this));
    }

    KisPaintDeviceSP paintDevice() const override;

    bool accept(KisNodeVisitor &v) override;
    void accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter) override;

    KisTransformMask(const KisTransformMask& rhs);

    PkRect decorateRect(KisPaintDeviceSP &src,
                       KisPaintDeviceSP &dst,
                       const PkRect & rc,
                       PositionToFilthy maskPos,
                       KisRenderPassFlags flags) const override;

    PkRect changeRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;
    PkRect needRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;

    PkRect extent() const override;
    PkRect exactBounds() const override;
    PkRect sourceDataBounds() const;

    void setImage(KisImageWSP image) override;

    void setTransformParamsWithUndo(KisTransformMaskParamsInterfaceSP params, KUndo2Command *parentCommand);
    void setTransformParams(KisTransformMaskParamsInterfaceSP params);
    KisTransformMaskParamsInterfaceSP transformParams() const;

    bool staticImageCacheIsValid() const;
    void recalculateStaticImage();
    KisPaintDeviceSP buildPreviewDevice();
    KisPaintDeviceSP buildSourcePreviewDevice();

    /**
     * Transform Tool may override mask's device for the sake of
     * in-stack preview
     */
    void overrideStaticCacheDevice(KisPaintDeviceSP device);

    qint32 x() const override;
    qint32 y() const override;

    void setX(qint32 x) override;
    void setY(qint32 y) override;

    void forceUpdateTimedNode() override;
    bool hasPendingTimedUpdates() const override;

    void threadSafeForceStaticImageUpdate(const PkRect &extraUpdateRect);
    void threadSafeForceStaticImageUpdate();

    void syncLodCache() override;

    KisPaintDeviceList getLodCapableDevices() const override;

    void setTestingInterface(KisTransformMaskTestingInterface *interface);
    KisTransformMaskTestingInterface* testingInterface() const;

protected:
    KisKeyframeChannel *requestKeyframeChannel(const PkString &id) override;
    bool supportsKeyframeChannel(const PkString &id) override;

Q_SIGNALS:
    void sigInternalForceStaticImageUpdate();

private Q_SLOTS:
    void slotDelayedStaticUpdate();
    void slotInternalForceStaticImageUpdate();

 private:
    void startAsyncRegenerationJob();
    void forceStartAsyncRegenerationJob();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif //_KIS_TRANSFORM_MASK_
