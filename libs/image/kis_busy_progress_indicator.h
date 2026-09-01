/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_BUSY_PROGRESS_INDICATOR_H
#define __KIS_BUSY_PROGRESS_INDICATOR_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkSignalCompat.h>

class KoProgressProxy;

class KisBusyProgressIndicator : public PkShellObject
{
public:
    explicit KisBusyProgressIndicator(KoProgressProxy *progressProxy);
    ~KisBusyProgressIndicator() override;

public:
    /**
     * To be called when progressProxy is and will be no longer available
     * and this object is going to be deleted as well.
     */
    void prepareDestroying();

public:
    /**
     * Trigger update of progress state.
     */
    void update();

private:
    /**
     * Call only via emitting sigStartTimer, to ensure it is called in
     * the context of the PkShellObject's thread.
     */
    void slotStartTimer();
    void timerFinished();

signals:
    void sigStartTimer();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_BUSY_PROGRESS_INDICATOR_H */
