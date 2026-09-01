/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSELECTIONUPDATECOMPRESSOR_H
#define KISSELECTIONUPDATECOMPRESSOR_H

#include "kritaimage_export.h"
#include "kis_thread_safe_signal_compressor.h"

#include "kis_types.h"
#include <PkRect.h>


class KisSelectionUpdateCompressor : public PkShellObject
{
public:
    KisSelectionUpdateCompressor(KisSelection *selection);
    ~KisSelectionUpdateCompressor();

public:
    void requestUpdate(const PkRect &updateRect);
    void tryProcessStalledUpdate();

private:
    void startUpdateJob();

private:
    KisSelection *m_parentSelection {0};
    KisThreadSafeSignalCompressor *m_updateSignalCompressor {0};
    PkRect m_updateRect;
    bool m_fullUpdateRequested {false};

    bool m_hasStalledUpdate {false};
};

#endif // KISSELECTIONUPDATECOMPRESSOR_H
