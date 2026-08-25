/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_image_signal_router.h"

#include <PkThread.h>
#include "kis_image.h"


KisImageSignalRouter::KisImageSignalRouter(KisImageWSP image)
    : PkShellObject(image.data()),
      m_image(image)
{
    PkObject::connect(this, &KisImageSignalRouter::sigNotification,
                      this, &KisImageSignalRouter::slotNotification);

    PkObject::connect(this, &KisImageSignalRouter::sigImageModified,
                      m_image.data(), &KisImage::sigImageModified, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigImageModifiedWithoutUndo,
                      m_image.data(), &KisImage::sigImageModifiedWithoutUndo, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigSizeChanged,
                      m_image.data(), &KisImage::sigSizeChanged, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigResolutionChanged,
                      m_image.data(), &KisImage::sigResolutionChanged, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigRequestNodeReselection,
                      m_image.data(), &KisImage::sigRequestNodeReselection, PkConnectionType::Direct);

    PkObject::connect(this, &KisImageSignalRouter::sigNodeChanged,
                      m_image.data(), &KisImage::sigNodeChanged, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigNodeAddedAsync,
                      m_image.data(), &KisImage::sigNodeAddedAsync, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigRemoveNodeAsync,
                      m_image.data(), &KisImage::sigRemoveNodeAsync, PkConnectionType::Direct);
    PkObject::connect(this, &KisImageSignalRouter::sigLayersChangedAsync,
                      m_image.data(), &KisImage::sigLayersChangedAsync, PkConnectionType::Direct);

    /**
     * Color space and profile conversion functions run without strokes,
     * therefore they are executed in GUI thread under the global lock held.
     *
     * To ensure that the receiver of the signal will not deadlock by
     * barrier-locking the image, we should make these signals queued.
     *
     * NOTE (R-30): a Pk queued connection is delivered through the target
     * thread's PkThreadCallQueue. The receiving (GUI) thread must have
     * called PkThreadCallQueue::warmUpCurrentThread() before publishing
     * its thread id anywhere (and keep pumping via processPendingCalls()),
     * otherwise the queued delivery has no pump to drain into.
     */

    PkObject::connect(this, &KisImageSignalRouter::sigProfileChanged,
                      m_image.data(), &KisImage::sigProfileChanged, PkConnectionType::Queued);
    PkObject::connect(this, &KisImageSignalRouter::sigColorSpaceChanged,
                      m_image.data(), &KisImage::sigColorSpaceChanged, PkConnectionType::Queued);
}

KisImageSignalRouter::~KisImageSignalRouter()
{
}

void KisImageSignalRouter::emitImageModifiedNotification()
{
    Q_EMIT sigImageModified();
}

void KisImageSignalRouter::emitNotifications(KisImageSignalVector notifications)
{
    Q_FOREACH (const KisImageSignalType &type, notifications) {
        emitNotification(type);
    }
}

void KisImageSignalRouter::emitNotification(KisImageSignalType type)
{
    /**
     * All the notifications except LayersChangedSignal should go in a
     * queued way. And LayersChangedSignal should be delivered to the
     * recipients in a non-reordered way
     */

    if (type.id == LayersChangedSignal ||
        type.id == NodeReselectionRequestSignal ||
        type.id == SizeChangedSignal) {
        slotNotification(type);
    } else {
        Q_EMIT sigNotification(type);
    }
}

void KisImageSignalRouter::emitNodeChanged(KisNodeSP node)
{
    Q_EMIT sigNodeChanged(node);
}

void KisImageSignalRouter::emitNodeHasBeenAdded(KisNode *parent, int index, KisNodeAdditionFlags flags)
{
    KisNodeSP newNode = parent->at(index);

    // overlay selection masks reset frames themselves
    if (!newNode->inherits("KisSelectionMask")) {
        KisImageSP image = m_image.toStrongRef();
        if (image) {
            image->invalidateAllFrames();
        }
    }

    Q_EMIT sigNodeAddedAsync(newNode, flags);
}

void KisImageSignalRouter::emitAboutToRemoveANode(KisNode *parent, int index)
{
    KisNodeSP removedNode = parent->at(index);

    // overlay selection masks reset frames themselves
    if (!removedNode->inherits("KisSelectionMask")) {
        KisImageSP image = m_image.toStrongRef();
        if (image) {
            image->invalidateAllFrames();
        }
    }

    Q_EMIT sigRemoveNodeAsync(removedNode);
}

void KisImageSignalRouter::emitRequestLodPlanesSyncBlocked(bool value)
{
    Q_EMIT sigRequestLodPlanesSyncBlocked(value);
}

void KisImageSignalRouter::emitNotifyBatchUpdateStarted()
{
    Q_EMIT sigNotifyBatchUpdateStarted();
}

void KisImageSignalRouter::emitNotifyBatchUpdateEnded()
{
    Q_EMIT sigNotifyBatchUpdateEnded();
}

void KisImageSignalRouter::slotNotification(KisImageSignalType type)
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }

    switch(type.id) {
    case LayersChangedSignal:
        image->invalidateAllFrames();
        Q_EMIT sigLayersChangedAsync();
        break;
    case ModifiedWithoutUndoSignal:
        Q_EMIT sigImageModifiedWithoutUndo();
        break;
    case SizeChangedSignal:
        image->invalidateAllFrames();
        Q_EMIT sigSizeChanged(type.sizeChangedSignal.oldStillPoint,
                            type.sizeChangedSignal.newStillPoint);
        break;
    case ProfileChangedSignal:
        image->invalidateAllFrames();
        Q_EMIT sigProfileChanged(image->profile());
        break;
    case ColorSpaceChangedSignal:
        image->invalidateAllFrames();
        Q_EMIT sigColorSpaceChanged(image->colorSpace());
        break;
    case ResolutionChangedSignal:
        image->invalidateAllFrames();
        Q_EMIT sigResolutionChanged(image->xRes(), image->yRes());
        break;
    case NodeReselectionRequestSignal:
        if (type.nodeReselectionSignal.newActiveNode ||
            !type.nodeReselectionSignal.newSelectedNodes.isEmpty()) {

            Q_EMIT sigRequestNodeReselection(type.nodeReselectionSignal.newActiveNode,
                                           type.nodeReselectionSignal.newSelectedNodes);
        }
        break;
    }
}
