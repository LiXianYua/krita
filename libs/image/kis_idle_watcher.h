/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_IDLE_WATCHER_H
#define __KIS_IDLE_WATCHER_H

#include "kritaimage_export.h"

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkSignalCompat.h>

#include "kis_types.h"


class KRITAIMAGE_EXPORT KisIdleWatcher : public PkShellObject
{
public:
    KisIdleWatcher(int delay = 200, PkObject* parent = 0);
    ~KisIdleWatcher() override;

    bool isIdle() const;
    bool isCounting() const;

    void setTrackedImages(const PkVector<KisImageSP> &images);
    void setTrackedImage(KisImageSP image);

    //Force to image modified state and start countdown to event
    void forceImageModified() { slotImageModified(); }
    void restartCountdown();
    void triggerCountdownNoDelay();

signals:
    void startedIdleMode();
    void imageModified();

private:
    void slotImageModified();
    void slotIdleCheckTick();

    void startIdleCheck();
    void stopIdleCheck();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_IDLE_WATCHER_H */
