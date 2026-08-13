/*
 * SPDX-FileCopyrightText: 2026 OpenAI
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISSELECTIONCLIPSTORE_H
#define KISSELECTIONCLIPSTORE_H

#include <QPoint>

#include <kis_types.h>

#include "kritalibkis_export.h"

/**
 * Process-local clipboard for the non-UI scripting selection API.
 *
 * The application clipboard also serializes images through QClipboard and
 * presents format-choice dialogs.  Selection scripting only needs to pass a
 * paint device between copy/cut and paste, so keeping that device in its own
 * domain avoids pulling the application UI into kritalibkis.
 */
class KRITALIBKIS_EXPORT KisSelectionClipStore
{
public:
    static KisSelectionClipStore *instance();

    void setClip(KisPaintDeviceSP clip, const QPoint &topLeft);
    KisPaintDeviceSP clip() const;
    QPoint topLeft() const;
    bool hasClip() const;
    void clear();

private:
    KisSelectionClipStore() = default;

    KisPaintDeviceSP m_clip;
    QPoint m_topLeft;
};

#endif // KISSELECTIONCLIPSTORE_H
