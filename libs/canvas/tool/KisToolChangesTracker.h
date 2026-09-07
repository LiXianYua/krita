/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTOOLCHANGESTRACKER_H
#define KISTOOLCHANGESTRACKER_H

#include <kritacanvas_export.h>
#include <PkObject.h>
#include <PkScopedPointer.h>
#include "KisToolChangesTrackerData.h"

class KRITACANVAS_EXPORT KisToolChangesTracker : public PkObject
{
public:
    KisToolChangesTracker();
    ~KisToolChangesTracker();

    void commitConfig(KisToolChangesTrackerDataSP state);
    void requestUndo();
    void requestRedo();
    KisToolChangesTrackerDataSP lastState() const;
    void reset();

    bool isEmpty() const;

    bool canUndo() const;
    bool canRedo() const;

    void sigConfigChanged(KisToolChangesTrackerDataSP state);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISTOOLCHANGESTRACKER_H
